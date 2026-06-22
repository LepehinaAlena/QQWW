//g++ -O2 -pthread qwqw.cpp -o qwqw

#include <iostream>
#include <thread>
#include <atomic>
using namespace std;

const int RUNS = 10'000'000;

volatile int x = 0;
volatile int y = 0;

int r1 = 0;
int r2 = 0;

atomic<int> start_counter{0};
atomic<int> done_counter{0};
atomic<long long> violations{0};

void thread1() {
    for (int i = 0; i < RUNS; i++) {
        while (start_counter.load(memory_order_acquire) != i + 1) {
            this_thread::yield();
        }
        x = 1;      // store
        r1 = y;     // load
        done_counter.fetch_add(1, memory_order_release);
    }
}

void thread2() {
    for (int i = 0; i < RUNS; i++) {
        while (start_counter.load(memory_order_acquire) != i + 1) {
            this_thread::yield();
        }
        y = 1;      // store
        r2 = x;     // load
        done_counter.fetch_add(1, memory_order_release);
    }
}

void coordinator() {
    for (int i = 0; i < RUNS; i++) {
        while (done_counter.load(memory_order_acquire) != i * 2) {
            this_thread::yield();
        }

        if (i > 0) {
            if (r1 == 0 && r2 == 0) {
                violations.fetch_add(1, memory_order_relaxed);
            }
        }

        x = 0;
        y = 0;
        r1 = 0;
        r2 = 0;

        start_counter.store(i + 1, memory_order_release);
    }

    while (done_counter.load(memory_order_acquire) != RUNS * 2) {
        this_thread::yield();
    }

    if (r1 == 0 && r2 == 0) {
        violations.fetch_add(1, memory_order_relaxed);
    }
}

int main() {
    cout<<"Runs:  "<<RUNS<<endl;
    thread t1(thread1);
    thread t2(thread2);
    thread coord(coordinator);

    t1.join();
    t2.join();
    coord.join();

    if (violations.load() > 0) {
        cout<<"Non zero violations: "<<violations.load()<<endl;
    } else {
        cout <<"All is ok";
    }

    return 0;
}

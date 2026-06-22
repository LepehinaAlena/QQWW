// store_buffering_final.cpp
// Компиляция: g++ -O2 -pthread store_buffering_final.cpp -o store_buffering
// Запуск: ./store_buffering

#include <iostream>
#include <thread>
#include <atomic>

const int RUNS = 10'000'000;

// Тестируемые переменные (volatile)
volatile int x = 0;
volatile int y = 0;
int r1 = 0;  // результат чтения thread1
int r2 = 0;  // результат чтения thread2

// Синхронизация через coordinator
std::atomic<int> start_counter{0};  // coordinator сигнализирует начать итерацию
std::atomic<int> done_counter{0};   // потоки сигнализируют о завершении
std::atomic<long long> violations{0};

void thread1() {
    for (int i = 0; i < RUNS; i++) {
        // Ждём сигнал от coordinator начать итерацию i
        while (start_counter.load(std::memory_order_acquire) != i + 1) {
            std::this_thread::yield();
        }
        
        // Store Buffering тест
        x = 1;      // store
        r1 = y;     // load (может быть спекулятивным!)
        
        // Сигнализируем coordinator о завершении
        done_counter.fetch_add(1, std::memory_order_release);
    }
}

void thread2() {
    for (int i = 0; i < RUNS; i++) {
        // Ждём сигнал от coordinator начать итерацию i
        while (start_counter.load(std::memory_order_acquire) != i + 1) {
            std::this_thread::yield();
        }
        
        // Store Buffering тест
        y = 1;      // store
        r2 = x;     // load (может быть спекулятивным!)
        
        // Сигнализируем coordinator о завершении
        done_counter.fetch_add(1, std::memory_order_release);
    }
}

void coordinator() {
    for (int i = 0; i < RUNS; i++) {
        // Ждём, пока оба потока закончат предыдущую итерацию
        while (done_counter.load(std::memory_order_acquire) != i * 2) {
            std::this_thread::yield();
        }
        
        // Проверяем результаты предыдущей итерации (кроме первой)
        if (i > 0) {
            if (r1 == 0 && r2 == 0) {
                violations.fetch_add(1, std::memory_order_relaxed);
            }
        }
        
        // Сбрасываем переменные для следующей итерации
        x = 0;
        y = 0;
        r1 = 0;
        r2 = 0;
        
        // Сигнализируем потокам начать новую итерацию
        start_counter.store(i + 1, std::memory_order_release);
    }
    
    // Ждём последнюю итерацию
    while (done_counter.load(std::memory_order_acquire) != RUNS * 2) {
        std::this_thread::yield();
    }
    
    // Проверяем последнюю итерацию
    if (r1 == 0 && r2 == 0) {
        violations.fetch_add(1, std::memory_order_relaxed);
    }
}

int main() {
    std::cout << "=== Store Buffering Test (volatile) ===\n";
    std::cout << "Итераций: " << RUNS << "\n\n";
    
    std::thread t1(thread1);
    std::thread t2(thread2);
    std::thread coord(coordinator);
    
    t1.join();
    t2.join();
    coord.join();
    
    std::cout << "Нарушений секвенциальной консистентности: " 
              << violations.load() << "\n\n";
    
    if (violations.load() > 0) {
        std::cout << "💥 VOLATILE СЛОМАЛСЯ!\n";
        std::cout << "Процессор спекулятивно выполнил чтения до записей.\n";
        std::cout << "Оба потока увидели 0, что невозможно при строгом порядке.\n";
    } else {
        std::cout << "✓ Нарушений не обнаружено.\n";
        std::cout << "(Это не значит, что код корректен — просто повезло)\n";
    }
    
    return 0;
}

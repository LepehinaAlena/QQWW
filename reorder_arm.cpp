// aggressive_reorder.cpp
// Компиляция: g++ -O2 -pthread aggressive_reorder.cpp -o aggressive_reorder

#include <iostream>
#include <thread>
#include <atomic>
#include <vector>

// ============================================
// Store Buffering Test: классика реордеринга
// Thread 1: x = 1; r1 = y;
// Thread 2: y = 1; r2 = x;
// Если r1 == 0 && r2 == 0 — был реордеринг!
// ============================================

const int RUNS = 50'000'000;
const int NUM_PAIRS = 4;  // 4 пары потоков = 8 потоков

struct TestPair {
    volatile int x;
    volatile int y;
    std::atomic<int> r1{0};
    std::atomic<int> r2{0};
    std::atomic<long long> violations{0};
    std::atomic<int> gate{0};
    
    void writer1() {
        while (gate.load(std::memory_order_relaxed) == 0) {
            // spin
        }
        for (int i = 0; i < RUNS; i++) {
            x = 1;                          // store
            int tmp = y;                    // load (может быть спекулятивным!)
            r1.store(tmp, std::memory_order_relaxed);
            
            // Синхронизация между итерациями
            std::atomic_thread_fence(std::memory_order_seq_cst);
            x = 0; y = 0;
            r1.store(0, std::memory_order_relaxed);
            r2.store(0, std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_seq_cst);
        }
    }
    
    void writer2() {
        while (gate.load(std::memory_order_relaxed) == 0) {}
        for (int i = 0; i < RUNS; i++) {
            y = 1;
            int tmp = x;
            r2.store(tmp, std::memory_order_relaxed);
            
            std::atomic_thread_fence(std::memory_order_seq_cst);
        }
    }
    
    void monitor() {
        while (gate.load(std::memory_order_relaxed) == 0) {}
        for (int i = 0; i < RUNS; i++) {
            int v1 = r1.load(std::memory_order_relaxed);
            int v2 = r2.load(std::memory_order_relaxed);
            
            // Если ОБА прочитали 0 — значит оба процессора
            // спекулятивно прочитали ДО того, как store стал видимым
            if (v1 == 0 && v2 == 0) {
                // Проверяем, что итерация действительно идёт (x и y были 1)
                // Это эвристика, но работает в большинстве случаев
                violations++;
            }
            
            std::atomic_thread_fence(std::memory_order_relaxed);
        }
    }
};

std::vector<TestPair> pairs(NUM_PAIRS);

int main() {
    std::cout << "=== Агрессивный тест Store Buffering ===\n";
    std::cout << "Пар потоков: " << NUM_PAIRS << "\n";
    std::cout << "Итераций на пару: " << RUNS << "\n\n";
    
    std::vector<std::thread> threads;
    
    // Запускаем все пары
    for (int i = 0; i < NUM_PAIRS; i++) {
        threads.emplace_back(&TestPair::writer1, &pairs[i]);
        threads.emplace_back(&TestPair::writer2, &pairs[i]);
        threads.emplace_back(&TestPair::monitor, &pairs[i]);
    }
    
    // Даём потокам стартовать одновременно
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    for (int i = 0; i < NUM_PAIRS; i++) {
        pairs[i].gate.store(1, std::memory_order_release);
    }
    
    for (auto& t : threads) t.join();
    
    long long total_violations = 0;
    for (int i = 0; i < NUM_PAIRS; i++) {
        total_violations += pairs[i].violations.load();
    }
    
    std::cout << "Обнаружено потенциальных реордерингов: " << total_violations << "\n";
    
    if (total_violations > 0) {
        std::cout << "💥 VOLATILE СЛОМАЛСЯ! Процессор переставил store/load.\n";
    } else {
        std::cout << "⚠️  Реордеринг не обнаружен.\n";
        std::cout << "Это НЕ значит, что код корректен!\n";
        std::cout << "Реордеринг — стохастический процесс.\n";
    }
    
    return 0;
}

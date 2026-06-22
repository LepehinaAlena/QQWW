// store_buffering_correct.cpp
// Компиляция: g++ -O2 -pthread store_buffering_correct.cpp -o store_buffering
// Запуск: ./store_buffering

#include <iostream>
#include <thread>
#include <atomic>
#include <vector>

const int RUNS = 10'000'000;

// ============================================
// ТЕСТ 1: volatile (ДОЛЖЕН СЛОМАТЬСЯ на ARM)
// ============================================
struct VolatileTest {
    volatile int x = 0;
    volatile int y = 0;
    
    // Результаты для каждого потока
    std::atomic<int> r1{0};  // что прочитал thread1 из y
    std::atomic<int> r2{0};  // что прочитал thread2 из x
    
    // Барьер для синхронизации итераций
    std::atomic<int> barrier{0};
    
    // Счётчик нарушений
    std::atomic<long long> violations{0};
    
    void thread1() {
        for (int i = 0; i < RUNS; i++) {
            // Ждём, пока оба потока будут готовы
            int expected = i * 2;
            while (barrier.load(std::memory_order_relaxed) != expected) {
                std::this_thread::yield();
            }
            
            // Store Buffering тест
            x = 1;                          // store
            r1.store(y, std::memory_order_relaxed);  // load
            
            // Сигнализируем, что закончили
            barrier.fetch_add(1, std::memory_order_release);
            
            // Ждём, пока thread2 тоже закончит
            while (barrier.load(std::memory_order_relaxed) != expected + 2) {
                std::this_thread::yield();
            }
            
            // Проверяем нарушение: оба прочитали 0
            int v1 = r1.load(std::memory_order_relaxed);
            int v2 = r2.load(std::memory_order_relaxed);
            
            if (v1 == 0 && v2 == 0) {
                violations.fetch_add(1, std::memory_order_relaxed);
            }
            
            // Сбрасываем для следующей итерации
            x = 0;
            y = 0;
            r1.store(0, std::memory_order_relaxed);
            r2.store(0, std::memory_order_relaxed);
            
            // Синхронизация перед следующей итерацией
            std::atomic_thread_fence(std::memory_order_seq_cst);
        }
    }
    
    void thread2() {
        for (int i = 0; i < RUNS; i++) {
            // Ждём, пока оба потока будут готовы
            int expected = i * 2;
            while (barrier.load(std::memory_order_relaxed) != expected) {
                std::this_thread::yield();
            }
            
            // Store Buffering тест
            y = 1;                          // store
            r2.store(x, std::memory_order_relaxed);  // load
            
            // Сигнализируем, что закончили
            barrier.fetch_add(1, std::memory_order_release);
            
            // Ждём, пока thread1 тоже закончит
            while (barrier.load(std::memory_order_relaxed) != expected + 2) {
                std::this_thread::yield();
            }
            
            // Сбрасываем для следующей итерации
            x = 0;
            y = 0;
            r1.store(0, std::memory_order_relaxed);
            r2.store(0, std::memory_order_relaxed);
            
            std::atomic_thread_fence(std::memory_order_seq_cst);
        }
    }
};

// ============================================
// ТЕСТ 2: std::atomic (НЕ ДОЛЖЕН СЛОМАТЬСЯ)
// ============================================
struct AtomicTest {
    std::atomic<int> x{0};
    std::atomic<int> y{0};
    
    std::atomic<int> r1{0};
    std::atomic<int> r2{0};
    
    std::atomic<int> barrier{0};
    std::atomic<long long> violations{0};
    
    void thread1() {
        for (int i = 0; i < RUNS; i++) {
            int expected = i * 2;
            while (barrier.load(std::memory_order_relaxed) != expected) {
                std::this_thread::yield();
            }
            
            // Store Buffering тест с seq_cst
            x.store(1, std::memory_order_seq_cst);
            r1.store(y.load(std::memory_order_seq_cst), std::memory_order_relaxed);
            
            barrier.fetch_add(1, std::memory_order_release);
            
            while (barrier.load(std::memory_order_relaxed) != expected + 2) {
                std::this_thread::yield();
            }
            
            int v1 = r1.load(std::memory_order_relaxed);
            int v2 = r2.load(std::memory_order_relaxed);
            
            if (v1 == 0 && v2 == 0) {
                violations.fetch_add(1, std::memory_order_relaxed);
            }
            
            x.store(0, std::memory_order_relaxed);
            y.store(0, std::memory_order_relaxed);
            r1.store(0, std::memory_order_relaxed);
            r2.store(0, std::memory_order_relaxed);
            
            std::atomic_thread_fence(std::memory_order_seq_cst);
        }
    }
    
    void thread2() {
        for (int i = 0; i < RUNS; i++) {
            int expected = i * 2;
            while (barrier.load(std::memory_order_relaxed) != expected) {
                std::this_thread::yield();
            }
            
            y.store(1, std::memory_order_seq_cst);
            r2.store(x.load(std::memory_order_seq_cst), std::memory_order_relaxed);
            
            barrier.fetch_add(1, std::memory_order_release);
            
            while (barrier.load(std::memory_order_relaxed) != expected + 2) {
                std::this_thread::yield();
            }
            
            x.store(0, std::memory_order_relaxed);
            y.store(0, std::memory_order_relaxed);
            r1.store(0, std::memory_order_relaxed);
            r2.store(0, std::memory_order_relaxed);
            
            std::atomic_thread_fence(std::memory_order_seq_cst);
        }
    }
};

int main() {
    std::cout << "=== Честный тест Store Buffering ===\n";
    std::cout << "Итераций: " << RUNS << "\n\n";
    
    // Тест volatile
    std::cout << "[1] Тест с volatile:\n";
    VolatileTest vtest;
    std::thread t1v(&VolatileTest::thread1, &vtest);
    std::thread t2v(&VolatileTest::thread2, &vtest);
    t1v.join();
    t2v.join();
    
    std::cout << "    Нарушений секвенциальной консистентности: " 
              << vtest.violations.load() << "\n";
    if (vtest.violations.load() > 0) {
        std::cout << "    💥 VOLATILE СЛОМАЛСЯ! Процессор переставил store/load.\n";
    } else {
        std::cout << "    ✓ Нарушений не обнаружено.\n";
    }
    
    std::cout << "\n";
    
    // Тест atomic
    std::cout << "[2] Тест с std::atomic (memory_order_seq_cst):\n";
    AtomicTest atest;
    std::thread t1a(&AtomicTest::thread1, &atest);
    std::thread t2a(&AtomicTest::thread2, &atest);
    t1a.join();
    t2a.join();
    
    std::cout << "    Нарушений секвенциальной консистентности: " 
              << atest.violations.load() << "\n";
    if (atest.violations.load() == 0) {
        std::cout << "    ✓ ATOMIC РАБОТАЕТ КОРЕКТНО. Барьеры предотвратили реордеринг.\n";
    } else {
        std::cout << "    ⚠️  Нарушения обнаружены (это странно, должно быть 0).\n";
    }
    
    std::cout << "\n=== Объяснение ===\n";
    std::cout << "Store Buffering тест:\n";
    std::cout << "  Thread 1: x = 1; r1 = y;\n";
    std::cout << "  Thread 2: y = 1; r2 = x;\n\n";
    std::cout << "Если процессор выполняет инструкции строго по порядку,\n";
    std::cout << "то НЕВОЗМОЖНО, чтобы оба потока увидели 0.\n";
    std::cout << "Если r1 == 0 && r2 == 0 — значит был реордеринг.\n\n";
    std::cout << "volatile не генерирует барьеров памяти, поэтому ARM-процессор\n";
    std::cout << "имеет право спекулятивно выполнять чтения до записей.\n";
    std::cout << "std::atomic с memory_order_seq_cst генерирует инструкции stlr/ldar,\n";
    std::cout << "которые аппаратно запрещают реордеринг.\n";
    
    return 0;
}

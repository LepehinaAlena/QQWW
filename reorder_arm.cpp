// honest_store_buffering.cpp
// Компиляция: g++ -O2 -pthread honest_store_buffering.cpp -o honest_store_buffering
// Запуск: ./honest_store_buffering

#include <iostream>
#include <thread>
#include <atomic>

const int RUNS = 10'000'000;

// ============================================
// ТЕСТ 1: volatile (ДОЛЖЕН СЛОМАТЬСЯ на ARM)
// ============================================
volatile int x_v = 0;
volatile int y_v = 0;
int r1_v = 0;  // НЕ atomic, НЕ volatile — просто результат
int r2_v = 0;

std::atomic<int> phase_v{0};  // coordinator управляет фазами
std::atomic<long long> violations_v{0};

void writer1_v() {
    for (int i = 0; i < RUNS; i++) {
        // Ждём сигнал от coordinator начать итерацию
        while (phase_v.load(std::memory_order_acquire) != i * 2 + 1) {
            std::this_thread::yield();
        }
        
        // Store Buffering тест
        x_v = 1;           // store (volatile, но без барьера!)
        r1_v = y_v;        // load (volatile, но без барьера!)
        
        // Сигнализируем coordinator, что закончили
        phase_v.fetch_add(1, std::memory_order_release);
        
        // Ждём сигнал начать следующую итерацию
        while (phase_v.load(std::memory_order_acquire) != (i + 1) * 2 + 1) {
            std::this_thread::yield();
        }
    }
}

void writer2_v() {
    for (int i = 0; i < RUNS; i++) {
        // Ждём сигнал от coordinator начать итерацию
        while (phase_v.load(std::memory_order_acquire) != i * 2 + 1) {
            std::this_thread::yield();
        }
        
        // Store Buffering тест
        y_v = 1;           // store (volatile, но без барьера!)
        r2_v = x_v;        // load (volatile, но без барьера!)
        
        // Сигнализируем coordinator, что закончили
        phase_v.fetch_add(1, std::memory_order_release);
        
        // Ждём сигнал начать следующую итерацию
        while (phase_v.load(std::memory_order_acquire) != (i + 1) * 2 + 1) {
            std::this_thread::yield();
        }
    }
}

void coordinator_v() {
    for (int i = 0; i < RUNS; i++) {
        // Сигнализируем потокам начать итерацию
        phase_v.store(i * 2 + 1, std::memory_order_release);
        
        // Ждём, пока оба потока закончат
        while (phase_v.load(std::memory_order_acquire) != i * 2 + 3) {
            std::this_thread::yield();
        }
        
        // Проверяем нарушение: оба прочитали 0
        if (r1_v == 0 && r2_v == 0) {
            violations_v.fetch_add(1, std::memory_order_relaxed);
        }
        
        // Сбрасываем переменные для следующей итерации
        x_v = 0;
        y_v = 0;
        r1_v = 0;
        r2_v = 0;
        
        // Барьер перед следующей итерацией
        std::atomic_thread_fence(std::memory_order_seq_cst);
    }
}

// ============================================
// ТЕСТ 2: std::atomic (НЕ ДОЛЖЕН СЛОМАТЬСЯ)
// ============================================
std::atomic<int> x_a{0};
std::atomic<int> y_a{0};
std::atomic<int> r1_a{0};
std::atomic<int> r2_a{0};

std::atomic<int> phase_a{0};
std::atomic<long long> violations_a{0};

void writer1_a() {
    for (int i = 0; i < RUNS; i++) {
        while (phase_a.load(std::memory_order_acquire) != i * 2 + 1) {
            std::this_thread::yield();
        }
        
        // Store Buffering тест с seq_cst
        x_a.store(1, std::memory_order_seq_cst);
        r1_a.store(y_a.load(std::memory_order_seq_cst), std::memory_order_relaxed);
        
        phase_a.fetch_add(1, std::memory_order_release);
        
        while (phase_a.load(std::memory_order_acquire) != (i + 1) * 2 + 1) {
            std::this_thread::yield();
        }
    }
}

void writer2_a() {
    for (int i = 0; i < RUNS; i++) {
        while (phase_a.load(std::memory_order_acquire) != i * 2 + 1) {
            std::this_thread::yield();
        }
        
        y_a.store(1, std::memory_order_seq_cst);
        r2_a.store(x_a.load(std::memory_order_seq_cst), std::memory_order_relaxed);
        
        phase_a.fetch_add(1, std::memory_order_release);
        
        while (phase_a.load(std::memory_order_acquire) != (i + 1) * 2 + 1) {
            std::this_thread::yield();
        }
    }
}

void coordinator_a() {
    for (int i = 0; i < RUNS; i++) {
        phase_a.store(i * 2 + 1, std::memory_order_release);
        
        while (phase_a.load(std::memory_order_acquire) != i * 2 + 3) {
            std::this_thread::yield();
        }
        
        int v1 = r1_a.load(std::memory_order_relaxed);
        int v2 = r2_a.load(std::memory_order_relaxed);
        
        if (v1 == 0 && v2 == 0) {
            violations_a.fetch_add(1, std::memory_order_relaxed);
        }
        
        x_a.store(0, std::memory_order_relaxed);
        y_a.store(0, std::memory_order_relaxed);
        r1_a.store(0, std::memory_order_relaxed);
        r2_a.store(0, std::memory_order_relaxed);
        
        std::atomic_thread_fence(std::memory_order_seq_cst);
    }
}

int main() {
    std::cout << "=== Честный тест Store Buffering ===\n";
    std::cout << "Итераций: " << RUNS << "\n\n";
    
    // Тест volatile
    std::cout << "[1] Тест с volatile:\n";
    std::thread w1v(writer1_v);
    std::thread w2v(writer2_v);
    std::thread cv(coordinator_v);
    
    w1v.join();
    w2v.join();
    cv.join();
    
    std::cout << "    Нарушений секвенциальной консистентности: " 
              << violations_v.load() << "\n";
    if (violations_v.load() > 0) {
        std::cout << "    💥 VOLATILE СЛОМАЛСЯ! Процессор переставил store/load.\n";
    } else {
        std::cout << "    ✓ Нарушений не обнаружено.\n";
    }
    
    std::cout << "\n";
    
    // Тест atomic
    std::cout << "[2] Тест с std::atomic (memory_order_seq_cst):\n";
    std::thread w1a(writer1_a);
    std::thread w2a(writer2_a);
    std::thread ca(coordinator_a);
    
    w1a.join();
    w2a.join();
    ca.join();
    
    std::cout << "    Нарушений секвенциальной консистентности: " 
              << violations_a.load() << "\n";
    if (violations_a.load() == 0) {
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

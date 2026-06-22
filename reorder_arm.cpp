// reorder_arm.cpp
// Компиляция: g++ -O2 -pthread reorder_arm.cpp -o reorder_arm
// Запуск: ./reorder_arm

#include <iostream>
#include <thread>
#include <atomic>
#include <vector>

// ============================================
// ТЕСТ 1: volatile (ДОЛЖЕН СЛОМАТЬСЯ на ARM)
// ============================================
volatile int data_v = 0;
volatile bool ready_v = false;

std::atomic<long long> violations_v{0};
std::atomic<long long> iterations_v{0};

const int RUNS = 10'000'000;

void writer_v() {
    for (int i = 0; i < RUNS; i++) {
        data_v = 42;       // (1) записываем данные
        ready_v = true;    // (2) поднимаем флаг
        
        // Ждём, пока читатель обработает
        while (ready_v) {
            std::this_thread::yield();
        }
        
        // Сбрасываем для следующей итерации
        data_v = 0;
    }
}

void reader_v() {
    for (int i = 0; i < RUNS; i++) {
        // Ждём флаг
        while (!ready_v) {
            std::this_thread::yield();
        }
        
        // На ARM процессор может спекулятивно загрузить data_v
        // ДО того, как запись data_v = 42 станет видимой
        int observed = data_v;
        
        if (observed != 42) {
            violations_v++;
        }
        
        iterations_v++;
        
        // Разрешаем писателю продолжить
        ready_v = false;
    }
}

// ============================================
// ТЕСТ 2: std::atomic (НЕ ДОЛЖЕН СЛОМАТЬСЯ)
// ============================================
std::atomic<int> data_a{0};
std::atomic<bool> ready_a{false};

std::atomic<long long> violations_a{0};
std::atomic<long long> iterations_a{0};

void writer_a() {
    for (int i = 0; i < RUNS; i++) {
        data_a.store(42, std::memory_order_relaxed);
        ready_a.store(true, std::memory_order_release);  // Release barrier!
        
        while (ready_a.load(std::memory_order_relaxed)) {
            std::this_thread::yield();
        }
        
        data_a.store(0, std::memory_order_relaxed);
    }
}

void reader_a() {
    for (int i = 0; i < RUNS; i++) {
        while (!ready_a.load(std::memory_order_acquire)) {  // Acquire barrier!
            std::this_thread::yield();
        }
        
        int observed = data_a.load(std::memory_order_relaxed);
        
        if (observed != 42) {
            violations_a++;
        }
        
        iterations_a++;
        
        ready_a.store(false, std::memory_order_relaxed);
    }
}

int main() {
    std::cout << "=== Тест реордеринга памяти на ARM (Apple Silicon) ===\n\n";
    std::cout << "Запуск " << RUNS << " итераций для каждого теста...\n\n";
    
    // Тест volatile
    std::cout << "[1] Тест с volatile:\n";
    std::thread t1v(writer_v);
    std::thread t2v(reader_v);
    t1v.join();
    t2v.join();
    
    std::cout << "    Итераций: " << iterations_v.load() << "\n";
    std::cout << "    Нарушений порядка: " << violations_v.load() << "\n";
    if (violations_v.load() > 0) {
        std::cout << "    💥 VOLATILE СЛОМАЛСЯ! Процессор переставил инструкции.\n";
    } else {
        std::cout << "    ✓ Нарушений не обнаружено (но это не значит, что код корректен).\n";
    }
    
    std::cout << "\n";
    
    // Тест atomic
    std::cout << "[2] Тест с std::atomic:\n";
    std::thread t1a(writer_a);
    std::thread t2a(reader_a);
    t1a.join();
    t2a.join();
    
    std::cout << "    Итераций: " << iterations_a.load() << "\n";
    std::cout << "    Нарушений порядка: " << violations_a.load() << "\n";
    if (violations_a.load() == 0) {
        std::cout << "    ✓ ATOMIC РАБОТАЕТ КОРЕКТНО. Барьеры предотвратили реордеринг.\n";
    } else {
        std::cout << "    ⚠️  Нарушения обнаружены (это странно, должно быть 0).\n";
    }
    
    std::cout << "\n=== Вывод ===\n";
    std::cout << "volatile защищает только от оптимизаций компилятора,\n";
    std::cout << "но не от реордеринга процессором. На ARM это приводит к гонкам.\n";
    std::cout << "std::atomic с acquire/release генерирует аппаратные барьеры,\n";
    std::cout << "которые гарантируют порядок видимости изменений между потоками.\n";
    
    return 0;
}

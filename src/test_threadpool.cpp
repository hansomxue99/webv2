#include "threadpool/threadpool.h"
#include <iostream>

void test_threadpool() {
    ThreadPool pool(4); // 创建一个包含4个线程的线程池

    std::atomic<int> counter(0);

    for (int i = 0; i < 1000; ++i) {
        pool.submit([&counter]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }

    // 等待所有任务完成
    std::this_thread::sleep_for(std::chrono::seconds(5));

    std::cout << "Counter value (should be 1000): " << counter.load(std::memory_order_relaxed) << std::endl;
}
#pragma once

#include <vector>
#include <thread>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <future>

class ThreadPool
{
private:
    std::vector<std::thread> m_threads;
    std::queue<std::function<void()>> m_tasks;
    std::mutex m_taskqueue_mutex;
    std::condition_variable m_condvar;
    bool m_stop;

    class ThreadWorker {
    private:
        ThreadPool *m_pool;
    public:
        ThreadWorker(ThreadPool *pool) : m_pool(pool) {}
        void operator()() {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(m_pool->m_taskqueue_mutex);
                    m_pool->m_condvar.wait(lock, [this] {
                        return this->m_pool->m_stop || !this->m_pool->m_tasks.empty();
                    });

                    if (m_pool->m_stop && m_pool->m_tasks.empty()) {
                        return;
                    }

                    if (m_pool->m_tasks.empty() == false) {
                        task = m_pool->m_tasks.front();
                        m_pool->m_tasks.pop();
                    } 
                }
                if (task) {
                    task();
                }
            }
        }
    };
public:
    ThreadPool() = default;
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool(ThreadPool &&) = delete;
    ThreadPool& operator=(const ThreadPool &) = delete;
    ThreadPool& operator=(ThreadPool &&) = delete;

    ThreadPool(int thread_num) {
        for (int i = 0; i < thread_num; ++i) {
            m_threads.emplace_back(std::thread(ThreadWorker(this)));
        }
    }

    ~ThreadPool() {
        m_stop = true;
        m_condvar.notify_all();

        for (auto &t : m_threads) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    template<typename F, typename... Args>
    auto submit(F &&f, Args &&... args) -> std::future<decltype(f(args...))> {
        using retType = typename std::result_of<F(Args...)>::type;
        std::function<retType(void)> func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        auto task_ptr = std::make_shared<std::packaged_task<retType()>>(func);
        std::function<void()> warpper_func = [task_ptr]() {
            (*task_ptr)();
        };
        {
            std::unique_lock<std::mutex> lock(m_taskqueue_mutex);
            m_tasks.push(warpper_func);
        }
        m_condvar.notify_one();
        return task_ptr->get_future();
    }
};

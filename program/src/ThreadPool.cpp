#include "ThreadPool.h"
#include <iostream>

namespace ScotlandYard {
namespace Threading {

std::vector<std::thread> ThreadPool::s_vec_WorkerThreads;
std::queue<std::function<void()>> ThreadPool::s_queue_Tasks;
std::mutex ThreadPool::s_mtx_Queue;
std::condition_variable ThreadPool::s_cv_Task;
std::atomic<bool> ThreadPool::s_b_Shutdown(false);
bool ThreadPool::s_b_Initialized = false;

void ThreadPool::Initialize(size_t numThreads) {
    if (s_b_Initialized) {
        return;
    }

    if (numThreads == 0) {
        numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) {
            numThreads = 4;
        }
    }

    s_b_Shutdown = false;

    for (size_t i = 0; i < numThreads; ++i) {
        s_vec_WorkerThreads.emplace_back(WorkerThread);
    }

    s_b_Initialized = true;
}

void ThreadPool::Shutdown() {
    if (!s_b_Initialized) {
        return;
    }

    {
        std::unique_lock<std::mutex> lock(s_mtx_Queue);
        s_b_Shutdown = true;
    }

    s_cv_Task.notify_all();

    for (auto& thread : s_vec_WorkerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    s_vec_WorkerThreads.clear();

    std::queue<std::function<void()>> empty;
    std::swap(s_queue_Tasks, empty);

    s_b_Initialized = false;
}

void ThreadPool::WorkerThread() {
    while (true) {
        std::function<void()> task;

        {
            std::unique_lock<std::mutex> lock(s_mtx_Queue);

            s_cv_Task.wait(lock, [] {
                return s_b_Shutdown || !s_queue_Tasks.empty();
            });

            if (s_b_Shutdown && s_queue_Tasks.empty()) {
                return;
            }

            if (!s_queue_Tasks.empty()) {
                task = std::move(s_queue_Tasks.front());
                s_queue_Tasks.pop();
            }
        }

        if (task) {
            try {
                task();
            } catch (const std::exception& e) {
                std::cerr << "Exception in worker thread: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "Unknown exception in worker thread" << std::endl;
            }
        }
    }
}

size_t ThreadPool::GetThreadCount() {
    return s_vec_WorkerThreads.size();
}

size_t ThreadPool::GetPendingTaskCount() {
    std::lock_guard<std::mutex> lock(s_mtx_Queue);
    return s_queue_Tasks.size();
}

} // namespace Threading
} // namespace ScotlandYard

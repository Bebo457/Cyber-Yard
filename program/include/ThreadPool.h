#ifndef SCOTLANDYARD_THREADING_THREADPOOL_H
#define SCOTLANDYARD_THREADING_THREADPOOL_H

#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <future>
#include <atomic>

namespace ScotlandYard {
namespace Threading {

enum class ThreadPriority {
    LOW,
    NORMAL,
    HIGH
};

class ThreadPool {
public:
    static void Initialize(size_t numThreads = 0);
    static void Shutdown();

    template<typename F, typename... Args>
    static auto Submit(F&& func, Args&&... args)
        -> std::future<typename std::invoke_result<F, Args...>::type>;

    static size_t GetThreadCount();
    static size_t GetPendingTaskCount();

private:
    ThreadPool() = delete;
    ~ThreadPool() = delete;

    static void WorkerThread();

private:
    static std::vector<std::thread> s_vec_WorkerThreads;
    static std::queue<std::function<void()>> s_queue_Tasks;
    static std::mutex s_mtx_Queue;
    static std::condition_variable s_cv_Task;
    static std::atomic<bool> s_b_Shutdown;
    static bool s_b_Initialized;
};

// Template implementation
template<typename F, typename... Args>
auto ThreadPool::Submit(F&& func, Args&&... args)
    -> std::future<typename std::invoke_result<F, Args...>::type> {

    using return_type = typename std::invoke_result<F, Args...>::type;

    auto pTask = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(func), std::forward<Args>(args)...)
    );

    std::future<return_type> result = pTask->get_future();

    {
        std::unique_lock<std::mutex> lock(s_mtx_Queue);
        s_queue_Tasks.emplace([pTask]() { (*pTask)(); });
    }

    s_cv_Task.notify_one();
    return result;
}

} // namespace Threading
} // namespace ScotlandYard

#endif // SCOTLANDYARD_THREADING_THREADPOOL_H

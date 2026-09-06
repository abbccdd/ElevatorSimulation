#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

class FixedThreadPool
{
public:
    explicit FixedThreadPool(std::size_t threadCount);
    ~FixedThreadPool();

    FixedThreadPool(const FixedThreadPool&) = delete;
    FixedThreadPool& operator=(const FixedThreadPool&) = delete;

    std::size_t GetThreadCount() const noexcept { return m_workers.size(); }

    template <typename Function>
    auto Submit(Function&& function) -> std::future<std::invoke_result_t<Function>>
    {
        using Result = std::invoke_result_t<Function>;
        auto task = std::make_shared<std::packaged_task<Result()>>(std::forward<Function>(function));
        auto future = task->get_future();
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_tasks.emplace([task] { (*task)(); });
        }
        m_condition.notify_one();
        return future;
    }

private:
    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    std::mutex m_mutex;
    std::condition_variable m_condition;
    bool m_stopping = false;

    void RunWorker();
};

#include "FixedThreadPool.h"

FixedThreadPool::FixedThreadPool(std::size_t threadCount)
{
    m_workers.reserve(threadCount);
    try
    {
        for (std::size_t index = 0; index < threadCount; ++index)
            m_workers.emplace_back([this] { RunWorker(); });
    }
    catch (...)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopping = true;
        }
        m_condition.notify_all();
        for (auto& worker : m_workers) worker.join();
        throw;
    }
}

FixedThreadPool::~FixedThreadPool()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopping = true;
    }
    m_condition.notify_all();
    for (auto& worker : m_workers)
        worker.join();
}

void FixedThreadPool::RunWorker()
{
    while (true)
    {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_condition.wait(lock, [this] { return m_stopping || !m_tasks.empty(); });
            if (m_stopping && m_tasks.empty()) return;
            task = std::move(m_tasks.front());
            m_tasks.pop();
        }
        task();
    }
}

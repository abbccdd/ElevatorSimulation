#include "SimulationWorker.h"

#include "Simulation.h"

#include <atomic>
#include <chrono>
#include <exception>
#include <utility>
#include <vector>

namespace
{
    constexpr auto UpdateInterval = std::chrono::milliseconds(16);
}

SimulationWorker::SimulationWorker(const SimulationConfig& config,
    DispatcherExecutionMode mode, std::size_t dispatcherWorkerCount)
    : m_config(config), m_dispatcherMode(mode), m_dispatcherWorkerCount(dispatcherWorkerCount),
      m_thread(&SimulationWorker::ThreadMain, this)
{
}

SimulationWorker::SimulationWorker(const SimulationConfig& config, std::uint32_t seed,
    DispatcherExecutionMode mode, std::size_t dispatcherWorkerCount)
    : m_config(config), m_seed(seed), m_hasFixedSeed(true), m_dispatcherMode(mode),
      m_dispatcherWorkerCount(dispatcherWorkerCount), m_thread(&SimulationWorker::ThreadMain, this)
{
}

SimulationWorker::~SimulationWorker()
{
    Stop();
}

void SimulationWorker::Start()
{
    Enqueue(Command::Start);
}

void SimulationWorker::Pause()
{
    Enqueue(Command::Pause);
}

void SimulationWorker::Resume()
{
    Enqueue(Command::Resume);
}

void SimulationWorker::Reset()
{
    Enqueue(Command::Reset);
}

void SimulationWorker::Stop()
{
    {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        if (!m_stopQueued)
        {
            m_commands.push_back(Command::Stop);
            m_stopQueued = true;
        }
    }
    m_commandCondition.notify_one();
    if (m_thread.joinable()) m_thread.join();
}

std::shared_ptr<const SimulationUISnapshot> SimulationWorker::GetLatestSnapshot() const
{
    return std::atomic_load_explicit(&m_latestSnapshot, std::memory_order_acquire);
}

void SimulationWorker::Enqueue(Command command)
{
    {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        if (m_stopQueued) return;
        m_commands.push_back(command);
    }
    m_commandCondition.notify_one();
}

void SimulationWorker::ThreadMain()
{
    Simulation simulation;
    try
    {
        simulation.SetDispatcherExecutionMode(m_dispatcherMode, m_dispatcherWorkerCount);
        if (m_hasFixedSeed)
            simulation.Initialize(m_config, m_seed);
        else
            simulation.Initialize(m_config);
        PublishSnapshot(simulation, true);

        using Clock = std::chrono::steady_clock;
        auto wallClockBase = Clock::now();
        bool stopping = false;
        while (!stopping)
        {
            std::vector<Command> commands;
            {
                std::unique_lock<std::mutex> lock(m_commandMutex);
                if (simulation.IsRunning())
                    m_commandCondition.wait_for(lock, UpdateInterval, [this] { return !m_commands.empty(); });
                else
                    m_commandCondition.wait(lock, [this] { return !m_commands.empty(); });
                commands.assign(m_commands.begin(), m_commands.end());
                m_commands.clear();
            }

            const auto now = Clock::now();
            if (simulation.IsRunning())
            {
                const std::chrono::duration<double> elapsed = now - wallClockBase;
                simulation.Update(elapsed.count());
            }
            wallClockBase = now;

            for (const Command command : commands)
            {
                switch (command)
                {
                case Command::Start: simulation.Start(); break;
                case Command::Pause: simulation.Pause(); break;
                case Command::Resume: simulation.Resume(); break;
                case Command::Reset: simulation.Reset(); break;
                case Command::Stop: stopping = true; break;
                }
                // 每个控制命令都建立新的墙钟基点，暂停时间不会进入下一次 Update。
                wallClockBase = Clock::now();
                if (stopping) break;
            }
            if (!stopping) PublishSnapshot(simulation, true);
        }
        PublishSnapshot(simulation, false);
    }
    catch (const std::exception& error)
    {
        auto failure = simulation.GetUISnapshot(false);
        failure.config = m_config;
        failure.lastError = error.what();
        std::shared_ptr<const SimulationUISnapshot> snapshot =
            std::make_shared<SimulationUISnapshot>(std::move(failure));
        std::atomic_store_explicit(&m_latestSnapshot, std::move(snapshot), std::memory_order_release);
    }
}

void SimulationWorker::PublishSnapshot(const Simulation& simulation, bool workerActive)
{
    std::shared_ptr<const SimulationUISnapshot> snapshot =
        std::make_shared<SimulationUISnapshot>(simulation.GetUISnapshot(workerActive));
    std::atomic_store_explicit(&m_latestSnapshot, std::move(snapshot), std::memory_order_release);
}

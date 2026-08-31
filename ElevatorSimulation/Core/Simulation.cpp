#include "Simulation.h"

#include <cmath>
#include <new>
#include <stdexcept>
#include <utility>

namespace
{
    bool IsPositiveFinite(double value)
    {
        return std::isfinite(value) && value > 0.0;
    }

    // 返回静态错误文字，空指针表示有效；不依赖 CString 或窗口。
    const char* ValidateConfig(const SimulationConfig& config)
    {
        if (config.floorCount < 2)
            return "floorCount must be >= 2";
        if (config.elevatorCount <= 0 || config.elevatorCount % 3 != 0)
            return "elevatorCount must be a positive multiple of 3";
        if (config.capacity <= 0)
            return "capacity must be > 0";
        if (!IsPositiveFinite(config.moveTimePerFloor))
            return "moveTimePerFloor must be finite and > 0";
        if (!IsPositiveFinite(config.personTime))
            return "personTime must be finite and > 0";
        if (!IsPositiveFinite(config.simulationDuration))
            return "simulationDuration must be finite and > 0";
        if (!IsPositiveFinite(config.simulationSpeed))
            return "simulationSpeed must be finite and > 0";
        if (!std::isfinite(config.passengerRate) || config.passengerRate < 0.0)
            return "passengerRate must be finite and >= 0";
        return nullptr;
    }
}

bool Simulation::Initialize(const SimulationConfig& config)
{
    if (const char* error = ValidateConfig(config))
    {
        m_lastError = error;
        return false;
    }

    try
    {
        // 在局部构造完成后再提交，分配失败不会破坏上一次有效初始化。
        std::vector<Floor> floors;
        std::vector<Elevator> elevators;
        Statistics statistics;
        floors.reserve(static_cast<std::size_t>(config.floorCount));
        elevators.reserve(static_cast<std::size_t>(config.elevatorCount));
        for (int index = 0; index < config.floorCount; ++index)
            floors.emplace_back(index + 1);

        const int groupSize = config.elevatorCount / 3;
        // 等价于 (L + 1) / 2，避免 L + 1 的有符号整数溢出。
        const int middleFloor = config.floorCount / 2 + config.floorCount % 2;
        for (int id = 0; id < config.elevatorCount; ++id)
        {
            const int initialFloor = id < groupSize ? 1 :
                (id < 2 * groupSize ? config.floorCount : middleFloor);
            elevators.emplace_back(id, initialFloor, config.capacity);
        }
        statistics.Reset(config.elevatorCount);

        m_floors = std::move(floors);
        m_elevators = std::move(elevators);
        m_statistics = std::move(statistics);
        m_passengers.clear();
        m_dispatcher = ElevatorDispatcher{};
        m_config = config;
        m_currentTime = 0.0;
        m_state = SimulationState::Ready;
        m_lastError.clear();
        return true;
    }
    catch (const std::bad_alloc&)
    {
        m_lastError = "Not enough memory to initialize simulation";
    }
    catch (const std::length_error&)
    {
        m_lastError = "Configuration exceeds container size limits";
    }
    return false;
}

void Simulation::Start()
{
    if (m_state == SimulationState::Ready)
        m_state = SimulationState::Running;
}

void Simulation::Pause()
{
    if (IsRunning())
        m_state = SimulationState::Paused;
}

void Simulation::Resume()
{
    if (m_state == SimulationState::Paused)
        m_state = SimulationState::Running;
}

void Simulation::Reset()
{
    if (m_state != SimulationState::Uninitialized)
        Initialize(m_config); // 若内存不足，可用 GetLastError() 获取原因。
}

void Simulation::Update(double deltaTime)
{
    if (!IsRunning() || !IsPositiveFinite(deltaTime))
        return;

    const double remainingTime = m_config.simulationDuration - m_currentTime;
    const double scaledTime = deltaTime * m_config.simulationSpeed;
    // 即使有限输入相乘溢出为 +inf，也只推进到截止时间。
    const double step = scaledTime < remainingTime ? scaledTime : remainingTime;

    // TODO(D): 在 step 内编排乘客生成、群控分配、单梯更新与上下客事件。
    // 需要先与 A/B/C/F 约定事件顺序；不能仅在 UI 里推进各模块。
    // 到达乘客先计入 Statistics，再移除楼层/轿厢中的 ID，最后从 m_passengers 删除。
    m_currentTime += step;
    if (m_currentTime >= m_config.simulationDuration)
    {
        m_currentTime = m_config.simulationDuration;
        m_state = SimulationState::Finished;
    }
}

std::vector<ElevatorSnapshot> Simulation::GetElevatorSnapshots() const
{
    std::vector<ElevatorSnapshot> snapshots;
    snapshots.reserve(m_elevators.size());
    for (const Elevator& elevator : m_elevators)
        snapshots.push_back(elevator.GetSnapshot());
    return snapshots;
}

std::vector<FloorSnapshot> Simulation::GetFloorSnapshots() const
{
    std::vector<FloorSnapshot> snapshots;
    snapshots.reserve(m_floors.size());
    for (const Floor& floor : m_floors)
        snapshots.push_back(floor.GetSnapshot());
    return snapshots;
}

StatisticsSnapshot Simulation::GetStatisticsSnapshot() const
{
    return m_statistics.GetSnapshot();
}

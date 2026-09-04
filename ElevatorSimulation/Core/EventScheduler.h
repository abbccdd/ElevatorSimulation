#pragma once

#include "CommonTypes.h"

#include <cstdint>
#include <queue>
#include <vector>

enum class SimulationEventType
{
    ElevatorAction,
    PassengerArrival,
    SimulationEnd
};

struct ScheduledEvent
{
    double time = 0.0;
    SimulationEventType type = SimulationEventType::ElevatorAction;
    int elevatorId = InvalidElevatorId;
    std::uint64_t sequence = 0;
};

// 只管理消耗仿真时间的事件；当前时刻的零耗时状态变化由 Simulation 收敛。
class EventScheduler
{
public:
    void Clear();
    bool Empty() const noexcept { return m_events.empty(); }
    void Push(double time, SimulationEventType type, int elevatorId = InvalidElevatorId);
    const ScheduledEvent& Top() const { return m_events.top(); }
    void Pop() { m_events.pop(); }

private:
    struct LaterEvent
    {
        bool operator()(const ScheduledEvent& left, const ScheduledEvent& right) const noexcept;
    };

    std::priority_queue<ScheduledEvent, std::vector<ScheduledEvent>, LaterEvent> m_events;
    std::uint64_t m_nextSequence = 0;
};

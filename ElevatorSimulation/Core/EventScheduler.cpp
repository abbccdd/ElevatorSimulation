#include "EventScheduler.h"

namespace
{
    int EventRank(SimulationEventType type) noexcept
    {
        switch (type)
        {
        case SimulationEventType::ElevatorAction: return 0;
        case SimulationEventType::TrafficPhaseChange: return 1;
        case SimulationEventType::PassengerArrival: return 2;
        case SimulationEventType::SimulationEnd: return 3;
        }
        return 4;
    }
}

bool EventScheduler::LaterEvent::operator()(
    const ScheduledEvent& left, const ScheduledEvent& right) const noexcept
{
    if (left.time != right.time) return left.time > right.time;
    const int leftRank = EventRank(left.type);
    const int rightRank = EventRank(right.type);
    if (leftRank != rightRank) return leftRank > rightRank;
    if (left.type == SimulationEventType::ElevatorAction && left.elevatorId != right.elevatorId)
        return left.elevatorId > right.elevatorId;
    return left.sequence > right.sequence;
}

void EventScheduler::Clear()
{
    m_events = {};
    m_nextSequence = 0;
}

void EventScheduler::Push(double time, SimulationEventType type, int elevatorId)
{
    m_events.push({ time, type, elevatorId, m_nextSequence++ });
}

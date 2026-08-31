#include "Statistics.h"

#include <stdexcept>
#include <utility>

void Statistics::Reset(int elevatorCount)
{
    if (elevatorCount < 0)
        throw std::invalid_argument("Elevator count cannot be negative");

    StatisticsSnapshot snapshot;
    snapshot.elevators.reserve(static_cast<std::size_t>(elevatorCount));
    for (int id = 0; id < elevatorCount; ++id)
        snapshot.elevators.push_back({ id, 0, 0, 0 });
    m_snapshot = std::move(snapshot);
}

StatisticsSnapshot Statistics::GetSnapshot() const
{
    return m_snapshot;
}

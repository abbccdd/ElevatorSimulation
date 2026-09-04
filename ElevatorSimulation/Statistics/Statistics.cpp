#include "Statistics.h"

#include <stdexcept>
#include <utility>
#include <algorithm>
#include <cmath>

void Statistics::Reset(int elevatorCount, int floorCount)
{
    if (elevatorCount < 0 || floorCount < 0)
        throw std::invalid_argument("Statistics dimensions cannot be negative");

    StatisticsSnapshot snapshot;
    snapshot.elevators.reserve(static_cast<std::size_t>(elevatorCount));
    for (int id = 0; id < elevatorCount; ++id)
        snapshot.elevators.push_back({ id, 0, 0, 0 });
    snapshot.floorTraffic.reserve(static_cast<std::size_t>(floorCount));
    for (int floor = 1; floor <= floorCount; ++floor)
        snapshot.floorTraffic.push_back({ floor });
    m_snapshot = std::move(snapshot);
    m_waitingTimeSum = 0.0;
    m_rideTimeSum = 0.0;
}

StatisticsSnapshot Statistics::GetSnapshot() const
{
    return m_snapshot;
}

void Statistics::PassengerCreated(int floor, Direction direction)
{
    if (direction != Direction::Up && direction != Direction::Down)
        throw std::logic_error("Invalid request direction statistics event");
    auto& traffic = m_snapshot.floorTraffic.at(static_cast<std::size_t>(floor - 1));
    if (traffic.floor != floor)
        throw std::logic_error("Invalid request floor statistics event");
    ++m_snapshot.totalPassengerCount;
    ++m_snapshot.waitingCount;
    ++traffic.generatedCount;
    if (direction == Direction::Up)
        ++traffic.upRequestCount;
    else
        ++traffic.downRequestCount;
}

void Statistics::PassengerBoarded(int floor, double waitingTime)
{
    if (m_snapshot.waitingCount == 0 || !std::isfinite(waitingTime) || waitingTime < 0.0)
        throw std::logic_error("Invalid boarding statistics event");
    auto& traffic = m_snapshot.floorTraffic.at(static_cast<std::size_t>(floor - 1));
    if (traffic.floor != floor || traffic.boardedCount >= traffic.generatedCount)
        throw std::logic_error("Invalid boarding floor statistics event");
    --m_snapshot.waitingCount;
    ++m_snapshot.ridingCount;
    ++m_snapshot.boardedCount;
    m_waitingTimeSum += waitingTime;
    m_snapshot.averageWaitingTime = m_waitingTimeSum / m_snapshot.boardedCount;
    m_snapshot.maxWaitingTime = (std::max)(m_snapshot.maxWaitingTime, waitingTime);
    ++traffic.boardedCount;
    traffic.totalWaitingTime += waitingTime;
    traffic.maxWaitingTime = (std::max)(traffic.maxWaitingTime, waitingTime);
}

void Statistics::PassengerArrived(int elevatorId, double rideTime)
{
    if (m_snapshot.ridingCount == 0 || !std::isfinite(rideTime) || rideTime < 0.0)
        throw std::logic_error("Invalid arrival statistics event");
    auto& elevator = m_snapshot.elevators.at(static_cast<std::size_t>(elevatorId));
    --m_snapshot.ridingCount;
    ++m_snapshot.arrivedCount;
    ++elevator.transportedCount;
    m_rideTimeSum += rideTime;
    m_snapshot.averageRideTime = m_rideTimeSum / m_snapshot.arrivedCount;
}

void Statistics::ElevatorMoved(int elevatorId, bool empty)
{
    auto& elevator = m_snapshot.elevators.at(static_cast<std::size_t>(elevatorId));
    ++elevator.traveledFloors;
    if (empty) ++elevator.emptyTravelFloors;
}

void Statistics::ElevatorTimeElapsed(int elevatorId, double seconds, ElevatorState state, bool full)
{
    if (!std::isfinite(seconds) || seconds < 0.0) throw std::invalid_argument("Invalid elapsed time");
    auto& elevator = m_snapshot.elevators.at(static_cast<std::size_t>(elevatorId));
    if (state == ElevatorState::Idle) elevator.idleTime += seconds;
    if (full) elevator.fullTime += seconds;
}

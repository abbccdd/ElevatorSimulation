#pragma once

#include "../Core/CommonTypes.h"

// 被 Simulation 组合，仅依赖 Common，不反向依赖 Simulation/UI。
class Statistics
{
public:
    void Reset(int elevatorCount, int floorCount);
    StatisticsSnapshot GetSnapshot() const;

    void PassengerCreated(int floor, Direction direction);
    void PassengerBoarded(int floor, double waitingTime);
    void PassengerArrived(int elevatorId, double rideTime);
    void ElevatorMoved(int elevatorId, bool empty);
    void ElevatorTimeElapsed(int elevatorId, double seconds, ElevatorState state, bool full);

private:
    StatisticsSnapshot m_snapshot;
    double m_waitingTimeSum = 0.0;
    double m_rideTimeSum = 0.0;
};

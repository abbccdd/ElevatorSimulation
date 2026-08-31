#pragma once

#include "CommonTypes.h"

// Simulation 按值拥有乘客；Floor/Elevator 仅保存 PassengerId。
class Passenger
{
public:
    Passenger(PassengerId id, int startFloor, int targetFloor, double requestTime);

    PassengerId GetId() const noexcept { return m_id; }
    int GetStartFloor() const noexcept { return m_startFloor; }
    int GetTargetFloor() const noexcept { return m_targetFloor; }
    Direction GetDirection() const noexcept { return m_direction; }
    PassengerState GetState() const noexcept { return m_state; }
    double GetRequestTime() const noexcept { return m_requestTime; }
    double GetBoardTime() const noexcept { return m_boardTime; }
    double GetArrivalTime() const noexcept { return m_arrivalTime; }

    // 状态仅在一人的 T 秒传送完成时转换；时间是仿真秒。
    bool MarkBoarded(int elevatorId, double time);
    bool MarkArrived(double time);
    PassengerSnapshot GetSnapshot() const;

private:
    PassengerId m_id;
    int m_startFloor;
    int m_targetFloor;
    Direction m_direction;
    PassengerState m_state = PassengerState::Waiting;
    double m_requestTime;
    double m_boardTime = UnsetTime;
    double m_arrivalTime = UnsetTime;
    int m_elevatorId = InvalidElevatorId;
};

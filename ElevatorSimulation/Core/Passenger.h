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

    // TODO(A/D): 增加合法的上梯/到达状态转换接口，统一记录仿真时间。
    // 随机创建属于 Simulation 的后续流程，不在构造函数中生成随机数。

private:
    PassengerId m_id;
    int m_startFloor;
    int m_targetFloor;
    Direction m_direction;
    PassengerState m_state = PassengerState::Waiting;
    double m_requestTime;
    double m_boardTime = UnsetTime;
    double m_arrivalTime = UnsetTime;
};

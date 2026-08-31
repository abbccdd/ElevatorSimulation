#include "Passenger.h"

#include <cmath>
#include <stdexcept>

Passenger::Passenger(PassengerId id, int startFloor, int targetFloor, double requestTime)
    : m_id(id), m_startFloor(startFloor), m_targetFloor(targetFloor),
      m_direction(::GetDirection(startFloor, targetFloor)), m_requestTime(requestTime)
{
    // 楼层上界依赖建筑参数，由 Simulation 创建乘客时负责检查。
    if (id < 0 || startFloor < 1 || targetFloor < 1 || startFloor == targetFloor ||
        !std::isfinite(requestTime) || requestTime < 0.0)
    {
        throw std::invalid_argument("Invalid passenger data");
    }
}

bool Passenger::MarkBoarded(int elevatorId, double time)
{
    if (m_state != PassengerState::Waiting || elevatorId < 0 || !std::isfinite(time) || time < m_requestTime)
        return false;
    m_elevatorId = elevatorId;
    m_boardTime = time;
    m_state = PassengerState::Riding;
    return true;
}

bool Passenger::MarkArrived(double time)
{
    if (m_state != PassengerState::Riding || !std::isfinite(time) || time < m_boardTime) return false;
    m_arrivalTime = time;
    m_state = PassengerState::Arrived;
    return true;
}

PassengerSnapshot Passenger::GetSnapshot() const
{
    return { m_id, m_startFloor, m_targetFloor, m_direction, m_state,
        m_requestTime, m_boardTime, m_arrivalTime, m_elevatorId };
}

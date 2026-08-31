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

#include "Elevator.h"

#include <stdexcept>

Elevator::Elevator(int id, int initialFloor, int capacity)
    : m_id(id), m_currentFloor(initialFloor), m_capacity(capacity)
{
    if (id < 0 || initialFloor < 1 || capacity <= 0)
        throw std::invalid_argument("Invalid elevator data");
}

ElevatorSnapshot Elevator::GetSnapshot() const
{
    return { m_id, m_currentFloor, m_direction, m_state,
        static_cast<int>(m_passengerIds.size()), m_capacity };
}

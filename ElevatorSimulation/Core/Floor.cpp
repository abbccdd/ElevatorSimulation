#include "Floor.h"

#include <stdexcept>

Floor::Floor(int floorNumber) : m_floorNumber(floorNumber)
{
    if (floorNumber < 1)
        throw std::invalid_argument("Floor number must be positive");
}

FloorSnapshot Floor::GetSnapshot() const
{
    return { m_floorNumber, GetUpWaitingCount(), GetDownWaitingCount() };
}

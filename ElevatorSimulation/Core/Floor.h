#pragma once

#include "CommonTypes.h"

#include <deque>

class Floor
{
public:
    explicit Floor(int floorNumber);

    int GetFloorNumber() const noexcept { return m_floorNumber; }
    std::size_t GetUpWaitingCount() const noexcept { return m_upWaitingPassengers.size(); }
    std::size_t GetDownWaitingCount() const noexcept { return m_downWaitingPassengers.size(); }
    FloorSnapshot GetSnapshot() const;

    bool Enqueue(PassengerId id, Direction direction);
    bool RemoveFront(PassengerId expectedId, Direction direction);
    PassengerId Peek(Direction direction) const;
    const std::deque<PassengerId>& GetWaitingIds(Direction direction) const;

private:
    int m_floorNumber;
    std::deque<PassengerId> m_upWaitingPassengers;
    std::deque<PassengerId> m_downWaitingPassengers;
};

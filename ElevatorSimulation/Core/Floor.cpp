#include "Floor.h"

#include <stdexcept>
#include <algorithm>

Floor::Floor(int floorNumber) : m_floorNumber(floorNumber)
{
    if (floorNumber < 1)
        throw std::invalid_argument("Floor number must be positive");
}

FloorSnapshot Floor::GetSnapshot() const
{
    return { m_floorNumber, GetUpWaitingCount(), GetDownWaitingCount() };
}

bool Floor::Enqueue(PassengerId id, Direction direction)
{
    if (id < 0 || (direction != Direction::Up && direction != Direction::Down)) return false;
    for (const auto* queue : { &m_upWaitingPassengers, &m_downWaitingPassengers })
        if (std::find(queue->begin(), queue->end(), id) != queue->end()) return false;
    (direction == Direction::Up ? m_upWaitingPassengers : m_downWaitingPassengers).push_back(id);
    return true;
}

bool Floor::RemoveFront(PassengerId expectedId, Direction direction)
{
    if (direction != Direction::Up && direction != Direction::Down) return false;
    auto& queue = direction == Direction::Up ? m_upWaitingPassengers : m_downWaitingPassengers;
    if (queue.empty() || queue.front() != expectedId) return false;
    queue.pop_front();
    return true;
}

PassengerId Floor::Peek(Direction direction) const
{
    if (direction != Direction::Up && direction != Direction::Down) return InvalidPassengerId;
    const auto& queue = GetWaitingIds(direction);
    return queue.empty() ? InvalidPassengerId : queue.front();
}

const std::deque<PassengerId>& Floor::GetWaitingIds(Direction direction) const
{
    if (direction == Direction::Up) return m_upWaitingPassengers;
    if (direction == Direction::Down) return m_downWaitingPassengers;
    throw std::invalid_argument("Waiting queue requires Up or Down");
}

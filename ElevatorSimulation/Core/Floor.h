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

    // TODO(A/D): 协商并添加入队、按方向出队接口；仅保存 ID，不拥有乘客。
    // 满载时未上梯的 ID 必须留队，不得直接清空楼层请求。

private:
    int m_floorNumber;
    std::deque<PassengerId> m_upWaitingPassengers;
    std::deque<PassengerId> m_downWaitingPassengers;
};

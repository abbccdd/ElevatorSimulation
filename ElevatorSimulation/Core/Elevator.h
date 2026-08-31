#pragma once

#include "CommonTypes.h"

#include <set>
#include <vector>

// 只负责单梯；不选择响应新请求的电梯，不访问任何 MFC 对象。
class Elevator
{
public:
    Elevator(int id, int initialFloor, int capacity);

    ElevatorSnapshot GetSnapshot() const;
    const std::vector<PassengerId>& GetPassengerIds() const noexcept { return m_passengerIds; }
    const std::set<int>& GetUpTasks() const noexcept { return m_upTasks; }
    const std::set<int>& GetDownTasks() const noexcept { return m_downTasks; }

    // TODO(B/D): 添加接收任务、时间推进和上下客接口，再实现单梯状态机。
    // 较早接受的任务决定当前方向；新请求不能强迫忙碌电梯立即掉头。
    // 只有当前方向任务完成后才可停止或反向；上客时必须检查 capacity。

private:
    int m_id;
    int m_currentFloor;
    Direction m_direction = Direction::Idle;
    ElevatorState m_state = ElevatorState::Idle;
    int m_capacity;
    std::vector<PassengerId> m_passengerIds;
    std::set<int> m_upTasks;
    std::set<int> m_downTasks;
};

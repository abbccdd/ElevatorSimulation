#pragma once

#include "CommonTypes.h"

#include <set>
#include <map>
#include <vector>

// 只负责单梯；不选择响应新请求的电梯，不访问任何 MFC 对象。
class Elevator
{
public:
    Elevator(int id, int initialFloor, int capacity);
    Elevator(int id, int initialFloor, const SimulationConfig& config);

    ElevatorSnapshot GetSnapshot() const;
    ElevatorDispatchSnapshot GetDispatchSnapshot() const;
    const std::vector<PassengerId>& GetPassengerIds() const noexcept { return m_passengerIds; }
    const std::set<int>& GetUpTasks() const noexcept { return m_upTasks; }
    const std::set<int>& GetDownTasks() const noexcept { return m_downTasks; }

    bool AddHallCall(int floor, Direction direction);
    // 仅撤销指定方向外呼；当前层 Stopped/Boarding/Alighting 禁止，已驶离可撤销。
    // 不触碰内呼、不改变当前动作、方向及剩余时间。
    bool RemoveHallCall(int floor, Direction direction);
    bool HasHallCall(int floor, Direction direction) const;
    bool AddInternalTarget(int floor);
    bool IsAtStop() const noexcept { return m_state == ElevatorState::Stopped; }
    bool CanBoard() const noexcept;
    PassengerId GetNextAlightingPassenger() const;
    bool BeginBoarding(PassengerId id, int targetFloor);
    bool BeginAlighting(PassengerId id);
    bool FinishStop();
    double GetTimeToNextEvent() const noexcept;
    // 最多消耗到一个事件，返回实际消耗时间；调用方处理后再推进剩余预算。
    ElevatorEvent Advance(double simulationSeconds);

private:
    int m_id;
    int m_currentFloor;
    Direction m_direction = Direction::Idle;
    ElevatorState m_state = ElevatorState::Idle;
    int m_capacity;
    std::vector<PassengerId> m_passengerIds;
    std::set<int> m_upTasks;
    std::set<int> m_downTasks;
    int m_floorCount = 0;
    double m_moveTimePerFloor = 2.0;
    double m_personTime = 3.0;
    double m_actionRemaining = 0.0;
    std::set<int> m_upHallCalls;
    std::set<int> m_downHallCalls;
    std::set<int> m_carCalls;
    // 只保存 ID 与必要的目标层数值，不拥有 Passenger 或其指针。
    std::map<PassengerId, int> m_destinations;
    PassengerId m_pendingPassengerId = InvalidPassengerId;
    int m_pendingTarget = 0;

    bool IsValidFloor(int floor) const noexcept;
    bool HasTasksAhead(Direction direction) const;
    void RebuildTasks();
    void ChooseActionAtFloor();
};

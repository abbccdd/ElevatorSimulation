#pragma once

#include "CommonTypes.h"
#include "Elevator.h"

#include <vector>

class ElevatorDispatcher
{
public:
    // Aging 每等待一秒提供的成本折扣；总折扣有上限，避免等待时间压倒路线质量。
    static constexpr double AgingBonusRate = 0.05;
    static constexpr double MaxAgingBonus = 8.0;

    // 返回 elevators 的下标 0~N-1；无法分配时返回 InvalidElevatorId。
    // 无副作用，不接管对象所有权，不推进仿真时间。
    int SelectElevator(int requestFloor, Direction requestDirection,
        const std::vector<Elevator>& elevators,
        double requestTime = UnsetTime, double currentTime = UnsetTime) const;

    // 同一评分实现的快照入口，便于测试，不需暴露可写 Elevator。
    // 返回容器下标；同分时按快照中的 id 排序。
    int SelectFromSnapshots(int requestFloor, Direction requestDirection,
        const std::vector<ElevatorDispatchSnapshot>& elevators,
        double requestTime = UnsetTime, double currentTime = UnsetTime) const;

    double GetAgingBonus(double requestTime, double currentTime) const noexcept;
};

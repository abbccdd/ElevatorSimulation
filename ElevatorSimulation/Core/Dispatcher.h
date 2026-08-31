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
    static constexpr double ReassignThresholdSeconds = 5.0; // 至少节省的仿真秒数。
    static constexpr double ReassignCooldownSeconds = 10.0; // 改派后保护期，非真实秒。
    static constexpr int ReassignLockDistanceFloors = 1; // 可行原梯真正朝请求层移动时的一层保护。
    static constexpr std::size_t MaxJointRequests = 3;
    static constexpr std::size_t MaxJointCandidates = 3;
    static constexpr std::size_t MaxJointCombinations = 64; // (3 台候选 + 暂不分配)^3。

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

    DispatchScore ScoreSnapshot(int requestFloor, Direction requestDirection,
        const ElevatorDispatchSnapshot& elevator,
        double requestTime = UnsetTime, double currentTime = UnsetTime) const;
    DispatchPlan PlanAssignments(const std::vector<HallCallDispatchSnapshot>& requests,
        const std::vector<ElevatorDispatchSnapshot>& elevators, double currentTime) const;
    // 保持原下标表示不改派；所有判断只读，实际撤销/添加由 Simulation 完成。
    int SelectReassignment(const HallCallDispatchSnapshot& request, int currentElevatorIndex,
        const std::vector<ElevatorDispatchSnapshot>& elevators, double currentTime,
        double lastReassignmentTime = UnsetTime) const;
};

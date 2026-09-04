#pragma once

#include "CommonTypes.h"

#include <vector>

class ElevatorDispatcher;

struct HallDemandWeight
{
    int floor = 1;
    Direction direction = Direction::Idle;
    double probability = 0.0;
};

struct RepositionAssignment
{
    int elevatorId = InvalidElevatorId;
    int targetFloor = InvalidFloor;
    double expectedBenefit = 0.0;
};

struct RebalancePlan
{
    std::vector<RepositionAssignment> assignments;
};

// 纯计算的预测式梯群覆盖再平衡；不拥有或修改 Elevator。
class FleetRebalancer
{
public:
    static constexpr double EmptyTravelPenalty = 0.25;
    static constexpr double FleetRebalanceCooldown = 5.0;

    static std::vector<HallDemandWeight> BuildDemandWeights(
        int floorCount, TrafficPattern pattern);

    RebalancePlan BuildPlan(
        const std::vector<ElevatorDispatchSnapshot>& elevators,
        const std::vector<int>& idleElevatorIndices,
        int floorCount,
        TrafficPattern pattern,
        double passengerRate,
        double currentTime,
        const SimulationConfig& config,
        const ElevatorDispatcher& dispatcher) const;
};

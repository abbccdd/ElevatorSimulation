#include "FleetRebalancer.h"

#include "Dispatcher.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

namespace
{
    constexpr double PeakTrafficShare = 0.75;
    constexpr double InterFloorTrafficShare = 0.90;

    ElevatorDispatchSnapshot VirtualIdleSnapshot(
        int floor, const SimulationConfig& config)
    {
        ElevatorDispatchSnapshot snapshot;
        snapshot.elevator.id = 0;
        snapshot.elevator.currentFloor = floor;
        snapshot.elevator.direction = Direction::Idle;
        snapshot.elevator.state = ElevatorState::Idle;
        snapshot.elevator.passengerCount = 0;
        snapshot.elevator.capacity = config.capacity;
        snapshot.floorCount = config.floorCount;
        snapshot.moveTimePerFloor = config.moveTimePerFloor;
        snapshot.personTime = config.personTime;
        return snapshot;
    }
}

std::vector<HallDemandWeight> FleetRebalancer::BuildDemandWeights(
    int floorCount, TrafficPattern pattern)
{
    std::vector<HallDemandWeight> weights;
    weights.reserve(static_cast<std::size_t>(floorCount) * 2);
    const double denominator = static_cast<double>(floorCount) * (floorCount - 1);
    for (int floor = 1; floor <= floorCount; ++floor)
    {
        const double uniformUp = static_cast<double>(floorCount - floor) / denominator;
        const double uniformDown = static_cast<double>(floor - 1) / denominator;
        double up = uniformUp;
        double down = uniformDown;
        if (pattern == TrafficPattern::UpPeak)
        {
            up *= 1.0 - PeakTrafficShare;
            down *= 1.0 - PeakTrafficShare;
            if (floor == 1) up += PeakTrafficShare;
        }
        else if (pattern == TrafficPattern::DownPeak)
        {
            up *= 1.0 - PeakTrafficShare;
            down *= 1.0 - PeakTrafficShare;
            if (floor >= 2) down += PeakTrafficShare / (floorCount - 1);
        }
        else if (pattern == TrafficPattern::InterFloor && floorCount >= 3)
        {
            const double interDenominator =
                static_cast<double>(floorCount - 1) * (floorCount - 2);
            const double interUp = floor >= 2 ?
                static_cast<double>(floorCount - floor) / interDenominator : 0.0;
            const double interDown = floor >= 2 ?
                static_cast<double>(floor - 2) / interDenominator : 0.0;
            up = InterFloorTrafficShare * interUp +
                (1.0 - InterFloorTrafficShare) * uniformUp;
            down = InterFloorTrafficShare * interDown +
                (1.0 - InterFloorTrafficShare) * uniformDown;
        }
        weights.push_back({ floor, Direction::Up, up });
        weights.push_back({ floor, Direction::Down, down });
    }
    return weights;
}

RebalancePlan FleetRebalancer::BuildPlan(
    const std::vector<ElevatorDispatchSnapshot>& elevators,
    const std::vector<int>& idleElevatorIndices,
    int floorCount,
    TrafficPattern pattern,
    double passengerRate,
    double currentTime,
    const SimulationConfig& config,
    const ElevatorDispatcher& dispatcher) const
{
    RebalancePlan plan;
    if (idleElevatorIndices.empty() || passengerRate == 0.0) return plan;

    const double horizon = (std::max)(15.0,
        0.5 * static_cast<double>(floorCount - 1) * config.moveTimePerFloor);
    const double expectedArrivals = passengerRate * horizon;
    const auto weights = BuildDemandWeights(floorCount, pattern);
    std::vector<bool> idle(elevators.size(), false);
    for (int index : idleElevatorIndices)
        idle[static_cast<std::size_t>(index)] = true;

    // 先只用忙碌/已承诺梯形成基线覆盖；所有 ETA 都由 Dispatcher 的 LOOK 评分给出。
    std::vector<double> coverage(weights.size(), horizon);
    for (std::size_t elevator = 0; elevator < elevators.size(); ++elevator)
    {
        if (idle[elevator]) continue;
        for (std::size_t request = 0; request < weights.size(); ++request)
        {
            if (weights[request].probability == 0.0) continue;
            const auto score = dispatcher.ScoreSnapshot(weights[request].floor,
                weights[request].direction, elevators[elevator], currentTime, currentTime);
            if (score.feasible)
                coverage[request] = (std::min)(coverage[request],
                    (std::min)(horizon, score.eta));
        }
    }

    // 每个候选驻点只构造一次虚拟 Idle 快照并复用同一 Dispatcher ETA。
    std::vector<std::vector<double>> virtualEta(static_cast<std::size_t>(floorCount),
        std::vector<double>(weights.size(), horizon));
    for (int target = 1; target <= floorCount; ++target)
    {
        const auto virtualIdle = VirtualIdleSnapshot(target, config);
        for (std::size_t request = 0; request < weights.size(); ++request)
        {
            if (weights[request].probability == 0.0) continue;
            const auto score = dispatcher.ScoreSnapshot(weights[request].floor,
                weights[request].direction, virtualIdle, currentTime, currentTime);
            if (score.feasible)
                virtualEta[static_cast<std::size_t>(target - 1)][request] =
                    (std::min)(horizon, score.eta);
        }
    }

    std::vector<int> remaining = idleElevatorIndices;
    while (!remaining.empty())
    {
        bool found = false;
        double bestScore = 0.0;
        int bestElevatorIndex = InvalidElevatorId;
        int bestFloor = InvalidFloor;
        for (int target = 1; target <= floorCount; ++target)
        {
            double coverageGain = 0.0;
            const auto& candidateEta = virtualEta[static_cast<std::size_t>(target - 1)];
            for (std::size_t request = 0; request < weights.size(); ++request)
            {
                coverageGain += weights[request].probability *
                    (coverage[request] - (std::min)(coverage[request], candidateEta[request]));
            }
            coverageGain *= expectedArrivals;

            int nearestIndex = remaining.front();
            auto nearestKey = std::make_tuple(
                std::abs(elevators[static_cast<std::size_t>(nearestIndex)].elevator.currentFloor - target),
                elevators[static_cast<std::size_t>(nearestIndex)].elevator.id);
            for (int index : remaining)
            {
                const auto key = std::make_tuple(
                    std::abs(elevators[static_cast<std::size_t>(index)].elevator.currentFloor - target),
                    elevators[static_cast<std::size_t>(index)].elevator.id);
                if (key < nearestKey)
                {
                    nearestIndex = index;
                    nearestKey = key;
                }
            }
            const double moveTime = static_cast<double>(std::get<0>(nearestKey)) *
                config.moveTimePerFloor;
            const double pairScore = coverageGain - EmptyTravelPenalty * moveTime;
            if (!found || pairScore > bestScore ||
                (pairScore == bestScore && std::tie(target,
                    elevators[static_cast<std::size_t>(nearestIndex)].elevator.id) <
                    std::tie(bestFloor,
                    elevators[static_cast<std::size_t>(bestElevatorIndex)].elevator.id)))
            {
                found = true;
                bestScore = pairScore;
                bestElevatorIndex = nearestIndex;
                bestFloor = target;
            }
        }
        if (!found || bestScore <= 0.0) break;

        const int currentFloor = elevators[static_cast<std::size_t>(bestElevatorIndex)].elevator.currentFloor;
        if (bestFloor != currentFloor)
        {
            plan.assignments.push_back({
                elevators[static_cast<std::size_t>(bestElevatorIndex)].elevator.id,
                bestFloor, bestScore });
        }
        const auto& selectedEta = virtualEta[static_cast<std::size_t>(bestFloor - 1)];
        for (std::size_t request = 0; request < coverage.size(); ++request)
            coverage[request] = (std::min)(coverage[request], selectedEta[request]);
        remaining.erase(std::find(remaining.begin(), remaining.end(), bestElevatorIndex));
    }
    return plan;
}

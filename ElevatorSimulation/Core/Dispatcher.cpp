#include "Dispatcher.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <tuple>

namespace
{
    // 集中定义 Aging 斜率与上限：成本折扣最多 8 秒，避免等待时间压倒路线质量。
    constexpr double kAgingRate = ElevatorDispatcher::AgingBonusRate;
    constexpr double kAgingCap = ElevatorDispatcher::MaxAgingBonus;

    bool IsTravelDirection(Direction direction)
    {
        return direction == Direction::Up || direction == Direction::Down;
    }

    struct RouteEstimate
    {
        double eta = std::numeric_limits<double>::infinity();
        int projectedOccupancy = 0;
        bool reachedRequest = false;
    };

    // 仅在局部副本上预演 LOOK，不修改真实 Elevator。每个快照中的已知
    // 下客/上客都按实际人数计时；Simulation 会在调用前回填外呼等待人数。
    RouteEstimate EstimatePickupTime(int requestFloor, Direction requestDirection,
        const ElevatorDispatchSnapshot& snapshot)
    {
        const auto& car = snapshot.elevator;
        RouteEstimate result;
        result.projectedOccupancy = car.passengerCount + snapshot.reservedBoardingCount;
        if (result.projectedOccupancy < 0 || result.projectedOccupancy > car.capacity)
            return result;
        std::set<int> up(snapshot.upTasks.begin(), snapshot.upTasks.end());
        std::set<int> down(snapshot.downTasks.begin(), snapshot.downTasks.end());
        (requestDirection == Direction::Up ? up : down).insert(requestFloor);
        // 请求在当前扫描方向后方时，先把它作为反向回程的定位点；到达时
        // 仍需再转回请求方向，才能完成外呼，避免把“经过楼层”误算为接客。
        if (requestDirection == Direction::Up && requestFloor < car.currentFloor)
            down.insert(requestFloor);
        if (requestDirection == Direction::Down && requestFloor > car.currentFloor)
            up.insert(requestFloor);
        std::map<std::pair<int, Direction>, int> service;
        for (const auto& stop : snapshot.stopServices)
        {
            if (stop.floor < 1 || (snapshot.floorCount > 0 && stop.floor > snapshot.floorCount) ||
                stop.alightingCount < 0 || stop.boardingCount < 0 ||
                (stop.direction != Direction::Idle && !IsTravelDirection(stop.direction)))
                return result;
            if (stop.direction == Direction::Idle)
                service[{ stop.floor, Direction::Idle }] += stop.alightingCount;
            else
                service[{ stop.floor, stop.direction }] += stop.boardingCount;
        }
        int floor = car.currentFloor;
        Direction direction = car.direction;
        double time = snapshot.remainingActionTime;
        if (snapshot.betweenFloors)
            floor += direction == Direction::Up ? 1 : -1;
        if (car.state == ElevatorState::Boarding || car.state == ElevatorState::Alighting)
        {
            // 当前一人的传送必须完成后才能响应其他请求；该时间已在快照中给出。
            if (direction != Direction::Up && direction != Direction::Down)
                return result;
        }
        if (car.state == ElevatorState::Idle)
        {
            result.eta = std::abs(static_cast<double>(requestFloor) - car.currentFloor) * snapshot.moveTimePerFloor;
            if (requestFloor == car.currentFloor)
                result.projectedOccupancy = (std::max)(0, result.projectedOccupancy -
                    service[{ requestFloor, Direction::Idle }]);
            result.reachedRequest = result.projectedOccupancy < car.capacity;
            return result;
        }
        const std::size_t visitLimit = 4 * (up.size() + down.size()) + 8;
        for (std::size_t visit = 0; visit < visitLimit; ++visit)
        {
            auto& active = direction == Direction::Up ? up : down;
            if (floor == requestFloor && direction == requestDirection)
            {
                // 先释放该层已知下客，再判断新外呼至少能否预留一席。
                result.projectedOccupancy = (std::max)(0, result.projectedOccupancy -
                    service[{ floor, Direction::Idle }]);
                result.eta = time;
                result.reachedRequest = result.projectedOccupancy < car.capacity;
                return result;
            }
            if (active.erase(floor) != 0)
            {
                result.projectedOccupancy = (std::max)(0, result.projectedOccupancy -
                    service[{ floor, Direction::Idle }]);
                const int boarding = (std::max)(0, service[{ floor, direction }]);
                const int available = (std::max)(0, car.capacity - result.projectedOccupancy);
                const int actualBoarding = (std::min)(boarding, available);
                result.projectedOccupancy += actualBoarding;
                time += static_cast<double>(actualBoarding + service[{ floor, Direction::Idle }]) * snapshot.personTime;
            }
            int nextFloor = floor;
            bool found = false;
            for (int target : active)
            {
                const bool ahead = direction == Direction::Up ? target > floor : target < floor;
                const bool nearer = direction == Direction::Up ? target < nextFloor : target > nextFloor;
                if (ahead && (!found || nearer))
                {
                    found = true;
                    nextFloor = target;
                }
            }
            if (found)
            {
                time += std::abs(static_cast<double>(nextFloor) - floor) * snapshot.moveTimePerFloor;
                floor = nextFloor;
            }
            else
            {
                direction = direction == Direction::Up ? Direction::Down : Direction::Up;
                if (up.empty() && down.empty())
                    return result;
            }
        }
        return result;
    }
}

double ElevatorDispatcher::GetAgingBonus(double requestTime, double currentTime) const noexcept
{
    if (!std::isfinite(requestTime) || !std::isfinite(currentTime) || currentTime <= requestTime)
        return 0.0;
    return (std::min)(kAgingCap, (currentTime - requestTime) * kAgingRate);
}

int ElevatorDispatcher::SelectElevator(int requestFloor, Direction requestDirection,
    const std::vector<Elevator>& elevators, double requestTime, double currentTime) const
{
    std::vector<ElevatorDispatchSnapshot> snapshots;
    snapshots.reserve(elevators.size());
    for (const auto& elevator : elevators)
        snapshots.push_back(elevator.GetDispatchSnapshot());
    return SelectFromSnapshots(requestFloor, requestDirection, snapshots, requestTime, currentTime);
}

int ElevatorDispatcher::SelectFromSnapshots(int requestFloor, Direction requestDirection,
    const std::vector<ElevatorDispatchSnapshot>& elevators,
    double requestTime, double currentTime) const
{
    if (requestFloor < 1 || !IsTravelDirection(requestDirection))
        return InvalidElevatorId;
    using Score = std::tuple<double, double, double, std::size_t, int>;
    Score bestScore;
    int selected = InvalidElevatorId;
    const double agingBonus = GetAgingBonus(requestTime, currentTime);
    for (std::size_t index = 0; index < elevators.size(); ++index)
    {
        const auto& snapshot = elevators[index];
        const auto& car = snapshot.elevator;
        if (car.id < 0 || car.currentFloor < 1 || car.capacity <= 0 || car.passengerCount < 0 ||
            snapshot.reservedBoardingCount < 0 ||
            snapshot.reservedBoardingCount > car.capacity ||
            car.passengerCount > car.capacity - snapshot.reservedBoardingCount ||
            (snapshot.floorCount > 0 && (requestFloor > snapshot.floorCount ||
                car.currentFloor > snapshot.floorCount ||
                (requestFloor == snapshot.floorCount && requestDirection == Direction::Up))) ||
            (requestFloor == 1 && requestDirection == Direction::Down) ||
            !std::isfinite(snapshot.moveTimePerFloor) || snapshot.moveTimePerFloor <= 0.0 ||
            !std::isfinite(snapshot.personTime) || snapshot.personTime <= 0.0 ||
            !std::isfinite(snapshot.remainingActionTime) || snapshot.remainingActionTime < 0.0)
            continue;
        if (snapshot.betweenFloors &&
            ((car.direction == Direction::Down && car.currentFloor == 1) ||
                (car.direction == Direction::Up && (car.currentFloor == (std::numeric_limits<int>::max)() ||
                    (snapshot.floorCount > 0 && car.currentFloor == snapshot.floorCount)))))
            continue;
        const auto validTask = [&](int floor)
        { return floor >= 1 && (snapshot.floorCount == 0 || floor <= snapshot.floorCount); };
        bool validTasks = true;
        for (int floor : snapshot.upTasks) validTasks = validTasks && validTask(floor);
        for (int floor : snapshot.downTasks) validTasks = validTasks && validTask(floor);
        if (!validTasks) continue;
        const bool idle = car.state == ElevatorState::Idle;
        if (!idle && !IsTravelDirection(car.direction))
            continue;
        const bool ahead = car.direction == Direction::Up ? requestFloor > car.currentFloor :
            requestFloor < car.currentFloor;
        const bool onWay = !idle && car.direction == requestDirection &&
            (ahead || (!snapshot.betweenFloors && requestFloor == car.currentFloor));
        const RouteEstimate estimate = EstimatePickupTime(requestFloor, requestDirection, snapshot);
        if (!estimate.reachedRequest)
            continue;
        const double eta = estimate.eta;
        // 顺路与空闲统一比较成本。非顺路忙碌梯增加 S+T 的有限策略成本，
        // 实际返程/折返耗时已计入 ETA；该附加项不代表额外物理耗时。
        const double directionCost = idle || onWay ? 0.0 :
            snapshot.moveTimePerFloor + snapshot.personTime;
        // 负载附加最多为一人的服务时间；任务数同时影响停靠 ETA 和同分排序。
        const double loadCost = snapshot.personTime *
            (static_cast<double>(car.passengerCount) + snapshot.reservedBoardingCount) / car.capacity;
        // AgingBonus 优先抵消非顺路方向成本，使老请求能在有限等待后接受
        // 合理的折返梯；折扣总额仍受 MaxAgingBonus 限制，不会无条件压过 ETA。
        const double adjustedCost = eta + loadCost +
            (std::max)(0.0, directionCost - agingBonus);
        if (!std::isfinite(adjustedCost))
            continue;
        const Score score{ adjustedCost, eta, std::abs(static_cast<double>(requestFloor) - car.currentFloor),
            snapshot.upTasks.size() + snapshot.downTasks.size(), car.id };
        if (selected == InvalidElevatorId || score < bestScore)
        {
            selected = static_cast<int>(index);
            bestScore = score;
        }
    }
    return selected;
}

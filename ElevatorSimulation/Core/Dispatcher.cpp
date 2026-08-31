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
        std::set<int> carTargets;
        std::map<int, int> alighting;
        std::map<std::pair<int, Direction>, ElevatorDispatchSnapshot::StopService> boarding;
        for (const auto& stop : snapshot.stopServices)
        {
            if (stop.floor < 1 || (snapshot.floorCount > 0 && stop.floor > snapshot.floorCount) ||
                stop.alightingCount < 0 || stop.boardingCount < 0 ||
                (stop.direction != Direction::Idle && !IsTravelDirection(stop.direction)) ||
                stop.boardingTargetFloors.size() > static_cast<std::size_t>(stop.boardingCount))
                return result;
            if (stop.direction == Direction::Idle)
            {
                if (stop.boardingCount != 0 || stop.alightingCount > car.capacity - alighting[stop.floor])
                    return result;
                carTargets.insert(stop.floor);
                alighting[stop.floor] += stop.alightingCount;
            }
            else
            {
                if (stop.alightingCount != 0 || !boarding.emplace(std::make_pair(stop.floor, stop.direction), stop).second)
                    return result;
                for (int target : stop.boardingTargetFloors)
                    if (target < 1 || (snapshot.floorCount > 0 && target > snapshot.floorCount) ||
                        GetDirection(stop.floor, target) != stop.direction)
                        return result;
                (stop.direction == Direction::Up ? up : down).insert(stop.floor);
            }
        }
        // upTasks/downTasks 原本混合内呼和外呼；Idle 服务记录将内呼独立出来，
        // 内呼在任一方向到达都会停站，外呼仅在对应方向停站。旧任务快照仍可使用。
        for (int target : carTargets)
        {
            if (boarding.count({ target, Direction::Up }) == 0) up.erase(target);
            if (boarding.count({ target, Direction::Down }) == 0) down.erase(target);
        }
        (requestDirection == Direction::Up ? up : down).insert(requestFloor);
        int floor = car.currentFloor;
        Direction direction = car.direction;
        double time = snapshot.remainingActionTime;
        if (snapshot.betweenFloors)
            floor += direction == Direction::Up ? 1 : -1;
        if (car.state == ElevatorState::Alighting)
        {
            // 当前人仍计入 passengerCount/下客记录，但只需 remainingActionTime。
            // 先完成该人，循环内再消费同层其余下客，不能额外补一个完整 T。
            if (alighting[floor] <= 0 || result.projectedOccupancy <= 0)
                return result;
            --alighting[floor];
            --result.projectedOccupancy;
        }
        if (car.state == ElevatorState::Stopped || car.state == ElevatorState::Boarding ||
            car.state == ElevatorState::Alighting)
            carTargets.insert(floor); // 当前停站必须完成，即使内呼已从真实任务集中移除。
        if (car.state == ElevatorState::Idle)
            direction = floor == requestFloor ? requestDirection : GetDirection(floor, requestFloor);

        // 每个外呼只服务一次，新增内呼只能来自这些有限的已知上客目标。
        // 每次扫描都会消费停站或到最远任务折返，因此无需按固定停靠次数截断。
        while (!up.empty() || !down.empty() || !carTargets.empty())
        {
            auto& active = direction == Direction::Up ? up : down;
            if (carTargets.erase(floor) != 0 || active.count(floor) != 0)
            {
                int& due = alighting[floor];
                if (due > result.projectedOccupancy) return result;
                result.projectedOccupancy -= due;
                time += static_cast<double>(due) * snapshot.personTime;
                due = 0; // 下客是可消费事件；返程/切换外呼方向不能再次释放这一批容量。
                if (floor == requestFloor && direction == requestDirection)
                {
                    // ETA 为可以开始接本外呼的时刻，包含请求层先下客的时间。
                    result.eta = time;
                    result.reachedRequest = result.projectedOccupancy < car.capacity;
                    return result;
                }
                if (active.erase(floor) != 0)
                {
                    const auto service = boarding.find({ floor, direction });
                    if (service != boarding.end())
                    {
                        const int actualBoarding = (std::min)(service->second.boardingCount,
                            car.capacity - result.projectedOccupancy);
                        result.projectedOccupancy += actualBoarding;
                        time += static_cast<double>(actualBoarding) * snapshot.personTime;
                        const auto& targets = service->second.boardingTargetFloors;
                        const auto knownCount = (std::min)(targets.size(), static_cast<std::size_t>(actualBoarding));
                        for (std::size_t passenger = 0; passenger < knownCount; ++passenger)
                        {
                            carTargets.insert(targets[passenger]);
                            ++alighting[targets[passenger]];
                        }
                        // 未上梯者交还 Simulation 重分配，不能在此次预演中重复登梯。
                        boarding.erase(service);
                    }
                }
            }
            int nextFloor = floor;
            bool found = false;
            // 和 HasTasksAhead 一致：任意方向外呼及内呼都决定当前扫描的延伸。
            // 经过反向外呼时不停车服务，但不能在抵达该折返点前提前反向。
            for (const auto* tasks : { &up, &down, &carTargets })
                for (int target : *tasks)
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
                direction = direction == Direction::Up ? Direction::Down : Direction::Up;
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
        // 以请求层完成下客、尚未接本外呼时的预计载荷评分，附加不超过一个 T。
        const double loadCost = snapshot.personTime *
            static_cast<double>(estimate.projectedOccupancy) / car.capacity;
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

#include "Dispatcher.h"

#include <cmath>
#include <limits>
#include <set>
#include <tuple>

namespace
{
    bool IsTravelDirection(Direction direction)
    {
        return direction == Direction::Up || direction == Direction::Down;
    }

    // 仅在局部副本上预演 LOOK，不修改真实 Elevator。
    // 每个已有中间停站估计处理一人，耗时 T；未知未来上客不作精确预测。
    double EstimatePickupTime(int requestFloor, Direction requestDirection,
        const ElevatorDispatchSnapshot& snapshot)
    {
        const auto& car = snapshot.elevator;
        if (car.state == ElevatorState::Idle)
            return std::abs(static_cast<double>(requestFloor) - car.currentFloor) * snapshot.moveTimePerFloor;
        std::set<int> up(snapshot.upTasks.begin(), snapshot.upTasks.end());
        std::set<int> down(snapshot.downTasks.begin(), snapshot.downTasks.end());
        (requestDirection == Direction::Up ? up : down).insert(requestFloor);
        int floor = car.currentFloor;
        Direction direction = car.direction;
        double time = snapshot.remainingActionTime;
        if (snapshot.betweenFloors)
            floor += direction == Direction::Up ? 1 : -1;
        const std::size_t visitLimit = 4 * (up.size() + down.size()) + 8;
        for (std::size_t visit = 0; visit < visitLimit; ++visit)
        {
            auto& active = direction == Direction::Up ? up : down;
            if (floor == requestFloor && direction == requestDirection)
                return time;
            if (active.erase(floor) != 0)
                time += snapshot.personTime;
            int nextFloor = floor;
            bool found = false;
            for (const auto* tasks : { &up, &down })
            {
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
            }
            if (found)
            {
                time += std::abs(static_cast<double>(nextFloor) - floor) * snapshot.moveTimePerFloor;
                floor = nextFloor;
            }
            else
                direction = direction == Direction::Up ? Direction::Down : Direction::Up;
        }
        return std::numeric_limits<double>::infinity();
    }
}

int ElevatorDispatcher::SelectElevator(int requestFloor, Direction requestDirection,
    const std::vector<Elevator>& elevators) const
{
    std::vector<ElevatorDispatchSnapshot> snapshots;
    snapshots.reserve(elevators.size());
    for (const auto& elevator : elevators)
        snapshots.push_back(elevator.GetDispatchSnapshot());
    return SelectFromSnapshots(requestFloor, requestDirection, snapshots);
}

int ElevatorDispatcher::SelectFromSnapshots(int requestFloor, Direction requestDirection,
    const std::vector<ElevatorDispatchSnapshot>& elevators) const
{
    if (requestFloor < 1 || !IsTravelDirection(requestDirection))
        return InvalidElevatorId;
    using Score = std::tuple<double, double, double, std::size_t, int>;
    Score bestScore;
    int selected = InvalidElevatorId;
    for (std::size_t index = 0; index < elevators.size(); ++index)
    {
        const auto& snapshot = elevators[index];
        const auto& car = snapshot.elevator;
        if (car.id < 0 || car.currentFloor < 1 || car.capacity <= 0 || car.passengerCount < 0 ||
            snapshot.reservedBoardingCount < 0 ||
            car.passengerCount >= car.capacity - snapshot.reservedBoardingCount ||
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
        const double eta = EstimatePickupTime(requestFloor, requestDirection, snapshot);
        // 顺路与空闲统一比较成本。非顺路忙碌梯增加 S+T 的有限策略成本，
        // 实际返程/折返耗时已计入 ETA；该附加项不代表额外物理耗时。
        const double directionCost = idle || onWay ? 0.0 :
            snapshot.moveTimePerFloor + snapshot.personTime;
        // 负载附加最多为一人的服务时间；任务数同时影响停靠 ETA 和同分排序。
        const double loadCost = snapshot.personTime *
            (static_cast<double>(car.passengerCount) + snapshot.reservedBoardingCount) / car.capacity;
        const double cost = eta + loadCost + directionCost;
        if (!std::isfinite(cost))
            continue;
        const Score score{ cost, eta, std::abs(static_cast<double>(requestFloor) - car.currentFloor),
            snapshot.upTasks.size() + snapshot.downTasks.size(), car.id };
        if (selected == InvalidElevatorId || score < bestScore)
        {
            selected = static_cast<int>(index);
            bestScore = score;
        }
    }
    return selected;
}

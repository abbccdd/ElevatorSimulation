#include "Simulation.h"

#include <cmath>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <algorithm>
#include <limits>
#include <set>
#include <tuple>

namespace
{
    bool IsPositiveFinite(double value)
    {
        return std::isfinite(value) && value > 0.0;
    }

    double NextArrivalTime(double now, double rate, std::mt19937& random)
    {
        if (rate == 0.0) return std::numeric_limits<double>::infinity();
        // U 严格在 (0,1)，指数间隔 -ln(U)/lambda；一轮中不按帧重新抽样。
        const double uniform = (static_cast<double>(random()) + 0.5) / 4294967296.0;
        const double next = now - std::log(uniform) / rate;
        return next > now ? next : std::nextafter(now, std::numeric_limits<double>::infinity());
    }

    // 返回静态错误文字，空指针表示有效；不依赖 CString 或窗口。
    const char* ValidateConfig(const SimulationConfig& config)
    {
        if (config.floorCount < 2)
            return "floorCount must be >= 2";
        if (config.elevatorCount <= 0 || config.elevatorCount % 3 != 0)
            return "elevatorCount must be a positive multiple of 3";
        if (config.capacity <= 0)
            return "capacity must be > 0";
        if (!IsPositiveFinite(config.moveTimePerFloor))
            return "moveTimePerFloor must be finite and > 0";
        if (!IsPositiveFinite(config.personTime))
            return "personTime must be finite and > 0";
        if (!IsPositiveFinite(config.simulationDuration))
            return "simulationDuration must be finite and > 0";
        if (!IsPositiveFinite(config.simulationSpeed))
            return "simulationSpeed must be finite and > 0";
        if (!std::isfinite(config.passengerRate) || config.passengerRate < 0.0)
            return "passengerRate must be finite and >= 0";
        return nullptr;
    }
}

bool Simulation::Initialize(const SimulationConfig& config)
{
    try { return Initialize(config, std::random_device{}()); }
    catch (const std::exception&)
    {
        m_lastError = "Unable to obtain a random seed; use Initialize(config, seed)";
        return false;
    }
}

bool Simulation::Initialize(const SimulationConfig& config, std::uint32_t seed)
{
    if (const char* error = ValidateConfig(config))
    {
        m_lastError = error;
        return false;
    }

    try
    {
        // 在局部构造完成后再提交，分配失败不会破坏上一次有效初始化。
        std::vector<Floor> floors;
        std::vector<Elevator> elevators;
        Statistics statistics;
        std::mt19937 random(seed);
        const double nextArrival = NextArrivalTime(0.0, config.passengerRate, random);
        floors.reserve(static_cast<std::size_t>(config.floorCount));
        elevators.reserve(static_cast<std::size_t>(config.elevatorCount));
        for (int index = 0; index < config.floorCount; ++index)
            floors.emplace_back(index + 1);

        const int groupSize = config.elevatorCount / 3;
        // 等价于 (L + 1) / 2，避免 L + 1 的有符号整数溢出。
        const int middleFloor = config.floorCount / 2 + config.floorCount % 2;
        for (int id = 0; id < config.elevatorCount; ++id)
        {
            const int initialFloor = id < groupSize ? 1 :
                (id < 2 * groupSize ? config.floorCount : middleFloor);
            elevators.emplace_back(id, initialFloor, config);
        }
        statistics.Reset(config.elevatorCount);

        m_floors = std::move(floors);
        m_elevators = std::move(elevators);
        m_statistics = std::move(statistics);
        m_passengers.clear();
        m_hallCalls.clear();
        m_nextPassengerId = 0;
        m_seed = seed;
        m_random = std::move(random);
        m_nextArrivalTime = nextArrival;
        m_config = config;
        m_currentTime = 0.0;
        m_dispatchDirty = true;
        m_lastReassessmentTime = UnsetTime;
        m_state = SimulationState::Ready;
        m_lastError.clear();
        return true;
    }
    catch (const std::bad_alloc&)
    {
        m_lastError = "Not enough memory to initialize simulation";
    }
    catch (const std::length_error&)
    {
        m_lastError = "Configuration exceeds container size limits";
    }
    return false;
}

void Simulation::Start()
{
    if (m_state == SimulationState::Ready)
        m_state = SimulationState::Running;
}

void Simulation::Pause()
{
    if (IsRunning())
        m_state = SimulationState::Paused;
}

void Simulation::Resume()
{
    if (m_state == SimulationState::Paused)
        m_state = SimulationState::Running;
}

void Simulation::Reset()
{
    if (m_state != SimulationState::Uninitialized)
        Initialize(m_config, m_seed); // 保持本轮 seed，重置后可复现；失败保留旧状态。
}

void Simulation::Update(double deltaTime)
{
    if (!IsRunning() || !IsPositiveFinite(deltaTime))
        return;

    const double scaledTime = deltaTime * m_config.simulationSpeed;
    const double remainingTime = m_config.simulationDuration - m_currentTime;
    const double targetTime = scaledTime < remainingTime ?
        m_currentTime + scaledTime : m_config.simulationDuration;
    GenerateDuePassengers();
    StabilizeCurrentTime();
    while (m_currentTime < targetTime)
    {
        const double toTarget = targetTime - m_currentTime;
        const double toArrival = m_nextArrivalTime < m_config.simulationDuration ?
            m_nextArrivalTime - m_currentTime : std::numeric_limits<double>::infinity();
        double step = (std::min)(toTarget, toArrival);
        for (const auto& elevator : m_elevators)
            step = (std::min)(step, elevator.GetTimeToNextEvent());

        std::vector<ElevatorEvent> events;
        events.reserve(m_elevators.size());
        for (auto& elevator : m_elevators)
        {
            const auto before = elevator.GetSnapshot();
            m_statistics.ElevatorTimeElapsed(before.id, step, before.state,
                before.passengerCount == before.capacity);
            events.push_back(elevator.Advance(step));
        }
        // 保持生成事件与帧结束的时间戳，不把 double 减加误差累积到下一轮。
        m_currentTime = step == toTarget ? targetTime :
            (step == toArrival ? m_nextArrivalTime : m_currentTime + step);
        for (std::size_t index = 0; index < events.size(); ++index)
            HandleElevatorEvent(static_cast<int>(index), events[index]);
        if (m_currentTime < m_config.simulationDuration)
        {
            GenerateDuePassengers();
            StabilizeCurrentTime();
        }
    }
    if (m_currentTime >= m_config.simulationDuration)
    {
        m_currentTime = m_config.simulationDuration;
        m_state = SimulationState::Finished;
    }
}

PassengerId Simulation::AddPassenger(int startFloor, int targetFloor)
{
    if (m_state == SimulationState::Uninitialized || IsFinished() ||
        startFloor < 1 || startFloor > m_config.floorCount ||
        targetFloor < 1 || targetFloor > m_config.floorCount || startFloor == targetFloor ||
        m_currentTime >= m_config.simulationDuration ||
        m_nextPassengerId > static_cast<std::uint64_t>((std::numeric_limits<PassengerId>::max)()))
        return InvalidPassengerId;
    const PassengerId id = static_cast<PassengerId>(m_nextPassengerId);
    const Direction direction = GetDirection(startFloor, targetFloor);
    m_passengers.emplace(id, Passenger(id, startFloor, targetFloor, m_currentTime));
    if (!m_floors[static_cast<std::size_t>(startFloor - 1)].Enqueue(id, direction))
        throw std::logic_error("Duplicate passenger in floor queue");
    m_hallCalls.try_emplace({ startFloor, direction }, HallCall{ InvalidElevatorId, m_currentTime, id });
    ++m_nextPassengerId;
    m_statistics.PassengerCreated();
    m_dispatchDirty = true;
    return id;
}

void Simulation::GenerateDuePassengers()
{
    while (m_nextArrivalTime <= m_currentTime && m_nextArrivalTime < m_config.simulationDuration)
    {
        const int start = std::uniform_int_distribution<int>(1, m_config.floorCount)(m_random);
        int target = std::uniform_int_distribution<int>(1, m_config.floorCount - 1)(m_random);
        if (target >= start) ++target; // 均匀选择除起点外的 L-1 层，不使用随机重试。
        if (AddPassenger(start, target) == InvalidPassengerId)
            throw std::overflow_error("Passenger ID space exhausted");
        m_nextArrivalTime = NextArrivalTime(m_nextArrivalTime, m_config.passengerRate, m_random);
    }
}

bool Simulation::DispatchCalls()
{
    if (!m_dispatchDirty) return false;
    m_dispatchDirty = false;
    bool changed = false;
    // 同一物理时刻仅有一轮改派，零耗时状态收敛过程中不反复抢单。
    if (m_lastReassessmentTime != m_currentTime)
    {
        m_lastReassessmentTime = m_currentTime;
        auto snapshots = BuildDispatchSnapshots();
        for (auto& [key, call] : m_hallCalls)
        {
            const int oldId = call.assignedElevatorId;
            if (oldId == InvalidElevatorId) continue;
            const auto request = BuildHallCallSnapshot(key, call);
            const int newId = m_dispatcher.SelectReassignment(request, oldId, snapshots,
                m_currentTime, call.lastReassignmentTime);
            if (newId == oldId || newId == InvalidElevatorId) continue;
            // 只为已选定改派准备提交，不用于试组合；分配失败不留下半次改派。
            auto oldOwner = m_elevators[static_cast<std::size_t>(oldId)];
            auto newOwner = m_elevators[static_cast<std::size_t>(newId)];
            static_assert(std::is_nothrow_move_assignable<Elevator>::value, "Reassignment commit must not throw");
            if (!oldOwner.RemoveHallCall(key.first, key.second))
                throw std::logic_error("Cannot remove reassigned hall call");
            if (!newOwner.AddHallCall(key.first, key.second))
                throw std::logic_error("Cannot accept reassigned hall call");
            m_elevators[static_cast<std::size_t>(oldId)] = std::move(oldOwner);
            m_elevators[static_cast<std::size_t>(newId)] = std::move(newOwner);
            call.assignedElevatorId = newId;
            call.lastReassignmentTime = m_currentTime;
            changed = true;
            snapshots = BuildDispatchSnapshots();
        }
    }
    // 每个成功批次严格减少 pending；零分配立即退出，不推进时间、不重复改派。
    while (true)
    {
        std::vector<std::map<HallCallKey, HallCall>::iterator> pending;
        for (auto call = m_hallCalls.begin(); call != m_hallCalls.end(); ++call)
            if (call->second.assignedElevatorId == InvalidElevatorId) pending.push_back(call);
        std::sort(pending.begin(), pending.end(), [](const auto& left, const auto& right)
        {
            return std::tie(left->second.firstRequestTime, left->second.firstPassengerId, left->first) <
                std::tie(right->second.firstRequestTime, right->second.firstPassengerId, right->first);
        });
        if (pending.empty()) break;
        const auto snapshots = BuildDispatchSnapshots();
        std::vector<std::map<HallCallKey, HallCall>::iterator> active;
        std::vector<HallCallDispatchSnapshot> requests;
        for (auto call : pending)
        {
            auto request = BuildHallCallSnapshot(call->first, call->second);
            const bool feasible = m_dispatcher.SelectFromSnapshots(request.floor, request.direction,
                snapshots, request.firstRequestTime, m_currentTime) != InvalidElevatorId;
            // 无候选时临时 DeferredCapacity：不占窗口，不改真实队列、时间或队头 ID。
            // 每批用新快照重判，不缓存状态；容量/路线事件仍使用 m_dispatchDirty 触发。
            if (!feasible) continue;
            active.push_back(call);
            requests.push_back(std::move(request));
            if (active.size() == ElevatorDispatcher::MaxJointRequests) break;
        }
        if (active.empty()) break;
        const auto plan = m_dispatcher.PlanAssignments(requests, snapshots, m_currentTime);
        if (plan.assignedCount == 0) break;
        for (std::size_t index = 0; index < active.size(); ++index)
        {
            const auto call = active[index];
            const auto [floor, direction] = call->first;
            const int id = plan.elevatorIndices[index];
            if (id != InvalidElevatorId)
            {
                if (!m_elevators[static_cast<std::size_t>(id)].AddHallCall(floor, direction))
                    throw std::logic_error("Dispatcher selected an invalid hall call");
                call->second.assignedElevatorId = id;
                changed = true;
            }
        }
    }
    return changed;
}

HallCallDispatchSnapshot Simulation::BuildHallCallSnapshot(const HallCallKey& key, const HallCall& call) const
{
    HallCallDispatchSnapshot request;
    request.floor = key.first; request.direction = key.second;
    request.firstRequestTime = call.firstRequestTime;
    request.firstPassengerId = call.firstPassengerId;
    const auto& queue = m_floors[static_cast<std::size_t>(key.first - 1)].GetWaitingIds(key.second);
    request.waitingCount = static_cast<int>((std::min)(queue.size(),
        static_cast<std::size_t>((std::numeric_limits<int>::max)())));
    const auto count = (std::min)(queue.size(), static_cast<std::size_t>(m_config.capacity));
    for (std::size_t index = 0; index < count; ++index)
        request.targetFloors.push_back(m_passengers.at(queue[index]).GetTargetFloor());
    return request;
}

std::vector<ElevatorDispatchSnapshot> Simulation::BuildDispatchSnapshots() const
{
    auto snapshots = std::vector<ElevatorDispatchSnapshot>();
    snapshots.reserve(m_elevators.size());
    for (const auto& elevator : m_elevators)
        snapshots.push_back(elevator.GetDispatchSnapshot());

    for (std::size_t index = 0; index < snapshots.size(); ++index)
    {
        const auto elevatorState = snapshots[index].elevator;
        const auto& elevator = m_elevators[index];
        for (auto& stop : snapshots[index].stopServices)
        {
            if ((stop.direction != Direction::Up && stop.direction != Direction::Down) ||
                !elevator.HasHallCall(stop.floor, stop.direction))
                continue;
            const auto& waiting = m_floors[static_cast<std::size_t>(stop.floor - 1)].GetWaitingIds(stop.direction);
            std::size_t reserved = 0;
            // Boarding 中的队头仍在 Floor，预留席位已经计入 occupancy，不能重复算入 ETA。
            if (elevatorState.state == ElevatorState::Boarding &&
                elevatorState.currentFloor == stop.floor && elevatorState.direction == stop.direction)
            {
                reserved = (std::min)(waiting.size(), static_cast<std::size_t>(snapshots[index].reservedBoardingCount));
            }
            const std::size_t waitingCount = waiting.size() - reserved;
            stop.boardingCount = static_cast<int>((std::min)(waitingCount,
                static_cast<std::size_t>((std::numeric_limits<int>::max)())));
            // 一次最多登梯 capacity 人，只需这个 FIFO 前缀；大队列不会放大每次快照。
            const std::size_t targetCount = (std::min)(waitingCount, static_cast<std::size_t>(elevatorState.capacity));
            stop.boardingTargetFloors.clear();
            stop.boardingTargetFloors.reserve(targetCount);
            for (std::size_t passenger = 0; passenger < targetCount; ++passenger)
                stop.boardingTargetFloors.push_back(m_passengers.at(waiting[reserved + passenger]).GetTargetFloor());
        }
    }
    return snapshots;
}

void Simulation::ReleaseHallCall(int floor, Direction direction, int elevatorId)
{
    const auto call = m_hallCalls.find({ floor, direction });
    if (call == m_hallCalls.end() || call->second.assignedElevatorId != elevatorId) return;
    m_dispatchDirty = true;
    const PassengerId next = m_floors[static_cast<std::size_t>(floor - 1)].Peek(direction);
    if (next == InvalidPassengerId)
        m_hallCalls.erase(call);
    else
    {
        // 满载剩余乘客保留 FIFO 顺序，外呼重回待分配而不是被删除。
        call->second = { InvalidElevatorId, m_passengers.at(next).GetRequestTime(), next };
    }
}

void Simulation::StabilizeCurrentTime()
{
    // 只执行零耗时决策；Boarding/Alighting/Moving 的完成必须等待计时事件。
    const std::size_t limit = 4 * (m_hallCalls.size() + m_elevators.size()) + 1;
    for (std::size_t pass = 0; pass < limit; ++pass)
    {
        bool changed = DispatchCalls();
        for (auto& elevator : m_elevators)
        {
            if (!elevator.IsAtStop()) continue;
            const auto snapshot = elevator.GetSnapshot();
            const PassengerId alighting = elevator.GetNextAlightingPassenger();
            if (alighting != InvalidPassengerId)
            {
                if (!elevator.BeginAlighting(alighting)) throw std::logic_error("Cannot alight due passenger");
                m_dispatchDirty = true;
                changed = true;
                continue;
            }
            const auto call = m_hallCalls.find({ snapshot.currentFloor, snapshot.direction });
            const PassengerId waiting = m_floors[static_cast<std::size_t>(snapshot.currentFloor - 1)].Peek(snapshot.direction);
            if (call != m_hallCalls.end() && call->second.assignedElevatorId == snapshot.id &&
                waiting != InvalidPassengerId && elevator.CanBoard())
            {
                if (!elevator.BeginBoarding(waiting, m_passengers.at(waiting).GetTargetFloor()))
                    throw std::logic_error("Cannot board assigned passenger");
            }
            else
            {
                if (!elevator.FinishStop()) throw std::logic_error("Cannot finish serviced stop");
                ReleaseHallCall(snapshot.currentFloor, snapshot.direction, snapshot.id);
            }
            changed = true;
            m_dispatchDirty = true;
        }
        if (!changed) return;
    }
    throw std::logic_error("Zero-time service decisions did not converge");
}

void Simulation::HandleElevatorEvent(int elevatorId, const ElevatorEvent& event)
{
    if (event.type == ElevatorEventType::None) return;
    m_dispatchDirty = true;
    if (event.type == ElevatorEventType::FloorReached)
    {
        m_statistics.ElevatorMoved(elevatorId, event.emptyMovement);
        return;
    }
    auto& passenger = m_passengers.at(event.passengerId);
    if (event.type == ElevatorEventType::Boarded)
    {
        auto& floor = m_floors[static_cast<std::size_t>(passenger.GetStartFloor() - 1)];
        if (!floor.RemoveFront(event.passengerId, passenger.GetDirection()) ||
            !passenger.MarkBoarded(elevatorId, m_currentTime))
            throw std::logic_error("Invalid passenger boarding transition");
        m_statistics.PassengerBoarded(m_currentTime - passenger.GetRequestTime());
    }
    else
    {
        if (!passenger.MarkArrived(m_currentTime)) throw std::logic_error("Invalid passenger arrival transition");
        m_statistics.PassengerArrived(elevatorId, m_currentTime - passenger.GetBoardTime());
        // Elevator 已先移除 ID；统计累计量保留，活动对象从此消失。
        m_passengers.erase(event.passengerId);
    }
}

std::vector<PassengerSnapshot> Simulation::GetPassengerSnapshots() const
{
    std::vector<PassengerSnapshot> snapshots;
    snapshots.reserve(m_passengers.size());
    for (const auto& entry : m_passengers) snapshots.push_back(entry.second.GetSnapshot());
    std::sort(snapshots.begin(), snapshots.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    return snapshots;
}

std::vector<HallCallSnapshot> Simulation::GetHallCallSnapshots() const
{
    std::vector<HallCallSnapshot> snapshots;
    for (const auto& [key, call] : m_hallCalls)
    {
        const auto& queue = m_floors[static_cast<std::size_t>(key.first - 1)].GetWaitingIds(key.second);
        if (!queue.empty()) snapshots.push_back({ key.first, key.second, queue.size(),
            call.assignedElevatorId, call.firstRequestTime });
    }
    return snapshots;
}

bool Simulation::ValidateState() const
{
    if (m_state == SimulationState::Uninitialized)
        return m_passengers.empty() && m_elevators.empty() && m_floors.empty();
    std::set<PassengerId> seen;
    std::size_t waiting = 0, riding = 0;
    for (const auto& floor : m_floors)
    {
        for (Direction direction : { Direction::Up, Direction::Down })
        {
            const auto& queue = floor.GetWaitingIds(direction);
            if (!queue.empty() && m_hallCalls.count({ floor.GetFloorNumber(), direction }) != 1) return false;
            for (PassengerId id : queue)
            {
                const auto passenger = m_passengers.find(id);
                if (!seen.insert(id).second || passenger == m_passengers.end() ||
                    passenger->second.GetState() != PassengerState::Waiting ||
                    passenger->second.GetStartFloor() != floor.GetFloorNumber() ||
                    passenger->second.GetDirection() != direction) return false;
                ++waiting;
            }
        }
    }
    for (const auto& elevator : m_elevators)
    {
        const auto snapshot = elevator.GetSnapshot();
        if (snapshot.currentFloor < 1 || snapshot.currentFloor > m_config.floorCount ||
            snapshot.passengerCount < 0 || snapshot.passengerCount > snapshot.capacity) return false;
        for (PassengerId id : elevator.GetPassengerIds())
        {
            const auto passenger = m_passengers.find(id);
            if (!seen.insert(id).second || passenger == m_passengers.end()) return false;
            const auto data = passenger->second.GetSnapshot();
            if (data.state != PassengerState::Riding || data.elevatorId != snapshot.id ||
                data.boardTime < data.requestTime || data.boardTime > m_currentTime) return false;
            ++riding;
        }
    }
    for (const auto& [key, call] : m_hallCalls)
    {
        int owners = 0;
        for (const auto& elevator : m_elevators)
            if (elevator.HasHallCall(key.first, key.second))
            {
                if (elevator.GetSnapshot().id != call.assignedElevatorId) return false;
                ++owners;
            }
        if (owners != (call.assignedElevatorId == InvalidElevatorId ? 0 : 1)) return false;
    }
    const auto stats = m_statistics.GetSnapshot();
    return seen.size() == m_passengers.size() && stats.waitingCount == waiting && stats.ridingCount == riding &&
        stats.totalPassengerCount == waiting + riding + stats.arrivedCount &&
        stats.boardedCount == riding + stats.arrivedCount;
}

std::vector<ElevatorSnapshot> Simulation::GetElevatorSnapshots() const
{
    std::vector<ElevatorSnapshot> snapshots;
    snapshots.reserve(m_elevators.size());
    for (const Elevator& elevator : m_elevators)
        snapshots.push_back(elevator.GetSnapshot());
    return snapshots;
}

std::vector<FloorSnapshot> Simulation::GetFloorSnapshots() const
{
    std::vector<FloorSnapshot> snapshots;
    snapshots.reserve(m_floors.size());
    for (const Floor& floor : m_floors)
        snapshots.push_back(floor.GetSnapshot());
    return snapshots;
}

StatisticsSnapshot Simulation::GetStatisticsSnapshot() const
{
    return m_statistics.GetSnapshot();
}

SimulationUISnapshot Simulation::GetUISnapshot(bool workerActive) const
{
    SimulationUISnapshot snapshot;
    snapshot.state = m_state;
    snapshot.dispatcherMode = m_dispatcher.GetExecutionMode();
    snapshot.dispatcherWorkerCount = m_dispatcher.GetWorkerCount();
    snapshot.workerActive = workerActive;
    snapshot.currentTime = m_currentTime;
    snapshot.randomSeed = m_seed;
    snapshot.config = m_config;
    snapshot.lastError = m_lastError;
    snapshot.elevators = GetElevatorSnapshots();
    snapshot.floors = GetFloorSnapshots();
    snapshot.statistics = GetStatisticsSnapshot();
    snapshot.hallCalls = GetHallCallSnapshots();
    return snapshot;
}

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
    constexpr double PeakTrafficShare = 0.75;
    constexpr double InterFloorTrafficShare = 0.90;

    bool IsPositiveFinite(double value)
    {
        return std::isfinite(value) && value > 0.0;
    }

    double NextUnitRandom(std::mt19937& random)
    {
        return (static_cast<double>(random()) + 0.5) / 4294967296.0;
    }

    double NextArrivalTime(double now, double rate, std::mt19937& random)
    {
        if (rate == 0.0) return std::numeric_limits<double>::infinity();
        // U 严格在 (0,1)，指数间隔 -ln(U)/lambda；一轮中不按帧重新抽样。
        const double uniform = NextUnitRandom(random);
        const double next = now - std::log(uniform) / rate;
        return next > now ? next : std::nextafter(now, std::numeric_limits<double>::infinity());
    }

    bool DrawProbability(double probability, std::mt19937& random)
    {
        return NextUnitRandom(random) < probability;
    }

    std::pair<int, int> GenerateUniformRoute(int floorCount, std::mt19937& random)
    {
        const int start = std::uniform_int_distribution<int>(1, floorCount)(random);
        int target = std::uniform_int_distribution<int>(1, floorCount - 1)(random);
        if (target >= start) ++target; // 均匀选择除起点外的 L-1 层，不使用随机重试。
        return { start, target };
    }

    std::pair<int, int> GeneratePassengerRoute(
        TrafficPattern pattern, int floorCount, std::mt19937& random)
    {
        switch (pattern)
        {
        case TrafficPattern::UpPeak:
            if (DrawProbability(PeakTrafficShare, random))
                return { 1, std::uniform_int_distribution<int>(2, floorCount)(random) };
            break;
        case TrafficPattern::DownPeak:
            if (DrawProbability(PeakTrafficShare, random))
                return { std::uniform_int_distribution<int>(2, floorCount)(random), 1 };
            break;
        case TrafficPattern::InterFloor:
            // L=2 时 2~L 只有一层，无法构造不同起终点，因此回退为 Uniform。
            if (floorCount >= 3 && DrawProbability(InterFloorTrafficShare, random))
            {
                const int start = std::uniform_int_distribution<int>(2, floorCount)(random);
                int target = std::uniform_int_distribution<int>(2, floorCount - 1)(random);
                if (target >= start) ++target;
                return { start, target };
            }
            break;
        case TrafficPattern::Uniform:
            break;
        }
        // Uniform 保留原有随机调用顺序，默认配置的固定 seed 轨迹不变。
        return GenerateUniformRoute(floorCount, random);
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
        switch (config.trafficPattern)
        {
        case TrafficPattern::Uniform:
        case TrafficPattern::UpPeak:
        case TrafficPattern::DownPeak:
        case TrafficPattern::InterFloor:
            break;
        default:
            return "trafficPattern is invalid";
        }
        switch (config.trafficScenario)
        {
        case TrafficScenario::Fixed:
        case TrafficScenario::OfficeDay:
            break;
        default:
            return "trafficScenario is invalid";
        }
        if (config.trafficScenario == TrafficScenario::OfficeDay &&
            !std::isfinite(config.passengerRate * 1.5))
            return "OfficeDay passenger rate exceeds finite range";
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
        const bool officeDay = config.trafficScenario == TrafficScenario::OfficeDay;
        const TrafficPattern activeTrafficPattern = officeDay ?
            TrafficPattern::UpPeak : config.trafficPattern;
        const double activePassengerRate = officeDay ?
            config.passengerRate * 1.5 : config.passengerRate;
        const double currentPhaseEnd = officeDay ?
            config.simulationDuration * 0.25 : config.simulationDuration;
        const double nextArrival = NextArrivalTime(0.0, activePassengerRate, random);
        EventScheduler eventScheduler;
        if (nextArrival < currentPhaseEnd)
            eventScheduler.Push(nextArrival, SimulationEventType::PassengerArrival);
        if (officeDay)
        {
            eventScheduler.Push(config.simulationDuration * 0.25,
                SimulationEventType::TrafficPhaseChange);
            eventScheduler.Push(config.simulationDuration * 0.70,
                SimulationEventType::TrafficPhaseChange);
        }
        eventScheduler.Push(config.simulationDuration, SimulationEventType::SimulationEnd);
        std::vector<double> elevatorScheduledTimes(static_cast<std::size_t>(config.elevatorCount),
            std::numeric_limits<double>::infinity());
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
        statistics.Reset(config.elevatorCount, config.floorCount);

        m_floors = std::move(floors);
        m_elevators = std::move(elevators);
        m_statistics = std::move(statistics);
        m_passengers.clear();
        m_hallCalls.clear();
        m_nextPassengerId = 0;
        m_seed = seed;
        m_random = std::move(random);
        m_nextArrivalTime = nextArrival;
        m_eventScheduler = std::move(eventScheduler);
        m_elevatorScheduledTimes = std::move(elevatorScheduledTimes);
        m_trafficPhaseIndex = 0;
        m_activeTrafficPattern = activeTrafficPattern;
        m_activePassengerRate = activePassengerRate;
        m_currentPhaseEnd = currentPhaseEnd;
        m_config = config;
        m_currentTime = 0.0;
        m_dispatchDirty = true;
        m_lastReassessmentTime = UnsetTime;
        m_rebalanceDirty = true;
        m_lastFleetRebalanceTime = UnsetTime;
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
    StabilizeCurrentTime();
    ScheduleMissingElevatorEvents();
    while (!m_eventScheduler.Empty() && m_eventScheduler.Top().time <= targetTime)
    {
        const double eventTime = m_eventScheduler.Top().time;
        AdvanceClockTo(eventTime);
        bool simulationEnded = false;
        // 比较的是入队时保存的同一绝对时间；同刻事件由堆的显式次序稳定弹出。
        while (!m_eventScheduler.Empty() && m_eventScheduler.Top().time == eventTime)
        {
            const ScheduledEvent scheduled = m_eventScheduler.Top();
            m_eventScheduler.Pop();
            if (scheduled.type == SimulationEventType::ElevatorAction)
            {
                const std::size_t index = static_cast<std::size_t>(scheduled.elevatorId);
                m_elevatorScheduledTimes[index] = std::numeric_limits<double>::infinity();
                auto& elevator = m_elevators[index];
                const ElevatorEvent event = elevator.Advance(elevator.GetTimeToNextEvent());
                if (event.type == ElevatorEventType::None)
                    throw std::logic_error("Scheduled elevator action did not complete");
                HandleElevatorEvent(scheduled.elevatorId, event);
            }
            else if (scheduled.type == SimulationEventType::TrafficPhaseChange)
                HandleTrafficPhaseChange();
            else if (scheduled.type == SimulationEventType::PassengerArrival)
                GeneratePassengerArrival();
            else
                simulationEnded = true;
        }
        if (simulationEnded)
        {
            m_state = SimulationState::Finished;
            break;
        }
        StabilizeCurrentTime();
        ScheduleMissingElevatorEvents();
    }
    if (IsRunning() && m_currentTime < targetTime)
        AdvanceClockTo(targetTime);
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
    m_statistics.PassengerCreated(startFloor, direction);
    m_dispatchDirty = true;
    m_rebalanceDirty = true;
    return id;
}

void Simulation::GeneratePassengerArrival()
{
    const auto [start, target] = GeneratePassengerRoute(
        m_activeTrafficPattern, m_config.floorCount, m_random);
    if (AddPassenger(start, target) == InvalidPassengerId)
        throw std::overflow_error("Passenger ID space exhausted");
    ScheduleNextPassengerArrival();
}

void Simulation::HandleTrafficPhaseChange()
{
    for (auto& elevator : m_elevators)
        elevator.ClearRepositionTarget();
    ++m_trafficPhaseIndex;
    if (m_trafficPhaseIndex == 1)
    {
        m_activeTrafficPattern = TrafficPattern::InterFloor;
        m_activePassengerRate = m_config.passengerRate * 0.75;
        m_currentPhaseEnd = m_config.simulationDuration * 0.70;
    }
    else
    {
        m_activeTrafficPattern = TrafficPattern::DownPeak;
        m_activePassengerRate = m_config.passengerRate * 1.5;
        m_currentPhaseEnd = m_config.simulationDuration;
    }
    m_rebalanceDirty = true;
    ScheduleNextPassengerArrival();
}

void Simulation::ScheduleNextPassengerArrival()
{
    m_nextArrivalTime = NextArrivalTime(m_currentTime, m_activePassengerRate, m_random);
    if (m_nextArrivalTime < m_currentPhaseEnd)
        m_eventScheduler.Push(m_nextArrivalTime, SimulationEventType::PassengerArrival);
}

void Simulation::AdvanceClockTo(double newTime)
{
    const double elapsed = newTime - m_currentTime;
    if (elapsed > 0.0)
    {
        for (const auto& elevator : m_elevators)
        {
            const auto snapshot = elevator.GetSnapshot();
            m_statistics.ElevatorTimeElapsed(snapshot.id, elapsed, snapshot.state,
                snapshot.passengerCount == snapshot.capacity);
        }
    }
    m_currentTime = newTime;
}

void Simulation::ScheduleMissingElevatorEvents()
{
    for (std::size_t index = 0; index < m_elevators.size(); ++index)
    {
        if (std::isfinite(m_elevatorScheduledTimes[index])) continue;
        const double remaining = m_elevators[index].GetTimeToNextEvent();
        if (!std::isfinite(remaining)) continue;
        const double completionTime = m_currentTime + remaining;
        m_elevatorScheduledTimes[index] = completionTime;
        m_eventScheduler.Push(completionTime, SimulationEventType::ElevatorAction,
            static_cast<int>(index));
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
    if (changed) m_rebalanceDirty = true;
    return changed;
}

void Simulation::RebalanceIdleFleet()
{
    if (!m_config.predictiveRebalancing)
    {
        m_rebalanceDirty = false;
        return;
    }
    if (!m_rebalanceDirty)
        return;
    if (m_lastFleetRebalanceTime == m_currentTime ||
        (m_lastFleetRebalanceTime != UnsetTime &&
            m_currentTime - m_lastFleetRebalanceTime < FleetRebalancer::FleetRebalanceCooldown))
    {
        // 冷却期内的本次模型事件不排队到未来帧边界；后续模型事件再重新请求。
        m_rebalanceDirty = false;
        return;
    }

    const auto snapshots = BuildDispatchSnapshots();
    std::vector<int> idleElevatorIndices;
    for (std::size_t index = 0; index < snapshots.size(); ++index)
    {
        const auto& snapshot = snapshots[index];
        if (snapshot.elevator.state == ElevatorState::Idle &&
            snapshot.elevator.passengerCount == 0 && !m_elevators[index].IsRepositioning() &&
            snapshot.upTasks.empty() && snapshot.downTasks.empty() && snapshot.stopServices.empty())
            idleElevatorIndices.push_back(static_cast<int>(index));
    }
    const auto plan = m_fleetRebalancer.BuildPlan(snapshots, idleElevatorIndices,
        m_config.floorCount, m_activeTrafficPattern, m_activePassengerRate,
        m_currentTime, m_config, m_dispatcher);
    for (const auto& assignment : plan.assignments)
    {
        if (!m_elevators[static_cast<std::size_t>(assignment.elevatorId)].
            SetRepositionTarget(assignment.targetFloor))
            throw std::logic_error("Cannot commit fleet reposition assignment");
    }
    m_rebalanceDirty = false;
    m_lastFleetRebalanceTime = m_currentTime;
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
        if (std::isfinite(m_elevatorScheduledTimes[index]))
            snapshots[index].remainingActionTime = (std::max)(0.0,
                m_elevatorScheduledTimes[index] - m_currentTime);
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
    m_rebalanceDirty = true;
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
        if (!changed)
        {
            RebalanceIdleFleet();
            return;
        }
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
        if (m_elevators[static_cast<std::size_t>(elevatorId)].GetSnapshot().state == ElevatorState::Idle)
            m_rebalanceDirty = true;
        return;
    }
    m_rebalanceDirty = true;
    auto& passenger = m_passengers.at(event.passengerId);
    if (event.type == ElevatorEventType::Boarded)
    {
        auto& floor = m_floors[static_cast<std::size_t>(passenger.GetStartFloor() - 1)];
        if (!floor.RemoveFront(event.passengerId, passenger.GetDirection()) ||
            !passenger.MarkBoarded(elevatorId, m_currentTime))
            throw std::logic_error("Invalid passenger boarding transition");
        m_statistics.PassengerBoarded(passenger.GetStartFloor(),
            m_currentTime - passenger.GetRequestTime());
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

DispatchObservationSnapshot Simulation::GetDispatchObservation(int floor, Direction direction) const
{
    DispatchObservationSnapshot observation;
    observation.floor = floor;
    observation.direction = direction;
    const auto call = m_hallCalls.find({ floor, direction });
    if (call == m_hallCalls.end()) return observation;

    const auto request = BuildHallCallSnapshot(call->first, call->second);
    if (request.waitingCount == 0) return observation;
    observation.valid = true;
    observation.waitingCount = static_cast<std::size_t>(request.waitingCount);
    observation.firstRequestTime = request.firstRequestTime;
    observation.currentTime = m_currentTime;
    observation.assignedElevatorId = call->second.assignedElevatorId;

    const auto elevators = BuildDispatchSnapshots();
    observation.candidates.reserve(elevators.size());
    for (const auto& elevator : elevators)
    {
        const auto score = m_dispatcher.ScoreSnapshot(floor, direction, elevator,
            request.firstRequestTime, m_currentTime);
        observation.candidates.push_back({ elevator.elevator.id, score.feasible,
            score.cost, score.eta, score.projectedOccupancy });
    }
    std::sort(observation.candidates.begin(), observation.candidates.end(),
        [](const DispatchCandidateObservation& left, const DispatchCandidateObservation& right)
        {
            if (left.feasible != right.feasible) return left.feasible > right.feasible;
            if (left.cost != right.cost) return left.cost < right.cost;
            if (left.eta != right.eta) return left.eta < right.eta;
            return left.elevatorId < right.elevatorId;
        });
    return observation;
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
    if (stats.floorTraffic.size() != m_floors.size()) return false;
    std::uint64_t generatedByFloor = 0;
    std::uint64_t boardedByFloor = 0;
    for (std::size_t index = 0; index < stats.floorTraffic.size(); ++index)
    {
        const auto& traffic = stats.floorTraffic[index];
        if (traffic.floor != static_cast<int>(index) + 1 ||
            traffic.generatedCount != traffic.upRequestCount + traffic.downRequestCount ||
            traffic.boardedCount > traffic.generatedCount ||
            !std::isfinite(traffic.totalWaitingTime) || traffic.totalWaitingTime < 0.0 ||
            !std::isfinite(traffic.maxWaitingTime) || traffic.maxWaitingTime < 0.0)
            return false;
        generatedByFloor += traffic.generatedCount;
        boardedByFloor += traffic.boardedCount;
    }
    return seen.size() == m_passengers.size() && stats.waitingCount == waiting && stats.ridingCount == riding &&
        stats.totalPassengerCount == waiting + riding + stats.arrivedCount &&
        stats.boardedCount == riding + stats.arrivedCount &&
        generatedByFloor == stats.totalPassengerCount && boardedByFloor == stats.boardedCount;
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

std::vector<FloorCoverageSnapshot> Simulation::GetFloorCoverageSnapshots() const
{
    if (m_state == SimulationState::Uninitialized) return {};
    return FleetRebalancer::BuildCoverageSnapshots(BuildDispatchSnapshots(),
        m_config.floorCount, m_activeTrafficPattern, m_currentTime, m_dispatcher);
}

StatisticsSnapshot Simulation::GetStatisticsSnapshot() const
{
    return m_statistics.GetSnapshot();
}

SimulationUISnapshot Simulation::GetUISnapshot(bool workerActive, bool includeFloorCoverage) const
{
    SimulationUISnapshot snapshot;
    snapshot.state = m_state;
    snapshot.dispatcherMode = m_dispatcher.GetExecutionMode();
    snapshot.dispatcherWorkerCount = m_dispatcher.GetWorkerCount();
    snapshot.workerActive = workerActive;
    snapshot.currentTime = m_currentTime;
    snapshot.randomSeed = m_seed;
    snapshot.config = m_config;
    snapshot.trafficScenario = m_config.trafficScenario;
    snapshot.activeTrafficPattern = m_activeTrafficPattern;
    snapshot.trafficPhaseIndex = m_trafficPhaseIndex;
    snapshot.lastError = m_lastError;
    snapshot.elevators = GetElevatorSnapshots();
    snapshot.floors = GetFloorSnapshots();
    snapshot.statistics = GetStatisticsSnapshot();
    snapshot.hallCalls = GetHallCallSnapshots();
    if (includeFloorCoverage)
        snapshot.floorCoverage = GetFloorCoverageSnapshots();
    return snapshot;
}

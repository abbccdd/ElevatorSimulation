#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

// 唯一公共契约：核心、统计和 UI 必须共用这些定义。
enum class Direction
{
    Down = -1,
    Idle = 0,
    Up = 1
};

enum class PassengerState
{
    Waiting,
    Riding,
    Arrived
};

enum class ElevatorState
{
    Idle,
    MovingUp,
    MovingDown,
    Boarding,
    Alighting,
    Stopped
};

enum class SimulationState
{
    Uninitialized,
    Ready,
    Running,
    Paused,
    Finished
};

enum class DispatcherExecutionMode
{
    Sequential,
    Parallel
};

enum class TrafficPattern
{
    Uniform,
    UpPeak,
    DownPeak,
    InterFloor
};

enum class TrafficScenario
{
    Fixed,
    OfficeDay
};

using PassengerId = int;
inline constexpr int InvalidElevatorId = -1;
inline constexpr PassengerId InvalidPassengerId = -1;
inline constexpr int InvalidFloor = 0;
inline constexpr double UnsetTime = -1.0;

struct SimulationConfig
{
    int floorCount = 20;
    int elevatorCount = 6;
    int capacity = 15;
    double moveTimePerFloor = 2.0;
    double personTime = 3.0;
    double simulationDuration = 600.0;
    // 全楼每仿真秒平均到达人数，Poisson 到达；0 关闭随机产生。
    double passengerRate = 0.2;
    double simulationSpeed = 1.0;
    TrafficPattern trafficPattern = TrafficPattern::Uniform;
    TrafficScenario trafficScenario = TrafficScenario::Fixed;
    bool predictiveRebalancing = false;
};

// 相同楼层返回 Idle；这不代表相同起终点的乘客合法。
inline Direction GetDirection(int startFloor, int targetFloor) noexcept
{
    if (targetFloor > startFloor)
        return Direction::Up;
    if (targetFloor < startFloor)
        return Direction::Down;
    return Direction::Idle;
}

// Snapshot 均按值返回。调用者可以改副本，但不能借此修改核心对象。
struct ElevatorSnapshot
{
    int id = 0; // 容器下标 0~N-1；UI 显示 E(id+1)。
    int currentFloor = 1;
    Direction direction = Direction::Idle;
    ElevatorState state = ElevatorState::Idle;
    int passengerCount = 0;
    int capacity = 0;
    int repositionTargetFloor = InvalidFloor;
};

struct FloorSnapshot
{
    int floorNumber = 1; // 始终使用真实楼层编号 1~L。
    std::size_t upWaitingCount = 0;
    std::size_t downWaitingCount = 0;
};

// 群控专用只读输入；任务按服务方向分类，包含内部目标与已接受外呼。
// floorCount=0 表示旧三参数构造函数未给出建筑上界。
struct ElevatorDispatchSnapshot
{
    ElevatorSnapshot elevator;
    int floorCount = 0;
    double moveTimePerFloor = 2.0;
    double personTime = 3.0;
    double remainingActionTime = 0.0;
    bool betweenFloors = false;
    int reservedBoardingCount = 0;
    std::vector<int> upTasks;
    std::vector<int> downTasks;
    struct StopService
    {
        int floor = 1;
        // Idle 表示内呼/下客事件（0 人也保留停站）；Up/Down 表示对应外呼。
        Direction direction = Direction::Idle;
        // 包含 Alighting 中当前一人；Boarding 预留者的未来下客也已包含。
        int alightingCount = 0;
        // 不含正在 Boarding 的队头，避免与 reservedBoardingCount 重复。
        int boardingCount = 0;
        // 已知 FIFO 目标层前缀，最多需要 capacity 人；空列表兼容旧人数快照。
        std::vector<int> boardingTargetFloors{};
    };
    // Simulation 补充已分配外呼的真实队列与目标层；Dispatcher 只消费局部副本。
    std::vector<StopService> stopServices;
};

// 调度专用请求副本；不拥有乘客，只包含 FIFO 目标楼层数值。
struct HallCallDispatchSnapshot
{
    int floor = 1;
    Direction direction = Direction::Idle;
    double firstRequestTime = 0.0;
    PassengerId firstPassengerId = InvalidPassengerId;
    int waitingCount = 0;
    std::vector<int> targetFloors;
};

struct DispatchScore
{
    bool feasible = false;
    double cost = std::numeric_limits<double>::infinity();
    double eta = std::numeric_limits<double>::infinity();
    double directionPenalty = 0.0; // 已扣除有上限的 Aging 折扣。
    int projectedOccupancy = 0;
};

// 单个外呼的只读评分观察结果。仅用于展示，不参与调度提交。
struct DispatchCandidateObservation
{
    int elevatorId = InvalidElevatorId;
    bool feasible = false;
    double cost = std::numeric_limits<double>::infinity();
    double eta = std::numeric_limits<double>::infinity();
    int projectedOccupancy = 0;
};

struct DispatchObservationSnapshot
{
    bool valid = false;
    int floor = 1;
    Direction direction = Direction::Idle;
    std::size_t waitingCount = 0;
    double firstRequestTime = 0.0;
    double currentTime = 0.0;
    int assignedElevatorId = InvalidElevatorId;
    // feasible、Cost、ETA、elevatorId 顺序，保留全部候选供 UI 取 Top 10 和当前归属。
    std::vector<DispatchCandidateObservation> candidates;
};

struct DispatchPlan
{
    // 与传入请求顺序一致，值为电梯容器下标；-1 表示本批未分配。
    std::vector<int> elevatorIndices;
    std::size_t assignedCount = 0;
    double totalCost = 0.0;
    double maxEta = 0.0;
    double totalEta = 0.0;
    std::size_t evaluatedCombinations = 0;
    std::size_t scoreEvaluations = 0;
};

// 单梯返回事件，Simulation 负责用统一时钟登记 Passenger 和 Statistics。
enum class ElevatorEventType { None, FloorReached, Boarded, Alighted };
struct ElevatorEvent
{
    ElevatorEventType type = ElevatorEventType::None;
    double elapsedTime = 0.0;
    PassengerId passengerId = InvalidPassengerId;
    bool emptyMovement = false;
};

struct PassengerSnapshot
{
    PassengerId id = InvalidPassengerId;
    int startFloor = 1;
    int targetFloor = 1;
    Direction direction = Direction::Idle;
    PassengerState state = PassengerState::Waiting;
    double requestTime = 0.0;
    double boardTime = UnsetTime;
    double arrivalTime = UnsetTime;
    int elevatorId = InvalidElevatorId;
};

struct HallCallSnapshot
{
    int floorNumber = 1;
    Direction direction = Direction::Idle;
    std::size_t waitingCount = 0;
    int assignedElevatorId = InvalidElevatorId;
    double firstRequestTime = 0.0;
};

struct ElevatorStatisticsSnapshot
{
    int id = 0;
    std::size_t transportedCount = 0;
    std::size_t traveledFloors = 0;
    std::size_t emptyTravelFloors = 0;
    double idleTime = 0.0;
    double fullTime = 0.0;
};

struct StatisticsSnapshot
{
    std::size_t totalPassengerCount = 0;
    std::size_t waitingCount = 0;
    std::size_t ridingCount = 0;
    std::size_t arrivedCount = 0;
    std::size_t boardedCount = 0;
    double averageWaitingTime = 0.0;
    double maxWaitingTime = 0.0;
    double averageRideTime = 0.0;
    std::vector<ElevatorStatisticsSnapshot> elevators;
};

// UI 只读取这一份按值构造的完整视图，不接触 Simulation 的可写状态。
struct SimulationUISnapshot
{
    SimulationState state = SimulationState::Uninitialized;
    DispatcherExecutionMode dispatcherMode = DispatcherExecutionMode::Sequential;
    std::size_t dispatcherWorkerCount = 0;
    bool workerActive = false;
    double currentTime = 0.0;
    std::uint32_t randomSeed = 0;
    SimulationConfig config;
    TrafficScenario trafficScenario = TrafficScenario::Fixed;
    TrafficPattern activeTrafficPattern = TrafficPattern::Uniform;
    std::size_t trafficPhaseIndex = 0;
    std::string lastError;
    std::vector<ElevatorSnapshot> elevators;
    std::vector<FloorSnapshot> floors;
    StatisticsSnapshot statistics;
    // 高频 UI 快照不填充乘客明细；按需使用 Simulation::GetPassengerSnapshots()。
    std::vector<PassengerSnapshot> passengers;
    std::vector<HallCallSnapshot> hallCalls;
};

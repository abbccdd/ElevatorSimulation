#pragma once

#include <cstddef>
#include <cstdint>
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

using PassengerId = int;
inline constexpr int InvalidElevatorId = -1;
inline constexpr PassengerId InvalidPassengerId = -1;
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

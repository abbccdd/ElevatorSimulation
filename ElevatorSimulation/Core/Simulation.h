#pragma once

#include "CommonTypes.h"
#include "Dispatcher.h"
#include "Elevator.h"
#include "Floor.h"
#include "Passenger.h"
#include "../Statistics/Statistics.h"

#include <string>
#include <unordered_map>
#include <vector>

class Simulation
{
public:
    // 成功：替换为全新的 Ready 状态。失败：保留原参数、容器和运行状态。
    bool Initialize(const SimulationConfig& config);
    const std::string& GetLastError() const noexcept { return m_lastError; }

    void Start();  // 仅 Ready -> Running，不隐式重置已结束的仿真。
    void Pause(); // 仅 Running -> Paused。
    void Resume(); // 仅 Paused -> Running。
    void Reset(); // 以最近一次有效参数重新初始化；未初始化时无操作。

    // deltaTime 是真实经过的秒数，内部只乘一次 simulationSpeed。
    // 当前仅推进时钟；尚不产生乘客、不调度、不移动电梯。
    void Update(double deltaTime);

    bool IsRunning() const noexcept { return m_state == SimulationState::Running; }
    bool IsFinished() const noexcept { return m_state == SimulationState::Finished; }
    SimulationState GetState() const noexcept { return m_state; }
    double GetCurrentTime() const noexcept { return m_currentTime; }
    SimulationConfig GetConfig() const { return m_config; }

    std::vector<ElevatorSnapshot> GetElevatorSnapshots() const;
    std::vector<FloorSnapshot> GetFloorSnapshots() const;
    StatisticsSnapshot GetStatisticsSnapshot() const;

private:
    SimulationConfig m_config;
    SimulationState m_state = SimulationState::Uninitialized;
    double m_currentTime = 0.0;
    std::string m_lastError;
    // m_floors 的存储下标不是楼层编号；Floor 自身和所有对外接口使用 1~L。
    std::vector<Floor> m_floors;
    std::vector<Elevator> m_elevators;
    std::unordered_map<PassengerId, Passenger> m_passengers;
    ElevatorDispatcher m_dispatcher;
    Statistics m_statistics;
};

#include "Core/Simulation.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
    int checkCount = 0;

    void Check(bool condition, const char* message)
    {
        ++checkCount;
        if (!condition)
            throw std::runtime_error(message);
    }

    void CheckInitialLayout(int floorCount, int elevatorCount, int middleFloor)
    {
        Simulation simulation;
        SimulationConfig config;
        config.floorCount = floorCount;
        config.elevatorCount = elevatorCount;
        Check(simulation.Initialize(config), "valid initialization");
        auto floors = simulation.GetFloorSnapshots();
        auto elevators = simulation.GetElevatorSnapshots();
        Check(floors.size() == static_cast<std::size_t>(floorCount), "floor count");
        Check(elevators.size() == static_cast<std::size_t>(elevatorCount), "elevator count");
        for (int index = 0; index < floorCount; ++index)
        {
            Check(floors[index].floorNumber == index + 1, "real floor numbering");
            Check(floors[index].upWaitingCount == 0 && floors[index].downWaitingCount == 0,
                "initial empty queues");
        }
        for (int id = 0; id < elevatorCount; ++id)
        {
            const auto& elevator = elevators[id];
            const int expectedFloor = id < elevatorCount / 3 ? 1 :
                (id < 2 * (elevatorCount / 3) ? floorCount : middleFloor);
            Check(elevator.id == id && elevator.currentFloor == expectedFloor, "initial groups");
            Check(elevator.direction == Direction::Idle && elevator.state == ElevatorState::Idle,
                "initial idle state");
            Check(elevator.passengerCount == 0 && elevator.capacity == config.capacity,
                "initial capacity");
        }
        // 副本隔离：模拟 UI 意外改动，核心状态不能被修改。
        elevators[0].currentFloor = 999;
        floors[0].upWaitingCount = 999;
        auto statistics = simulation.GetStatisticsSnapshot();
        Check(statistics.elevators.size() == elevators.size(), "per-elevator statistics slots");
        Check(statistics.totalPassengerCount == 0 && statistics.arrivedCount == 0, "empty statistics");
        statistics.elevators[0].transportedCount = 999;
        auto copiedConfig = simulation.GetConfig();
        copiedConfig.capacity = 999;
        Check(simulation.GetElevatorSnapshots()[0].currentFloor == 1, "elevator snapshot isolation");
        Check(simulation.GetFloorSnapshots()[0].upWaitingCount == 0, "floor snapshot isolation");
        Check(simulation.GetStatisticsSnapshot().elevators[0].transportedCount == 0,
            "statistics snapshot isolation");
        Check(simulation.GetConfig().capacity != copiedConfig.capacity, "config copy isolation");
    }

    void CheckInvalidConfigs()
    {
        Simulation simulation;
        const SimulationConfig valid;
        Check(simulation.Initialize(valid), "baseline config");
        simulation.Start();
        simulation.Update(1.0);
        const auto reject = [&](const SimulationConfig& invalid)
        {
            Check(!simulation.Initialize(invalid), "invalid configuration rejected");
            Check(!simulation.GetLastError().empty(), "diagnostic available");
            Check(simulation.IsRunning() && simulation.GetCurrentTime() == 1.0,
                "failed init preserves runtime state");
            Check(simulation.GetConfig().floorCount == valid.floorCount &&
                simulation.GetElevatorSnapshots().size() == 6, "failed init preserves model");
        };
        for (int value : { -1, 0, 1 })
        {
            auto invalid = valid;
            invalid.floorCount = value;
            reject(invalid);
        }
        for (int value : { -3, 0, 1, 4 })
        {
            auto invalid = valid;
            invalid.elevatorCount = value;
            reject(invalid);
        }
        for (int value : { -1, 0 })
        {
            auto invalid = valid;
            invalid.capacity = value;
            reject(invalid);
        }
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const double inf = std::numeric_limits<double>::infinity();
        for (auto field : { &SimulationConfig::moveTimePerFloor, &SimulationConfig::personTime,
            &SimulationConfig::simulationDuration, &SimulationConfig::simulationSpeed })
        {
            for (double value : { -1.0, 0.0, nan, inf, -inf })
            {
                auto invalid = valid;
                invalid.*field = value;
                reject(invalid);
            }
        }
        for (double value : { -1.0, nan, inf, -inf })
        {
            auto invalid = valid;
            invalid.passengerRate = value;
            reject(invalid);
        }
        auto invalidPattern = valid;
        invalidPattern.trafficPattern = static_cast<TrafficPattern>(999);
        reject(invalidPattern);
        auto invalidScenario = valid;
        invalidScenario.trafficScenario = static_cast<TrafficScenario>(999);
        reject(invalidScenario);
        auto noPassengers = valid;
        noPassengers.passengerRate = 0.0;
        Check(simulation.Initialize(noPassengers) && simulation.GetLastError().empty(),
            "zero rate allowed and diagnostic cleared");
    }

    void CheckLifecycle()
    {
        Simulation simulation;
        simulation.Start();
        simulation.Pause();
        simulation.Resume();
        simulation.Reset();
        simulation.Update(10.0);
        Check(simulation.GetState() == SimulationState::Uninitialized &&
            !simulation.IsFinished() && simulation.GetCurrentTime() == 0.0, "safe before init");
        Check(simulation.GetElevatorSnapshots().empty(), "no phantom elevators before init");
        SimulationConfig config;
        config.simulationSpeed = 2.0;
        config.simulationDuration = 5.0;
        Check(simulation.Initialize(config), "lifecycle init");
        simulation.Resume();
        simulation.Update(10.0);
        Check(simulation.GetState() == SimulationState::Ready, "resume cannot start ready state");
        simulation.Start();
        simulation.Update(0.5);
        Check(simulation.GetCurrentTime() == 1.0, "speed applied exactly once");
        simulation.Start();
        Check(simulation.GetCurrentTime() == 1.0, "repeated start does not reset");
        simulation.Pause();
        simulation.Update(10.0);
        simulation.Start();
        Check(simulation.GetCurrentTime() == 1.0 && !simulation.IsRunning(), "pause freezes time");
        simulation.Resume();
        for (double value : { 0.0, -1.0, std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity() })
            simulation.Update(value);
        Check(simulation.GetCurrentTime() == 1.0, "invalid deltas ignored");
        simulation.Update(100.0);
        Check(simulation.GetCurrentTime() == 5.0 && simulation.IsFinished() && !simulation.IsRunning(),
            "stop at duration without overshoot");
        simulation.Start();
        simulation.Resume();
        simulation.Update(100.0);
        Check(simulation.IsFinished() && simulation.GetCurrentTime() == 5.0, "finished cannot restart");
        simulation.Reset();
        Check(simulation.GetState() == SimulationState::Ready && simulation.GetCurrentTime() == 0.0 &&
            simulation.GetConfig().simulationSpeed == 2.0, "reset preserves valid config");
        simulation.Start();
        simulation.Update(std::numeric_limits<double>::max());
        Check(simulation.GetCurrentTime() == 5.0 && simulation.IsFinished(), "large finite delta clamps");
    }
}

int main()
{
    try
    {
        CheckInitialLayout(30, 6, 15);
        CheckInitialLayout(21, 9, 11);
        CheckInitialLayout(2, 3, 1);
        CheckInitialLayout(20, 6, 10);
        CheckInvalidConfigs();
        CheckLifecycle();
        Passenger passenger(0, 1, 3, 0.0);
        Check(passenger.GetDirection() == Direction::Up && passenger.GetState() == PassengerState::Waiting,
            "passenger initial direction and state");
        Check(passenger.GetBoardTime() == UnsetTime && passenger.GetArrivalTime() == UnsetTime,
            "unset timestamps");
        Check(GetDirection(3, 1) == Direction::Down && GetDirection(2, 2) == Direction::Idle,
            "direction helper");
        bool rejected = false;
        try { Passenger invalid(0, 2, 2, 0.0); }
        catch (const std::invalid_argument&) { rejected = true; }
        Check(rejected, "same-floor passenger rejected");
        const ElevatorDispatcher dispatcher;
        Check(dispatcher.SelectElevator(1, Direction::Up, {}) == InvalidElevatorId,
            "dispatcher reports unassigned");
        Check(dispatcher.SelectElevator(1, Direction::Up, { Elevator(0, 1, 15) }) == 0,
            "implemented dispatcher assigns a valid idle elevator");
        std::cout << "PASS: " << checkCount << " core smoke checks (no MFC dependency).\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}

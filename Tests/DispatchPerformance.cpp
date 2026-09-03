#include "Core/Dispatcher.h"

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{
    constexpr int FloorCount = 120;
    constexpr int Iterations = 120;

    std::vector<ElevatorDispatchSnapshot> BuildFleet(int elevatorCount)
    {
        std::vector<ElevatorDispatchSnapshot> fleet;
        fleet.reserve(static_cast<std::size_t>(elevatorCount));
        for (int id = 0; id < elevatorCount; ++id)
        {
            ElevatorDispatchSnapshot snapshot;
            snapshot.elevator.id = id;
            snapshot.elevator.currentFloor = 2 + id * 17 % (FloorCount - 2);
            snapshot.elevator.direction = id % 2 == 0 ? Direction::Up : Direction::Down;
            snapshot.elevator.state = id % 2 == 0 ? ElevatorState::MovingUp : ElevatorState::MovingDown;
            snapshot.elevator.passengerCount = 5;
            snapshot.elevator.capacity = 15;
            snapshot.floorCount = FloorCount;
            snapshot.moveTimePerFloor = 0.5;
            snapshot.personTime = 0.25;
            snapshot.remainingActionTime = 0.25;
            snapshot.betweenFloors = true;
            for (int floor = 3; floor < FloorCount; floor += 4)
            {
                snapshot.upTasks.push_back(floor);
                if (floor + 1 < FloorCount) snapshot.downTasks.push_back(floor + 1);
            }
            fleet.push_back(std::move(snapshot));
        }
        return fleet;
    }

    std::pair<double, std::vector<int>> Measure(ElevatorDispatcher& dispatcher,
        const std::vector<ElevatorDispatchSnapshot>& fleet)
    {
        std::vector<int> selections;
        selections.reserve(Iterations);
        const auto begin = std::chrono::steady_clock::now();
        for (int iteration = 0; iteration < Iterations; ++iteration)
        {
            const int floor = 2 + iteration * 37 % (FloorCount - 2);
            const Direction direction = iteration % 2 == 0 ? Direction::Up : Direction::Down;
            selections.push_back(dispatcher.SelectFromSnapshots(floor, direction, fleet,
                static_cast<double>(iteration), 200.0 + iteration));
        }
        const double elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - begin).count();
        return { elapsed / Iterations, std::move(selections) };
    }
}

int main()
{
    try
    {
        std::cout << "elevators,workers,sequential_ms,parallel_ms,speedup,identical\n";
        for (const int elevatorCount : { 6, 30, 60, 120 })
        {
            const auto fleet = BuildFleet(elevatorCount);
            ElevatorDispatcher sequential(DispatcherExecutionMode::Sequential);
            ElevatorDispatcher parallel(DispatcherExecutionMode::Parallel);
            sequential.SelectFromSnapshots(60, Direction::Up, fleet, 0.0, 200.0);
            parallel.SelectFromSnapshots(60, Direction::Up, fleet, 0.0, 200.0);
            const auto sequentialResult = Measure(sequential, fleet);
            const auto parallelResult = Measure(parallel, fleet);
            const bool identical = sequentialResult.second == parallelResult.second;
            if (!identical) throw std::runtime_error("sequential/parallel selection mismatch");
            std::cout << elevatorCount << ',' << parallel.GetWorkerCount() << ','
                << std::fixed << std::setprecision(4) << sequentialResult.first << ','
                << parallelResult.first << ',' << sequentialResult.first / parallelResult.first
                << ",yes\n";
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}

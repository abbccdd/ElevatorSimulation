#include "Core/Simulation.h"

#include <chrono>
#include <cstddef>
#include <iostream>

int main()
{
    SimulationConfig config;
    config.floorCount = 100;
    config.elevatorCount = 99;
    config.capacity = 20;
    config.moveTimePerFloor = 0.75;
    config.personTime = 0.35;
    config.passengerRate = 1.5;
    config.simulationDuration = 1200.0;
    config.trafficPattern = TrafficPattern::Uniform;

    Simulation simulation;
    if (!simulation.Initialize(config, 20260904u))
        return 2;
    simulation.Start();
    const auto started = std::chrono::steady_clock::now();
    simulation.Update(config.simulationDuration);
    const auto finished = std::chrono::steady_clock::now();
    if (!simulation.IsFinished() || !simulation.ValidateState())
        return 1;

    const auto statistics = simulation.GetStatisticsSnapshot();
    std::size_t traveledFloors = 0;
    for (const auto& elevator : statistics.elevators)
        traveledFloors += elevator.traveledFloors;
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(finished - started).count();
    std::cout << "wall_ms=" << milliseconds
        << " generated=" << statistics.totalPassengerCount
        << " arrived=" << statistics.arrivedCount
        << " waiting=" << statistics.waitingCount
        << " riding=" << statistics.ridingCount
        << " traveled_floors=" << traveledFloors << '\n';
    return 0;
}

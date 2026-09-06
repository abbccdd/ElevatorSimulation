#include "Core/Dispatcher.h"
#include "Core/Simulation.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    const char* DirectionName(Direction direction)
    {
        if (direction == Direction::Up) return "上行";
        if (direction == Direction::Down) return "下行";
        return "空闲";
    }

    const char* StateName(ElevatorState state)
    {
        switch (state)
        {
        case ElevatorState::Idle: return "Idle";
        case ElevatorState::MovingUp: return "MovingUp";
        case ElevatorState::MovingDown: return "MovingDown";
        case ElevatorState::Stopped: return "Stopped";
        case ElevatorState::Boarding: return "Boarding";
        case ElevatorState::Alighting: return "Alighting";
        }
        return "Unknown";
    }

    void Title(const char* number, const char* title, const char* point)
    {
        std::cout << "\n============================================================\n"
            << "场景 " << number << "：" << title << '\n'
            << "展示重点：" << point << '\n'
            << "============================================================\n";
    }

    ElevatorDispatchSnapshot IdleCar(int id, int floor, int capacity = 10)
    {
        ElevatorDispatchSnapshot car;
        car.elevator = { id, floor, Direction::Idle, ElevatorState::Idle, 0, capacity };
        car.floorCount = 20;
        return car;
    }

    void AddInternalStop(ElevatorDispatchSnapshot& car, int floor, int alightingCount = 0)
    {
        auto& tasks = floor > car.elevator.currentFloor ? car.upTasks : car.downTasks;
        tasks.push_back(floor);
        car.stopServices.push_back({ floor, Direction::Idle, alightingCount, 0, {} });
    }

    void PrintScore(const ElevatorDispatcher& dispatcher, const HallCallDispatchSnapshot& request,
        const ElevatorDispatchSnapshot& car, double currentTime)
    {
        const auto score = dispatcher.ScoreSnapshot(request.floor, request.direction, car,
            request.firstRequestTime, currentTime);
        std::cout << "  E" << car.elevator.id + 1 << "：" << car.elevator.currentFloor << "F "
            << StateName(car.elevator.state) << '/' << DirectionName(car.elevator.direction);
        if (!score.feasible)
        {
            std::cout << " -> 不可行（预测到请求层仍无容量）\n";
            return;
        }
        std::cout << std::fixed << std::setprecision(2)
            << " -> ETA=" << score.eta << "s, Cost=" << score.cost
            << ", 请求层预计载客=" << score.projectedOccupancy << '\n';
    }

    void PrintHallCalls(const Simulation& simulation, const char* label)
    {
        std::cout << label << "（t=" << std::fixed << std::setprecision(3)
            << simulation.GetCurrentTime() << "s）\n";
        for (const auto& call : simulation.GetHallCallSnapshots())
        {
            std::cout << "  " << call.floorNumber << "F " << DirectionName(call.direction)
                << "，等待=" << call.waitingCount << "，负责梯=";
            if (call.assignedElevatorId == InvalidElevatorId) std::cout << "未分配/Deferred";
            else std::cout << 'E' << call.assignedElevatorId + 1;
            std::cout << "，首次请求=" << call.firstRequestTime << "s\n";
        }
    }

    void ScenarioEta()
    {
        Title("1", "ETA 与容量路线预演", "空闲梯和顺路梯统一比较；当前满载不等于永远不可用");
        ElevatorDispatcher dispatcher;
        HallCallDispatchSnapshot request{ 10, Direction::Up, 0.0, 0, 1, { 15 } };

        auto onWay = IdleCar(0, 1);
        onWay.elevator.direction = Direction::Up;
        onWay.elevator.state = ElevatorState::MovingUp;
        onWay.betweenFloors = true;
        onWay.remainingActionTime = 2.0;
        AddInternalStop(onWay, 15);

        auto immediate = IdleCar(1, 10);

        auto fullButReleases = IdleCar(2, 7, 1);
        fullButReleases.elevator = { 2, 7, Direction::Up, ElevatorState::MovingUp, 1, 1 };
        fullButReleases.betweenFloors = true;
        fullButReleases.remainingActionTime = 2.0;
        AddInternalStop(fullButReleases, 8, 1);

        const std::vector<ElevatorDispatchSnapshot> cars{ onWay, immediate, fullButReleases };
        std::cout << "请求：10F 上行，候梯评分如下：\n";
        for (const auto& car : cars) PrintScore(dispatcher, request, car, 0.0);
        const int selected = dispatcher.SelectFromSnapshots(request.floor, request.direction, cars, 0.0, 0.0);
        if (selected == InvalidElevatorId) throw std::runtime_error("ETA 场景没有可行候选");
        std::cout << "结论：选择 E" << selected + 1
            << "。E1 虽同向顺路，但 E2 已在请求层；E3 当前满载，却因 8F 已知下客仍是可行候选。\n";
    }

    void ScenarioAging()
    {
        Title("2", "Aging 防止请求长期饥饿", "等待时间只抵消有限方向成本，不覆盖真实 ETA");
        ElevatorDispatcher dispatcher;
        HallCallDispatchSnapshot request{ 4, Direction::Up, 0.0, 10, 1, { 8 } };

        auto reverseBusy = IdleCar(0, 3);
        reverseBusy.elevator.direction = Direction::Down;
        reverseBusy.elevator.state = ElevatorState::MovingDown;
        reverseBusy.betweenFloors = true;
        reverseBusy.remainingActionTime = 2.0;
        AddInternalStop(reverseBusy, 1);
        const auto fartherIdle = IdleCar(1, 11);
        const std::vector<ElevatorDispatchSnapshot> cars{ reverseBusy, fartherIdle };

        std::cout << "新请求（等待 0 秒）：\n";
        for (const auto& car : cars) PrintScore(dispatcher, request, car, 0.0);
        const int fresh = dispatcher.SelectFromSnapshots(4, Direction::Up, cars, 0.0, 0.0);
        if (fresh == InvalidElevatorId) throw std::runtime_error("新请求没有可行候选");
        std::cout << "  选择 E" << fresh + 1 << "\n\n长时间等待（200 秒，Aging 已达到 8 秒上限）：\n";
        for (const auto& car : cars) PrintScore(dispatcher, request, car, 200.0);
        const int aged = dispatcher.SelectFromSnapshots(4, Direction::Up, cars, 0.0, 200.0);
        if (aged == InvalidElevatorId) throw std::runtime_error("Aging 请求没有可行候选");
        std::cout << "  选择 E" << aged + 1 << "；AgingBonus="
            << dispatcher.GetAgingBonus(0.0, 200.0) << "s。\n";
    }

    void ScenarioJoint()
    {
        Title("3", "有限联合分配", "同时看两个请求，避免逐个贪心造成更高总成本");
        SimulationConfig config;
        config.floorCount = 20;
        config.elevatorCount = 3;
        config.capacity = 10;
        config.moveTimePerFloor = 2.0;
        config.personTime = 3.0;
        ElevatorDispatcher dispatcher;

        std::vector<Elevator> greedy;
        greedy.emplace_back(0, 5, config);
        greedy.emplace_back(1, 1, config);
        const int first = dispatcher.SelectElevator(4, Direction::Up, greedy, 0.0, 0.0);
        if (first == InvalidElevatorId) throw std::runtime_error("贪心第一项无法分配");
        const auto firstScore = dispatcher.ScoreSnapshot(4, Direction::Up,
            greedy[static_cast<std::size_t>(first)].GetDispatchSnapshot(), 0.0, 0.0);
        greedy[static_cast<std::size_t>(first)].AddHallCall(4, Direction::Up);
        const int second = dispatcher.SelectElevator(6, Direction::Up, greedy, 0.0, 0.0);
        if (second == InvalidElevatorId) throw std::runtime_error("贪心第二项无法分配");
        const auto secondScore = dispatcher.ScoreSnapshot(6, Direction::Up,
            greedy[static_cast<std::size_t>(second)].GetDispatchSnapshot(), 0.0, 0.0);

        std::vector<ElevatorDispatchSnapshot> original;
        original.push_back(Elevator(0, 5, config).GetDispatchSnapshot());
        original.push_back(Elevator(1, 1, config).GetDispatchSnapshot());
        const std::vector<HallCallDispatchSnapshot> requests{
            { 4, Direction::Up, 0.0, 0, 1, { 20 } },
            { 6, Direction::Up, 0.0, 1, 1, { 20 } }
        };
        const auto plan = dispatcher.PlanAssignments(requests, original, 0.0);
        if (plan.assignedCount != requests.size()) throw std::runtime_error("联合分配没有覆盖两个请求");

        std::cout << "逐个贪心：4F 请求->E" << first + 1 << "，6F 请求->E" << second + 1
            << "，两次即时 Cost 合计=" << firstScore.cost + secondScore.cost << "\n";
        std::cout << "联合分配：4F 请求->E" << plan.elevatorIndices[0] + 1
            << "，6F 请求->E" << plan.elevatorIndices[1] + 1
            << "，最终路线总 Cost=" << plan.totalCost << "\n";
        std::cout << "搜索规模：" << plan.evaluatedCombinations << " 个叶组合，"
            << plan.scoreEvaluations << " 次评分；结论是 12 -> 8，但不声称全局最优。\n";
    }

    void ScenarioReassignment()
    {
        Title("4", "带滞回的动态改派", "模型事件触发重评估，并同步旧梯、新梯与 Hall Call 归属");
        SimulationConfig config;
        config.floorCount = 20;
        config.elevatorCount = 3;
        config.capacity = 4;
        config.passengerRate = 0.0;
        config.simulationDuration = 100.0;
        Simulation simulation;
        if (!simulation.Initialize(config, 42)) throw std::runtime_error(simulation.GetLastError());
        simulation.AddPassenger(15, 20);
        simulation.Start();
        simulation.Update(2.0);
        PrintHallCalls(simulation, "加入绕行请求前");

        simulation.AddPassenger(17, 1);
        simulation.Update(2.0);
        PrintHallCalls(simulation, "加入 17F 下行请求后");
        if (!simulation.ValidateState()) throw std::runtime_error("改派后所有权校验失败");
        std::cout << "结论：15F 上行从 E2 改派给 E3；收益需达到 5 秒，"
            << "且服务锁、1 层接近保护与 10 秒冷却防止来回抢单。\n";
    }

    Simulation MakeFullUpFleet()
    {
        SimulationConfig config;
        config.floorCount = 20;
        config.elevatorCount = 3;
        config.capacity = 1;
        config.passengerRate = 0.0;
        config.simulationDuration = 300.0;
        Simulation simulation;
        if (!simulation.Initialize(config, 42)) throw std::runtime_error(simulation.GetLastError());
        simulation.AddPassenger(20, 1);
        simulation.Start();
        simulation.Update(44.0);
        simulation.AddPassenger(1, 20);
        simulation.AddPassenger(1, 20);
        simulation.AddPassenger(10, 20);
        simulation.Update(6.125);
        return simulation;
    }

    void ScenarioDeferred()
    {
        Title("5", "DeferredCapacity 不阻塞后续请求", "不可行请求保留 FIFO/Aging，但不占三个 Active 名额");
        auto simulation = MakeFullUpFleet();
        for (int floor = 12; floor <= 14; ++floor) simulation.AddPassenger(floor, 20);
        simulation.Update(0.125);
        PrintHallCalls(simulation, "前三个请求：三台梯都满载且在请求层前不释放容量");

        for (int floor = 15; floor <= 17; ++floor) simulation.AddPassenger(floor, 1);
        simulation.Update(0.125);
        PrintHallCalls(simulation, "再加入三个当前可行的下行请求");

        simulation.Update(0.625);
        PrintHallCalls(simulation, "路线顺序变化后的事件重评估");
        if (!simulation.ValidateState()) throw std::runtime_error("Deferred 场景所有权校验失败");
        std::cout << "结论：12~14F 上行先临时 Deferred，15~17F 下行仍在同一调度事件分配；"
            << "Deferred 请求时间不重置，并在路线/容量变化后自动恢复。\n";
    }

    void ScenarioBatch()
    {
        Title("6", "固定批次端到端仿真", "正式 Simulation 统一拥有乘客、推进事件并统计结果");
        SimulationConfig config;
        config.floorCount = 20;
        config.elevatorCount = 6;
        config.capacity = 3;
        config.moveTimePerFloor = 0.5;
        config.personTime = 0.25;
        config.passengerRate = 0.0;
        config.simulationDuration = 20000.0;
        Simulation simulation;
        if (!simulation.Initialize(config, 2026)) throw std::runtime_error(simulation.GetLastError());
        constexpr int passengerCount = 90;
        for (int index = 0; index < passengerCount; ++index)
        {
            const int start = index % 20 + 1;
            const int target = (start - 1 + 1 + index % 19) % 20 + 1;
            simulation.AddPassenger(start, target);
        }
        simulation.Start();
        simulation.Update(config.simulationDuration);
        const auto stats = simulation.GetStatisticsSnapshot();
        if (!simulation.ValidateState()) throw std::runtime_error("批次仿真人数守恒失败");
        std::cout << "固定 seed=2026，乘客=" << stats.totalPassengerCount
            << "，送达=" << stats.arrivedCount << "，等待=" << stats.waitingCount
            << "，乘梯中=" << stats.ridingCount << '\n'
            << "平均等待=" << stats.averageWaitingTime << "s，最大等待=" << stats.maxWaitingTime
            << "s，平均乘梯=" << stats.averageRideTime << "s\n";
        std::cout << "结论：90 人全部送达，活动对象、楼层队列、电梯 ID 和统计人数守恒。\n";
    }

    void RunScenario(int number)
    {
        switch (number)
        {
        case 1: ScenarioEta(); break;
        case 2: ScenarioAging(); break;
        case 3: ScenarioJoint(); break;
        case 4: ScenarioReassignment(); break;
        case 5: ScenarioDeferred(); break;
        case 6: ScenarioBatch(); break;
        default: throw std::invalid_argument("场景编号必须是 1~6");
        }
    }

    void RunAll(bool pauseBetween)
    {
        for (int scenario = 1; scenario <= 6; ++scenario)
        {
            RunScenario(scenario);
            if (pauseBetween && scenario != 6)
            {
                std::cout << "\n按回车进入下一场景..." << std::flush;
                std::string line;
                std::getline(std::cin, line);
            }
        }
    }

    void PrintMenu()
    {
        std::cout << "多电梯群控调度算法演示（固定场景，可重复）\n"
            << "  1. ETA 与容量路线预演\n"
            << "  2. Aging 防饥饿\n"
            << "  3. 有限联合分配\n"
            << "  4. 带滞回动态改派\n"
            << "  5. DeferredCapacity\n"
            << "  6. 固定批次端到端仿真\n"
            << "  0. 全部运行\n"
            << "请选择：" << std::flush;
    }
}

int main(int argumentCount, char* arguments[])
{
    try
    {
        std::cout << std::fixed << std::setprecision(2);
        if (argumentCount > 1)
        {
            const std::string mode = arguments[1];
            if (mode == "--all") RunAll(false);
            else if (mode == "--step") RunAll(true);
            else RunScenario(std::stoi(mode));
        }
        else
        {
            PrintMenu();
            std::string input;
            std::getline(std::cin, input);
            const int selection = std::stoi(input);
            if (selection == 0) RunAll(false);
            else RunScenario(selection);
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << "演示失败：" << error.what() << '\n';
        return 1;
    }
    return 0;
}

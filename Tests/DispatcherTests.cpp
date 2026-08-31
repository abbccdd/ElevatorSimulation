#include "Core/Dispatcher.h"
#include "TestSupport.h"
#include <limits>

namespace
{
    ElevatorDispatchSnapshot Car(int id, int floor, Direction direction = Direction::Idle,
        std::vector<int> up = {}, std::vector<int> down = {}, int passengers = 0, int capacity = 10)
    {
        ElevatorDispatchSnapshot car;
        car.elevator = { id, floor, direction,
            direction == Direction::Idle ? ElevatorState::Idle : ElevatorState::Stopped, passengers, capacity };
        car.floorCount = 20;
        car.upTasks = std::move(up);
        car.downTasks = std::move(down);
        for (int stop : car.upTasks) car.stopServices.push_back({ stop, Direction::Up, 0, 1 });
        for (int stop : car.downTasks) car.stopServices.push_back({ stop, Direction::Down, 0, 1 });
        return car;
    }

    // 用两台略快/略慢的空闲梯夹住预期成本，检查 ETA 数值而非只看大致排序。
    void CheckCost(TestSuite& tests, const ElevatorDispatchSnapshot& route,
        int floor, Direction direction, double expectedCost)
    {
        const ElevatorDispatcher dispatcher;
        auto idle = Car(99, floor == 1 ? 2 : floor - 1);
        idle.moveTimePerFloor = expectedCost - 0.01;
        tests.Check(dispatcher.SelectFromSnapshots(floor, direction, {route, idle}) == 1,
            "idle just below expected cost wins");
        idle.moveTimePerFloor = expectedCost + 0.01;
        tests.Check(dispatcher.SelectFromSnapshots(floor, direction, {route, idle}) == 0,
            "route beats idle just above expected cost");
    }

    HallCallDispatchSnapshot Call(int floor, Direction direction, std::vector<int> targets,
        PassengerId id = 0, double time = 0.0)
    {
        HallCallDispatchSnapshot request;
        request.floor=floor; request.direction=direction;
        request.firstPassengerId=id; request.firstRequestTime=time;
        request.waitingCount=static_cast<int>(targets.size()); request.targetFloors=std::move(targets);
        return request;
    }

    Elevator AlightingCar(int count)
    {
        SimulationConfig config;
        Elevator car(0, 1, config);
        car.AddHallCall(1, Direction::Up);
        for (int id = 0; id < count; ++id)
        {
            if (!car.BeginBoarding(id, 5)) throw std::runtime_error("boarding fixture failed");
            car.Advance(config.personTime);
        }
        car.FinishStop();
        while (!car.IsAtStop()) car.Advance(car.GetTimeToNextEvent());
        if (!car.BeginAlighting(0)) throw std::runtime_error("alighting fixture failed");
        car.Advance(2.0); // 当前一人还剩 1 秒，其他人各需完整 T=3 秒。
        return car;
    }

    double RealPickupTime(Elevator car, int floor, Direction direction)
    {
        car.AddHallCall(floor, direction);
        double elapsed = 0.0;
        for (int event = 0; event < 1000; ++event)
        {
            if (car.IsAtStop())
            {
                const auto alighting = car.GetNextAlightingPassenger();
                if (alighting != InvalidPassengerId) car.BeginAlighting(alighting);
                else if (car.GetSnapshot().currentFloor == floor && car.GetSnapshot().direction == direction)
                    return elapsed;
                else car.FinishStop();
            }
            else
            {
                if (car.GetSnapshot().state == ElevatorState::Idle)
                    throw std::runtime_error("reference elevator lost request");
                elapsed += car.Advance(car.GetTimeToNextEvent()).elapsedTime;
            }
        }
        throw std::runtime_error("reference route did not finish");
    }
}

int main()
{
    TestSuite tests("Dispatcher");
    const ElevatorDispatcher dispatcher;
    const auto select = [&](int floor, Direction direction, const std::vector<ElevatorDispatchSnapshot>& cars,
        double requestTime = UnsetTime, double currentTime = UnsetTime)
    { return dispatcher.SelectFromSnapshots(floor, direction, cars, requestTime, currentTime); };
    tests.Run("nearest idle", [&] { tests.Check(select(7, Direction::Up,
        { Car(0,1), Car(1,6), Car(2,20) }) == 1, "nearest idle"); });
    tests.Run("idle at landing", [&] { tests.Check(select(4, Direction::Down,
        { Car(0,8), Car(1,4) }) == 1, "same floor"); });
    tests.Run("up on-way beats closer opposite", [&] { tests.Check(select(10, Direction::Up,
        { Car(0,5,Direction::Up,{15}), Car(1,11,Direction::Down,{}, {2}) }) == 0, "opposite route costs more"); });
    tests.Run("down on-way beats closer opposite", [&] { tests.Check(select(10, Direction::Down,
        { Car(0,15,Direction::Down,{}, {2}), Car(1,9,Direction::Up,{19}) }) == 0, "opposite route costs more"); });
    tests.Run("idle at request beats distant on-way", [&] { tests.Check(select(10, Direction::Up,
        { Car(0,1,Direction::Up,{15}), Car(1,10) }) == 1, "immediate idle response"); });
    tests.Run("near idle beats distant on-way", [&] { tests.Check(select(10, Direction::Up,
        { Car(0,1,Direction::Up,{15}), Car(1,9) }) == 1, "compare ETA across idle and on-way"); });
    tests.Run("fast on-way beats distant idle", [&] { tests.Check(select(10, Direction::Up,
        { Car(0,9,Direction::Up,{15}), Car(1,1) }) == 0, "idle has no absolute priority either"); });
    tests.Run("downward idle and on-way use same cost", [&] {
        tests.Check(select(10,Direction::Down,{Car(0,20,Direction::Down,{}, {1}),Car(1,10)})==1,"idle at down call");
        tests.Check(select(10,Direction::Down,{Car(0,20,Direction::Down,{}, {1}),Car(1,11)})==1,"near idle down call");
        tests.Check(select(10,Direction::Down,{Car(0,11,Direction::Down,{}, {1}),Car(1,20)})==0,"fast down on-way");
    });
    tests.Run("reasonable idle beats opposite busy", [&] { tests.Check(select(10,Direction::Up,
        {Car(0,11,Direction::Down,{}, {2}),Car(1,9)})==1,"opposite must finish accepted route"); });
    tests.Run("opposite surcharge is significant but finite", [&] {
        // 反向梯预计 5 秒接客：移动 2 秒 + 已有反向停站 3 秒；S+T 附加后成本 10。
        const auto opposite=Car(0,11,Direction::Down,{}, {10});
        tests.Check(select(10,Direction::Up,{opposite,Car(1,6)})==1,"idle ETA 8 beats opposite cost 10");
        tests.Check(select(10,Direction::Up,{opposite,Car(1,1)})==0,"cost 10 can still beat idle ETA 18");
    });
    tests.Run("same-direction behind penalty is not absolute", [&] { tests.Check(select(10,Direction::Up,
        {Car(0,11,Direction::Up,{12}),Car(1,1)})==0,"short remaining sweep can beat distant idle"); });
    tests.Run("load cost can outweigh faster on-way ETA", [&] {
        auto loaded=Car(0,9,Direction::Up,{15},{},9); loaded.personTime=10;
        tests.Check(select(10,Direction::Up,{loaded,Car(1,8)})==1,"ETA 2 plus load 9 loses to idle 4");
    });
    tests.Run("intermediate stops compared against idle", [&] { tests.Check(select(10,Direction::Up,
        {Car(0,6,Direction::Up,{7,8,9,15}),Car(1,4)})==1,"nearer moving car has more pickup delay"); });
    tests.Run("remaining boarding compared against idle", [&] {
        auto boarding=Car(0,9,Direction::Up,{15}); boarding.elevator.state=ElevatorState::Boarding;
        boarding.personTime=10; boarding.remainingActionTime=8; boarding.reservedBoardingCount=1;
        tests.Check(select(10,Direction::Up,{boarding,Car(1,7)})==1,"remaining action belongs in pickup ETA");
    });
    tests.Run("multiple passenger events beat fixed stop estimate", [&] {
        auto route=Car(0,5,Direction::Up,{7,8,15},{},2);
        route.stopServices.clear();
        route.stopServices.push_back({7,Direction::Idle,2,0});
        route.stopServices.push_back({8,Direction::Up,0,3});
        // Route ETA = 10 seconds movement + 15 seconds for five known transfers;
        // the idle car reaches the request in 18 seconds.
        tests.Check(select(10,Direction::Up,{route,Car(1,1)})==1,"all known passenger transfers count");
        CheckCost(tests,route,10,Direction::Up,25.9);
    });
    tests.Run("known alighting count affects ETA", [&] {
        auto route=Car(0,5,Direction::Up,{7,15},{},3);
        route.stopServices.clear();
        route.stopServices.push_back({7,Direction::Idle,3,0});
        tests.Check(select(10,Direction::Up,{route,Car(1,1)})==1,"known alighting passengers add T each");
        CheckCost(tests,route,10,Direction::Up,19);
    });
    tests.Run("full car can free a seat before request", [&] {
        auto full=Car(0,1,Direction::Up,{5}, {}, 2, 2);
        full.stopServices.clear();
        full.stopServices.push_back({5,Direction::Idle,1,0});
        auto distant=Car(1,1); distant.moveTimePerFloor=10;
        tests.Check(select(10,Direction::Up,{full,distant})==0,"projected alighting frees capacity");
    });
    tests.Run("full car without an earlier alighting is rejected", [&] {
        auto full=Car(0,1,Direction::Up,{5}, {}, 2, 2);
        full.stopServices.clear();
        full.stopServices.push_back({5,Direction::Idle,0,0});
        tests.Check(select(10,Direction::Up,{full,Car(1,1)})==1,"still full at request");
    });
    tests.Run("waiting five uses only two remaining seats", [&] {
        auto route=Car(0,5,Direction::Up,{7,10}, {}, 8, 10);
        route.stopServices.clear();
        route.stopServices.push_back({7,Direction::Up,0,5});
        route.stopServices.push_back({10,Direction::Idle,1,0});
        // 移动 10 秒、只上 2 人 6 秒、请求层先下 1 人 3 秒；预计载荷 9/10。
        // 成本 21.7 秒，优于空闲梯的 22.5 秒；若错误计 5 人上梯就会落败。
        auto distant=Car(1,1); distant.moveTimePerFloor=2.5;
        tests.Check(select(10,Direction::Up,{route,distant})==0,"boarding is capped by projected capacity");
        CheckCost(tests,route,10,Direction::Up,21.7);
    });
    tests.Run("waiting count changes intermediate ETA", [&] {
        auto one=Car(0,5,Direction::Up,{7,10});
        one.stopServices.clear();
        one.stopServices.push_back({7,Direction::Up,0,1});
        auto five=one; five.stopServices[0].boardingCount=5; five.elevator.id=0;
        const auto alternate=Car(1,1);
        tests.Check(select(10,Direction::Up,{one,alternate})==0,"one waiting passenger route wins");
        tests.Check(select(10,Direction::Up,{five,alternate})==1,"five waiting passengers delay route");
    });
    tests.Run("boarding snapshot includes future alighting", [&] {
        SimulationConfig config; config.capacity=1;
        Elevator car(0,1,config);
        tests.Check(car.AddHallCall(1,Direction::Up) && car.BeginBoarding(7,8),"begin boarding");
        car.Advance(1.0);
        const auto boarding=car.GetDispatchSnapshot();
        bool found=false;
        for(const auto& stop:boarding.stopServices)
            if(stop.floor==8 && stop.direction==Direction::Idle && stop.alightingCount==1) found=true;
        auto distant=Car(1,1); distant.moveTimePerFloor=3;
        tests.Check(found && select(10,Direction::Up,{boarding,distant})==0,"pending target frees projected seat");
    });
    tests.Run("boarding reservation is not counted as another waiter", [&] {
        auto boarding=Car(0,5,Direction::Up,{7,10}, {}, 0, 2);
        boarding.elevator.state=ElevatorState::Boarding;
        boarding.remainingActionTime=1;
        boarding.reservedBoardingCount=1;
        boarding.stopServices.clear();
        boarding.stopServices.push_back({5,Direction::Up,0,0});
        boarding.stopServices.push_back({7,Direction::Idle,0,0});
        boarding.stopServices.push_back({10,Direction::Idle,0,0});
        auto alternate=Car(1,1); alternate.moveTimePerFloor=2;
        tests.Check(select(10,Direction::Up,{boarding,alternate})==0,"queue head reservation is not duplicated");
    });
    tests.Run("all projected cars full remain unassigned", [&] {
        auto first=Car(0,1,Direction::Up,{5}, {}, 2, 2);
        auto second=Car(1,6,Direction::Down,{}, {2}, 2, 2);
        first.stopServices.clear();
        second.stopServices.clear();
        first.stopServices.push_back({5,Direction::Idle,0,0});
        second.stopServices.push_back({2,Direction::Idle,0,0});
        tests.Check(select(10,Direction::Up,{first,second})==InvalidElevatorId,"no projected seat");
    });
    tests.Run("opposite hall ahead fixes LOOK turnaround", [&] {
        SimulationConfig config;
        Elevator real(0,5,config); real.AddHallCall(8,Direction::Down);
        auto route=real.GetDispatchSnapshot();
        for(auto& stop:route.stopServices) stop.boardingCount=0;
        // 真梯必须 5->8->3；空反向外呼不计传送时间，16 秒路程 + 5 秒方向成本。
        tests.Near(RealPickupTime(real,3,Direction::Up),16,"real LOOK visits opposite call ahead");
        CheckCost(tests,route,3,Direction::Up,21);
    });
    tests.Run("alighting is consumed once across both hall directions", [&] {
        auto route=Car(0,3,Direction::Up,{5},{5,8},1,2);
        route.stopServices={{5,Direction::Idle,1,0},{5,Direction::Down,0,0},
            {8,Direction::Down,0,2,{2,2}}};
        tests.Check(select(4,Direction::Down,{route})==InvalidElevatorId,
            "5F alighting cannot free seats again after boarding at 8F");
        route.stopServices.back().boardingCount=1;
        route.stopServices.back().boardingTargetFloors={2};
        // 移动 18 秒，下客/上客各 3 秒；到请求层 1/2 载荷 + 方向成本。
        CheckCost(tests,route,4,Direction::Down,30.5);
    });
    tests.Run("in-progress alighting uses remaining time only", [&] {
        const auto real=AlightingCar(1);
        tests.Near(RealPickupTime(real,6,Direction::Up),3,"1 remaining + 2 travel");
        CheckCost(tests,real.GetDispatchSnapshot(),6,Direction::Up,3);
    });
    tests.Run("remaining alighting passengers all count", [&] {
        const auto real=AlightingCar(3);
        tests.Near(RealPickupTime(real,6,Direction::Up),9,"1 remaining + 2 full transfers + travel");
        CheckCost(tests,real.GetDispatchSnapshot(),6,Direction::Up,9);
        CheckCost(tests,real.GetDispatchSnapshot(),5,Direction::Up,7);
    });
    tests.Run("boarding snapshot does not duplicate reserved queue head", [&] {
        SimulationConfig config; config.capacity=2;
        Elevator real(0,5,config); real.AddHallCall(5,Direction::Up);
        real.BeginBoarding(1,9); real.Advance(2);
        tests.Near(RealPickupTime(real,6,Direction::Up),3,"only reserved passenger remaining second");
        CheckCost(tests,real.GetDispatchSnapshot(),6,Direction::Up,4.5);
    });
    tests.Run("known boarding destinations release future capacity", [&] {
        auto route=Car(0,3,Direction::Up,{4}); route.elevator.capacity=2;
        route.stopServices={{4,Direction::Up,0,2,{6,6}}};
        // 3->8 为 10 秒，两人上/下共 12 秒，到 8F 空载。
        CheckCost(tests,route,8,Direction::Up,22);
        route.stopServices[0].boardingTargetFloors.clear();
        tests.Check(select(8,Direction::Up,{route})==InvalidElevatorId,
            "unknown destinations cannot invent released capacity");
    });
    tests.Run("FIFO destinations determine who can board", [&] {
        auto route=Car(0,3,Direction::Up,{4}); route.elevator.capacity=1;
        route.stopServices={{4,Direction::Up,0,2,{6,12}}};
        CheckCost(tests,route,8,Direction::Up,16);
        route.stopServices[0].boardingTargetFloors={12,6};
        tests.Check(select(8,Direction::Up,{route})==InvalidElevatorId,
            "cannot skip far-destination queue head for second passenger");
    });
    tests.Run("downward FIFO releases capacity at destinations", [&] {
        auto route=Car(0,15,Direction::Down,{}, {14},0,2);
        route.stopServices={{14,Direction::Down,0,2,{12,12}}};
        CheckCost(tests,route,10,Direction::Down,22);
    });
    tests.Run("new passengers can alight at a previously serviced floor", [&] {
        auto route=Car(0,3,Direction::Up,{5},{8},1,2);
        route.stopServices={{5,Direction::Idle,1,0},{8,Direction::Down,0,1,{5}}};
        // 5F 的原乘客消费后清零；8F 新上梯者产生另一批 5F 下客事件。
        CheckCost(tests,route,4,Direction::Down,32);
    });
    tests.Run("FIFO destinations change turnaround ETA", [&] {
        auto route=Car(0,5,Direction::Up,{6},{},0,2);
        route.stopServices={{6,Direction::Up,0,3,{7,8,14}}};
        // 前两人到 7/8F，后一个不登梯；5->8->3=16 秒，4 次传送=12 秒。
        CheckCost(tests,route,3,Direction::Up,33);
        route.stopServices[0].boardingTargetFloors={14,7,8};
        CheckCost(tests,route,3,Direction::Up,57);
    });
    tests.Run("load cost uses occupancy at request", [&] {
        auto route=Car(0,5,Direction::Up,{6},{},2,2);
        route.stopServices={{6,Direction::Idle,2,0}};
        // 到 8F 前两人下客：ETA=12，预计空载；按当前满载加 T 会错选空闲梯。
        CheckCost(tests,route,8,Direction::Up,12);
        route=Car(0,5,Direction::Up,{6},{},0,2);
        route.stopServices={{6,Direction::Up,0,1,{12}}};
        CheckCost(tests,route,8,Direction::Up,10.5);
    });
    tests.Run("known FIFO services remain read-only and deterministic", [&] {
        const auto original=Car(0,3,Direction::Up,{4},{},0,2);
        auto route=original; route.stopServices={{4,Direction::Up,0,2,{6,6}}};
        for(int repeat=0;repeat<10;++repeat) CheckCost(tests,route,8,Direction::Up,22);
        tests.Check(route.stopServices[0].boardingCount==2 && route.stopServices[0].alightingCount==0 &&
            route.stopServices[0].boardingTargetFloors==std::vector<int>({6,6}) &&
            route.upTasks==original.upTasks && route.downTasks==original.downTasks &&
            route.elevator.passengerCount==0,"preview does not consume source snapshot");
    });
    tests.Run("invalid FIFO destination metadata rejected", [&] {
        auto route=Car(0,3,Direction::Up,{4});
        route.stopServices={{4,Direction::Up,0,1,{2}}};
        tests.Check(select(8,Direction::Up,{route})==InvalidElevatorId,"wrong direction");
        route.stopServices[0].boardingTargetFloors={21};
        tests.Check(select(8,Direction::Up,{route})==InvalidElevatorId,"above building");
        route.stopServices[0].boardingTargetFloors={6,7};
        tests.Check(select(8,Direction::Up,{route})==InvalidElevatorId,"more targets than waiters");
    });
    tests.Run("ETA agrees with real LOOK across mixed task routes", [&] {
        // 108 组混合内呼/双向外呼、层间剩余时间、前方/后方请求，直接对照真实状态机。
        for(int start:{2,5,10}) for(int target:{3,8,12})
            for(Direction initial:{Direction::Up,Direction::Down})
                for(int request:{2,5,11}) for(Direction direction:{Direction::Up,Direction::Down})
                {
                    SimulationConfig config; Elevator real(0,start,config);
                    real.AddHallCall(target,initial); real.AddInternalTarget(7);
                    real.AddHallCall(9,Direction::Down); real.AddHallCall(4,Direction::Up);
                    real.Advance(0.5);
                    auto route=real.GetDispatchSnapshot();
                    for(auto& stop:route.stopServices) stop.boardingCount=0;
                    const bool ahead=route.elevator.direction==Direction::Up ? request>start : request<start;
                    const bool onWay=route.elevator.direction==direction &&
                        (ahead || (!route.betweenFloors && request==start));
                    const double actual=RealPickupTime(real,request,direction);
                    CheckCost(tests,route,request,direction,actual+(onWay ? 0.0 : 5.0));
                }
    });
    tests.Run("aging bonus grows and is capped", [&] {
        tests.Near(dispatcher.GetAgingBonus(10,10),0,"no waiting bonus");
        tests.Near(dispatcher.GetAgingBonus(10,20),0.5,"continuous aging rate");
        tests.Near(dispatcher.GetAgingBonus(10,1000),ElevatorDispatcher::MaxAgingBonus,"aging cap");
        tests.Near(dispatcher.GetAgingBonus(20,10),0,"invalid time order");
    });
    tests.Run("aging admits a reasonable reverse route", [&] {
        const auto reverse=Car(0,11,Direction::Down,{}, {10});
        const auto idle=Car(1,6);
        tests.Check(select(10,Direction::Up,{reverse,idle},0,0)==1,"fresh request prefers idle");
        tests.Check(select(10,Direction::Up,{reverse,idle},0,200)==0,"old request receives bounded relief");
    });
    tests.Run("aging cannot force an obviously poor route", [&] {
        const auto reverse=Car(0,20,Direction::Down,{}, {1});
        const auto idle=Car(1,9);
        tests.Check(select(10,Direction::Up,{reverse,idle},0,10000)==1,"cap does not erase huge ETA");
    });
    tests.Run("equal cost prefers lower ETA before distance and ID", [&] {
        auto faster=Car(1,9,Direction::Up,{15},{},5); faster.personTime=4;
        tests.Check(select(10,Direction::Up,{Car(0,8,Direction::Up,{15}),faster})==1,"both cost 4, ETA 2 beats 4");
    });
    tests.Run("equal cost and ETA prefer distance before ID", [&] {
        auto farther=Car(0,8,Direction::Up,{15}); farther.moveTimePerFloor=1;
        tests.Check(select(10,Direction::Up,{farther,Car(1,9,Direction::Up,{15})})==1,"both ETA 2, distance 1 beats 2");
    });
    tests.Run("equal cost ETA and distance prefer fewer tasks", [&] { tests.Check(select(10,Direction::Up,
        {Car(0,9,Direction::Up,{15,16}),Car(1,9,Direction::Up,{15})})==1,"post-pickup task count before ID"); });
    tests.Run("real snapshot selection leaves elevator state untouched", [&] {
        SimulationConfig config;
        std::vector<Elevator> cars{Elevator(0,1,config),Elevator(1,10,config)};
        cars[0].AddInternalTarget(15); cars[0].Advance(0.5);
        const auto movingBefore=cars[0].GetDispatchSnapshot();
        const auto idleBefore=cars[1].GetDispatchSnapshot();
        for(int repeat=0;repeat<10;++repeat)
            tests.Check(dispatcher.SelectElevator(10,Direction::Up,cars)==1,"repeatable real idle choice");
        const auto movingAfter=cars[0].GetDispatchSnapshot();
        const auto idleAfter=cars[1].GetDispatchSnapshot();
        tests.Check(movingBefore.elevator.currentFloor==movingAfter.elevator.currentFloor &&
            movingBefore.elevator.direction==movingAfter.elevator.direction &&
            movingBefore.elevator.state==movingAfter.elevator.state &&
            movingBefore.remainingActionTime==movingAfter.remainingActionTime &&
            movingBefore.upTasks==movingAfter.upTasks && movingBefore.downTasks==movingAfter.downTasks,
            "dispatch cannot change route, state or remaining timer");
        tests.Check(idleBefore.elevator.currentFloor==idleAfter.elevator.currentFloor &&
            idleAfter.elevator.state==ElevatorState::Idle && idleAfter.elevator.direction==Direction::Idle &&
            idleAfter.remainingActionTime==0 && idleAfter.upTasks.empty() && idleAfter.downTasks.empty() &&
            cars[0].GetPassengerIds().empty() && cars[1].GetPassengerIds().empty(),"dispatch does not assign or move cars");
    });
    tests.Run("full on-way rejected", [&] { tests.Check(select(10, Direction::Up,
        { Car(0,5,Direction::Up,{15},{},10), Car(1,1) }) == 1, "full bypass"); });
    tests.Run("boarding reserves last seat", [&] { auto car=Car(0,5,Direction::Up,{15},{},9);
        car.reservedBoardingCount=1; tests.Check(select(10,Direction::Up,{car,Car(1,1)})==1,"reserved capacity"); });
    tests.Run("all full", [&] { tests.Check(select(10,Direction::Up,
        {Car(0,1,Direction::Up,{20},{},10),Car(1,20,Direction::Down,{}, {1},10)})==-1,"no capacity"); });
    tests.Run("no elevators", [&] { tests.Check(select(5,Direction::Up,{})==-1,"empty group"); });
    tests.Run("idle request invalid", [&] { tests.Check(select(5,Direction::Idle,{Car(0,1)})==-1,"direction invalid"); });
    tests.Run("invalid floor", [&] { tests.Check(select(0,Direction::Up,{Car(0,1)})==-1,"lower bound"); });
    tests.Run("above building", [&] { tests.Check(select(21,Direction::Down,{Car(0,1)})==-1,"upper bound"); });
    tests.Run("top down", [&] { tests.Check(select(20,Direction::Down,{Car(0,1),Car(1,19)})==1,"top call"); });
    tests.Run("bottom up", [&] { tests.Check(select(1,Direction::Up,{Car(0,2),Car(1,19)})==0,"bottom call"); });
    tests.Run("impossible boundary directions", [&] {
        tests.Check(select(20,Direction::Up,{Car(0,1)})==-1,"top up");
        tests.Check(select(1,Direction::Down,{Car(0,1)})==-1,"bottom down"); });
    tests.Run("up request behind uses idle", [&] { tests.Check(select(3,Direction::Up,
        {Car(0,5,Direction::Up,{15}),Car(1,20)})==1,"behind is not on-way"); });
    tests.Run("down request behind uses idle", [&] { tests.Check(select(17,Direction::Down,
        {Car(0,15,Direction::Down,{}, {2}),Car(1,1)})==1,"behind is not on-way"); });
    tests.Run("all busy deferred route", [&] { tests.Check(select(3,Direction::Up,
        {Car(0,5,Direction::Up,{19}),Car(1,8,Direction::Up,{10})})==1,"shorter remaining sweep"); });
    tests.Run("stable id not vector order", [&] { tests.Check(select(5,Direction::Up,
        {Car(9,1),Car(2,1)})==1,"lower ID wins, return index"); });
    tests.Run("load breaks equal route", [&] { tests.Check(select(10,Direction::Up,
        {Car(0,5,Direction::Up,{15},{},8),Car(1,5,Direction::Up,{15},{},1)})==1,"load cost"); });
    tests.Run("intermediate stops cost time", [&] { tests.Check(select(10,Direction::Up,
        {Car(0,6,Direction::Up,{7,8,9,15}),Car(1,5,Direction::Up,{15})})==1,"ETA beats distance"); });
    tests.Run("remaining movement time", [&] { auto a=Car(0,5,Direction::Up,{15}); auto b=a;
        a.betweenFloors=b.betweenFloors=true; a.remainingActionTime=1.8; b.remainingActionTime=0.2;
        b.elevator.id=1; tests.Check(select(10,Direction::Up,{a,b})==1,"fractional movement"); });
    tests.Run("remaining service time", [&] { auto a=Car(0,5,Direction::Up,{15}); auto b=a;
        a.remainingActionTime=2.5; b.remainingActionTime=0.5; b.elevator.id=1;
        tests.Check(select(10,Direction::Up,{a,b})==1,"service delay"); });
    tests.Run("departed landing is behind", [&] { auto a=Car(0,5,Direction::Up,{15});
        a.betweenFloors=true; a.remainingActionTime=1.0;
        tests.Check(select(5,Direction::Up,{a,Car(1,1)})==1,"cannot stop after departure"); });
    tests.Run("no side effects and repeatable", [&] { std::vector<ElevatorDispatchSnapshot> cars{
        Car(0,5,Direction::Up,{8,12,15}),Car(1,20,Direction::Down,{}, {1})};
        for(int repeat=0;repeat<10;++repeat) tests.Check(select(10,Direction::Up,cars)==0,"deterministic");
        tests.Check(cars[0].upTasks==std::vector<int>({8,12,15}) && cars[0].elevator.currentFloor==5,"immutable"); });
    tests.Run("nonfinite estimate rejected", [&] { auto a=Car(0,5); a.moveTimePerFloor=std::numeric_limits<double>::infinity();
        tests.Check(select(10,Direction::Up,{a})==-1,"finite cost"); });
    tests.Run("invalid route snapshot rejected", [&] {
        auto a=Car(0,20,Direction::Up,{21}); a.betweenFloors=true; a.remainingActionTime=1;
        tests.Check(select(10,Direction::Up,{a})==-1,"cannot travel above roof");
        a=Car(0,5,Direction::Up,{0}); tests.Check(select(10,Direction::Up,{a})==-1,"bad task floor");
        a=Car(0,(std::numeric_limits<int>::max)(),Direction::Up); a.floorCount=0; a.betweenFloors=true;
        tests.Check(select(10,Direction::Up,{a})==-1,"no integer overflow at unknown upper bound");
    });
    tests.Run("reassign only for a material ETA improvement", [&] {
        const auto request=Call(10,Direction::Up,{15});
        const auto owner=Car(0,1,Direction::Up,{10});
        tests.Check(dispatcher.SelectReassignment(request,0,{owner,Car(1,6)},20)==1,"18s to 8s qualifies");
        tests.Check(dispatcher.SelectReassignment(request,0,{owner,Car(1,3)},20)==0,"4s gain below 5s threshold");
        auto boundary=Car(1,6); boundary.moveTimePerFloor=3.25;
        tests.Check(dispatcher.SelectReassignment(request,0,{owner,boundary},20)==1,"exactly 5s gain qualifies");
    });
    tests.Run("near and serving owners are protected", [&] {
        const auto request=Call(10,Direction::Up,{15});
        auto owner=Car(0,9,Direction::Up,{10}); owner.remainingActionTime=50;
        tests.Check(dispatcher.SelectReassignment(request,0,{owner,Car(1,10)},20)==0,"one floor proximity lock");
        owner.elevator.currentFloor=10; owner.elevator.state=ElevatorState::Boarding;
        owner.reservedBoardingCount=1;
        tests.Check(dispatcher.SelectReassignment(request,0,{owner,Car(1,10)},20)==0,"boarding at request locked");
        owner.elevator.state=ElevatorState::Alighting;
        tests.Check(dispatcher.SelectReassignment(request,0,{owner,Car(1,10)},20)==0,"alighting at request locked");
    });
    tests.Run("reassignment cooldown prevents oscillation", [&] {
        const auto request=Call(10,Direction::Up,{15});
        const std::vector<ElevatorDispatchSnapshot> cars{Car(0,1),Car(1,6)};
        int owner=dispatcher.SelectReassignment(request,0,cars,20);
        tests.Check(owner==1,"initial reassignment");
        for(int repeat=0;repeat<20;++repeat)
            tests.Check(dispatcher.SelectReassignment(request,owner,cars,20,20)==owner,"stable same timestamp");
        const std::vector<ElevatorDispatchSnapshot> changed{Car(0,10),Car(1,6)};
        tests.Check(dispatcher.SelectReassignment(request,1,changed,29.9,20)==1,"10 second cooldown");
        tests.Check(dispatcher.SelectReassignment(request,1,changed,30,20)==0,"cooldown expires at physical event");
    });
    tests.Run("joint assignment beats a fixed greedy counterexample", [&] {
        const std::vector<ElevatorDispatchSnapshot> cars{Car(0,5),Car(1,1)};
        const std::vector<HallCallDispatchSnapshot> requests{
            Call(4,Direction::Up,{20},0),Call(6,Direction::Up,{20},1)};
        const auto first=dispatcher.SelectFromSnapshots(4,Direction::Up,cars);
        SimulationConfig config; config.capacity=10;
        Elevator greedyFirst(0,5,config); greedyFirst.AddHallCall(4,Direction::Up);
        auto accepted=greedyFirst.GetDispatchSnapshot();
        for(auto& stop:accepted.stopServices) if(stop.direction==Direction::Up) stop.boardingTargetFloors={20};
        const auto second=dispatcher.SelectFromSnapshots(6,Direction::Up,{accepted,cars[1]});
        tests.Check(first==0 && second==1,"greedy E1 then E2");
        const double greedy=dispatcher.ScoreSnapshot(4,Direction::Up,cars[0]).cost+
            dispatcher.ScoreSnapshot(6,Direction::Up,cars[1]).cost;
        const auto plan=dispatcher.PlanAssignments(requests,cars,0);
        tests.Check(plan.elevatorIndices==std::vector<int>({1,0}),"joint E2 then E1");
        tests.Near(greedy,12,"greedy total cost"); tests.Near(plan.totalCost,8,"joint total cost");
        std::cout << "Two-call cost: greedy=" << greedy << ", joint=" << plan.totalCost << '\n';
    });
    tests.Run("three request search is bounded and deterministic", [&] {
        const std::vector<ElevatorDispatchSnapshot> cars{Car(0,2),Car(1,6),Car(2,10)};
        const std::vector<HallCallDispatchSnapshot> calls{
            Call(2,Direction::Up,{3},0),Call(6,Direction::Up,{7},1),Call(10,Direction::Down,{9},2)};
        const auto plan=dispatcher.PlanAssignments(calls,cars,0);
        tests.Check(plan.elevatorIndices==std::vector<int>({0,1,2}) && plan.assignedCount==3,"three immediate pickups");
        tests.Near(plan.totalCost,0,"no travel or earlier service");
        tests.Check(plan.evaluatedCombinations<=ElevatorDispatcher::MaxJointCombinations &&
            plan.scoreEvaluations<=21*cars.size()+192,"bounded branching and scoring");
        for(int repeat=0;repeat<5;++repeat)
            tests.Check(dispatcher.PlanAssignments(calls,cars,0).elevatorIndices==plan.elevatorIndices,"stable tie breaks");
        tests.Check(cars[0].upTasks.empty() && cars[0].elevator.state==ElevatorState::Idle &&
            calls[0].targetFloors==std::vector<int>({3}),"search only changes local copies");
    });
    tests.Run("joint search selects oldest three even if input unsorted", [&] {
        const auto plan=dispatcher.PlanAssignments({Call(2,Direction::Up,{3},0,30),Call(4,Direction::Up,{5},1,0),
            Call(6,Direction::Up,{7},2,10),Call(8,Direction::Up,{9},3,20)},
            {Car(0,2),Car(1,4),Car(2,6),Car(3,8)},40);
        tests.Check(plan.assignedCount==3 && plan.elevatorIndices[0]==InvalidElevatorId,"fourth newest waits");
        tests.Check(plan.evaluatedCombinations<=64,"large input does not grow batch");
    });
    tests.Run("joint requests may share a car with future capacity", [&] {
        const auto plan=dispatcher.PlanAssignments({Call(2,Direction::Up,{3},0),Call(4,Direction::Up,{5},1),
            Call(6,Direction::Up,{7},2)}, {Car(0,1,Direction::Idle,{}, {},0,1)},0);
        tests.Check(plan.elevatorIndices==std::vector<int>({0,0,0}),"all three fit along one route");
        tests.Near(plan.totalEta,36,"ETA 2 + 12 + 22 includes prior boarding and drops");
        tests.Near(plan.maxEta,22,"latest pickup");
    });
    tests.Run("final joint route reevaluates earlier request costs", [&] {
        const auto plan=dispatcher.PlanAssignments({Call(6,Direction::Up,{7},0),Call(2,Direction::Up,{3},1)},
            {Car(0,1,Direction::Idle,{}, {},0,1)},0);
        tests.Check(plan.assignedCount==2,"same car route feasible");
        tests.Near(plan.totalCost,18,"older 6F ETA becomes 16 after adding 2F service; not 10+2");
    });
    tests.Run("insufficient capacity leaves partial plan", [&] {
        const auto plan=dispatcher.PlanAssignments({Call(4,Direction::Up,{8},0),Call(5,Direction::Up,{9},1)},
            {Car(0,1,Direction::Idle,{}, {},0,1),Car(1,2,Direction::Up,{20},{},1,1)},0);
        tests.Check(plan.assignedCount==1 && plan.elevatorIndices==std::vector<int>({0,-1}),"cannot fill same seat twice");
        const auto empty=dispatcher.PlanAssignments({Call(4,Direction::Up,{8})},{Car(0,1,Direction::Up,{20},{},1,1)},0);
        tests.Check(empty.assignedCount==0 && empty.elevatorIndices[0]==-1,"all infeasible stays pending");
    });
    tests.Run("aging remains active in joint cost", [&] {
        const std::vector<ElevatorDispatchSnapshot> cars{Car(0,11,Direction::Down,{}, {10}),Car(1,6)};
        const std::vector<HallCallDispatchSnapshot> calls{Call(10,Direction::Up,{15})};
        tests.Check(dispatcher.PlanAssignments(calls,cars,0).elevatorIndices[0]==1,"fresh prefers idle");
        tests.Check(dispatcher.PlanAssignments(calls,cars,200).elevatorIndices[0]==0,"aging admits reverse route");
    });
    tests.Run("large fleet still respects joint search bound", [&] {
        std::vector<ElevatorDispatchSnapshot> cars;
        for(int id=0;id<60;++id) cars.push_back(Car(id,id%18+1));
        const auto plan=dispatcher.PlanAssignments({Call(4,Direction::Up,{8},0),Call(9,Direction::Down,{2},1),
            Call(15,Direction::Up,{19},2)},cars,0);
        tests.Check(plan.assignedCount==3 && plan.evaluatedCombinations<=64,"60 cars do not cause 60 cubed search");
        tests.Check(plan.scoreEvaluations<=21*cars.size()+192,"candidate scan is linear in fleet size");
    });
    tests.Run("joint ties use IDs rather than input order", [&] {
        const std::vector<HallCallDispatchSnapshot> calls{Call(5,Direction::Up,{20})};
        tests.Check(dispatcher.PlanAssignments(calls,{Car(9,1),Car(2,1)},0).elevatorIndices[0]==1,"lower ID selected");
        tests.Check(dispatcher.PlanAssignments(calls,{Car(2,1),Car(9,1)},0).elevatorIndices[0]==0,"stable after permutation");
    });
    return tests.Finish();
}

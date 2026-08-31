#include "Core/Dispatcher.h"
#include "TestSupport.h"
#include <limits>

namespace
{
    ElevatorDispatchSnapshot Car(int id, int floor, Direction direction = Direction::Idle,
        std::vector<int> up = {}, std::vector<int> down = {}, int passengers = 0)
    {
        ElevatorDispatchSnapshot car;
        car.elevator = { id, floor, direction,
            direction == Direction::Idle ? ElevatorState::Idle : ElevatorState::Stopped, passengers, 10 };
        car.floorCount = 20;
        car.upTasks = std::move(up);
        car.downTasks = std::move(down);
        return car;
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
        auto route=Car(0,5,Direction::Up,{7,8,15});
        route.stopServices.push_back({7,Direction::Idle,2,0});
        route.stopServices.push_back({8,Direction::Up,0,3});
        // Route ETA = 10 seconds movement + 15 seconds for five known transfers;
        // the idle car reaches the request in 18 seconds.
        tests.Check(select(10,Direction::Up,{route,Car(1,1)})==1,"all known passenger transfers count");
    });
    tests.Run("known alighting count affects ETA", [&] {
        auto route=Car(0,5,Direction::Up,{7,15});
        route.stopServices.push_back({7,Direction::Idle,3,0});
        tests.Check(select(10,Direction::Up,{route,Car(1,1)})==1,"known alighting passengers add T each");
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
    return tests.Finish();
}

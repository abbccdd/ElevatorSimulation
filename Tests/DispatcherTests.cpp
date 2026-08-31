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
    const auto select = [&](int floor, Direction direction, const std::vector<ElevatorDispatchSnapshot>& cars)
    { return dispatcher.SelectFromSnapshots(floor, direction, cars); };
    tests.Run("nearest idle", [&] { tests.Check(select(7, Direction::Up,
        { Car(0,1), Car(1,6), Car(2,20) }) == 1, "nearest idle"); });
    tests.Run("idle at landing", [&] { tests.Check(select(4, Direction::Down,
        { Car(0,8), Car(1,4) }) == 1, "same floor"); });
    tests.Run("up on-way beats closer opposite", [&] { tests.Check(select(10, Direction::Up,
        { Car(0,5,Direction::Up,{15}), Car(1,11,Direction::Down,{}, {2}) }) == 0, "direction priority"); });
    tests.Run("down on-way beats closer opposite", [&] { tests.Check(select(10, Direction::Down,
        { Car(0,15,Direction::Down,{}, {2}), Car(1,9,Direction::Up,{19}) }) == 0, "direction priority"); });
    tests.Run("on-way before idle", [&] { tests.Check(select(10, Direction::Up,
        { Car(0,5,Direction::Up,{15}), Car(1,10) }) == 0, "documented class priority"); });
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

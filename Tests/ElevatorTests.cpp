#include "Core/Dispatcher.h"
#include "TestSupport.h"
#include <limits>

namespace
{
    SimulationConfig Config(int capacity = 3)
    {
        SimulationConfig config;
        config.floorCount = 20;
        config.capacity = capacity;
        config.moveTimePerFloor = 2.0;
        config.personTime = 3.0;
        return config;
    }
    std::vector<int> EmptyStops(Elevator& elevator, double& elapsed)
    {
        std::vector<int> stops;
        for (int event = 0; event < 1000; ++event)
        {
            if (elevator.GetSnapshot().state == ElevatorState::Idle) return stops;
            if (elevator.IsAtStop())
            {
                stops.push_back(elevator.GetSnapshot().currentFloor);
                if (!elevator.FinishStop()) throw std::runtime_error("cannot finish empty stop");
            }
            else elapsed += elevator.Advance(elevator.GetTimeToNextEvent()).elapsedTime;
        }
        throw std::runtime_error("empty route did not finish");
    }
}

int main()
{
    TestSuite tests("Elevator");
    tests.Run("fractional movement", [&] {
        Elevator car(0,1,Config()); tests.Check(car.AddHallCall(3,Direction::Up),"accept");
        auto event=car.Advance(1.5); tests.Check(event.type==ElevatorEventType::None,"no early arrival");
        tests.Check(car.GetSnapshot().currentFloor==1,"last reached floor");
        tests.Near(car.GetTimeToNextEvent(),0.5,"remaining time");
        event=car.Advance(0.5); tests.Check(event.type==ElevatorEventType::FloorReached,"arrive at S");
        tests.Check(car.GetSnapshot().currentFloor==2,"next floor");
    });
    tests.Run("large budget stops at first event", [&] {
        Elevator car(0,1,Config()); car.AddHallCall(5,Direction::Up);
        const auto event=car.Advance(100); tests.Near(event.elapsedTime,2,"caller keeps unused budget");
        tests.Check(car.GetSnapshot().currentFloor==2,"no teleport");
    });
    tests.Run("up direction lock and sorted tasks", [&] {
        Elevator car(0,5,Config()); car.AddInternalTarget(8); car.AddInternalTarget(12); car.AddInternalTarget(15);
        car.Advance(0.5); car.AddHallCall(3,Direction::Up); car.AddHallCall(10,Direction::Up);
        tests.Check(car.GetSnapshot().direction==Direction::Up,"new lower call cannot reverse");
        double elapsed=0.5; const auto stops=EmptyStops(car,elapsed);
        tests.Check(stops==std::vector<int>({8,10,12,15,3}),"LOOK order"); tests.Near(elapsed,44,"route travel");
    });
    tests.Run("down direction lock", [&] {
        Elevator car(0,15,Config()); car.AddInternalTarget(12); car.AddInternalTarget(5); car.AddInternalTarget(2);
        car.AddHallCall(17,Direction::Down); car.AddHallCall(8,Direction::Down);
        tests.Check(car.GetSnapshot().direction==Direction::Down,"new upper call cannot reverse");
        double elapsed=0; tests.Check(EmptyStops(car,elapsed)==std::vector<int>({12,8,5,2,17}),"descending order");
    });
    tests.Run("opposite call ahead waits for return", [&] {
        Elevator car(0,5,Config()); car.AddHallCall(10,Direction::Up); car.AddHallCall(8,Direction::Down);
        double elapsed=0; tests.Check(EmptyStops(car,elapsed)==std::vector<int>({10,8}),"opposite bypass");
    });
    tests.Run("empty positioning reverses at pickup", [&] {
        Elevator car(0,1,Config()); car.AddHallCall(3,Direction::Down); car.Advance(2); car.Advance(2);
        tests.Check(car.IsAtStop() && car.GetSnapshot().direction==Direction::Down,"pickup direction");
    });
    tests.Run("same landing immediate stop", [&] {
        Elevator car(0,5,Config()); car.AddHallCall(5,Direction::Up);
        tests.Check(car.IsAtStop(),"at landing"); tests.Check(car.GetSnapshot().direction==Direction::Up,"first call");
    });
    tests.Run("boarding consumes T", [&] {
        Elevator car(0,1,Config()); car.AddHallCall(1,Direction::Up); tests.Check(car.BeginBoarding(7,3),"board start");
        tests.Check(car.GetSnapshot().state==ElevatorState::Boarding && car.GetPassengerIds().empty(),"pending transfer");
        tests.Check(car.Advance(2).type==ElevatorEventType::None,"not yet boarded");
        const auto event=car.Advance(1); tests.Check(event.type==ElevatorEventType::Boarded && event.passengerId==7,"board completion");
        tests.Check(car.GetPassengerIds()==std::vector<PassengerId>({7}),"ID stored");
        tests.Check(car.GetUpTasks().count(3)==1,"internal target registered");
    });
    tests.Run("alighting consumes T and precedes boarding", [&] {
        Elevator car(0,1,Config()); car.AddHallCall(1,Direction::Up); car.BeginBoarding(7,2); car.Advance(3);
        car.FinishStop(); car.Advance(2); tests.Check(car.GetNextAlightingPassenger()==7,"due passenger");
        tests.Check(!car.BeginBoarding(8,3) && !car.FinishStop(),"must alight first");
        tests.Check(car.BeginAlighting(7),"alight start"); car.Advance(2.5);
        tests.Check(car.GetSnapshot().passengerCount==1,"still riding during transfer");
        tests.Check(car.Advance(0.5).type==ElevatorEventType::Alighted,"alight completion");
        tests.Check(car.GetPassengerIds().empty(),"only remove at completion");
    });
    tests.Run("capacity includes reservation", [&] {
        Elevator car(0,1,Config(1)); car.AddHallCall(1,Direction::Up); car.BeginBoarding(1,3);
        tests.Check(!car.CanBoard() && !car.BeginBoarding(2,4),"reserved last seat"); car.Advance(3);
        tests.Check(!car.CanBoard() && !car.BeginBoarding(2,4),"full car");
    });
    tests.Run("duplicate passenger rejected", [&] {
        Elevator car(0,1,Config()); car.AddHallCall(1,Direction::Up); car.BeginBoarding(1,3); car.Advance(3);
        tests.Check(!car.BeginBoarding(1,4),"ID duplicate");
    });
    tests.Run("wrong direction boarding rejected", [&] {
        Elevator car(0,5,Config()); car.AddHallCall(5,Direction::Up);
        tests.Check(!car.BeginBoarding(1,3) && !car.BeginBoarding(1,5),"wrong destination");
    });
    tests.Run("invalid hall calls", [&] {
        Elevator car(0,5,Config()); tests.Check(!car.AddHallCall(0,Direction::Up),"floor 0");
        tests.Check(!car.AddHallCall(21,Direction::Down),"above L"); tests.Check(!car.AddHallCall(1,Direction::Down),"bottom down");
        tests.Check(!car.AddHallCall(20,Direction::Up),"top up"); tests.Check(!car.AddHallCall(5,Direction::Idle),"idle call");
    });
    tests.Run("invalid transfer and internal target", [&] {
        Elevator car(0,5,Config()); tests.Check(!car.BeginBoarding(1,6),"idle is not open stop");
        tests.Check(!car.AddInternalTarget(5) && !car.AddInternalTarget(21),"target bounds");
        car.AddHallCall(5,Direction::Up); tests.Check(!car.BeginAlighting(99),"absent ID");
        tests.Check(!car.BeginBoarding(-1,6),"invalid ID");
    });
    tests.Run("duplicate hall idempotent", [&] {
        Elevator car(0,1,Config()); car.AddHallCall(3,Direction::Up); car.AddHallCall(3,Direction::Up);
        tests.Check(car.GetUpTasks().size()==1,"deduplicated call"); double elapsed=0;
        tests.Check(EmptyStops(car,elapsed)==std::vector<int>({3}),"one stop");
    });
    tests.Run("invalid time ignored", [&] {
        Elevator car(0,1,Config()); car.AddHallCall(3,Direction::Up);
        for(double value:{0.0,-1.0,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()})
            tests.Check(car.Advance(value).elapsedTime==0,"invalid elapsed");
        tests.Near(car.GetTimeToNextEvent(),2,"no timer changes");
    });
    tests.Run("last task returns idle", [&] {
        Elevator car(0,1,Config()); car.AddInternalTarget(2); car.Advance(2); car.FinishStop();
        tests.Check(car.GetSnapshot().state==ElevatorState::Idle && car.GetSnapshot().direction==Direction::Idle,"idle");
        tests.Check(std::isinf(car.GetTimeToNextEvent()),"no pending event");
    });
    tests.Run("two directions at same landing", [&] {
        Elevator car(0,5,Config()); car.AddHallCall(5,Direction::Up); car.AddHallCall(5,Direction::Down);
        tests.Check(car.GetSnapshot().direction==Direction::Up,"first direction"); car.FinishStop();
        tests.Check(car.IsAtStop() && car.GetSnapshot().direction==Direction::Down,"second direction after completion");
        car.FinishStop(); tests.Check(car.GetSnapshot().state==ElevatorState::Idle,"finished both");
    });
    tests.Run("dispatch and moving elevator integration", [&] {
        std::vector<Elevator> cars{Elevator(0,5,Config()),Elevator(1,11,Config())};
        cars[0].AddInternalTarget(15); cars[1].AddInternalTarget(2);
        const ElevatorDispatcher dispatcher; tests.Check(dispatcher.SelectElevator(10,Direction::Up,cars)==0,"real snapshot route");
        cars[0].AddHallCall(3,Direction::Up); cars[0].Advance(1);
        tests.Check(cars[0].GetSnapshot().direction==Direction::Up,"assignment cannot reverse");
    });
    tests.Run("full real elevator excluded", [&] {
        std::vector<Elevator> cars{Elevator(0,1,Config(1)),Elevator(1,20,Config(1))};
        cars[0].AddHallCall(1,Direction::Up); cars[0].BeginBoarding(1,10); cars[0].Advance(3); cars[0].FinishStop();
        const ElevatorDispatcher dispatcher; tests.Check(dispatcher.SelectElevator(3,Direction::Up,cars)==1,"real load");
    });
    tests.Run("elapsed partition does not change result", [&] {
        Elevator a(0,1,Config()),b=a; a.AddInternalTarget(10); b.AddInternalTarget(10);
        for(int floor=0;floor<4;++floor) a.Advance(2);
        for(int frame=0;frame<32;++frame) b.Advance(0.25);
        tests.Check(a.GetSnapshot().currentFloor==b.GetSnapshot().currentFloor,"frame independence");
        tests.Near(a.GetTimeToNextEvent(),b.GetTimeToNextEvent(),"same progress");
    });
    tests.Run("actual response compared with pure nearest distance", [&] {
        std::vector<Elevator> cars{Elevator(0,5,Config()),Elevator(1,9,Config())};
        cars[0].AddInternalTarget(15); cars[1].AddInternalTarget(2);
        const ElevatorDispatcher dispatcher; const int selected=dispatcher.SelectElevator(10,Direction::Up,cars);
        tests.Check(selected==0,"direction-aware selection");
        const auto pickupTime=[](Elevator car)
        {
            car.AddHallCall(10,Direction::Up);
            double elapsed=0;
            for(int event=0;event<100;++event)
            {
                if(car.IsAtStop())
                {
                    if(car.GetSnapshot().currentFloor==10 && car.GetSnapshot().direction==Direction::Up) return elapsed;
                    car.FinishStop();
                }
                else elapsed+=car.Advance(car.GetTimeToNextEvent()).elapsedTime;
            }
            throw std::runtime_error("pickup never reached");
        };
        const double chosen=pickupTime(cars[static_cast<std::size_t>(selected)]);
        const double nearest=pickupTime(cars[1]); // 距离 1 层，纯最近距离法会选它。
        tests.Near(chosen,10,"chosen actual response seconds"); tests.Near(nearest,30,"nearest actual response seconds");
        std::cout << "Controlled pickup comparison: directional=" << chosen << "s, pure nearest=" << nearest << "s\n";
    });
    return tests.Finish();
}

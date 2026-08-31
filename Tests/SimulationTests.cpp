#include "Core/Simulation.h"
#include "TestSupport.h"
#include <algorithm>
#include <limits>

namespace
{
    SimulationConfig Config()
    {
        SimulationConfig config;
        config.floorCount = 6;
        config.elevatorCount = 3;
        config.capacity = 2;
        config.passengerRate = 0.0;
        config.simulationDuration = 100.0;
        return config;
    }

    void SameState(TestSuite& tests, const Simulation& a, const Simulation& b)
    {
        tests.Check(a.ValidateState() && b.ValidateState(), "both states valid");
        tests.Near(a.GetCurrentTime(), b.GetCurrentTime(), "same clock");
        const auto sa=a.GetStatisticsSnapshot(), sb=b.GetStatisticsSnapshot();
        tests.Check(sa.totalPassengerCount==sb.totalPassengerCount && sa.waitingCount==sb.waitingCount &&
            sa.ridingCount==sb.ridingCount && sa.arrivedCount==sb.arrivedCount,"same population");
        tests.Near(sa.averageWaitingTime,sb.averageWaitingTime,"same mean wait",1e-7);
        tests.Near(sa.averageRideTime,sb.averageRideTime,"same mean ride",1e-7);
        const auto ea=a.GetElevatorSnapshots(), eb=b.GetElevatorSnapshots();
        tests.Check(ea.size()==eb.size(),"same car count");
        for(std::size_t i=0;i<ea.size();++i)
            tests.Check(ea[i].currentFloor==eb[i].currentFloor && ea[i].direction==eb[i].direction &&
                ea[i].state==eb[i].state && ea[i].passengerCount==eb[i].passengerCount,"same car state");
        const auto pa=a.GetPassengerSnapshots(), pb=b.GetPassengerSnapshots();
        tests.Check(pa.size()==pb.size(),"same active count");
        for(std::size_t i=0;i<pa.size();++i)
        {
            tests.Check(pa[i].id==pb[i].id && pa[i].startFloor==pb[i].startFloor &&
                pa[i].targetFloor==pb[i].targetFloor && pa[i].state==pb[i].state &&
                pa[i].elevatorId==pb[i].elevatorId,"same passenger state");
            tests.Near(pa[i].requestTime,pb[i].requestTime,"same request timestamp",1e-7);
            tests.Near(pa[i].boardTime,pb[i].boardTime,"same boarding timestamp",1e-7);
        }
        const auto ha=a.GetHallCallSnapshots(), hb=b.GetHallCallSnapshots();
        tests.Check(ha.size()==hb.size(),"same hall calls");
        for(std::size_t i=0;i<ha.size();++i)
            tests.Check(ha[i].floorNumber==hb[i].floorNumber && ha[i].direction==hb[i].direction &&
                ha[i].assignedElevatorId==hb[i].assignedElevatorId && ha[i].waitingCount==hb[i].waitingCount,
                "same request ownership");
    }
}

int main()
{
    TestSuite tests("Simulation");
    tests.Run("single passenger exact timeline", [&] {
        Simulation simulation; tests.Check(simulation.Initialize(Config(),42),"initialize");
        tests.Check(simulation.AddPassenger(1,3)==0,"first ID"); simulation.Start(); simulation.Update(2.0);
        auto people=simulation.GetPassengerSnapshots(); tests.Check(people.size()==1 && people[0].state==PassengerState::Waiting,"boarding pending");
        tests.Check(simulation.GetFloorSnapshots()[0].upWaitingCount==1,"remain queued until transfer completes");
        simulation.Update(1.0); people=simulation.GetPassengerSnapshots();
        tests.Check(people[0].state==PassengerState::Riding,"boarded at T"); tests.Near(people[0].boardTime,3,"board time");
        simulation.Update(4.0); tests.Check(simulation.GetElevatorSnapshots()[0].state==ElevatorState::Alighting,"alighting at destination");
        simulation.Update(2.0); tests.Check(simulation.GetStatisticsSnapshot().arrivedCount==0,"not arrived early");
        simulation.Update(1.0); const auto stats=simulation.GetStatisticsSnapshot();
        tests.Check(stats.arrivedCount==1 && simulation.GetPassengerSnapshots().empty(),"arrival deletes active object");
        tests.Near(stats.averageWaitingTime,3,"waiting includes boarding"); tests.Near(stats.averageRideTime,7,"ride includes alighting");
        tests.Check(stats.elevators[0].transportedCount==1 && stats.elevators[0].traveledFloors==2,"transport statistics");
        tests.Check(simulation.ValidateState(),"ownership and conservation");
    });
    tests.Run("single downward passenger", [&] {
        Simulation simulation; simulation.Initialize(Config(),42); simulation.AddPassenger(6,1); simulation.Start(); simulation.Update(16);
        const auto stats=simulation.GetStatisticsSnapshot(); tests.Check(stats.arrivedCount==1,"down arrival");
        tests.Near(stats.averageWaitingTime,3,"down wait"); tests.Near(stats.averageRideTime,13,"down ride");
        tests.Check(stats.elevators[1].transportedCount==1 && simulation.ValidateState(),"top elevator assignment");
    });
    tests.Run("pause freezes transfer", [&] {
        Simulation simulation; simulation.Initialize(Config(),42); simulation.AddPassenger(1,3); simulation.Start(); simulation.Update(2);
        simulation.Pause(); simulation.Update(100); tests.Near(simulation.GetCurrentTime(),2,"paused clock");
        tests.Check(simulation.GetPassengerSnapshots()[0].state==PassengerState::Waiting,"paused boarding");
        simulation.Resume(); simulation.Update(1); tests.Near(simulation.GetPassengerSnapshots()[0].boardTime,3,"resume remaining T");
    });
    tests.Run("speed applied once to all events", [&] {
        auto config=Config(); config.simulationSpeed=2;
        Simulation simulation; simulation.Initialize(config,42); simulation.AddPassenger(1,3); simulation.Start(); simulation.Update(1.5);
        tests.Near(simulation.GetPassengerSnapshots()[0].boardTime,3,"not double multiplied"); simulation.Update(3.5);
        tests.Check(simulation.GetStatisticsSnapshot().arrivedCount==1,"same simulated timeline");
    });
    tests.Run("deadline during transfer", [&] {
        auto config=Config(); config.simulationDuration=2;
        Simulation simulation; simulation.Initialize(config,42); simulation.AddPassenger(1,3); simulation.Start(); simulation.Update(100);
        tests.Check(simulation.IsFinished() && simulation.GetStatisticsSnapshot().waitingCount==1,"cutoff freezes pending board");
        tests.Check(simulation.GetPassengerSnapshots()[0].boardTime==UnsetTime && simulation.ValidateState(),"no event after deadline");
    });
    tests.Run("FIFO boarding order", [&] {
        Simulation simulation; simulation.Initialize(Config(),42); simulation.AddPassenger(1,6); simulation.AddPassenger(1,5);
        simulation.Start(); simulation.Update(3); auto people=simulation.GetPassengerSnapshots();
        tests.Check(people[0].state==PassengerState::Riding && people[1].state==PassengerState::Waiting,"oldest first");
        simulation.Update(3); people=simulation.GetPassengerSnapshots(); tests.Near(people[1].boardTime,6,"second sequential T");
        tests.Check(simulation.ValidateState(),"FIFO conservation");
    });
    tests.Run("multiple elevators run concurrently", [&] {
        auto config=Config(); config.capacity=1;
        Simulation simulation; simulation.Initialize(config,42);
        simulation.AddPassenger(1,6); simulation.AddPassenger(6,1);
        simulation.Start(); simulation.Update(0.01);
        // 首两台已经预留最后座位，第三个请求才能按容量规则分给中间梯。
        // 同一批提交三个请求会合法触发“顺路优先于空闲”，不能假设自动均分。
        simulation.AddPassenger(3,5); simulation.Update(3.1);
        tests.Check(simulation.GetStatisticsSnapshot().ridingCount==3,"three transfers overlap in simulated time");
        simulation.Update(30); const auto stats=simulation.GetStatisticsSnapshot();
        tests.Check(stats.arrivedCount==3,"all delivered");
        for(const auto& car:stats.elevators) tests.Check(car.transportedCount==1,"group distribution");
    });
    tests.Run("partial boarding retains and reassigns call", [&] {
        auto config=Config(); config.simulationDuration=200;
        Simulation simulation; simulation.Initialize(config,42); for(int i=0;i<5;++i) simulation.AddPassenger(5,6);
        simulation.Start(); simulation.Update(8); const auto calls=simulation.GetHallCallSnapshots();
        tests.Check(simulation.GetFloorSnapshots()[4].upWaitingCount==3,"three remain at landing");
        tests.Check(calls.size()==1 && calls[0].waitingCount==3 && calls[0].assignedElevatorId!=1,"not cleared or assigned to full car");
        for(int second=0;second<100;++second) { simulation.Update(1); tests.Check(simulation.ValidateState(),"partial-load conservation"); }
        tests.Check(simulation.GetStatisticsSnapshot().arrivedCount==5 && simulation.GetHallCallSnapshots().empty(),"all remainder served");
    });
    tests.Run("one owner per direction call", [&] {
        Simulation simulation; simulation.Initialize(Config(),42); for(int i=0;i<8;++i) simulation.AddPassenger(3,6);
        simulation.Start(); simulation.Update(0.5); auto calls=simulation.GetHallCallSnapshots();
        tests.Check(calls.size()==1 && calls[0].assignedElevatorId==2 && calls[0].waitingCount==8,"one group call");
        tests.Check(simulation.ValidateState(),"no duplicate car reservations");
    });
    tests.Run("opposite hall calls are independent", [&] {
        Simulation simulation; simulation.Initialize(Config(),42); simulation.AddPassenger(3,6); simulation.AddPassenger(3,1);
        simulation.Start(); simulation.Update(0.5); tests.Check(simulation.GetHallCallSnapshots().size()==2,"two directions");
        simulation.Update(50); tests.Check(simulation.GetStatisticsSnapshot().arrivedCount==2 && simulation.ValidateState(),"both served");
    });
    tests.Run("new request cannot reverse occupied car", [&] {
        Simulation simulation; simulation.Initialize(Config(),42); simulation.AddPassenger(1,6); simulation.Start(); simulation.Update(4);
        simulation.AddPassenger(2,1); simulation.AddPassenger(5,6); simulation.Update(0.5);
        tests.Check(simulation.GetElevatorSnapshots()[0].direction==Direction::Up,"old internal target retained");
        simulation.Update(80); tests.Check(simulation.GetStatisticsSnapshot().arrivedCount==3,"all calls eventually served");
    });
    tests.Run("zero random rate", [&] {
        Simulation simulation; simulation.Initialize(Config(),42); simulation.Start(); simulation.Update(100);
        tests.Check(simulation.GetStatisticsSnapshot().totalPassengerCount==0,"no accidental random passenger");
        for(const auto& car:simulation.GetStatisticsSnapshot().elevators) tests.Near(car.idleTime,100,"idle seconds");
    });
    tests.Run("arrival exactly at cutoff included", [&] {
        auto config=Config(); config.simulationDuration=8;
        Simulation simulation; simulation.Initialize(config,42); simulation.AddPassenger(1,2); simulation.Start(); simulation.Update(100);
        tests.Check(simulation.IsFinished() && simulation.GetStatisticsSnapshot().arrivedCount==1,"completion at deadline");
        tests.Check(simulation.ValidateState(),"cutoff conservation");
    });
    tests.Run("boarding exactly at cutoff included", [&] {
        auto config=Config(); config.simulationDuration=3;
        Simulation simulation; simulation.Initialize(config,42); simulation.AddPassenger(1,2); simulation.Start(); simulation.Update(100);
        tests.Check(simulation.GetStatisticsSnapshot().ridingCount==1,"completed boarding retained");
        tests.Check(simulation.GetElevatorSnapshots()[0].currentFloor==1 && simulation.ValidateState(),"no travel beyond end");
    });
    tests.Run("cutoff during movement", [&] {
        auto config=Config(); config.simulationDuration=4;
        Simulation simulation; simulation.Initialize(config,42); simulation.AddPassenger(1,6); simulation.Start(); simulation.Update(100);
        tests.Check(simulation.GetElevatorSnapshots()[0].currentFloor==1,"unfinished segment does not increment floor");
        tests.Check(simulation.GetStatisticsSnapshot().arrivedCount==0,"still active at cutoff");
    });
    tests.Run("fractional travel and transfer duration", [&] {
        auto config=Config(); config.moveTimePerFloor=0.2; config.personTime=0.3; config.simulationDuration=0.8;
        Simulation simulation; simulation.Initialize(config,42); simulation.AddPassenger(1,2); simulation.Start(); simulation.Update(10);
        tests.Check(simulation.GetStatisticsSnapshot().arrivedCount==1,"fractional completion deadline");
        tests.Near(simulation.GetStatisticsSnapshot().averageRideTime,0.5,"fractional ride");
    });
    tests.Run("invalid injection preserves state", [&] {
        Simulation simulation; tests.Check(simulation.AddPassenger(1,2)==-1,"uninitialized"); simulation.Initialize(Config(),42);
        tests.Check(simulation.AddPassenger(1,1)==-1 && simulation.AddPassenger(0,2)==-1 && simulation.AddPassenger(1,7)==-1,"bounds");
        tests.Check(simulation.AddPassenger(1,2)==0,"no ID consumed by invalid request"); simulation.Start(); simulation.Update(100);
        tests.Check(simulation.AddPassenger(1,2)==-1,"finished");
    });
    tests.Run("IDs are not reused after arrival", [&] {
        Simulation simulation; simulation.Initialize(Config(),42); simulation.AddPassenger(1,2); simulation.Start(); simulation.Update(10);
        tests.Check(simulation.AddPassenger(1,2)==1,"monotonic ID"); tests.Check(simulation.ValidateState(),"registry");
    });
    tests.Run("active snapshots cannot change core", [&] {
        Simulation simulation; simulation.Initialize(Config(),42); simulation.AddPassenger(1,3);
        auto people=simulation.GetPassengerSnapshots(); auto calls=simulation.GetHallCallSnapshots();
        people[0].targetFloor=999; calls[0].assignedElevatorId=999;
        tests.Check(simulation.GetPassengerSnapshots()[0].targetFloor==3 &&
            simulation.GetHallCallSnapshots()[0].assignedElevatorId==-1,"copy isolation");
    });
    tests.Run("fixed seed replay", [&] {
        auto config=Config(); config.passengerRate=0.4;
        Simulation a,b; a.Initialize(config,123); b.Initialize(config,123); a.Start(); b.Start(); a.Update(80); b.Update(80);
        tests.Check(a.GetStatisticsSnapshot().totalPassengerCount>0,"random events generated"); SameState(tests,a,b);
    });
    tests.Run("update partition independent random timeline", [&] {
        auto config=Config(); config.passengerRate=0.4; config.simulationDuration=200;
        Simulation a,b; a.Initialize(config,123); b.Initialize(config,123); a.Start(); b.Start(); a.Update(100);
        for(int frame=0;frame<800;++frame) b.Update(0.125);
        SameState(tests,a,b);
    });
    tests.Run("speed equivalence with random events", [&] {
        auto config=Config(); config.passengerRate=0.4; config.simulationDuration=200;
        Simulation a,b; a.Initialize(config,123); config.simulationSpeed=5; b.Initialize(config,123);
        a.Start(); b.Start(); a.Update(100); b.Update(20); SameState(tests,a,b);
    });
    tests.Run("reset clears population and replays seed", [&] {
        auto config=Config(); config.passengerRate=0.4;
        Simulation a,b; a.Initialize(config,123); b.Initialize(config,123); a.Start(); a.Update(80); a.Reset();
        tests.Check(a.GetRandomSeed()==123 && a.GetPassengerSnapshots().empty() && a.GetHallCallSnapshots().empty(),"reset clears live state");
        a.Start(); b.Start(); a.Update(80); b.Update(80); SameState(tests,a,b);
    });
    tests.Run("failed initialize preserves future random stream", [&] {
        auto config=Config(); config.passengerRate=0.4;
        Simulation a,b; a.Initialize(config,123); b.Initialize(config,123); a.Start(); b.Start(); a.Update(10); b.Update(10);
        config.capacity=0; tests.Check(!a.Initialize(config,999),"invalid config rejected");
        tests.Check(a.GetRandomSeed()==123,"seed preserved"); a.Update(50); b.Update(50); SameState(tests,a,b);
    });
    tests.Run("different seeds give different requests", [&] {
        auto config=Config(); config.passengerRate=2;
        Simulation a,b; a.Initialize(config,1); b.Initialize(config,2); a.Start(); b.Start(); a.Update(1); b.Update(1);
        const auto pa=a.GetPassengerSnapshots(),pb=b.GetPassengerSnapshots();
        tests.Check(!pa.empty() && !pb.empty() && pa[0].requestTime!=pb[0].requestTime,"different random timeline");
    });
    tests.Run("all full cars leave pending requests alive", [&] {
        auto config=Config(); config.capacity=1; config.simulationDuration=300;
        Simulation simulation; simulation.Initialize(config,42);
        simulation.AddPassenger(1,6); simulation.AddPassenger(6,1); simulation.Start(); simulation.Update(0.01);
        simulation.AddPassenger(3,6); simulation.Update(3.1);
        tests.Check(simulation.GetStatisticsSnapshot().ridingCount==3,"fixture has three full cars");
        simulation.AddPassenger(4,1); simulation.Update(0.1); const auto calls=simulation.GetHallCallSnapshots();
        tests.Check(calls.size()==1 && calls[0].assignedElevatorId==-1,"all full returns unassigned");
        simulation.Update(200); tests.Check(simulation.GetStatisticsSnapshot().arrivedCount==4 && simulation.ValidateState(),"retry after capacity released");
    });
    tests.Run("finite batch drains without passenger loss", [&] {
        auto config=Config(); config.floorCount=20; config.elevatorCount=6; config.capacity=3;
        config.moveTimePerFloor=0.5; config.personTime=0.25; config.simulationDuration=20000;
        Simulation simulation; simulation.Initialize(config,42);
        for(int i=0;i<2000;++i) { const int start=i%20+1; const int target=(start-1+1+i%19)%20+1; simulation.AddPassenger(start,target); }
        simulation.Start(); simulation.Update(20000); const auto stats=simulation.GetStatisticsSnapshot();
        tests.Check(stats.totalPassengerCount==2000 && stats.arrivedCount==2000,"all 2000 delivered");
        tests.Check(simulation.GetPassengerSnapshots().empty() && simulation.GetHallCallSnapshots().empty() && simulation.ValidateState(),"no leaked active IDs");
    });
    tests.Run("high Poisson traffic remains consistent", [&] {
        auto config=Config(); config.floorCount=20; config.elevatorCount=6; config.capacity=4;
        config.moveTimePerFloor=0.3; config.personTime=0.2; config.passengerRate=8; config.simulationDuration=600;
        Simulation simulation; simulation.Initialize(config,321); simulation.Start();
        for(int sample=0;sample<1200;++sample) { simulation.Update(0.5); tests.Check(simulation.ValidateState(),"high-flow invariants"); }
        const auto stats=simulation.GetStatisticsSnapshot();
        tests.Check(stats.totalPassengerCount>4300 && stats.totalPassengerCount<5300,"plausible Poisson count, not forced exact count");
        tests.Check(stats.arrivedCount>0 && stats.waitingCount>0,"loaded service with backlog");
        double fullTime=0; for(const auto& car:stats.elevators) fullTime+=car.fullTime;
        tests.Check(fullTime>0,"full operation recorded");
        std::cout << "High flow: generated=" << stats.totalPassengerCount << ", arrived=" << stats.arrivedCount
            << ", waiting=" << stats.waitingCount << ", riding=" << stats.ridingCount << '\n';
    });
    tests.Run("one-hour stability", [&] {
        auto config=Config(); config.floorCount=20; config.elevatorCount=6; config.capacity=15;
        config.moveTimePerFloor=1.5; config.personTime=0.5; config.passengerRate=0.6; config.simulationDuration=3600;
        Simulation simulation; simulation.Initialize(config,987); simulation.Start();
        for(int minute=0;minute<60;++minute) { simulation.Update(60); tests.Check(simulation.ValidateState(),"long-run invariant"); }
        const auto stats=simulation.GetStatisticsSnapshot();
        tests.Check(simulation.IsFinished() && stats.arrivedCount>1000,"long run completes");
        tests.Check(std::isfinite(stats.averageWaitingTime) && std::isfinite(stats.averageRideTime),"finite aggregates");
        std::cout << "One hour: generated=" << stats.totalPassengerCount << ", arrived=" << stats.arrivedCount << '\n';
    });
    tests.Run("very large real delta clamped", [&] {
        auto config=Config(); config.simulationSpeed=10;
        Simulation simulation; simulation.Initialize(config,42); simulation.AddPassenger(1,6); simulation.Start();
        simulation.Update((std::numeric_limits<double>::max)());
        tests.Near(simulation.GetCurrentTime(),100,"clamped time");
        tests.Check(simulation.GetStatisticsSnapshot().arrivedCount==1 && simulation.ValidateState(),"events handled before cutoff");
    });
    tests.Run("non-binary update partitions across seeds", [&] {
        auto config=Config(); config.passengerRate=1; config.moveTimePerFloor=0.3; config.personTime=0.2;
        for(std::uint32_t seed=1;seed<=10;++seed)
        {
            Simulation a,b; a.Initialize(config,seed); b.Initialize(config,seed); a.Start(); b.Start(); a.Update(100);
            for(int frame=0;frame<3333;++frame) b.Update(0.03);
            b.Update(100-b.GetCurrentTime()); SameState(tests,a,b);
        }
    });
    tests.Run("passenger transition guards", [&] {
        Passenger passenger(0,1,3,2);
        tests.Check(!passenger.MarkArrived(4) && !passenger.MarkBoarded(0,1),"cannot skip state or go backward in time");
        tests.Check(passenger.MarkBoarded(0,5) && !passenger.MarkBoarded(1,6),"one boarding");
        tests.Check(!passenger.MarkArrived(4) && passenger.MarkArrived(10) && !passenger.MarkArrived(11),"one arrival");
    });
    tests.Run("floor FIFO and ID guards", [&] {
        Floor floor(3); tests.Check(floor.Enqueue(1,Direction::Up) && floor.Enqueue(2,Direction::Up),"enqueue");
        tests.Check(!floor.Enqueue(1,Direction::Down) && !floor.Enqueue(-1,Direction::Up),"duplicate or invalid ID");
        tests.Check(!floor.RemoveFront(2,Direction::Up) && floor.Peek(Direction::Up)==1,"cannot bypass front");
        tests.Check(floor.RemoveFront(1,Direction::Up) && floor.Peek(Direction::Up)==2,"remove front only");
    });
    return tests.Finish();
}

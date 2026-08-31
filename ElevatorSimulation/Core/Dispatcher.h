#pragma once

#include "CommonTypes.h"
#include "Elevator.h"

#include <vector>

class ElevatorDispatcher
{
public:
    // 返回 elevators 的下标 0~N-1；无法分配时返回 InvalidElevatorId。
    // 无副作用，不接管对象所有权，不推进仿真时间。
    int SelectElevator(int requestFloor, Direction requestDirection,
        const std::vector<Elevator>& elevators) const;
};

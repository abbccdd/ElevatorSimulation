#include "Dispatcher.h"

int ElevatorDispatcher::SelectElevator(int requestFloor, Direction requestDirection,
    const std::vector<Elevator>& elevators) const
{
    // TODO: 后续由调度模块负责人实现最终群控算法
    // TODO(C): 顺路同向优先，其次空闲，最后其他忙碌电梯；代价函数留待后续。
    // 现在明确表示“未分配”，调用方不得拿 -1 访问 elevators。
    (void)requestFloor;
    (void)requestDirection;
    (void)elevators;
    return InvalidElevatorId;
}

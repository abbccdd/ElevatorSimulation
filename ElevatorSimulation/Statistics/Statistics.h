#pragma once

#include "../Core/CommonTypes.h"

// 被 Simulation 组合，仅依赖 Common，不反向依赖 Simulation/UI。
class Statistics
{
public:
    void Reset(int elevatorCount);
    StatisticsSnapshot GetSnapshot() const;

    // TODO(F/D): 接入生成、上梯、到达和移动事件，计算统计而非扫描已删除乘客。
    // 平均值的样本口径、结束时未完成乘客的处理需先在 README 中约定。

private:
    StatisticsSnapshot m_snapshot;
};

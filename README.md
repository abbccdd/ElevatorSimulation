# 多电梯群控调度仿真系统

本项目是 Visual Studio C++ / MFC 多人协作课程设计，用于在 L 层建筑中仿真 N 部载客电梯的群控调度。**当前已实现可独立运行和测试的核心仿真，以及基于 SimulationWorker、命令队列和只读 Snapshot 的 MFC 动态操作界面。**

## 开发环境与启动

- Windows、Visual Studio 2022、MSVC v143、Windows SDK 10.0、MFC（动态库）、Unicode。
- C++17 + 标准 STL；使用一个 Simulation 工作线程和 Dispatcher 固定线程池，不引入第三方并发库、数据库或网络服务。
- 直接打开根目录 `ElevatorSimulation.sln`，不要另建重复 MFC 工程。
- 选择 Debug / x64，生成解决方案后按 F5 或 Ctrl+F5 启动。
- 解决方案的 x86 对应项目的 Win32；同时保留 Debug/Release × x64/x86 四种配置。
- `Core`、`Statistics` 的 `.cpp` 单独设置为“不使用预编译头”；MFC 文件继续使用现有 `pch.h`。
- 所有配置启用 `/utf-8`，新增 C++ 源文件为 UTF-8；原资源文件保留 UTF-16，避免破坏中文资源。

原项目是 **MFC 对话框应用**。入口为 `CElevatorSimulationApp::InitInstance()`，通过 `CElevatorSimulationDlg::DoModal()` 启动主对话框。App、对话框、资源与 PCH 都保留原物理位置；Visual Studio 中增加 `Core`、`Statistics`、`UI`、`Development` 筛选器。

当前 MFC 启动后仍仅显示默认初始化快照：20 层、6 部电梯、0 秒、无乘客，保留“确定”“取消”和“关于”。核心已能运行，但主窗口暂未提供控制按钮、定时器或动画；不要把静态初始化窗口误当成核心尚未实现。

## 目录与文件职责

```text
ElevatorSimulation/
├─ ElevatorSimulation.sln                 现有解决方案
├─ ElevatorSimulation/
│  ├─ Core/
│  │  ├─ CommonTypes.h                    唯一公共枚举、参数、快照
│  │  ├─ Passenger.h / Passenger.cpp       乘客模型、状态转换与时间戳
│  │  ├─ Floor.h / Floor.cpp               楼层与上下行 ID FIFO 队列
│  │  ├─ Elevator.h / Elevator.cpp         方向保持、S/T 计时与上下客状态机
│  │  ├─ Dispatcher.h / Dispatcher.cpp     Cost/ETA 与方向成本的 LOOK 群控
│  │  └─ Simulation.h / Simulation.cpp     事件编排、随机到达、外呼归属与快照
│  ├─ Statistics/
│  │  └─ Statistics.h / Statistics.cpp     事件统计与只读快照
│  ├─ ElevatorSimulation.cpp / .h         原 App 入口（未修改）
│  ├─ ElevatorSimulationDlg.cpp / .h      原对话框，最小快照接入
│  ├─ StatisticsTrendView.cpp / .h        UI 层低频统计趋势自绘
│  ├─ ElevatorSimulation.vcxproj           编译项与各配置
│  ├─ ElevatorSimulation.vcxproj.filters   VS 虚拟分组
│  ├─ ElevatorSimulation.rc / Resource.h  原资源，增加初始化状态控件标识
│  ├─ pch.h / pch.cpp / framework.h        原 MFC 框架（未移动）
│  └─ res/                                原图标与资源
├─ Tests/
│  ├─ CoreSmokeTests.cpp                   初始化与公共接口行为验证
│  ├─ RunCoreSmokeTests.cmd                保留原 406 项检查
│  ├─ DispatcherTests.cpp                  独立调度场景
│  ├─ ElevatorTests.cpp                    状态机与联合测试
│  ├─ SimulationTests.cpp                  集成、随机复现和压力测试
│  ├─ TestSupport.h                        轻量断言与场景汇报
│  └─ RunCoreTests.cmd                     x64/x86 核心测试入口
├─ docs/AlgorithmDesign.md                 调研来源、算法与时间/统计口径
├─ README.md
├─ AGENTS.md                              后续开发规则与本机工具记录
└─ .gitignore                             排除缓存、个人设置和构建产物
```

`build/`、`.vs/`、`x64/`、Debug/Release 等为本机生成目录，不属于业务代码。不移动或重建原 `resource.h`、`.rc`、`pch.*`、`framework.h`、App 类来追求目录外观。

## 模块关系

下表箭头表示“调用或组合”，并非运行先后顺序。所有模块均允许依赖 Common。

| 模块 | 依赖 | 不应承担的职责 |
| --- | --- | --- |
| Passenger / Floor | Common；通过 PassengerId 关联 | 不知道窗口，不选择电梯 |
| Elevator | Common；只存乘客 ID | 不决定哪部电梯响应新请求 |
| Dispatcher | Common、只读 Elevator、固定线程池 | 不画 UI、不推进时间、不改电梯 |
| Statistics | Common | 不反向依赖 Simulation 或 MFC |
| Simulation | Passenger、Floor、Elevator、Dispatcher、Statistics | 不访问具体对话框和控件 |
| SimulationWorker | Simulation、命令队列、UI Snapshot | 独占真实 Simulation，不依赖 MFC |
| MFC UI | SimulationWorker 的命令接口与只读 UI Snapshot | 不实现乘客生成、运行或群控算法 |

`UI → SimulationWorker → Simulation → {Floor, Passenger, Elevator, Dispatcher, Statistics}`；`Dispatcher → {Elevator, FixedThreadPool}`。统计由 Simulation 组合，后续由其传入事件，因此无需让 Statistics 反向包含 Simulation，不形成循环依赖。

## 唯一公共契约

`ElevatorSimulation/Core/CommonTypes.h` 统一定义：

- `Direction`：Down=-1、Idle=0、Up=1。
- `PassengerState`：Waiting、Riding、Arrived。
- `ElevatorState`：Idle、MovingUp、MovingDown、Boarding、Alighting、Stopped。
- `SimulationState`：Uninitialized、Ready、Running、Paused、Finished，用于总控制器生命周期。
- `SimulationConfig`：L/N/K、S 秒/层、T 秒/人、总时长、全楼到达速率、仿真倍速与 `TrafficPattern` 客流模式；模式默认 `Uniform`。
- `PassengerId`、`InvalidPassengerId=-1`、`InvalidElevatorId=-1`、`UnsetTime=-1.0`。
- 原 `ElevatorSnapshot`、`FloorSnapshot`、`StatisticsSnapshot`、`ElevatorStatisticsSnapshot`；新增只读调度、乘客、外呼快照、UI 高频视图 `SimulationUISnapshot`，以及单梯动作事件，不重新定义原状态类型。
- 无状态函数 `GetDirection(startFloor, targetFloor)`；同层返回 Idle，但同层起终点的乘客仍属非法。

禁止在其他文件重新定义 `ElevatorDirection`、`MoveDirection`、`PassengerStatus`、`ElevatorStatus`、`Config`、`SimulationParameter` 等语义重复类型。修改公共契约前，必须检查全部调用点并告知受影响的模块负责人。

Snapshot 按值构造：`SimulationWorker` 将 UI 所需的 `SimulationUISnapshot` 发布为 `shared_ptr<const ...>`，UI 原子读取最近一版，不与 Simulation 并发。高频 UI Snapshot 不填充乘客明细，按需通过独立 `GetPassengerSnapshots()` 获取；UI 不获得 Floor/Elevator/Passenger 的可写指针、引用或容器，`GetConfig()` 同样返回副本。

调度快照 `ElevatorDispatchSnapshot::StopService` 的 Idle 记录表示内呼/下客，Up/Down 记录表示外呼；新增 `boardingTargetFloors` 为已知等待乘客的 FIFO 目标层前缀，最多需要 capacity 人。Simulation 回填真实人数并跳过正在 Boarding 的队头；纯人数旧快照仍可使用，但不会推测未知下客楼层。`SelectElevator` / `SelectFromSnapshots` 签名保持兼容，Dispatcher 不直接读取 Passenger、Floor 或 Simulation。

新增 `HallCallDispatchSnapshot`（请求及 FIFO 目标副本）、`DispatchScore`（可行性、ETA、Cost、预计人数）与 `DispatchPlan`（归属方案、总成本及搜索计数）。`ScoreSnapshot` 复用同一个评分实现，`SelectReassignment` / `PlanAssignments` 只读决策。`DispatchObservationSnapshot` 由 Simulation 针对真实 Hall Call 构造，只读复用 `BuildDispatchSnapshots` 与 `ScoreSnapshot`，按 feasible、Cost、ETA、ID 排序，绝不提交调度状态。Simulation 负责真实提交，Elevator 仅新增安全的 `RemoveHallCall`，不改变原状态机。

## 编号、初始化与所有权

楼层编号统一为 **1~L**。`m_floors` 按楼层递增存储，存储下标只是 STL 的实现细节；各对象、任务、乘客、调度请求与 Snapshot 中均使用真实楼层编号，不向调用方暴露第二套楼层编号。

电梯容器下标与 `id` 统一为 **0~N-1**；UI 统一显示 **E1~EN**，即 `id + 1`。调度失败的 `-1` 不是可用下标，调用前必须检查。

初始化固定分三组，N 必须为 3 的正倍数：

| 容器下标 | 初始楼层 |
| --- | --- |
| `[0, N/3)` | 1 |
| `[N/3, 2N/3)` | L |
| `[2N/3, N)` | `(L+1)/2`，整数除法 |

例如 L=30、N=6：E1/E2 在 1 层，E3/E4 在 30 层，E5/E6 在 15 层。L=21 时中间层为 11；L=2 时中间层为 1，允许分组位置重合。实现使用等价的 `L/2 + L%2` 避免 `L+1` 溢出。

所有电梯初始零乘客、`Direction::Idle`、`ElevatorState::Idle`，内部上下行任务为空。所有楼层等待队列为空，统计归零。

`Simulation::m_passengers` 是 `unordered_map<PassengerId, Passenger>`，**唯一按值拥有乘客对象**。Floor 的两个 `deque` 和 Elevator 的 `vector` 仅保存 ID，不拥有或释放乘客，也不长期缓存容器元素地址。同一轮仿真内 ID 不复用，由 Simulation 统一分配；每次查找 ID 必须检查是否存在。下梯完成时电梯先移除 ID，总控制器更新统计后再从活动乘客表删除对象。禁止多个模块保存同一对象并各自 delete。

运行时只有 `SimulationWorker` 的工作线程构造并调用真实 `Simulation`。Elevator、Passenger、Floor、Hall Call、Statistics 和 `mt19937` 都只在该线程写入；MFC 主线程仅入队 Start/Pause/Resume/Reset/Stop 命令和显示最近的 UI Snapshot。命令队列使用一个短时互斥量和条件变量，Snapshot 使用 C++17 的 `shared_ptr` 原子发布，不给每个核心容器分别加锁。

## 已实现的接口行为

| 接口 | 当前行为 |
| --- | --- |
| `Initialize(config)` | 校验参数，建立 L 层/N 梯及统计槽位，清空活动乘客，时间归零，状态 Ready |
| `GetLastError()` | 获取最近初始化的诊断文字；成功初始化后清空 |
| `Start()` | 仅 Ready → Running，重复调用无效果 |
| `Pause()` | 仅 Running → Paused |
| `Resume()` | 仅 Paused → Running；不能代替首次 Start |
| `Reset()` | 用最近成功的配置与同一个 seed 重建，回到 Ready；未初始化时无操作 |
| `Update(deltaTime)` | Running 时按事件边界推进全部电梯、随机乘客、调度、上下客与统计；到总时长转 Finished |
| `IsRunning()` / `IsFinished()` / `GetState()` | 查询生命周期 |
| `GetCurrentTime()` / `GetConfig()` | 查询时间与配置副本 |
| `GetElevatorSnapshots()` / `GetFloorSnapshots()` | 按电梯 ID / 楼层升序返回状态副本 |
| `GetStatisticsSnapshot()` | 返回真实事件累计统计副本 |
| `Initialize(config, seed)` / `GetRandomSeed()` | 固定种子初始化或读取本轮种子，便于复现 |
| `AddPassenger(start, target)` | 当前时刻手工注入乘客，非法输入返回 InvalidPassengerId |
| `GetPassengerSnapshots()` / `GetHallCallSnapshots()` | 读取活动乘客及外呼唯一归属副本 |
| `ValidateState()` | 只读检查人数守恒、ID 所有权和外呼归属 |
| `SetDispatcherExecutionMode(mode, workers)` | 选择 Sequential 或固定线程池 Parallel 评分；默认核心模式为 Sequential |
| `GetUISnapshot()` | 由 Worker 线程构造 UI 所需只读副本；乘客明细按需使用独立接口 |
| `GetDispatchObservation(floor, direction)` | 只读返回真实 Hall Call 的单梯候选评分；请求不存在时返回 invalid |
| `SimulationWorker::{Start,Pause,Resume,Reset,Stop}` | 将控制命令按 FIFO 交给工作线程；Stop 正常 join |
| `SimulationWorker::GetLatestSnapshot()` | UI 线程原子读取最近发布的不可变快照 |
| `SimulationWorker::{ObserveHallCall,ClearObservedHallCall,GetLatestObservation}` | Worker 线程最多约 5Hz 计算并原子发布只读观察快照 |

初始化要求：`floorCount >= 2`，`elevatorCount > 0 && elevatorCount % 3 == 0`，`capacity > 0`，S/T/总时长/倍速均为**有限正数**，`passengerRate` 为**有限非负数**，`trafficPattern` 为四个已定义枚举值之一。拒绝 NaN、正负无穷。K=10~20、S=1~5、T=2~10 是背景中的典型范围，当前不将其作为硬性上限。零速率关闭随机生成。passengerRate 为全楼每仿真秒的平均到达人数，仍采用指数间隔的单人 Poisson 到达；`Uniform` 均匀选择不同起终点，`UpPeak` 以 75% 概率从 1F 去往 2~L，`DownPeak` 以 75% 概率从 2~L 去往 1F，`InterFloor` 在 L>=3 时以 90% 概率从 2~L 选择不同起终点（L=2 回退 Uniform）。所有抽样共用本轮单一 `mt19937`。

参数不合法时 `Initialize` 返回 false，不抛参数异常，保留上一次有效参数、容器、时钟和运行状态。创建容器时的 `bad_alloc` / `length_error` 也转换为失败诊断，不故意设置未经约定的参数上限；极端规模仍受机器内存限制。`Reset` 为 void，如分配失败会保留旧状态，调用者可检查 `GetLastError()`。

Passenger/Floor/Elevator 的直接构造函数对基础非法字段抛出 `std::invalid_argument`。Simulation 检查乘客楼层上界，构造 Elevator 时传入完整配置使其校验任务上界；旧三参数 Elevator 构造函数仍保留兼容性，未指定建筑上界。

时间约定：所有模型时间单位为**仿真秒**；`Update` 的入参为**真实经过秒数**，内部乘一次 `simulationSpeed`。UI 不再调用 Update，Worker 使用 `steady_clock` 采样真实时间。Start/Pause/Resume/Reset 每次处理后都重置墙钟基点，暂停时段不会注入下一次 Update。非有限、零或负 deltaTime 被忽略；结束后不能 Start/Resume 重启，需 Reset。

Update 在下一个乘客到达、任意电梯动作完成、当前帧目标时间三者中取最早边界；同时推进全部电梯，再处理事件。到总时长即结束，不延长到全员送达；随机到达在截止前发生，截止时刻完成的动作仍计入。

## 算法与模块分工

详细调研来源、评分公式、状态机、事件顺序、统计口径与简化边界见 [算法设计](docs/AlgorithmDesign.md)。

| 负责人 | 文件范围 | 本轮状态 / 后续方向 |
| --- | --- | --- |
| A | Core/Passenger.*、Core/Floor.* | 已补齐状态转换、FIFO 与 ID 校验；后续可扩展交通输入方式 |
| B | Core/Elevator.* | 已实现方向保持、顺路停靠、S/T 计时、容量及上下客 |
| C | Core/Dispatcher.* | 统一 Cost/ETA 与 LOOK 预演；有限联合分配、滞回改派决策、稳定 tie-break |
| D | Core/Simulation.* | 已集成种子、Poisson 到达、手工注入、唯一外呼归属、事件推进和清理 |
| E | ElevatorSimulationDlg.* 与必要资源 | 已完成初步参数输入、运行控制、倍速、Timer 与 Snapshot 动态列表；正式动画和视觉优化后续再做 |
| F | Statistics/*、Tests/* | 已接入事件统计、回归及压力测试；后续扩展算法对照场景和展示 |

Dispatcher 无副作用。当前满载梯若预测在请求层接客前释放容量，可以参与候选；若到请求层完成下客后仍满载，则不分配。无合理候选返回 -1。Elevator 只执行已接受任务，不自行寻找整栋楼的乘客。当前方向前方的内呼及双向外呼全部处理至可折返位置后才能反向；仅内呼和同向外呼触发停站服务。满载未上梯乘客继续留队，外呼解除旧归属并重试，不丢弃剩余请求。

Dispatcher 的 Aging 保持 `AgingBonus = min(8.0, waitingSeconds × 0.05)`；`AdjustedCost = ETA + LoadCost + max(0, DirectionCost - AgingBonus)`，其中 `LoadCost = T × 请求层完成下客后的 projectedOccupancy / capacity`。顺路与空闲统一比较 Cost，反向忙碌的方向成本为 S+T，按 Cost、ETA、距离、任务数、ID 稳定排序。

ETA 包含当前动作剩余时间、LOOK 路线移动、实际可上下客的逐人 T，以及请求层接客前的下客时间。下客事件消费后清零，Alighting 当前一人只计剩余时间，其余人各计 T；Boarding 预留者的席位与未来下客只计一次。已有等待乘客按 FIFO 和剩余容量预计上梯，其目标层加入局部预演任务，后续下客释放容量。预演不修改真实对象，不预测未来新乘客或未来分配；单次评分不保证全局最优及持续超载下的等待上界。

Hall Call 动态改派只由新乘客、到层、上下客完成和零耗时状态变化触发，不受 UI 帧率驱动。同一仿真时刻最多一轮改派。先用 `ScoreSnapshot` 评估原梯；在请求层真正 Stopped/Boarding/Alighting 且 `betweenFloors=false` 时始终禁止改派。原梯不可行时立即找其他可行候选，不受普通 proximity/cooldown/收益阈值限制。原梯仍可行时，改派后冷却 10 仿真秒，且必须改善 ETA 至少 5 秒；仅在 Moving、betweenFloors 且方向真正朝请求层、整数层距离不超过 1 时使用临近锁。同层已驶离或相邻但正在远离不触发临近锁。所有阈值集中定义于 Dispatcher.h。

`RemoveHallCall` 只删除指定方向外呼：当前层 Stopped/Boarding/Alighting 禁止撤销，Moving 中已经离开的整数层允许撤销。内呼、当前方向、动作状态和剩余时间保持不变，后续事件继续由既有 Elevator 状态机处理。

未分配请求按 firstRequestTime / firstPassengerId / key 排序，再逐个用当前完整快照调用现有 ScoreSnapshot。存在至少一台 feasible 候选时为 Active；全部不可服务时为临时 DeferredCapacity，不占联合分配的三个名额。例如前三个 Deferred、后面三个 Active，后面三个直接组成联合批次。Deferred 不保存为永久请求状态，不删除真实 Floor/HallCall，不重置原等待时间或 FIFO 队头；容量释放或路线结构变化后的调度事件会重新评估。沿用 m_dispatchDirty，包括 Alighted 和外呼改派/释放/撤销，不增加定时事件或 UI 每帧扫描，也不扩 UI 公共接口。DeferredCapacity 是核心调度语义，后续 UI 可显示为灰色“等待运力”。

每批只规划最老 3 个 Active 请求；每个请求最多选 3 台候选梯和“暂不分配”，每批最多 64 个组合。每插入一个请求就在局部快照增加任务、FIFO 上客及未来下客，再计算后续请求；最终还会重算较早请求被新增任务影响后的 ETA/载荷。先最大化可分配数量，再依总 Cost、最大 ETA、总 ETA、请求顺序中的电梯 ID 比较。一次 DispatchCalls 内提交本批、重建快照并重新预筛剩余 pending，直到没有 Active pending 或本批 assignedCount == 0。每个成功批次至少减少一个 pending，不推进时间、不反复改派；所有请求 Deferred 时正常退出并全部留队。候选截断和有限批次不等于全局最优。

统计的等待时间包含上梯 T，以完成上梯者为样本；乘梯时间包含下梯 T，以已到达者为样本。截止仍等待/乘梯者保持活动状态。比较算法时必须同时看送达量与积压，不能只比较已完成样本的均值。

## 公共代码规范

- 类名/函数 PascalCase，局部变量 camelCase，新增成员变量统一 `m_` 前缀。
- 中文注释，使用 `enum class`、`nullptr`、RAII、STL；新增代码避免裸 new/delete，不使用 C 数组存动态数据。
- 不使用全局变量保存仿真；向导原有 `theApp` 是 MFC 应用对象，不是仿真数据全局容器。
- 变更尽量限于负责模块；公共类型或接口变更须搜索所有调用者、更新说明并重新编译。
- 不通过 `friend`、公共容器或可写指针绕过模块接口；不把业务代码堆进 Dialog。
- 不在 Core/Statistics 包含 `pch.h`、`framework.h`、`afx*.h` 或窗口类。新核心 `.cpp` 必须加入 `.vcxproj` 和 `.filters`，并禁用其 PCH。

## 验证与排障

在项目根目录 PowerShell 中使用已安装的工具（不要重复安装）：

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' '.\ElevatorSimulation.sln' /t:Build /p:Configuration=Debug /p:Platform=x64 /m /nologo
& '.\Tests\RunCoreSmokeTests.cmd'
& '.\Tests\RunCoreSmokeTests.cmd' x86
& '.\Tests\RunCoreTests.cmd' All x64
& '.\Tests\RunCoreTests.cmd' All x86
```

测试脚本通过 vswhere 使用现有 MSVC，以 C++17、`/W4 /WX` 独立编译 Core/Statistics，不包含或链接 MFC，也不增加第二个 VS 工程。冒烟脚本默认 x64，也支持参数 x86；新增核心测试脚本支持 x64/x86。输出分别位于 `build/core-smoke/<arch>/` 与 `build/core-tests/<arch>/`。冒烟测试的原 406 项检查完整保留；新增测试覆盖状态机、FIFO 预演和完整事件流程。退出码非零表示编译或验证失败。

若出现 C1010，检查新核心文件是否误启用了 MFC PCH；若中文乱码，检查 C++ 的 `/utf-8` 和资源编码；若启动提示 MFC DLL 缺失，检查对应架构的现有 VS/MFC 运行环境，不能以改写核心依赖来规避。Debug 运行需要开发环境，不作为分发包；后续分发 Release 再处理相应 VC++ 运行库。

2026-08-31 工程准备验证：

| 验证项 | 结果 |
| --- | --- |
| 修改前原工程 Debug/x64 生成 | 通过，未发现原有编译问题 |
| 修改后 Debug/x64、Release/x64、Debug/x86、Release/x86 全量重新生成 | 全部通过，每种配置 0 警告、0 错误 |
| 独立 Core/Statistics 测试，MSVC `/W4 /WX` | 406 项检查通过 |
| Debug/x64、Release/x64 MFC 实际启动 | 通过，正确显示六部电梯分组、零乘客和中文文字 |
| 原有“关于”对话框、主窗口“取消”/“确定”退出 | 通过 |
| 工程编译项、筛选器与文件存在性 | 全部匹配，六个核心/统计源文件均禁用 PCH |

以上是工程准备阶段的历史基线。本轮仍保留 406 项检查，只把一项“调度器永远返回未分配”的旧占位预期更新为有效空闲梯可分配。新增测试覆盖调度、单梯、联合流程、种子复现、分帧一致性、截止边界、2,000 人有限批次、高客流及一小时稳定性。本机日志保存在 build/verification/，不纳入源码交付。

2026-08-31 核心实现阶段验证：

| 验证项 | 结果 |
| --- | --- |
| 原 CoreSmokeTests | 406 项检查通过，未删除任何检查 |
| Dispatcher | 83 个场景、444 项断言通过（含 108 组真实 LOOK 路线对照） |
| Elevator（含撤销外呼及内呼保护） | 26 个场景、87 项断言通过 |
| Simulation（含改派、随机、压力、所有权） | 41 个场景、2,149 项断言通过 |
| 新增测试架构 | x64 与 x86 均通过，合计每个架构 150 场景 / 2,680 断言，另有各 406 项 Smoke |
| Debug x64 / Release x64 / Debug x86 / Release x86 全量重新生成 | 全部 0 警告、0 错误 |
| 2,000 人有限批次 | 足够时长后全部送达，无活动 ID 或外呼遗留 |
| 高客流：seed=321，λ=8，600 秒 | 生成 4,815，送达 3,422，等待 1,369，乘梯 24，人数守恒 |
| 一小时：seed=987，λ=0.6 | 生成 2,186，送达 2,176，全部采样一致性检查通过 |
| 固定任务对照 | 同向路线实际响应 10 秒，纯最近距离选择需 30 秒；不代表所有客流均优 |

具体配置见 Tests/SimulationTests.cpp。本次 Deferred 修复日志位于 `build/verification/deferred-capacity/`，不提交生成文件；核心只改 Simulation.cpp 的窗口预筛，既有 Dispatcher 评分、Elevator 状态机和公共接口均未修改。新增回归覆盖三个 Deferred 不占 Active 窗口、实际分配与三个 Active 的联合计划对照、FIFO/Aging 连续、路线撤销/顺序变化后恢复、Alighted 后正常服务及含 Deferred 的分帧一致性；原全不可行批次退出和 2000 人等测试继续保留。

2026-09-02 多线程架构验证：

| 验证项 | 结果 |
| --- | --- |
| Sequential / Parallel fixed seed | 120 个逐秒检查点的完整调度、乘客、外呼和统计一致；9,255 项并发断言通过 |
| Worker Pause / Resume | 暂停期间仿真时间不增长，恢复后不注入暂停墙钟时间 |
| Worker Reset / Running 中关闭 | 固定 seed 恢复 Ready 初态；析构正常 Stop 并 join，无 detached thread |
| Dispatcher 性能（x64 `/O2`，默认 8 个评分线程） | N=6/30/60/120 为 1.28× / 2.59× / 3.95× / 4.46×；所有选择结果一致 |

性能数据由 `Tests/RunDispatchPerformance.ps1 x64` 在 120 层混合 LOOK 任务快照上测得，每种 N 执行 120 次选择；它衡量候选评分，不代表完整 UI 帧率或所有机器的固定加速比。

以下保留固定归属贪心 `0fade61` 与联合分配初版 `e7b96a6` 的历史对照，不是本轮 Deferred 修复的重新测量。两版使用相同 S/T、FIFO 注入、seed 和 MSVC `/O2 /MD`；`Tests/RunDispatchComparison.ps1 x64` 可在 build 内导出固定旧基线与当前源码重新对照，不切换分支。对照程序不加入 MFC 可执行文件。平均等待包含上梯 T：

| 固定场景 | 原贪心固定归属均等候（秒） | 联合分配+动态改派均等候（秒） | 送达旧/新 |
| --- | ---: | ---: | ---: |
| 9F↑、11F↓ 两请求 | 13.0000 | 12.0000 | 2 / 2 |
| 15F↑ 后新增 17F↓ 导致绕行 | 41.0000 | 12.0000 | 2 / 2 |
| 90 人有限批次 | 13.0889 | 13.1806 | 90 / 90 |
| 2000 人有限批次 | 306.5200 | 304.2986 | 2000 / 2000 |
| seed=321 高客流 600 秒 | 60.5609 | 62.2858 | 3381 / 3418 |

历史对照的高客流等待队列旧/新为 1413/1383，乘梯中为 21/14；两版已上梯样本不同，不能只比较均值。90 人场景略有退化。联合分配初版增加了计算量：本机 x64 优化编译、每场景 3 次平均，2000 人约 54→356 ms，高客流约 91→4044 ms（不含编译）；不是所有客流都更快或等待更短。完整配置、成本反例、搜索开销及局限见 docs/AlgorithmDesign.md。

## MFC UI 初步集成

当前主对话框已经打通“输入参数 → Start → 实时运行与显示 → Pause/Resume → Finished → Reset”的最小完整链路，界面保持原生 MFC 风格，暂不包含主题、自绘、图片或复杂动画。

- 参数区提供楼层数 L、电梯数 N、容量 K、每层运行时间 S、每人上下客时间 T、总时长、全楼乘客产生率、客流模式、仿真倍速与固定 seed。客流模式下拉框依次为均匀随机、上行高峰、下行高峰、层间交通；与其他参数一样，仅在 Ready 时可修改，Start 时写入 `SimulationConfig::trafficPattern`。UI 只做严格的字符串/数值转换，参数范围和 N 为 3 的正倍数等规则仍由工作线程中的 `Simulation::Initialize` 统一校验，失败信息通过 Snapshot 显示。
- 控制区提供开始、暂停、继续和重置。按钮及参数编辑框随 `Ready`、`Running`、`Paused`、`Finished` 状态启用或禁用；Finished 保留最终快照，必须 Reset 后才能开始下一轮。
- 对话框使用 33 ms MFC Timer，只读取最新 `SimulationUISnapshot` 并更新控件。真实时间采样和 `Simulation::Update(realDelta)` 只在 SimulationWorker 中发生；Start、Pause、Resume、Reset 命令都会重设工作线程的墙钟基准。
- UI 从一份不可变快照读取电梯、楼层、Hall Call 和统计。电梯列表显示 E1~EN、真实楼层、方向、动作状态和载客量；楼层列表按高层到低层显示上下行等待；Hall Call 列表显示等待人数和归属；统计区显示模型时间、生成/等待/乘梯/到达人数及平均等待、平均乘梯、最大等待。
- Reset 命令沿用最近成功配置和 seed，清空模型时间、乘客、楼层队列和统计并回到 Ready。Ready 状态再次 Start 时会根据当前输入框重建 Worker，因此用户修改参数后不会误用旧配置；关闭窗口会发送 Stop 并 join 所有线程。
- 当前 `HallCallSnapshot` 只能表达 assigned/unassigned，不能区分临时 `DeferredCapacity`。界面暂将其显示为“未分配”，灰色 Deferred 展示保留为 TODO；不为此增加第二套容量判断或扩展核心接口。

## 使用核心与下一步

UI 尚未完成时，可使用独立控制台入口展示正式调度核心：

```powershell
& '.\Demo\RunAlgorithmDemo.cmd' x64 --step
```

也可以直接双击项目根目录的 `启动算法演示.bat`，它会自动启动 x64 逐场演示并在结束后保留窗口。首次运行或 Core/Demo 源码内容变化时自动编译；内容未变化时复用缓存程序，哈希检查失败则回退到完整编译。

它提供 ETA/容量预演、Aging、有限联合分配、动态改派、DeferredCapacity 和固定批次六个可重复场景，不实现第二套算法，也不加入 MFC 可执行文件。完整组会话术、命令和现场检查见 `docs/AlgorithmDemoGuide.md`。

```cpp
SimulationConfig config;
config.passengerRate = 0.2; // 全楼 人/仿真秒
SimulationWorker worker(config, 42, DispatcherExecutionMode::Parallel);
worker.Start();
const auto snapshot = worker.GetLatestSnapshot(); // 可能尚在初始化，先检查空指针
worker.Pause();
worker.Resume();
worker.Reset();
worker.Stop(); // 正常停止并 join
```

核心单元测试仍可直接构造 `Simulation`，设置 passengerRate=0 并通过 AddPassenger 注入确定请求。`RunCoreTests.cmd` 可指定 Dispatcher、Elevator、Simulation、Concurrency 或 All，第二个参数为 x64/x86；`RunDispatchPerformance.ps1` 生成候选评分性能表。测试源在 VS 中作为 Development 文件显示，不加入 MFC 可执行文件，以免产生第二个 main。

下一步可在不改变 UI/核心边界的前提下完善窗口缩放、视觉样式、Deferred 专门标识和正式动画。所有控件更新仍在主线程，不得把业务推进或可写核心对象移回 UI。当前无独立开关门时间、加减速、分区停车、峰值预测或全局最优保证；这些是明确的课程设计简化，不是已经实现的高级群控。

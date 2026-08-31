# 多电梯群控调度仿真系统

本项目是 Visual Studio C++ / MFC 多人协作课程设计，最终目标是在 L 层建筑中仿真 N 部载客电梯的群控调度。**当前仅完成正式编码前的工程准备，不是可运行的完整调度仿真。**

## 开发环境与启动

- Windows、Visual Studio 2022、MSVC v143、Windows SDK 10.0、MFC（动态库）、Unicode。
- C++17 + 标准 STL；不引入第三方库、数据库、网络服务或后台线程。
- 直接打开根目录 `ElevatorSimulation.sln`，不要另建重复 MFC 工程。
- 选择 Debug / x64，生成解决方案后按 F5 或 Ctrl+F5 启动。
- 解决方案的 x86 对应项目的 Win32；同时保留 Debug/Release × x64/x86 四种配置。
- `Core`、`Statistics` 的 `.cpp` 单独设置为“不使用预编译头”；MFC 文件继续使用现有 `pch.h`。
- 所有配置启用 `/utf-8`，新增 C++ 源文件为 UTF-8；原资源文件保留 UTF-16，避免破坏中文资源。

原项目是 **MFC 对话框应用**。入口为 `CElevatorSimulationApp::InitInstance()`，通过 `CElevatorSimulationDlg::DoModal()` 启动主对话框。App、对话框、资源与 PCH 都保留原物理位置；Visual Studio 中增加 `Core`、`Statistics`、`UI`、`Development` 筛选器。

当前启动后仅显示默认初始化快照：20 层、6 部电梯、0 秒、无乘客。保留原有“确定”“取消”和“关于”功能，暂未提供仿真控制按钮、定时器或动画。

## 目录与文件职责

```text
ElevatorSimulation/
├─ ElevatorSimulation.sln                 现有解决方案
├─ ElevatorSimulation/
│  ├─ Core/
│  │  ├─ CommonTypes.h                    唯一公共枚举、参数、快照
│  │  ├─ Passenger.h / Passenger.cpp       乘客基础模型
│  │  ├─ Floor.h / Floor.cpp               楼层与上下行 ID 等待队列
│  │  ├─ Elevator.h / Elevator.cpp         单梯状态、乘客 ID 与任务容器
│  │  ├─ Dispatcher.h / Dispatcher.cpp     ElevatorDispatcher 群控接口
│  │  └─ Simulation.h / Simulation.cpp     初始化、生命周期、时钟与快照
│  ├─ Statistics/
│  │  └─ Statistics.h / Statistics.cpp     统计结构归零与读取骨架
│  ├─ ElevatorSimulation.cpp / .h         原 App 入口（未修改）
│  ├─ ElevatorSimulationDlg.cpp / .h      原对话框，最小快照接入
│  ├─ ElevatorSimulation.vcxproj           编译项与各配置
│  ├─ ElevatorSimulation.vcxproj.filters   VS 虚拟分组
│  ├─ ElevatorSimulation.rc / Resource.h  原资源，增加初始化状态控件标识
│  ├─ pch.h / pch.cpp / framework.h        原 MFC 框架（未移动）
│  └─ res/                                原图标与资源
├─ Tests/
│  ├─ CoreSmokeTests.cpp                   初始化与公共接口行为验证
│  └─ RunCoreSmokeTests.cmd                使用已安装 MSVC，无需第二个工程
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
| Dispatcher | Common、只读 Elevator | 不画 UI、不推进时间、不改电梯 |
| Statistics | Common | 不反向依赖 Simulation 或 MFC |
| Simulation | Passenger、Floor、Elevator、Dispatcher、Statistics | 不访问具体对话框和控件 |
| MFC UI | Simulation 提供的控制接口与快照 | 不实现乘客生成、运行或群控算法 |

`UI → Simulation → {Floor, Passenger, Elevator, Dispatcher, Statistics}`；`Dispatcher → Elevator`。统计由 Simulation 组合，后续由其传入事件，因此无需让 Statistics 反向包含 Simulation，不形成循环依赖。

## 唯一公共契约

`ElevatorSimulation/Core/CommonTypes.h` 统一定义：

- `Direction`：Down=-1、Idle=0、Up=1。
- `PassengerState`：Waiting、Riding、Arrived。
- `ElevatorState`：Idle、MovingUp、MovingDown、Boarding、Alighting、Stopped。
- `SimulationState`：Uninitialized、Ready、Running、Paused、Finished，用于总控制器生命周期。
- `SimulationConfig`：L/N/K、S 秒/层、T 秒/人、总时长、乘客速率预留值、仿真倍速。
- `PassengerId`、`InvalidElevatorId=-1`、`UnsetTime=-1.0`。
- `ElevatorSnapshot`、`FloorSnapshot`、`StatisticsSnapshot`、`ElevatorStatisticsSnapshot`。
- 无状态函数 `GetDirection(startFloor, targetFloor)`；同层返回 Idle，但同层起终点的乘客仍属非法。

禁止在其他文件重新定义 `ElevatorDirection`、`MoveDirection`、`PassengerStatus`、`ElevatorStatus`、`Config`、`SimulationParameter` 等语义重复类型。修改公共契约前，必须检查全部调用点并告知受影响的模块负责人。

Snapshot 按值返回：UI 可以读取或修改自己的副本，但无法通过它修改仿真内部对象。UI 不获得 Floor/Elevator/Passenger 的可写指针、引用或容器；`GetConfig()` 同样返回副本。

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

`Simulation::m_passengers` 是 `unordered_map<PassengerId, Passenger>`，**唯一按值拥有乘客对象**。Floor 的两个 `deque` 和 Elevator 的 `vector` 仅保存 ID，不拥有或释放乘客，也不长期缓存容器元素地址。后续同一轮仿真内 ID 不复用，由 D 统一分配；每次查找 ID 必须检查是否存在。到达时先记录统计、清除队列/轿厢内的 ID，再从活动乘客表删除对象。禁止多个模块保存同一对象并各自 delete。

## 已实现的接口行为

| 接口 | 当前行为 |
| --- | --- |
| `Initialize(config)` | 校验参数，建立 L 层/N 梯及统计槽位，清空活动乘客，时间归零，状态 Ready |
| `GetLastError()` | 获取最近初始化的诊断文字；成功初始化后清空 |
| `Start()` | 仅 Ready → Running，重复调用无效果 |
| `Pause()` | 仅 Running → Paused |
| `Resume()` | 仅 Paused → Running；不能代替首次 Start |
| `Reset()` | 用最近成功的配置重建，回到 Ready；未初始化时无操作 |
| `Update(deltaTime)` | 只在 Running 推进仿真时钟；到总时长转 Finished |
| `IsRunning()` / `IsFinished()` / `GetState()` | 查询生命周期 |
| `GetCurrentTime()` / `GetConfig()` | 查询时间与配置副本 |
| `GetElevatorSnapshots()` / `GetFloorSnapshots()` | 按电梯 ID / 楼层升序返回状态副本 |
| `GetStatisticsSnapshot()` | 返回统计副本；目前只有归零后的数据 |

初始化要求：`floorCount >= 2`，`elevatorCount > 0 && elevatorCount % 3 == 0`，`capacity > 0`，S/T/总时长/倍速均为**有限正数**，`passengerRate` 为**有限非负数**。拒绝 NaN、正负无穷。K=10~20、S=1~5、T=2~10 是背景中的典型范围，当前不将其作为硬性上限。零乘客速率允许使用，具体概率模型与速率单位仍待 A/D 确定。

参数不合法时 `Initialize` 返回 false，不抛参数异常，保留上一次有效参数、容器、时钟和运行状态。创建容器时的 `bad_alloc` / `length_error` 也转换为失败诊断，不故意设置未经约定的参数上限；极端规模仍受机器内存限制。`Reset` 为 void，如分配失败会保留旧状态，调用者可检查 `GetLastError()`。

Passenger/Floor/Elevator 的直接构造函数对基础非法字段抛出 `std::invalid_argument`；它们不知道建筑 L，楼层上界由 Simulation 在后续创建乘客、接收任务时检查。构造失败不会暴露半成品对象。

时间约定：所有模型时间单位为**仿真秒**；`Update` 的入参为**真实经过秒数**，内部乘一次 `simulationSpeed`。UI 不应再次乘倍速。非有限、零或负 deltaTime 被忽略；暂停不累积时间，结束后不能 Start/Resume 重启，需 Reset。后续 UI 恢复时应重设真实时间采样基点，不能把暂停时段作为下一帧 deltaTime。

当前仅有时钟推进，尚未编排仿真事件；到达总时长即结束，不自动延长到全部乘客送达。正式实现时应保证一个更新步内的事件不会发生在截止时间之后。

## 尚未实现与组员任务

代码中的 `TODO(A)`~`TODO(F)` 或联合标记说明模块负责人。未实现的 API 以 TODO 注释预留，没有只声明不定义、导致链接失败的假接口。

| 负责人 | 应主要修改的文件 | 下一步任务与边界 |
| --- | --- | --- |
| A | `Core/Passenger.*`、`Core/Floor.*` | 状态转换、请求数据创建、按方向入队/出队；与 D 约定 ID、时间、生成速率单位；不能提前在 UI 生成乘客 |
| B | `Core/Elevator.*` | 接受任务、单梯状态机、S/T 计时、容量、上下客与方向保持；不做群控选择 |
| C | `Core/Dispatcher.*` | 最终群控代价函数，先同向顺路、再空闲、最后其他忙碌电梯；只做选择 |
| D | `Core/Simulation.*` | 唯一乘客注册表/ID 分配、随机产生的编排、事件先后顺序、调用 B/C、完成后清理 |
| E | `ElevatorSimulationDlg.*`、必要资源 | L/N/K/S/T/总时长/速率输入、开始暂停继续重置、倍速、时间/楼层/电梯/等待人数/统计动态显示与后续动画 |
| F | `Statistics/Statistics.*`、`Tests/` | 生成/上梯/到达/移动事件统计、等待及乘梯时间、空载楼层数、算法评价与边界验证 |

当前 Dispatcher 对任何输入均返回 `InvalidElevatorId`，**没有实际分配请求**。Passenger 仅有构造与读取，Floor 仅有构造和等待人数查询，Elevator 仅有初始状态和只读任务/乘客 ID 容器。Statistics 仅有数据归零与快照，没有实时统计。UI 仅为一次性初始化视图，显示“停止”是对初始化状态的说明，后续动态显示须按快照枚举映射。

正式开发必须保留的业务约束：新请求不强迫忙碌电梯反向；先前接受的任务决定方向，当前方向任务完成后方可停止/反向；上下行分别顺路服务；先下后上等具体事件顺序由 B/D 协商；任何时刻不能超过 K；部分上客后其余乘客继续留队，不能清除仍有效的楼层请求。最终系统必须统一群控，不能变成 N 部互相独立响应请求的电梯。

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
```

测试脚本通过 vswhere 使用现有 MSVC x64，以 C++17、`/W4 /WX` 独立编译 Core/Statistics，不包含或链接 MFC，也不增加第二个 VS 工程。输出位于 `build/core-tests/`。它验证偶数/奇数/最低楼层初始化、三组分配、非法参数（含 NaN/inf）、失败保留旧状态、快照副本隔离、暂停/继续/重置、倍速及截止时间、乘客基本约束、调度占位返回值。尚不验证未来算法的正确性。退出码非零表示编译或验证失败。

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

本机编译与测试日志在 `build/verification/`，不纳入源码交付。当前验证覆盖初始化契约和 MFC 启动，不代表群控或电梯状态机已完成。

## 建议实施顺序

先安排 **A 的 Passenger/Floor** 与 **D 的最小事件接口协商**：明确 ID、队列操作、状态时间戳和乘客生成的单位，给上下客提供稳定数据基础。之后 B 实现单梯状态机并由 F 验证方向/容量/耗时约束，再由 C 接入统一群控，D 集成完整推进流程，E 基于快照完善 UI，F 接入正式统计。当前不提前代写各组员的核心算法。

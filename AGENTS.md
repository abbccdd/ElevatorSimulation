# 项目开发规则

这是 Windows / Visual Studio 2022 / C++17 / MFC 的“多电梯群控调度仿真系统”，当前阶段为多人协作的工程准备。

## 必须遵守

- 优先保证 `ElevatorSimulation.sln` 可编译，在现有唯一工程内增量开发。
- 核心逻辑与 MFC UI 解耦；Core/Statistics 不包含 `pch.h`、`framework.h`、`afx*.h` 或具体窗口头，不访问 MFC 控件。
- `ElevatorSimulation/Core/CommonTypes.h` 是唯一公共定义。禁止重新定义 Direction/PassengerState/ElevatorState/SimulationConfig 及其语义重复类型。
- 楼层编号为 1~L，禁止混用 0 起始楼层编号。电梯容器下标和 id 为 0~N-1，UI 显示 E1~EN。
- 初始化 N 为 3 的正倍数，前 N/3 位于 1 层、中 N/3 位于 L 层、后 N/3 位于 `(L+1)/2`，初始空梯/Idle。
- Simulation 唯一拥有乘客对象，Floor/Elevator 仅存 PassengerId；禁止重复释放、长期缓存易失效元素地址或用全局变量保存仿真。
- UI 只调用 Simulation 公开控制接口、读取快照副本。禁止可写容器出口、访问 private 数据或在 Dialog 内实现业务算法。
- 每次修改尽量限于负责模块。公共接口修改前检查所有调用位置、更新 README，并编译所有受影响配置。
- 不擅自大规模重构 MFC 自动生成代码；App、资源、PCH、framework 文件保留原位置和有效功能。
- 新增核心 `.cpp` 要同时加入 `.vcxproj` / `.filters`，对该文件禁用 PCH；保留 MFC 文件的 PCH 设置。
- 新增 C++ 文件 UTF-8，各配置 `/utf-8`；原 `.rc` 保留 UTF-16，不随意改变资源编码。
- PascalCase 类名/函数，camelCase 局部变量，`m_` 成员；中文注释，RAII、STL、enum class、nullptr，新增代码避免裸 new/delete。
- 当前不要提前实现完整群控、单梯状态机、随机概率模型、正式动画；后续用户明确要求相应模块后再实施。
- 未实现部分必须明确 TODO，不能将占位返回值或零统计声称为完整功能。
- 基础验证：生成现有解决方案，运行 `Tests/RunCoreSmokeTests.cmd`；测试无需另建 VS 工程或安装库。

## 模块负责人

| 负责人 | 文件范围 |
| --- | --- |
| A | Core/Passenger.*、Core/Floor.* |
| B | Core/Elevator.* |
| C | Core/Dispatcher.*（类名 ElevatorDispatcher） |
| D | Core/Simulation.* |
| E | ElevatorSimulationDlg.* 与 UI 资源 |
| F | Statistics/*、Tests/* |

完整接口语义、所有权、时间单位和开发 TODO 见 README.md。

## Environment（保留用户提供的工具规则）

优先使用已有工具，不要重复安装。新增常用工具时更新本文件，记录名称、路径、用途。

| 工具 | 路径 / 地址 | 用途 |
| --- | --- | --- |
| Locale Emulator | `E:\Locale.Emulator.2.5.0.1\LEProc.exe` | 日文程序区域模拟 |
| ReverseTools | `C:\Users\20938\AppData\Local\Programs\ReverseTools` | 游戏逆向、资源分析和解包 |
| DirectXTex texconv | `C:\Users\20938\AppData\Local\Programs\ReverseTools\DirectXTex\texconv.exe` | DDS / 3Dmigoto 纹理转换 |
| Bandizip | 已安装，用户未指定路径 | 解压缩 |
| Ollama | `http://127.0.0.1:11434` | 本地模型服务 |
| Visual Studio 2022 Community | `C:\Program Files\Microsoft Visual Studio\2022\Community` | 本项目 MSVC / MFC / MSBuild 编译调试 |
| MSBuild | `C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe` | 生成现有解决方案 |
| Python 3.11 | `C:\Users\20938\AppData\Local\Programs\Python\Python311\python.exe` | 辅助文件检查与脚本，非运行依赖 |
| VS Code / Conda | 已安装，用户未指定路径 | 编辑与开发辅助 |

ReverseTools 已包含 AssetStudio、GARbro、Kuriimu2、QuickBMS、arc_unpacker、Resource Hacker、dnSpy、x64dbg、DirectXTex/texconv。本次未安装新工具。

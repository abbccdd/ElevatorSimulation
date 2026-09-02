
// ElevatorSimulationDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "ElevatorSimulation.h"
#include "ElevatorSimulationDlg.h"
#include "afxdialogex.h"

#include <cerrno>
#include <cmath>
#include <cwchar>
#include <limits>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
	const wchar_t* DirectionText(Direction direction)
	{
		switch (direction)
		{
		case Direction::Up: return L"↑";
		case Direction::Down: return L"↓";
		default: return L"-";
		}
	}

	const wchar_t* ElevatorStateText(ElevatorState state)
	{
		switch (state)
		{
		case ElevatorState::MovingUp: return L"向上移动";
		case ElevatorState::MovingDown: return L"向下移动";
		case ElevatorState::Boarding: return L"上客";
		case ElevatorState::Alighting: return L"下客";
		case ElevatorState::Stopped: return L"停站";
		default: return L"空闲";
		}
	}

	const wchar_t* SimulationStateText(SimulationState state)
	{
		switch (state)
		{
		case SimulationState::Ready: return L"Ready / 就绪";
		case SimulationState::Running: return L"Running / 运行中";
		case SimulationState::Paused: return L"Paused / 已暂停";
		case SimulationState::Finished: return L"Finished / 已结束";
		default: return L"Error / 未初始化";
		}
	}

	CString Utf8ToCString(const std::string& text)
	{
		if (text.empty()) return CString();
		const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
			static_cast<int>(text.size()), nullptr, 0);
		if (length <= 0) return CString(L"未知错误");
		CString result;
		wchar_t* buffer = result.GetBuffer(length);
		MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), buffer, length);
		result.ReleaseBuffer(length);
		return result;
	}
}


// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CElevatorSimulationDlg 对话框



CElevatorSimulationDlg::CElevatorSimulationDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_ELEVATORSIMULATION_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CElevatorSimulationDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_ELEVATORS, m_elevatorList);
	DDX_Control(pDX, IDC_LIST_FLOORS, m_floorList);
	DDX_Control(pDX, IDC_LIST_HALL_CALLS, m_hallCallList);
}

BEGIN_MESSAGE_MAP(CElevatorSimulationDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_TIMER()
	ON_WM_DESTROY()
	ON_BN_CLICKED(IDC_BUTTON_START, &CElevatorSimulationDlg::OnBnClickedStart)
	ON_BN_CLICKED(IDC_BUTTON_PAUSE, &CElevatorSimulationDlg::OnBnClickedPause)
	ON_BN_CLICKED(IDC_BUTTON_RESUME, &CElevatorSimulationDlg::OnBnClickedResume)
	ON_BN_CLICKED(IDC_BUTTON_RESET, &CElevatorSimulationDlg::OnBnClickedReset)
END_MESSAGE_MAP()


// CElevatorSimulationDlg 消息处理程序

BOOL CElevatorSimulationDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	SetWindowTextW(L"多电梯群控调度仿真系统");
	InitializeListControls();
	SetDlgItemTextW(IDC_EDIT_FLOOR_COUNT, L"20");
	SetDlgItemTextW(IDC_EDIT_ELEVATOR_COUNT, L"6");
	SetDlgItemTextW(IDC_EDIT_CAPACITY, L"10");
	SetDlgItemTextW(IDC_EDIT_MOVE_TIME, L"2.0");
	SetDlgItemTextW(IDC_EDIT_PERSON_TIME, L"1.0");
	SetDlgItemTextW(IDC_EDIT_DURATION, L"300");
	SetDlgItemTextW(IDC_EDIT_PASSENGER_RATE, L"0.2");
	SetDlgItemTextW(IDC_EDIT_SPEED, L"1");
	SetDlgItemTextW(IDC_EDIT_SEED, L"42");

	SimulationConfig config;
	config.capacity = 10;
	config.personTime = 1.0;
	config.simulationDuration = 300.0;
	m_simulationWorker = std::make_unique<SimulationWorker>(config, 42,
		DispatcherExecutionMode::Parallel);
	if (SetTimer(SimulationTimerId, SimulationTimerIntervalMs, nullptr) == 0)
	{
		AfxMessageBox(L"无法创建 UI 刷新计时器。", MB_ICONERROR);
	}
	RefreshSimulationView();

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CElevatorSimulationDlg::InitializeListControls()
{
	const DWORD extendedStyle = LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER;
	m_elevatorList.SetExtendedStyle(m_elevatorList.GetExtendedStyle() | extendedStyle);
	m_floorList.SetExtendedStyle(m_floorList.GetExtendedStyle() | extendedStyle);
	m_hallCallList.SetExtendedStyle(m_hallCallList.GetExtendedStyle() | extendedStyle);

	m_elevatorList.InsertColumn(0, L"电梯", LVCFMT_LEFT, 52);
	m_elevatorList.InsertColumn(1, L"楼层", LVCFMT_RIGHT, 58);
	m_elevatorList.InsertColumn(2, L"方向", LVCFMT_CENTER, 52);
	m_elevatorList.InsertColumn(3, L"状态", LVCFMT_LEFT, 92);
	m_elevatorList.InsertColumn(4, L"载客", LVCFMT_RIGHT, 72);

	m_floorList.InsertColumn(0, L"楼层", LVCFMT_RIGHT, 62);
	m_floorList.InsertColumn(1, L"上行等待", LVCFMT_RIGHT, 82);
	m_floorList.InsertColumn(2, L"下行等待", LVCFMT_RIGHT, 82);

	m_hallCallList.InsertColumn(0, L"楼层", LVCFMT_RIGHT, 58);
	m_hallCallList.InsertColumn(1, L"方向", LVCFMT_CENTER, 52);
	m_hallCallList.InsertColumn(2, L"等待", LVCFMT_RIGHT, 65);
	m_hallCallList.InsertColumn(3, L"归属", LVCFMT_LEFT, 88);
}

bool CElevatorSimulationDlg::ReadIntControl(int controlId, const wchar_t* fieldName, int& value)
{
	CString text;
	GetDlgItemTextW(controlId, text);
	text.Trim();
	errno = 0;
	wchar_t* end = nullptr;
	const long parsed = std::wcstol(text.GetString(), &end, 10);
	if (text.IsEmpty() || end == text.GetString() || *end != L'\0' || errno == ERANGE ||
		parsed < (std::numeric_limits<int>::min)() || parsed > (std::numeric_limits<int>::max)())
	{
		CString message;
		message.Format(L"%s 必须是有效整数。", fieldName);
		ShowInputError(message);
		return false;
	}
	value = static_cast<int>(parsed);
	return true;
}

bool CElevatorSimulationDlg::ReadDoubleControl(int controlId, const wchar_t* fieldName, double& value)
{
	CString text;
	GetDlgItemTextW(controlId, text);
	text.Trim();
	errno = 0;
	wchar_t* end = nullptr;
	const double parsed = std::wcstod(text.GetString(), &end);
	if (text.IsEmpty() || end == text.GetString() || *end != L'\0' || errno == ERANGE)
	{
		CString message;
		message.Format(L"%s 必须是有效数字。", fieldName);
		ShowInputError(message);
		return false;
	}
	value = parsed;
	return true;
}

bool CElevatorSimulationDlg::ReadConfiguration(SimulationConfig& config, std::uint32_t& seed)
{
	if (!ReadIntControl(IDC_EDIT_FLOOR_COUNT, L"楼层数 L", config.floorCount) ||
		!ReadIntControl(IDC_EDIT_ELEVATOR_COUNT, L"电梯数 N", config.elevatorCount) ||
		!ReadIntControl(IDC_EDIT_CAPACITY, L"容量 K", config.capacity) ||
		!ReadDoubleControl(IDC_EDIT_MOVE_TIME, L"每层运行时间 S", config.moveTimePerFloor) ||
		!ReadDoubleControl(IDC_EDIT_PERSON_TIME, L"每人上下客时间 T", config.personTime) ||
		!ReadDoubleControl(IDC_EDIT_DURATION, L"仿真总时长", config.simulationDuration) ||
		!ReadDoubleControl(IDC_EDIT_PASSENGER_RATE, L"乘客产生率", config.passengerRate) ||
		!ReadDoubleControl(IDC_EDIT_SPEED, L"仿真倍速", config.simulationSpeed))
	{
		return false;
	}

	CString seedText;
	GetDlgItemTextW(IDC_EDIT_SEED, seedText);
	seedText.Trim();
	errno = 0;
	wchar_t* end = nullptr;
	const unsigned long long parsedSeed = std::wcstoull(seedText.GetString(), &end, 10);
	if (seedText.IsEmpty() || end == seedText.GetString() || *end != L'\0' || errno == ERANGE ||
		parsedSeed > (std::numeric_limits<std::uint32_t>::max)())
	{
		ShowInputError(L"随机种子 seed 必须是 0~4294967295 的整数。");
		return false;
	}
	seed = static_cast<std::uint32_t>(parsedSeed);
	return true;
}

void CElevatorSimulationDlg::ShowInputError(const CString& message)
{
	SetDlgItemTextW(IDC_SIMULATION_STATE, L"Error / 参数无效");
	AfxMessageBox(message, MB_ICONWARNING);
}

void CElevatorSimulationDlg::UpdateControlStates(
	const std::shared_ptr<const SimulationUISnapshot>& snapshot)
{
	const SimulationState state = snapshot ? snapshot->state : SimulationState::Uninitialized;
	const bool active = snapshot && snapshot->workerActive;
	const bool ready = state == SimulationState::Ready || state == SimulationState::Uninitialized;
	GetDlgItem(IDC_BUTTON_START)->EnableWindow(active && ready);
	GetDlgItem(IDC_BUTTON_PAUSE)->EnableWindow(active && state == SimulationState::Running);
	GetDlgItem(IDC_BUTTON_RESUME)->EnableWindow(active && state == SimulationState::Paused);
	GetDlgItem(IDC_BUTTON_RESET)->EnableWindow(active && state != SimulationState::Uninitialized);

	for (int controlId : { IDC_EDIT_FLOOR_COUNT, IDC_EDIT_ELEVATOR_COUNT, IDC_EDIT_CAPACITY,
		IDC_EDIT_MOVE_TIME, IDC_EDIT_PERSON_TIME, IDC_EDIT_DURATION, IDC_EDIT_PASSENGER_RATE,
		IDC_EDIT_SPEED, IDC_EDIT_SEED })
	{
		GetDlgItem(controlId)->EnableWindow(active && ready);
	}
}

void CElevatorSimulationDlg::RefreshSimulationView()
{
	const auto snapshot = m_simulationWorker ? m_simulationWorker->GetLatestSnapshot() : nullptr;
	if (!snapshot)
	{
		SetDlgItemTextW(IDC_SIMULATION_STATE, L"Initializing / 正在初始化");
		UpdateControlStates(snapshot);
		return;
	}
	const auto& elevators = snapshot->elevators;
	const auto& floors = snapshot->floors;
	const auto& statistics = snapshot->statistics;
	const auto& hallCalls = snapshot->hallCalls;
	const auto& config = snapshot->config;
	CString stateText = snapshot->workerActive ? SimulationStateText(snapshot->state) : L"Stopped / 已停止";
	if (snapshot->state == SimulationState::Uninitialized && !snapshot->lastError.empty())
		stateText = L"Error / " + Utf8ToCString(snapshot->lastError);
	SetDlgItemTextW(IDC_SIMULATION_STATE, stateText);
	CString modelTime;
	modelTime.Format(L"Model Time: %.1f / %.1f s", snapshot->currentTime, config.simulationDuration);
	SetDlgItemTextW(IDC_MODEL_TIME, modelTime);

	if (m_elevatorList.GetItemCount() != static_cast<int>(elevators.size()))
	{
		m_elevatorList.DeleteAllItems();
		for (std::size_t index = 0; index < elevators.size(); ++index)
			m_elevatorList.InsertItem(static_cast<int>(index), L"");
	}
	for (std::size_t index = 0; index < elevators.size(); ++index)
	{
		const auto& elevator = elevators[index];
		const int row = static_cast<int>(index);
		CString value;
		value.Format(L"E%d", elevator.id + 1);
		m_elevatorList.SetItemText(row, 0, value);
		value.Format(L"%dF", elevator.currentFloor);
		m_elevatorList.SetItemText(row, 1, value);
		m_elevatorList.SetItemText(row, 2, DirectionText(elevator.direction));
		m_elevatorList.SetItemText(row, 3, ElevatorStateText(elevator.state));
		value.Format(L"%d / %d", elevator.passengerCount, elevator.capacity);
		m_elevatorList.SetItemText(row, 4, value);
	}

	if (m_floorList.GetItemCount() != static_cast<int>(floors.size()))
	{
		m_floorList.DeleteAllItems();
		for (std::size_t index = 0; index < floors.size(); ++index)
			m_floorList.InsertItem(static_cast<int>(index), L"");
	}
	for (std::size_t index = 0; index < floors.size(); ++index)
	{
		const auto& floor = floors[floors.size() - 1 - index];
		const int row = static_cast<int>(index);
		CString value;
		value.Format(L"%dF", floor.floorNumber);
		m_floorList.SetItemText(row, 0, value);
		value.Format(L"%zu", floor.upWaitingCount);
		m_floorList.SetItemText(row, 1, value);
		value.Format(L"%zu", floor.downWaitingCount);
		m_floorList.SetItemText(row, 2, value);
	}

	m_hallCallList.SetRedraw(FALSE);
	m_hallCallList.DeleteAllItems();
	for (std::size_t index = 0; index < hallCalls.size(); ++index)
	{
		const auto& call = hallCalls[index];
		const int row = static_cast<int>(index);
		CString value;
		value.Format(L"%dF", call.floorNumber);
		m_hallCallList.InsertItem(row, value);
		m_hallCallList.SetItemText(row, 1, DirectionText(call.direction));
		value.Format(L"%zu", call.waitingCount);
		m_hallCallList.SetItemText(row, 2, value);
		if (call.assignedElevatorId == InvalidElevatorId)
			value = L"未分配";
		else
			value.Format(L"E%d", call.assignedElevatorId + 1);
		m_hallCallList.SetItemText(row, 3, value);
	}
	m_hallCallList.SetRedraw(TRUE);
	m_hallCallList.Invalidate(FALSE);

	CString summary;
	summary.Format(L"生成: %zu    等待: %zu    乘梯: %zu    到达: %zu    "
		L"平均等待: %.2f s    平均乘梯: %.2f s    最大等待: %.2f s",
		statistics.totalPassengerCount, statistics.waitingCount, statistics.ridingCount,
		statistics.arrivedCount, statistics.averageWaitingTime, statistics.averageRideTime,
		statistics.maxWaitingTime);
	SetDlgItemTextW(IDC_STATISTICS, summary);
	UpdateControlStates(snapshot);
}

void CElevatorSimulationDlg::OnBnClickedStart()
{
	SimulationConfig config;
	std::uint32_t seed = 0;
	if (!ReadConfiguration(config, seed)) return;
	if (m_simulationWorker) m_simulationWorker->Stop();
	m_simulationWorker = std::make_unique<SimulationWorker>(config, seed,
		DispatcherExecutionMode::Parallel);
	m_simulationWorker->Start();
	RefreshSimulationView();
}

void CElevatorSimulationDlg::OnBnClickedPause()
{
	if (m_simulationWorker) m_simulationWorker->Pause();
	RefreshSimulationView();
}

void CElevatorSimulationDlg::OnBnClickedResume()
{
	if (m_simulationWorker) m_simulationWorker->Resume();
	RefreshSimulationView();
}

void CElevatorSimulationDlg::OnBnClickedReset()
{
	if (m_simulationWorker) m_simulationWorker->Reset();
	RefreshSimulationView();
}

void CElevatorSimulationDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == SimulationTimerId)
		RefreshSimulationView();
	CDialogEx::OnTimer(nIDEvent);
}

void CElevatorSimulationDlg::OnDestroy()
{
	KillTimer(SimulationTimerId);
	if (m_simulationWorker)
	{
		m_simulationWorker->Stop();
		m_simulationWorker.reset();
	}
	CDialogEx::OnDestroy();
}

void CElevatorSimulationDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CElevatorSimulationDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CElevatorSimulationDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


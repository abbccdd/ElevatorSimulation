
// ElevatorSimulationDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "ElevatorSimulation.h"
#include "ElevatorSimulationDlg.h"
#include "afxdialogex.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cwchar>
#include <limits>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
	constexpr int ParameterControlIds[] = {
		IDC_EDIT_FLOOR_COUNT, IDC_EDIT_ELEVATOR_COUNT, IDC_EDIT_CAPACITY,
		IDC_EDIT_MOVE_TIME, IDC_EDIT_PERSON_TIME, IDC_EDIT_PASSENGER_RATE,
		IDC_COMBO_TRAFFIC_SCENARIO, IDC_COMBO_TRAFFIC_PATTERN,
		IDC_EDIT_DURATION, IDC_EDIT_SEED, IDC_EDIT_SPEED
	};

	constexpr int ParameterLabelIds[] = {
		IDC_PARAMETER_LABEL_FIRST, IDC_PARAMETER_LABEL_FIRST + 1,
		IDC_PARAMETER_LABEL_FIRST + 2, IDC_PARAMETER_LABEL_FIRST + 3,
		IDC_PARAMETER_LABEL_FIRST + 4, IDC_PARAMETER_LABEL_FIRST + 5,
		IDC_PARAMETER_LABEL_SCENARIO, IDC_PARAMETER_LABEL_FIRST + 6,
		IDC_PARAMETER_LABEL_FIRST + 7, IDC_PARAMETER_LABEL_FIRST + 8,
		IDC_PARAMETER_LABEL_FIRST + 9
	};

	constexpr const wchar_t* ParameterLabels[] = {
		L"楼层数 L", L"电梯数量 N", L"容量 K", L"每层时间 S (s)",
		L"上下客时间 T (s)", L"客流率 (人/仿真秒)", L"客流场景", L"客流模式",
		L"总时长 (s)", L"随机种子 seed", L"仿真倍速"
	};

	constexpr const wchar_t* StatisticTitles[] = {
		L"Generated", L"Waiting", L"Riding", L"Arrived", L"Average Wait", L"Max Wait"
	};

	const wchar_t* DirectionText(Direction direction)
	{
		switch (direction)
		{
		case Direction::Up: return L"↑";
		case Direction::Down: return L"↓";
		default: return L"Idle";
		}
	}

	const wchar_t* ElevatorStateText(ElevatorState state)
	{
		switch (state)
		{
		case ElevatorState::MovingUp: return L"MovingUp";
		case ElevatorState::MovingDown: return L"MovingDown";
		case ElevatorState::Boarding: return L"Boarding";
		case ElevatorState::Alighting: return L"Alighting";
		case ElevatorState::Stopped: return L"Stopped";
		default: return L"Idle";
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

	const wchar_t* TrafficPatternText(TrafficPattern pattern)
	{
		switch (pattern)
		{
		case TrafficPattern::UpPeak: return L"上行高峰";
		case TrafficPattern::DownPeak: return L"下行高峰";
		case TrafficPattern::InterFloor: return L"层间交通";
		default: return L"均匀随机";
		}
	}

	const wchar_t* OfficePhaseText(std::size_t phaseIndex)
	{
		if (phaseIndex == 0) return L"早高峰";
		if (phaseIndex == 1) return L"日间层间";
		return L"晚高峰";
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

CElevatorSimulationDlg::~CElevatorSimulationDlg()
{
	if (m_simulationWorker) m_simulationWorker->Stop();
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
	ON_WM_SIZE()
	ON_WM_GETMINMAXINFO()
	ON_BN_CLICKED(IDC_BUTTON_START, &CElevatorSimulationDlg::OnBnClickedStart)
	ON_BN_CLICKED(IDC_BUTTON_PAUSE, &CElevatorSimulationDlg::OnBnClickedPause)
	ON_BN_CLICKED(IDC_BUTTON_RESUME, &CElevatorSimulationDlg::OnBnClickedResume)
	ON_BN_CLICKED(IDC_BUTTON_RESET, &CElevatorSimulationDlg::OnBnClickedReset)
	ON_BN_CLICKED(IDC_BUTTON_SPEED_1, &CElevatorSimulationDlg::OnBnClickedSpeed1)
	ON_BN_CLICKED(IDC_BUTTON_SPEED_2, &CElevatorSimulationDlg::OnBnClickedSpeed2)
	ON_BN_CLICKED(IDC_BUTTON_SPEED_5, &CElevatorSimulationDlg::OnBnClickedSpeed5)
	ON_BN_CLICKED(IDC_BUTTON_SPEED_10, &CElevatorSimulationDlg::OnBnClickedSpeed10)
	ON_CBN_SELCHANGE(IDC_COMBO_TRAFFIC_SCENARIO,
		&CElevatorSimulationDlg::OnCbnSelchangeTrafficScenario)
	ON_BN_CLICKED(IDC_BUTTON_PANEL_TOGGLE, &CElevatorSimulationDlg::OnBnClickedPanelToggle)
	ON_NOTIFY(TCN_SELCHANGE, IDC_TAB_PAGES, &CElevatorSimulationDlg::OnTcnSelchangePages)
	ON_NOTIFY(TCN_SELCHANGE, IDC_TAB_RIGHT, &CElevatorSimulationDlg::OnTcnSelchangeRightTabs)
	ON_NOTIFY(NM_CLICK, IDC_LIST_HALL_CALLS, &CElevatorSimulationDlg::OnNMClickHallCallList)
	ON_MESSAGE(WM_ELEVATOR_SELECTION_CHANGED,
		&CElevatorSimulationDlg::OnElevatorSelectionChanged)
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
	ModifyStyle(0, WS_THICKFRAME | WS_MAXIMIZEBOX);
	const UINT dpi = GetDpiForWindow(m_hWnd);
	SetWindowPos(nullptr, 0, 0, MulDiv(1280, dpi, 96), MulDiv(780, dpi, 96),
		SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
	CenterWindow();
	for (CWnd* child = GetWindow(GW_CHILD); child != nullptr; child = child->GetNextWindow())
		child->ShowWindow(SW_HIDE);
	CreateUIFramework();
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
	m_uiReady = true;
	UpdateSpeedDisplay(1.0);
	UpdateTabPageVisibility();

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
	RefreshSimulationView(true);

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CElevatorSimulationDlg::CreateUIFramework()
{
	LOGFONT baseFont{};
	GetFont()->GetLogFont(&baseFont);
	LOGFONT titleFont = baseFont;
	titleFont.lfHeight = baseFont.lfHeight * 2;
	titleFont.lfWeight = FW_BOLD;
	m_titleFont.CreateFontIndirect(&titleFont);
	LOGFONT sectionFont = baseFont;
	sectionFont.lfHeight = baseFont.lfHeight * 3 / 2;
	sectionFont.lfWeight = FW_BOLD;
	m_sectionFont.CreateFontIndirect(&sectionFont);
	LOGFONT valueFont = baseFont;
	valueFont.lfHeight = baseFont.lfHeight * 7 / 4;
	valueFont.lfWeight = FW_SEMIBOLD;
	m_statValueFont.CreateFontIndirect(&valueFont);

	const DWORD labelStyle = WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE;
	m_headerTitle.Create(L"多电梯群控调度仿真系统", labelStyle, CRect(), this, IDC_HEADER_TITLE);
	m_headerTitle.SetFont(&m_titleFont);
	m_headerStateLabel.Create(L"运行状态：", labelStyle, CRect(), this, IDC_HEADER_STATE_LABEL);
	m_headerTimeLabel.Create(L"模型时间：", labelStyle, CRect(), this, IDC_HEADER_TIME_LABEL);
	m_headerSpeedLabel.Create(L"仿真倍速：", labelStyle, CRect(), this, IDC_HEADER_SPEED_LABEL);
	m_headerSpeed.Create(L"x1", labelStyle, CRect(), this, IDC_HEADER_SPEED);
	m_headerTraffic.Create(L"场景：固定模式 · 当前模式：均匀随机",
		labelStyle, CRect(), this, IDC_HEADER_TRAFFIC);

	m_leftPanel.Create(L"参数与控制", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
		CRect(), this, IDC_PANEL_LEFT);
	m_mainPanel.Create(L"实时电梯群控主视图", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
		CRect(), this, IDC_PANEL_MAIN);
	m_rightPanel.Create(L"信息侧栏", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
		CRect(), this, IDC_PANEL_RIGHT);
	m_panelToggle.Create(L"<<", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		CRect(), this, IDC_BUTTON_PANEL_TOGGLE);
	m_buildingView.Create(&m_mainPanel, IDC_BUILDING_VIEW);
	m_buildingView.SetFont(GetFont());

	m_parameterSection.Create(L"参数输入", labelStyle, CRect(), this, IDC_SECTION_PARAMETERS);
	m_controlSection.Create(L"仿真控制", labelStyle, CRect(), this, IDC_SECTION_CONTROLS);
	m_speedSection.Create(L"快捷倍速", labelStyle, CRect(), this, IDC_SECTION_SPEED);
	m_parameterSection.SetFont(&m_sectionFont);
	m_controlSection.SetFont(&m_sectionFont);
	m_speedSection.SetFont(&m_sectionFont);
	m_trafficScenarioCombo.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
		CBS_DROPDOWNLIST, CRect(), this, IDC_COMBO_TRAFFIC_SCENARIO);
	for (const wchar_t* item : { L"固定模式", L"办公楼日周期" })
		m_trafficScenarioCombo.AddString(item);
	m_trafficScenarioCombo.SetCurSel(0);
	m_trafficPatternCombo.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
		CBS_DROPDOWNLIST, CRect(), this, IDC_COMBO_TRAFFIC_PATTERN);
	for (const wchar_t* item : { L"均匀随机", L"上行高峰", L"下行高峰", L"层间交通" })
		m_trafficPatternCombo.AddString(item);
	m_trafficPatternCombo.SetCurSel(0);

	for (std::size_t index = 0; index < m_parameterLabels.size(); ++index)
	{
		m_parameterLabels[index].Create(ParameterLabels[index], labelStyle, CRect(), this,
			ParameterLabelIds[index]);
	}

	constexpr const wchar_t* SpeedLabels[] = { L"x1", L"x2", L"x5", L"x10" };
	for (std::size_t index = 0; index < m_speedButtons.size(); ++index)
	{
		m_speedButtons[index].Create(SpeedLabels[index],
			WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, CRect(), this,
			IDC_BUTTON_SPEED_1 + static_cast<UINT>(index));
	}

	m_rightTabs.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | TCS_TABS | TCS_SINGLELINE,
		CRect(), this, IDC_TAB_RIGHT);
	m_rightTabs.InsertItem(0, L"Hall Call");
	m_rightTabs.InsertItem(1, L"电梯详情");
	m_rightTabs.InsertItem(2, L"算法观察");
	m_rightTabs.SetCurSel(0);
	m_elevatorDetailTitle.Create(L"未选择电梯", labelStyle, CRect(), this,
		IDC_RIGHT_ELEVATOR_TITLE);
	m_elevatorDetailTitle.SetFont(&m_sectionFont);
	m_elevatorDetailBody.Create(L"请在中央视图选择一台电梯",
		WS_CHILD | WS_VISIBLE | SS_LEFT, CRect(), this, IDC_RIGHT_ELEVATOR_DETAILS);
	m_algorithmPlaceholder.Create(L"请在 Hall Call 页选择一个请求",
		WS_CHILD | WS_VISIBLE | WS_BORDER | SS_LEFT,
		CRect(), this, IDC_RIGHT_ALGORITHM_PLACEHOLDER);

	m_pageTabs.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | TCS_TABS | TCS_SINGLELINE,
		CRect(), this, IDC_TAB_PAGES);
	m_pageTabs.InsertItem(0, L"实时监控");
	m_pageTabs.InsertItem(1, L"统计分析");
	m_pageTabs.InsertItem(2, L"算法观察");
	m_pageTabs.SetCurSel(0);
	m_pagePlaceholder.Create(L"", WS_CHILD | WS_BORDER | SS_CENTER | SS_CENTERIMAGE,
		CRect(), this, IDC_PAGE_PLACEHOLDER);
	m_statisticsTrendView.Create(this, IDC_STATISTICS_TREND_VIEW);
	m_statisticsTrendView.SetFont(GetFont());
	m_algorithmPageSummary.Create(L"请在实时监控页的 Hall Call 列表中选择一个请求",
		WS_CHILD | WS_BORDER | SS_LEFT | SS_CENTERIMAGE, CRect(), this,
		IDC_ALGORITHM_PAGE_SUMMARY);
	m_algorithmCandidateList.Create(WS_CHILD | WS_BORDER | WS_TABSTOP | LVS_REPORT |
		LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER, CRect(), this,
		IDC_LIST_ALGORITHM_CANDIDATES);

	for (std::size_t index = 0; index < m_statCards.size(); ++index)
	{
		m_statCards[index].Create(L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
			CRect(), this, IDC_STAT_CARD_FIRST + static_cast<UINT>(index));
		m_statTitles[index].Create(StatisticTitles[index],
			WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE, CRect(), this,
			IDC_STAT_TITLE_FIRST + static_cast<UINT>(index));
		m_statValues[index].Create(index >= 4 ? L"0.00 s" : L"0",
			WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE, CRect(), this,
			IDC_STAT_VALUE_FIRST + static_cast<UINT>(index));
		m_statValues[index].SetFont(&m_statValueFont);
	}

	for (int controlId : ParameterControlIds)
		GetDlgItem(controlId)->ShowWindow(SW_SHOW);
	for (int controlId : { IDC_BUTTON_START, IDC_BUTTON_PAUSE, IDC_BUTTON_RESUME,
		IDC_BUTTON_RESET, IDC_SIMULATION_STATE, IDC_MODEL_TIME,
		IDC_LIST_HALL_CALLS })
	{
		GetDlgItem(controlId)->ShowWindow(SW_SHOW);
	}
	m_elevatorList.ShowWindow(SW_HIDE);
	m_floorList.ShowWindow(SW_HIDE);
}

void CElevatorSimulationDlg::RelayoutUI()
{
	if (!m_uiReady) return;

	CRect client;
	GetClientRect(&client);
	const UINT dpi = GetDpiForWindow(m_hWnd);
	auto toDevice = [dpi](int value) { return MulDiv(value, dpi, 96); };
	const int clientWidth = MulDiv(client.Width(), 96, dpi);
	const int clientHeight = MulDiv(client.Height(), 96, dpi);
	const int margin = 12;
	const int gap = 8;
	const int headerHeight = 66;
	const int leftWidth = 220;
	const int tabsHeight = 31;
	const int statsHeight = 94;
	const int contentTop = headerHeight + gap;
	const int contentBottom = clientHeight - margin;
	const int mainBottom = contentBottom - tabsHeight - statsHeight - gap * 2;
	const int centerX = margin + leftWidth + gap;
	const bool realTimePage = m_pageTabs.GetCurSel() == 0;
	const int rightWidth = m_rightPanelExpanded ? 280 : 46;
	const int rightX = clientWidth - margin - rightWidth;
	const int centerRight = realTimePage ? rightX - gap : clientWidth - margin;
	const int centerWidth = centerRight - centerX;
	const int mainHeight = mainBottom - contentTop;
	auto place = [&toDevice](CWnd& control, int x, int y, int width, int height)
	{
		control.MoveWindow(toDevice(x), toDevice(y), toDevice(width), toDevice(height), TRUE);
	};
	auto move = [this, &toDevice](int controlId, int x, int y, int width, int height)
	{
		GetDlgItem(controlId)->MoveWindow(toDevice(x), toDevice(y),
			toDevice(width), toDevice(height), TRUE);
	};

	place(m_headerTitle, margin + 4, 8, 350, 40);
	const int headerInfoX = (std::max)(380, centerX);
	const int headerInfoWidth = clientWidth - margin - headerInfoX;
	const int headerPart = headerInfoWidth / 3;
	place(m_headerStateLabel, headerInfoX, 5, 76, 28);
	move(IDC_SIMULATION_STATE, headerInfoX + 76, 5, headerPart - 76, 28);
	place(m_headerTimeLabel, headerInfoX + headerPart, 5, 76, 28);
	move(IDC_MODEL_TIME, headerInfoX + headerPart + 76, 5, headerPart - 76, 28);
	place(m_headerSpeedLabel, headerInfoX + headerPart * 2, 5, 76, 28);
	place(m_headerSpeed, headerInfoX + headerPart * 2 + 76, 5,
		headerInfoWidth - headerPart * 2 - 76, 28);
	place(m_headerTraffic, headerInfoX, 34, headerInfoWidth, 24);

	place(m_leftPanel, margin, contentTop, leftWidth, contentBottom - contentTop);
	const int leftInnerX = margin + 14;
	const int labelWidth = 110;
	const int editX = leftInnerX + labelWidth;
	const int editWidth = leftWidth - 28 - labelWidth;
	place(m_parameterSection, leftInnerX, contentTop + 20, leftWidth - 28, 22);
	const int firstRowY = contentTop + 46;
	const int rowHeight = 29;
	for (std::size_t index = 0; index < m_parameterLabels.size(); ++index)
	{
		const int rowY = firstRowY + static_cast<int>(index) * rowHeight;
		place(m_parameterLabels[index], leftInnerX, rowY, labelWidth - 6, 22);
		const int controlId = ParameterControlIds[index];
		move(controlId, editX, rowY, editWidth,
			controlId == IDC_COMBO_TRAFFIC_SCENARIO ||
			controlId == IDC_COMBO_TRAFFIC_PATTERN ? 120 : 22);
	}

	const int controlsY = firstRowY + static_cast<int>(m_parameterLabels.size()) * rowHeight + 8;
	place(m_controlSection, leftInnerX, controlsY, leftWidth - 28, 22);
	const int actionY = controlsY + 28;
	const int actionWidth = (leftWidth - 36) / 2;
	move(IDC_BUTTON_START, leftInnerX, actionY, actionWidth, 34);
	move(IDC_BUTTON_PAUSE, leftInnerX + actionWidth + 8, actionY, actionWidth, 34);
	move(IDC_BUTTON_RESUME, leftInnerX, actionY + 42, actionWidth, 34);
	move(IDC_BUTTON_RESET, leftInnerX + actionWidth + 8, actionY + 42, actionWidth, 34);

	const int speedY = actionY + 88;
	place(m_speedSection, leftInnerX, speedY, leftWidth - 28, 22);
	const int speedButtonY = speedY + 27;
	const int speedButtonWidth = (leftWidth - 52) / 4;
	for (std::size_t index = 0; index < m_speedButtons.size(); ++index)
	{
		place(m_speedButtons[index], leftInnerX +
			static_cast<int>(index) * (speedButtonWidth + 4), speedButtonY,
			speedButtonWidth, 32);
	}

	if (realTimePage)
	{
		place(m_mainPanel, centerX, contentTop, centerWidth, mainHeight);
		place(m_buildingView, 10, 22, centerWidth - 20, mainHeight - 32);

		place(m_rightPanel, rightX, contentTop, rightWidth, contentBottom - contentTop);
		place(m_panelToggle, rightX + rightWidth - 38, contentTop + 15, 30, 27);
		if (m_rightPanelExpanded)
		{
			place(m_rightTabs, rightX + 12, contentTop + 43, rightWidth - 24, 29);
			place(m_hallCallList, rightX + 12, contentTop + 78,
				rightWidth - 24, contentBottom - contentTop - 91);
			place(m_elevatorDetailTitle, rightX + 16, contentTop + 84,
				rightWidth - 32, 30);
			place(m_elevatorDetailBody, rightX + 16, contentTop + 124,
				rightWidth - 32, 130);
			place(m_algorithmPlaceholder, rightX + 12, contentTop + 78,
				rightWidth - 24, contentBottom - contentTop - 91);
		}
	}
	else
	{
		const int pageWidth = clientWidth - margin - centerX;
		if (m_pageTabs.GetCurSel() == 1)
		{
			place(m_statisticsTrendView, centerX, contentTop, pageWidth, mainHeight);
		}
		else
		{
			place(m_algorithmPageSummary, centerX, contentTop, pageWidth, 64);
			place(m_algorithmCandidateList, centerX, contentTop + 72,
				pageWidth, mainHeight - 72);
			if (m_algorithmCandidateList.GetHeaderCtrl() != nullptr)
			{
				m_algorithmCandidateList.SetColumnWidth(0, toDevice(pageWidth * 10 / 100));
				m_algorithmCandidateList.SetColumnWidth(1, toDevice(pageWidth * 12 / 100));
				m_algorithmCandidateList.SetColumnWidth(2, toDevice(pageWidth * 12 / 100));
				m_algorithmCandidateList.SetColumnWidth(3, toDevice(pageWidth * 12 / 100));
				m_algorithmCandidateList.SetColumnWidth(4, toDevice(pageWidth * 14 / 100));
				m_algorithmCandidateList.SetColumnWidth(5, toDevice(pageWidth * 36 / 100));
			}
		}
	}

	place(m_pageTabs, centerX, mainBottom + gap, centerWidth, tabsHeight);
	const int statsY = mainBottom + gap + tabsHeight + gap;
	const int statGap = 6;
	const int statWidth = (centerWidth - statGap * 5) / 6;
	for (std::size_t index = 0; index < m_statCards.size(); ++index)
	{
		const int statX = centerX + static_cast<int>(index) * (statWidth + statGap);
		const int width = index + 1 == m_statCards.size() ? centerRight - statX : statWidth;
		place(m_statCards[index], statX, statsY, width, statsHeight);
		place(m_statTitles[index], statX + 4, statsY + 8, width - 8, 26);
		place(m_statValues[index], statX + 4, statsY + 37, width - 8, 43);
	}

	if (m_rightPanelExpanded && m_hallCallList.GetHeaderCtrl() != nullptr)
	{
		const int listWidth = rightWidth - 24;
		m_hallCallList.SetColumnWidth(0, toDevice(listWidth * 22 / 100));
		m_hallCallList.SetColumnWidth(1, toDevice(listWidth * 20 / 100));
		m_hallCallList.SetColumnWidth(2, toDevice(listWidth * 22 / 100));
		m_hallCallList.SetColumnWidth(3, toDevice(listWidth * 32 / 100));
	}
}

void CElevatorSimulationDlg::UpdateTabPageVisibility()
{
	const int page = m_pageTabs.GetCurSel();
	const bool realTimePage = page == 0;
	const int realTimeCommand = realTimePage ? SW_SHOW : SW_HIDE;
	for (CWnd* control : { static_cast<CWnd*>(&m_mainPanel), static_cast<CWnd*>(&m_rightPanel),
		static_cast<CWnd*>(&m_panelToggle), static_cast<CWnd*>(&m_buildingView) })
	{
		control->ShowWindow(realTimeCommand);
	}
	UpdateRightPanelVisibility();
	m_pagePlaceholder.ShowWindow(SW_HIDE);
	m_statisticsTrendView.ShowWindow(page == 1 ? SW_SHOW : SW_HIDE);
	m_algorithmPageSummary.ShowWindow(page == 2 ? SW_SHOW : SW_HIDE);
	m_algorithmCandidateList.ShowWindow(page == 2 ? SW_SHOW : SW_HIDE);
	RelayoutUI();
}

void CElevatorSimulationDlg::UpdateRightPanelVisibility()
{
	const bool panelVisible = m_pageTabs.GetCurSel() == 0 && m_rightPanelExpanded;
	m_rightTabs.ShowWindow(panelVisible ? SW_SHOW : SW_HIDE);
	const int selectedTab = m_rightTabs.GetCurSel();
	m_hallCallList.ShowWindow(panelVisible && selectedTab == 0 ? SW_SHOW : SW_HIDE);
	m_elevatorDetailTitle.ShowWindow(panelVisible && selectedTab == 1 ? SW_SHOW : SW_HIDE);
	m_elevatorDetailBody.ShowWindow(panelVisible && selectedTab == 1 ? SW_SHOW : SW_HIDE);
	m_algorithmPlaceholder.ShowWindow(panelVisible && selectedTab == 2 ? SW_SHOW : SW_HIDE);
}

void CElevatorSimulationDlg::UpdateSpeedDisplay(double speed)
{
	CString text;
	text.Format(L"x%g", speed);
	m_headerSpeed.SetWindowTextW(text);
}

void CElevatorSimulationDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	if (nType != SIZE_MINIMIZED) RelayoutUI();
}

void CElevatorSimulationDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
	CDialogEx::OnGetMinMaxInfo(lpMMI);
	const UINT dpi = GetDpiForWindow(m_hWnd);
	lpMMI->ptMinTrackSize.x = MulDiv(1120, dpi, 96);
	lpMMI->ptMinTrackSize.y = MulDiv(700, dpi, 96);
}

void CElevatorSimulationDlg::SetSpeedPreset(const wchar_t* speedText)
{
	SetDlgItemTextW(IDC_EDIT_SPEED, speedText);
	UpdateSpeedDisplay(_wtof(speedText));
}

void CElevatorSimulationDlg::OnBnClickedSpeed1() { SetSpeedPreset(L"1"); }
void CElevatorSimulationDlg::OnBnClickedSpeed2() { SetSpeedPreset(L"2"); }
void CElevatorSimulationDlg::OnBnClickedSpeed5() { SetSpeedPreset(L"5"); }
void CElevatorSimulationDlg::OnBnClickedSpeed10() { SetSpeedPreset(L"10"); }

void CElevatorSimulationDlg::OnBnClickedPanelToggle()
{
	m_rightPanelExpanded = !m_rightPanelExpanded;
	m_panelToggle.SetWindowTextW(m_rightPanelExpanded ? L"<<" : L">>");
	UpdateTabPageVisibility();
	if (m_rightPanelExpanded) RefreshSimulationView();
}

void CElevatorSimulationDlg::OnTcnSelchangePages(NMHDR*, LRESULT* pResult)
{
	UpdateTabPageVisibility();
	if (m_pageTabs.GetCurSel() == 0)
		RefreshSimulationView(true);
	else if (m_pageTabs.GetCurSel() == 1)
		UpdateStatisticsTrend(m_simulationWorker ? m_simulationWorker->GetLatestSnapshot() : nullptr, true);
	else
		RefreshObservationViews(true);
	*pResult = 0;
}

void CElevatorSimulationDlg::OnTcnSelchangeRightTabs(NMHDR*, LRESULT* pResult)
{
	UpdateRightPanelVisibility();
	RelayoutUI();
	RefreshSimulationView();
	if (m_rightTabs.GetCurSel() == 2) RefreshObservationViews(true);
	*pResult = 0;
}

void CElevatorSimulationDlg::OnNMClickHallCallList(NMHDR* pNMHDR, LRESULT* pResult)
{
	if (!m_rebuildingHallCallList)
	{
		const auto* activate = reinterpret_cast<NMITEMACTIVATE*>(pNMHDR);
		if (activate->iItem >= 0)
		{
			const DWORD_PTR data = m_hallCallList.GetItemData(activate->iItem);
			HallCallIdentity identity;
			identity.floor = static_cast<int>(data >> 1);
			identity.direction = (data & 1) != 0 ? Direction::Up : Direction::Down;
			SelectHallCall(identity);
		}
	}
	*pResult = 0;
}

LRESULT CElevatorSimulationDlg::OnElevatorSelectionChanged(WPARAM, LPARAM)
{
	m_rightTabs.SetCurSel(1);
	UpdateRightPanelVisibility();
	RelayoutUI();
	RefreshSimulationView();
	return 0;
}

void CElevatorSimulationDlg::InitializeListControls()
{
	const DWORD extendedStyle = LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER;
	m_elevatorList.SetExtendedStyle(m_elevatorList.GetExtendedStyle() | extendedStyle);
	m_floorList.SetExtendedStyle(m_floorList.GetExtendedStyle() | extendedStyle);
	m_hallCallList.SetExtendedStyle(m_hallCallList.GetExtendedStyle() | extendedStyle);
	m_algorithmCandidateList.SetExtendedStyle(
		m_algorithmCandidateList.GetExtendedStyle() | extendedStyle);

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

	m_algorithmCandidateList.InsertColumn(0, L"电梯", LVCFMT_LEFT, 90);
	m_algorithmCandidateList.InsertColumn(1, L"ETA (s)", LVCFMT_RIGHT, 110);
	m_algorithmCandidateList.InsertColumn(2, L"Cost", LVCFMT_RIGHT, 110);
	m_algorithmCandidateList.InsertColumn(3, L"feasible", LVCFMT_CENTER, 100);
	m_algorithmCandidateList.InsertColumn(4, L"预计载客", LVCFMT_RIGHT, 110);
	m_algorithmCandidateList.InsertColumn(5, L"标记", LVCFMT_LEFT, 220);
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
	const int trafficPattern = m_trafficPatternCombo.GetCurSel();
	if (trafficPattern < 0 || trafficPattern > static_cast<int>(TrafficPattern::InterFloor))
	{
		ShowInputError(L"请选择有效的客流模式。");
		return false;
	}
	config.trafficPattern = static_cast<TrafficPattern>(trafficPattern);
	const int trafficScenario = m_trafficScenarioCombo.GetCurSel();
	if (trafficScenario < 0 || trafficScenario > static_cast<int>(TrafficScenario::OfficeDay))
	{
		ShowInputError(L"请选择有效的客流场景。");
		return false;
	}
	config.trafficScenario = static_cast<TrafficScenario>(trafficScenario);

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

	for (int controlId : ParameterControlIds)
	{
		GetDlgItem(controlId)->EnableWindow(active && ready);
	}
	const bool fixedScenario =
		m_trafficScenarioCombo.GetCurSel() == static_cast<int>(TrafficScenario::Fixed);
	m_trafficPatternCombo.EnableWindow(active && ready && fixedScenario);
	for (auto& speedButton : m_speedButtons)
		speedButton.EnableWindow(active && ready);
}

void CElevatorSimulationDlg::UpdateElevatorDetails(
	const std::shared_ptr<const SimulationUISnapshot>& snapshot)
{
	const int selectedElevatorId = m_buildingView.GetSelectedElevatorId();
	if (!snapshot || selectedElevatorId == InvalidElevatorId)
	{
		m_elevatorDetailTitle.SetWindowTextW(L"未选择电梯");
		m_elevatorDetailBody.SetWindowTextW(L"请在中央视图选择一台电梯");
		return;
	}

	const auto elevator = std::find_if(snapshot->elevators.begin(), snapshot->elevators.end(),
		[selectedElevatorId](const ElevatorSnapshot& item)
		{
			return item.id == selectedElevatorId;
		});
	if (elevator == snapshot->elevators.end())
	{
		m_elevatorDetailTitle.SetWindowTextW(L"未选择电梯");
		m_elevatorDetailBody.SetWindowTextW(L"请在中央视图选择一台电梯");
		return;
	}

	CString title;
	title.Format(L"E%d", elevator->id + 1);
	m_elevatorDetailTitle.SetWindowTextW(title);
	CString details;
	details.Format(L"当前楼层：%dF\r\n\r\n方向：%s\r\n\r\n状态：%s\r\n\r\n载客：%d / %d",
		elevator->currentFloor, DirectionText(elevator->direction),
		ElevatorStateText(elevator->state), elevator->passengerCount, elevator->capacity);
	m_elevatorDetailBody.SetWindowTextW(details);
}

void CElevatorSimulationDlg::ClearStatisticsTrend()
{
	m_statisticsTrend.clear();
	m_nextTrendSampleTime = 0.0;
	m_lastTrendSimulationTime = 0.0;
	m_trendHasSnapshot = false;
	m_statisticsRefreshScheduled = false;
	m_statisticsTrendView.SetTrendPoints(m_statisticsTrend);
}

void CElevatorSimulationDlg::UpdateStatisticsTrend(
	const std::shared_ptr<const SimulationUISnapshot>& snapshot, bool forceRefresh)
{
	if (!snapshot) return;
	if (m_trendHasSnapshot && snapshot->currentTime < m_lastTrendSimulationTime)
		ClearStatisticsTrend();
	m_lastTrendSimulationTime = snapshot->currentTime;
	m_trendHasSnapshot = true;

	bool sampled = false;
	if (snapshot->state == SimulationState::Running ||
		snapshot->state == SimulationState::Paused ||
		snapshot->state == SimulationState::Finished)
	{
		const double interval = (std::max)(0.5, snapshot->config.simulationDuration / 500.0);
		if (m_statisticsTrend.empty() || snapshot->currentTime >= m_nextTrendSampleTime)
		{
			StatisticsTrendPoint point;
			point.time = snapshot->currentTime;
			point.waitingCount = snapshot->statistics.waitingCount;
			point.arrivedCount = snapshot->statistics.arrivedCount;
			point.averageWaitingTime = snapshot->statistics.averageWaitingTime;
			if (m_statisticsTrend.size() < 500)
				m_statisticsTrend.push_back(point);
			else
				m_statisticsTrend.back() = point;
			m_nextTrendSampleTime = snapshot->currentTime + interval;
			sampled = true;
		}
	}

	const auto now = std::chrono::steady_clock::now();
	const bool refreshDue = !m_statisticsRefreshScheduled || now >= m_nextStatisticsRefresh;
	if (forceRefresh || (sampled && m_statisticsTrendView.IsWindowVisible() && refreshDue))
	{
		m_statisticsTrendView.SetTrendPoints(m_statisticsTrend);
		m_nextStatisticsRefresh = now + std::chrono::milliseconds(StatisticsRefreshMs);
		m_statisticsRefreshScheduled = true;
	}
}

void CElevatorSimulationDlg::SelectHallCall(HallCallIdentity identity)
{
	m_observedHallCall = identity;
	m_lastRenderedObservation.reset();
	if (m_simulationWorker)
		m_simulationWorker->ObserveHallCall(identity.floor, identity.direction);
	m_rightTabs.SetCurSel(2);
	UpdateRightPanelVisibility();
	RelayoutUI();
	ShowObservationEmptyState(L"正在计算候选电梯评分...");
}

void CElevatorSimulationDlg::ClearHallCallObservation()
{
	if (m_simulationWorker) m_simulationWorker->ClearObservedHallCall();
	m_observedHallCall.reset();
	m_lastRenderedObservation.reset();
	ShowObservationEmptyState(L"请在 Hall Call 页选择一个请求");
}

void CElevatorSimulationDlg::ValidateObservedHallCall(
	const std::shared_ptr<const SimulationUISnapshot>& snapshot)
{
	if (!m_observedHallCall) return;
	const bool exists = snapshot && std::any_of(snapshot->hallCalls.begin(), snapshot->hallCalls.end(),
		[this](const HallCallSnapshot& call)
		{
			return call.floorNumber == m_observedHallCall->floor &&
				call.direction == m_observedHallCall->direction;
		});
	if (!exists) ClearHallCallObservation();
}

void CElevatorSimulationDlg::RefreshObservationViews(bool forceRefresh)
{
	if (!m_observedHallCall || !m_simulationWorker)
	{
		if (forceRefresh) ShowObservationEmptyState(L"请在 Hall Call 页选择一个请求");
		return;
	}
	const auto observation = m_simulationWorker->GetLatestObservation();
	if (!observation || observation->floor != m_observedHallCall->floor ||
		observation->direction != m_observedHallCall->direction)
	{
		if (forceRefresh) ShowObservationEmptyState(L"正在计算候选电梯评分...");
		return;
	}
	if (!observation->valid)
	{
		ClearHallCallObservation();
		return;
	}
	if (!forceRefresh && observation == m_lastRenderedObservation) return;
	m_lastRenderedObservation = observation;
	PopulateObservationViews(*observation);
}

void CElevatorSimulationDlg::ShowObservationEmptyState(const wchar_t* message)
{
	m_algorithmPlaceholder.SetWindowTextW(message);
	m_algorithmPageSummary.SetWindowTextW(message);
	m_algorithmCandidateList.DeleteAllItems();
}

void CElevatorSimulationDlg::PopulateObservationViews(
	const DispatchObservationSnapshot& observation)
{
	const auto best = std::find_if(observation.candidates.begin(), observation.candidates.end(),
		[](const DispatchCandidateObservation& candidate) { return candidate.feasible; });
	CString ownerText = L"未分配";
	if (observation.assignedElevatorId != InvalidElevatorId)
		ownerText.Format(L"E%d", observation.assignedElevatorId + 1);

	CString rightText;
	if (best == observation.candidates.end())
	{
		rightText.Format(L"当前请求：%dF %s\r\n\r\n当前归属：%s\r\n\r\n最佳单梯候选：无可行候选",
			observation.floor, DirectionText(observation.direction), ownerText.GetString());
	}
	else
	{
		rightText.Format(L"当前请求：%dF %s\r\n\r\n当前归属：%s\r\n\r\n最佳单梯候选：E%d\r\n\r\nETA：%.2f s\r\nCost：%.2f",
			observation.floor, DirectionText(observation.direction), ownerText.GetString(),
			best->elevatorId + 1, best->eta, best->cost);
	}
	m_algorithmPlaceholder.SetWindowTextW(rightText);

	CString pageSummary;
	pageSummary.Format(L"%dF %s    等待人数：%zu    已等待：%.1f s    当前归属：%s\r\n候选为单请求评分；当前归属还会受到 Joint Dispatch、Reassignment 与 Hysteresis 影响。",
		observation.floor, DirectionText(observation.direction), observation.waitingCount,
		(std::max)(0.0, observation.currentTime - observation.firstRequestTime), ownerText.GetString());
	m_algorithmPageSummary.SetWindowTextW(pageSummary);

	std::vector<const DispatchCandidateObservation*> rows;
	const std::size_t topCount = (std::min)(std::size_t{ 10 }, observation.candidates.size());
	for (std::size_t index = 0; index < topCount; ++index)
		rows.push_back(&observation.candidates[index]);
	const auto owner = std::find_if(observation.candidates.begin(), observation.candidates.end(),
		[&observation](const DispatchCandidateObservation& candidate)
		{
			return candidate.elevatorId == observation.assignedElevatorId;
		});
	if (owner != observation.candidates.end() &&
		std::none_of(rows.begin(), rows.end(), [owner](const auto* candidate)
			{ return candidate->elevatorId == owner->elevatorId; }))
	{
		rows.push_back(&*owner);
	}

	m_algorithmCandidateList.SetRedraw(FALSE);
	m_algorithmCandidateList.DeleteAllItems();
	for (std::size_t index = 0; index < rows.size(); ++index)
	{
		const auto& candidate = *rows[index];
		CString value;
		value.Format(L"E%d", candidate.elevatorId + 1);
		const int row = m_algorithmCandidateList.InsertItem(static_cast<int>(index), value);
		if (candidate.feasible)
		{
			value.Format(L"%.2f", candidate.eta);
			m_algorithmCandidateList.SetItemText(row, 1, value);
			value.Format(L"%.2f", candidate.cost);
			m_algorithmCandidateList.SetItemText(row, 2, value);
		}
		else
		{
			m_algorithmCandidateList.SetItemText(row, 1, L"—");
			m_algorithmCandidateList.SetItemText(row, 2, L"—");
		}
		m_algorithmCandidateList.SetItemText(row, 3, candidate.feasible ? L"Yes" : L"No");
		value.Format(L"%d", candidate.projectedOccupancy);
		m_algorithmCandidateList.SetItemText(row, 4, value);
		CString mark;
		if (best != observation.candidates.end() && candidate.elevatorId == best->elevatorId)
			mark = L"最佳单梯候选";
		if (candidate.elevatorId == observation.assignedElevatorId)
			mark += mark.IsEmpty() ? L"当前归属" : L" / 当前归属";
		m_algorithmCandidateList.SetItemText(row, 5, mark);
	}
	m_algorithmCandidateList.SetRedraw(TRUE);
	m_algorithmCandidateList.Invalidate(FALSE);
}

void CElevatorSimulationDlg::RefreshBuildingView(
	const std::shared_ptr<const SimulationUISnapshot>& snapshot, bool forceRefresh)
{
	if (!m_buildingView.IsWindowVisible()) return;

	const bool largeScaleMode = snapshot &&
		(snapshot->config.floorCount > 80 || snapshot->elevators.size() > 30);
	const bool modeChanged = m_buildingRefreshScheduled &&
		largeScaleMode != m_lastBuildingLargeScaleMode;
	const auto now = std::chrono::steady_clock::now();
	const auto interval = std::chrono::milliseconds(largeScaleMode
		? LargeBuildingRefreshMs : NormalBuildingRefreshMs);
	if (!forceRefresh && !modeChanged && m_buildingRefreshScheduled &&
		now < m_nextBuildingRefresh)
	{
		return;
	}

	m_buildingView.SetSnapshot(snapshot);
	if (forceRefresh || modeChanged || !m_buildingRefreshScheduled)
	{
		m_nextBuildingRefresh = now + interval;
	}
	else
	{
		do
		{
			m_nextBuildingRefresh += interval;
		} while (m_nextBuildingRefresh <= now);
	}
	m_buildingRefreshScheduled = true;
	m_lastBuildingLargeScaleMode = largeScaleMode;
}

void CElevatorSimulationDlg::RefreshSimulationView(bool forceBuildingRefresh)
{
	const auto snapshot = m_simulationWorker ? m_simulationWorker->GetLatestSnapshot() : nullptr;
	if (!snapshot)
	{
		SetDlgItemTextW(IDC_SIMULATION_STATE, L"Initializing / 正在初始化");
		RefreshBuildingView(snapshot, forceBuildingRefresh);
		UpdateElevatorDetails(snapshot);
		UpdateControlStates(snapshot);
		return;
	}
	const auto& statistics = snapshot->statistics;
	const auto& hallCalls = snapshot->hallCalls;
	const auto& config = snapshot->config;
	CString stateText = snapshot->workerActive ? SimulationStateText(snapshot->state) : L"Stopped / 已停止";
	if (!snapshot->lastError.empty() &&
		(!snapshot->workerActive || snapshot->state == SimulationState::Uninitialized))
		stateText = L"Error / " + Utf8ToCString(snapshot->lastError);
	SetDlgItemTextW(IDC_SIMULATION_STATE, stateText);
	CString modelTime;
	modelTime.Format(L"%.1f / %.1f s", snapshot->currentTime, config.simulationDuration);
	SetDlgItemTextW(IDC_MODEL_TIME, modelTime);
	CString trafficText;
	if (snapshot->trafficScenario == TrafficScenario::OfficeDay)
	{
		trafficText.Format(L"场景：办公楼日周期 · 当前阶段：%s",
			OfficePhaseText(snapshot->trafficPhaseIndex));
	}
	else
	{
		trafficText.Format(L"场景：固定模式 · 当前模式：%s",
			TrafficPatternText(snapshot->activeTrafficPattern));
	}
	m_headerTraffic.SetWindowTextW(trafficText);
	if (snapshot->state != SimulationState::Ready &&
		snapshot->state != SimulationState::Uninitialized)
	{
		m_trafficPatternCombo.SetCurSel(static_cast<int>(snapshot->activeTrafficPattern));
	}
	if (snapshot->state == SimulationState::Ready)
	{
		CString speedText;
		GetDlgItemTextW(IDC_EDIT_SPEED, speedText);
		m_headerSpeed.SetWindowTextW(L"x" + speedText);
	}
	else
	{
		UpdateSpeedDisplay(config.simulationSpeed);
	}

	RefreshBuildingView(snapshot, forceBuildingRefresh);
	UpdateElevatorDetails(snapshot);
	UpdateStatisticsTrend(snapshot);
	ValidateObservedHallCall(snapshot);
	RefreshObservationViews();

	if (m_pageTabs.GetCurSel() == 0 && m_rightPanelExpanded &&
		m_rightTabs.GetCurSel() == 0)
	{
		m_rebuildingHallCallList = true;
		m_hallCallList.SetRedraw(FALSE);
		m_hallCallList.DeleteAllItems();
		for (std::size_t index = 0; index < hallCalls.size(); ++index)
		{
			const auto& call = hallCalls[index];
			const int row = static_cast<int>(index);
			CString value;
			value.Format(L"%dF", call.floorNumber);
			m_hallCallList.InsertItem(row, value);
			const DWORD_PTR identity = (static_cast<DWORD_PTR>(call.floorNumber) << 1) |
				(call.direction == Direction::Up ? 1u : 0u);
			m_hallCallList.SetItemData(row, identity);
			m_hallCallList.SetItemText(row, 1, DirectionText(call.direction));
			value.Format(L"%zu", call.waitingCount);
			m_hallCallList.SetItemText(row, 2, value);
			if (call.assignedElevatorId == InvalidElevatorId)
				value = L"未分配";
			else
				value.Format(L"E%d", call.assignedElevatorId + 1);
			m_hallCallList.SetItemText(row, 3, value);
			if (m_observedHallCall && call.floorNumber == m_observedHallCall->floor &&
				call.direction == m_observedHallCall->direction)
			{
				m_hallCallList.SetItemState(row, LVIS_SELECTED | LVIS_FOCUSED,
					LVIS_SELECTED | LVIS_FOCUSED);
			}
		}
		m_hallCallList.SetRedraw(TRUE);
		m_hallCallList.Invalidate(FALSE);
		m_rebuildingHallCallList = false;
	}

	CString statisticValues[6];
	statisticValues[0].Format(L"%zu", statistics.totalPassengerCount);
	statisticValues[1].Format(L"%zu", statistics.waitingCount);
	statisticValues[2].Format(L"%zu", statistics.ridingCount);
	statisticValues[3].Format(L"%zu", statistics.arrivedCount);
	statisticValues[4].Format(L"%.2f s", statistics.averageWaitingTime);
	statisticValues[5].Format(L"%.2f s", statistics.maxWaitingTime);
	for (std::size_t index = 0; index < m_statValues.size(); ++index)
		m_statValues[index].SetWindowTextW(statisticValues[index]);
	UpdateControlStates(snapshot);
}

void CElevatorSimulationDlg::OnBnClickedStart()
{
	SimulationConfig config;
	std::uint32_t seed = 0;
	if (!ReadConfiguration(config, seed)) return;
	ClearStatisticsTrend();
	ClearHallCallObservation();
	if (m_simulationWorker) m_simulationWorker->Stop();
	m_simulationWorker = std::make_unique<SimulationWorker>(config, seed,
		DispatcherExecutionMode::Parallel);
	m_simulationWorker->Start();
	RefreshSimulationView(true);
}

void CElevatorSimulationDlg::OnCbnSelchangeTrafficScenario()
{
	const auto snapshot = m_simulationWorker ? m_simulationWorker->GetLatestSnapshot() : nullptr;
	UpdateControlStates(snapshot);
}

void CElevatorSimulationDlg::OnBnClickedPause()
{
	if (m_simulationWorker) m_simulationWorker->Pause();
	RefreshSimulationView(true);
}

void CElevatorSimulationDlg::OnBnClickedResume()
{
	if (m_simulationWorker) m_simulationWorker->Resume();
	RefreshSimulationView(true);
}

void CElevatorSimulationDlg::OnBnClickedReset()
{
	ClearStatisticsTrend();
	ClearHallCallObservation();
	if (m_simulationWorker) m_simulationWorker->Reset();
	RefreshSimulationView(true);
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



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
		IDC_EDIT_DURATION, IDC_EDIT_SEED, IDC_EDIT_SPEED
	};

	constexpr const wchar_t* ParameterLabels[] = {
		L"楼层数 L", L"电梯数量 N", L"容量 K", L"每层时间 S (s)",
		L"上下客时间 T (s)", L"客流率 (人/仿真秒)", L"总时长 (s)",
		L"随机种子 seed", L"仿真倍速"
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
	ON_BN_CLICKED(IDC_BUTTON_PANEL_TOGGLE, &CElevatorSimulationDlg::OnBnClickedPanelToggle)
	ON_NOTIFY(TCN_SELCHANGE, IDC_TAB_PAGES, &CElevatorSimulationDlg::OnTcnSelchangePages)
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
	RefreshSimulationView();

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

	m_leftPanel.Create(L"参数与控制", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
		CRect(), this, IDC_PANEL_LEFT);
	m_mainPanel.Create(L"实时电梯群控主视图", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
		CRect(), this, IDC_PANEL_MAIN);
	m_rightPanel.Create(L"信息侧栏", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
		CRect(), this, IDC_PANEL_RIGHT);
	m_panelToggle.Create(L"<<", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		CRect(), this, IDC_BUTTON_PANEL_TOGGLE);

	m_parameterSection.Create(L"参数输入", labelStyle, CRect(), this, IDC_SECTION_PARAMETERS);
	m_controlSection.Create(L"仿真控制", labelStyle, CRect(), this, IDC_SECTION_CONTROLS);
	m_speedSection.Create(L"快捷倍速", labelStyle, CRect(), this, IDC_SECTION_SPEED);
	m_parameterSection.SetFont(&m_sectionFont);
	m_controlSection.SetFont(&m_sectionFont);
	m_speedSection.SetFont(&m_sectionFont);

	for (std::size_t index = 0; index < m_parameterLabels.size(); ++index)
	{
		m_parameterLabels[index].Create(ParameterLabels[index], labelStyle, CRect(), this,
			IDC_PARAMETER_LABEL_FIRST + static_cast<UINT>(index));
	}

	constexpr const wchar_t* SpeedLabels[] = { L"x1", L"x2", L"x5", L"x10" };
	for (std::size_t index = 0; index < m_speedButtons.size(); ++index)
	{
		m_speedButtons[index].Create(SpeedLabels[index],
			WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, CRect(), this,
			IDC_BUTTON_SPEED_1 + static_cast<UINT>(index));
	}

	m_mainFloorLabel.Create(L"楼层等待（数据占位）", labelStyle, CRect(), this,
		IDC_MAIN_FLOOR_LABEL);
	m_mainElevatorLabel.Create(L"电梯状态（数据占位）", labelStyle, CRect(), this,
		IDC_MAIN_ELEVATOR_LABEL);
	m_rightHint.Create(L"Hall Call 归属（Panel 数据占位）", labelStyle, CRect(), this,
		IDC_RIGHT_HINT);

	m_pageTabs.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | TCS_TABS | TCS_SINGLELINE,
		CRect(), this, IDC_TAB_PAGES);
	m_pageTabs.InsertItem(0, L"实时监控");
	m_pageTabs.InsertItem(1, L"统计分析");
	m_pageTabs.InsertItem(2, L"算法观察");
	m_pageTabs.SetCurSel(0);
	m_pagePlaceholder.Create(L"", WS_CHILD | WS_BORDER | SS_CENTER | SS_CENTERIMAGE,
		CRect(), this, IDC_PAGE_PLACEHOLDER);

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
		IDC_LIST_ELEVATORS, IDC_LIST_FLOORS, IDC_LIST_HALL_CALLS })
	{
		GetDlgItem(controlId)->ShowWindow(SW_SHOW);
	}
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
	const int headerHeight = 58;
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
	const int innerX = centerX + 12;
	const int innerY = contentTop + 26;
	const int innerWidth = centerWidth - 24;
	const int floorWidth = (std::max)(160, innerWidth * 28 / 100);

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
	place(m_headerStateLabel, headerInfoX, 12, 76, 30);
	move(IDC_SIMULATION_STATE, headerInfoX + 76, 12, headerPart - 76, 30);
	place(m_headerTimeLabel, headerInfoX + headerPart, 12, 76, 30);
	move(IDC_MODEL_TIME, headerInfoX + headerPart + 76, 12, headerPart - 76, 30);
	place(m_headerSpeedLabel, headerInfoX + headerPart * 2, 12, 76, 30);
	place(m_headerSpeed, headerInfoX + headerPart * 2 + 76, 12,
		headerInfoWidth - headerPart * 2 - 76, 30);

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
		move(ParameterControlIds[index], editX, rowY, editWidth, 22);
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
		place(m_mainFloorLabel, innerX, innerY, floorWidth, 22);
		place(m_mainElevatorLabel, innerX + floorWidth + gap, innerY,
			innerWidth - floorWidth - gap, 22);
		place(m_floorList, innerX, innerY + 24, floorWidth, mainHeight - 62);
		place(m_elevatorList, innerX + floorWidth + gap, innerY + 24,
			innerWidth - floorWidth - gap, mainHeight - 62);

		place(m_rightPanel, rightX, contentTop, rightWidth, contentBottom - contentTop);
		place(m_panelToggle, rightX + rightWidth - 38, contentTop + 15, 30, 27);
		if (m_rightPanelExpanded)
		{
			place(m_rightHint, rightX + 12, contentTop + 43, rightWidth - 24, 24);
			place(m_hallCallList, rightX + 12, contentTop + 69,
				rightWidth - 24, contentBottom - contentTop - 82);
		}
	}
	else
	{
		place(m_pagePlaceholder, centerX, contentTop,
			clientWidth - margin - centerX, mainHeight);
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

	if (m_floorList.GetHeaderCtrl() != nullptr)
	{
		m_floorList.SetColumnWidth(0, toDevice(floorWidth / 4));
		m_floorList.SetColumnWidth(1, toDevice((floorWidth * 3 / 4) / 2));
		m_floorList.SetColumnWidth(2, toDevice((floorWidth * 3 / 4) / 2));
	}
	if (m_elevatorList.GetHeaderCtrl() != nullptr)
	{
		const int elevatorWidth = innerWidth - floorWidth - gap;
		m_elevatorList.SetColumnWidth(0, toDevice(elevatorWidth * 14 / 100));
		m_elevatorList.SetColumnWidth(1, toDevice(elevatorWidth * 15 / 100));
		m_elevatorList.SetColumnWidth(2, toDevice(elevatorWidth * 15 / 100));
		m_elevatorList.SetColumnWidth(3, toDevice(elevatorWidth * 32 / 100));
		m_elevatorList.SetColumnWidth(4, toDevice(elevatorWidth * 20 / 100));
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
	const bool realTimePage = m_pageTabs.GetCurSel() == 0;
	const int realTimeCommand = realTimePage ? SW_SHOW : SW_HIDE;
	for (CWnd* control : { static_cast<CWnd*>(&m_mainPanel), static_cast<CWnd*>(&m_rightPanel),
		static_cast<CWnd*>(&m_panelToggle), static_cast<CWnd*>(&m_mainFloorLabel),
		static_cast<CWnd*>(&m_mainElevatorLabel), static_cast<CWnd*>(&m_floorList),
		static_cast<CWnd*>(&m_elevatorList) })
	{
		control->ShowWindow(realTimeCommand);
	}
	m_rightHint.ShowWindow(realTimePage && m_rightPanelExpanded ? SW_SHOW : SW_HIDE);
	m_hallCallList.ShowWindow(realTimePage && m_rightPanelExpanded ? SW_SHOW : SW_HIDE);
	m_pagePlaceholder.ShowWindow(realTimePage ? SW_HIDE : SW_SHOW);
	if (!realTimePage)
	{
		m_pagePlaceholder.SetWindowTextW(m_pageTabs.GetCurSel() == 1
			? L"统计分析页面结构已预留，本轮不接入图表。"
			: L"算法观察页面结构已预留，本轮不接入调度可视化。");
	}
	RelayoutUI();
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
}

void CElevatorSimulationDlg::OnTcnSelchangePages(NMHDR*, LRESULT* pResult)
{
	UpdateTabPageVisibility();
	*pResult = 0;
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
	for (auto& speedButton : m_speedButtons)
		speedButton.EnableWindow(active && ready);
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
	if (!snapshot->lastError.empty() &&
		(!snapshot->workerActive || snapshot->state == SimulationState::Uninitialized))
		stateText = L"Error / " + Utf8ToCString(snapshot->lastError);
	SetDlgItemTextW(IDC_SIMULATION_STATE, stateText);
	CString modelTime;
	modelTime.Format(L"%.1f / %.1f s", snapshot->currentTime, config.simulationDuration);
	SetDlgItemTextW(IDC_MODEL_TIME, modelTime);
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


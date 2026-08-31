
// ElevatorSimulationDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "ElevatorSimulation.h"
#include "ElevatorSimulationDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


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
}

BEGIN_MESSAGE_MAP(CElevatorSimulationDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
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

	SetWindowTextW(L"多电梯群控调度仿真系统 - 工程准备");
	if (!m_simulation.Initialize(SimulationConfig{}))
	{
		AfxMessageBox(L"默认参数初始化失败，请检查 Simulation::GetLastError()。", MB_ICONERROR);
		SetDlgItemTextW(IDC_SIMULATION_STATUS, L"初始化失败。");
	}
	else
	{
		RefreshSimulationView();
	}
	// TODO(E): 后续添加参数输入、开始/暂停/继续/重置按钮与定时刷新。
	// UI 只调用 Simulation 控制接口并显示快照，不自行实现仿真算法。

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CElevatorSimulationDlg::RefreshSimulationView()
{
	const auto elevators = m_simulation.GetElevatorSnapshots();
	const auto floors = m_simulation.GetFloorSnapshots();
	const auto statistics = m_simulation.GetStatisticsSnapshot();
	CString text;
	text.Format(L"工程骨架已就绪（尚未运行调度仿真）\r\n"
		L"楼层：%zu  电梯：%zu  仿真时间：%.1f 秒\r\n\r\n",
		floors.size(), elevators.size(), m_simulation.GetCurrentTime());
	for (const auto& elevator : elevators)
	{
		CString line;
		// 当前视图仅展示初始化快照，初始状态统一为 Idle。
		line.Format(L"E%d    %d 层    %d/%d 人    停止\r\n",
			elevator.id + 1, elevator.currentFloor,
			elevator.passengerCount, elevator.capacity);
		text += line;
	}
	CString footer;
	footer.Format(L"\r\n等待：%zu  乘梯：%zu  已到达：%zu\r\n"
		L"调度、运动、乘客生成和正式 UI：待后续分工实现。",
		statistics.waitingCount, statistics.ridingCount, statistics.arrivedCount);
	text += footer;
	SetDlgItemTextW(IDC_SIMULATION_STATUS, text);
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


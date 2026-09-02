
// ElevatorSimulationDlg.h: 头文件
//

#pragma once

#include "Core/SimulationWorker.h"

#include <memory>


// CElevatorSimulationDlg 对话框
class CElevatorSimulationDlg : public CDialogEx
{
// 构造
public:
	CElevatorSimulationDlg(CWnd* pParent = nullptr);	// 标准构造函数
	~CElevatorSimulationDlg() override;

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ELEVATORSIMULATION_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnDestroy();
	afx_msg void OnBnClickedStart();
	afx_msg void OnBnClickedPause();
	afx_msg void OnBnClickedResume();
	afx_msg void OnBnClickedReset();
	DECLARE_MESSAGE_MAP()

private:
	static constexpr UINT_PTR SimulationTimerId = 1;
	static constexpr UINT SimulationTimerIntervalMs = 33;

	std::unique_ptr<SimulationWorker> m_simulationWorker;
	CListCtrl m_elevatorList;
	CListCtrl m_floorList;
	CListCtrl m_hallCallList;

	void InitializeListControls();
	bool ReadConfiguration(SimulationConfig& config, std::uint32_t& seed);
	bool ReadIntControl(int controlId, const wchar_t* fieldName, int& value);
	bool ReadDoubleControl(int controlId, const wchar_t* fieldName, double& value);
	void ShowInputError(const CString& message);
	void UpdateControlStates(const std::shared_ptr<const SimulationUISnapshot>& snapshot);
	void RefreshSimulationView();
};

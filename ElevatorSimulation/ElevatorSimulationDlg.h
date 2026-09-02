
// ElevatorSimulationDlg.h: 头文件
//

#pragma once

#include "Core/SimulationWorker.h"

#include <array>
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
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
	afx_msg void OnBnClickedStart();
	afx_msg void OnBnClickedPause();
	afx_msg void OnBnClickedResume();
	afx_msg void OnBnClickedReset();
	afx_msg void OnBnClickedSpeed1();
	afx_msg void OnBnClickedSpeed2();
	afx_msg void OnBnClickedSpeed5();
	afx_msg void OnBnClickedSpeed10();
	afx_msg void OnBnClickedPanelToggle();
	afx_msg void OnTcnSelchangePages(NMHDR* pNMHDR, LRESULT* pResult);
	DECLARE_MESSAGE_MAP()

private:
	static constexpr UINT_PTR SimulationTimerId = 1;
	static constexpr UINT SimulationTimerIntervalMs = 33;

	std::unique_ptr<SimulationWorker> m_simulationWorker;
	CListCtrl m_elevatorList;
	CListCtrl m_floorList;
	CListCtrl m_hallCallList;
	CFont m_titleFont;
	CFont m_sectionFont;
	CFont m_statValueFont;
	CStatic m_headerTitle;
	CStatic m_headerStateLabel;
	CStatic m_headerTimeLabel;
	CStatic m_headerSpeedLabel;
	CStatic m_headerSpeed;
	CButton m_leftPanel;
	CButton m_mainPanel;
	CButton m_rightPanel;
	CButton m_panelToggle;
	CTabCtrl m_pageTabs;
	CStatic m_pagePlaceholder;
	CStatic m_mainFloorLabel;
	CStatic m_mainElevatorLabel;
	CStatic m_rightHint;
	CStatic m_parameterSection;
	CStatic m_controlSection;
	CStatic m_speedSection;
	std::array<CStatic, 9> m_parameterLabels;
	std::array<CButton, 4> m_speedButtons;
	std::array<CStatic, 6> m_statCards;
	std::array<CStatic, 6> m_statTitles;
	std::array<CStatic, 6> m_statValues;
	bool m_uiReady = false;
	bool m_rightPanelExpanded = true;

	void CreateUIFramework();
	void InitializeListControls();
	void RelayoutUI();
	void UpdateTabPageVisibility();
	void SetSpeedPreset(const wchar_t* speedText);
	void UpdateSpeedDisplay(double speed);
	bool ReadConfiguration(SimulationConfig& config, std::uint32_t& seed);
	bool ReadIntControl(int controlId, const wchar_t* fieldName, int& value);
	bool ReadDoubleControl(int controlId, const wchar_t* fieldName, double& value);
	void ShowInputError(const CString& message);
	void UpdateControlStates(const std::shared_ptr<const SimulationUISnapshot>& snapshot);
	void RefreshSimulationView();
};

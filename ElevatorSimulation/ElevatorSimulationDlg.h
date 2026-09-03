
// ElevatorSimulationDlg.h: 头文件
//

#pragma once

#include "Core/SimulationWorker.h"
#include "ElevatorBuildingView.h"
#include "StatisticsTrendView.h"

#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <vector>


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
	afx_msg void OnTcnSelchangeRightTabs(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnNMClickHallCallList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg LRESULT OnElevatorSelectionChanged(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()

private:
	static constexpr UINT_PTR SimulationTimerId = 1;
	static constexpr UINT SimulationTimerIntervalMs = 33;
	static constexpr int NormalBuildingRefreshMs = 33;
	static constexpr int LargeBuildingRefreshMs = 50;
	static constexpr int StatisticsRefreshMs = 250;

	struct HallCallIdentity
	{
		int floor = 1;
		Direction direction = Direction::Idle;

		bool operator==(const HallCallIdentity& other) const noexcept
		{
			return floor == other.floor && direction == other.direction;
		}
	};

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
	ElevatorBuildingView m_buildingView;
	StatisticsTrendView m_statisticsTrendView;
	CTabCtrl m_pageTabs;
	CTabCtrl m_rightTabs;
	CStatic m_pagePlaceholder;
	CStatic m_algorithmPageSummary;
	CListCtrl m_algorithmCandidateList;
	CStatic m_elevatorDetailTitle;
	CStatic m_elevatorDetailBody;
	CStatic m_algorithmPlaceholder;
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
	bool m_buildingRefreshScheduled = false;
	bool m_lastBuildingLargeScaleMode = false;
	std::chrono::steady_clock::time_point m_nextBuildingRefresh;
	std::vector<StatisticsTrendPoint> m_statisticsTrend;
	double m_nextTrendSampleTime = 0.0;
	double m_lastTrendSimulationTime = 0.0;
	bool m_trendHasSnapshot = false;
	bool m_statisticsRefreshScheduled = false;
	std::chrono::steady_clock::time_point m_nextStatisticsRefresh;
	std::optional<HallCallIdentity> m_observedHallCall;
	std::shared_ptr<const DispatchObservationSnapshot> m_lastRenderedObservation;
	bool m_rebuildingHallCallList = false;

	void CreateUIFramework();
	void InitializeListControls();
	void RelayoutUI();
	void UpdateTabPageVisibility();
	void UpdateRightPanelVisibility();
	void UpdateElevatorDetails(const std::shared_ptr<const SimulationUISnapshot>& snapshot);
	void ClearStatisticsTrend();
	void UpdateStatisticsTrend(const std::shared_ptr<const SimulationUISnapshot>& snapshot,
		bool forceRefresh = false);
	void SelectHallCall(HallCallIdentity identity);
	void ClearHallCallObservation();
	void ValidateObservedHallCall(const std::shared_ptr<const SimulationUISnapshot>& snapshot);
	void RefreshObservationViews(bool forceRefresh = false);
	void ShowObservationEmptyState(const wchar_t* message);
	void PopulateObservationViews(const DispatchObservationSnapshot& observation);
	void SetSpeedPreset(const wchar_t* speedText);
	void UpdateSpeedDisplay(double speed);
	bool ReadConfiguration(SimulationConfig& config, std::uint32_t& seed);
	bool ReadIntControl(int controlId, const wchar_t* fieldName, int& value);
	bool ReadDoubleControl(int controlId, const wchar_t* fieldName, double& value);
	void ShowInputError(const CString& message);
	void UpdateControlStates(const std::shared_ptr<const SimulationUISnapshot>& snapshot);
	void RefreshBuildingView(const std::shared_ptr<const SimulationUISnapshot>& snapshot,
		bool forceRefresh);
	void RefreshSimulationView(bool forceBuildingRefresh = false);
};

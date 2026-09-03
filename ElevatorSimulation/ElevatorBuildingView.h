#pragma once

#include "Core/CommonTypes.h"

#include <afxwin.h>

#include <chrono>
#include <memory>
#include <vector>

inline constexpr UINT WM_ELEVATOR_SELECTION_CHANGED = WM_APP + 100;

class ElevatorBuildingView : public CWnd
{
public:
	BOOL Create(CWnd* parent, UINT controlId);
	void SetSnapshot(std::shared_ptr<const SimulationUISnapshot> snapshot);
	int GetSelectedElevatorId() const noexcept;

protected:
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* dc);
	afx_msg void OnLButtonUp(UINT flags, CPoint point);
	afx_msg BOOL OnSetCursor(CWnd* window, UINT hitTest, UINT message);
	DECLARE_MESSAGE_MAP()

private:
	static constexpr int DetailedElevatorLimit = 12;
	static constexpr int ElevatorsPerGroup = 10;
	static constexpr int MinimumVisibleFloorCount = 5;

	struct ElevatorHitArea
	{
		int elevatorId = InvalidElevatorId;
		CRect rect;
	};

	struct ElevatorVisualState
	{
		double visualFloor = 1.0;
		double targetFloor = 1.0;
		double speedFloorsPerSecond = 0.0;
	};

	std::shared_ptr<const SimulationUISnapshot> m_snapshot;
	std::vector<ElevatorVisualState> m_elevatorVisualStates;
	std::chrono::steady_clock::time_point m_lastAnimationUpdate;
	bool m_animationClockInitialized = false;
	int m_selectedGroup = -1;
	int m_selectedElevatorId = InvalidElevatorId;
	int m_visibleFloorMin = 1;
	int m_visibleFloorMax = 1;
	int m_zoomCenterFloor = 1;
	std::vector<CRect> m_groupHitRects;
	std::vector<ElevatorHitArea> m_elevatorHitAreas;
	CRect m_backHitRect;
	CRect m_zoomFitHitRect;
	CRect m_zoomInHitRect;
	CRect m_zoomOutHitRect;
	CRect m_floorScaleHitRect;

	void DrawView(CDC& dc, const CRect& client);
	void DrawDetailed(CDC& dc, const CRect& content, int firstElevator, int elevatorCount);
	void DrawOverview(CDC& dc, const CRect& content);
	void DrawFloorScale(CDC& dc, const CRect& plot) const;
	void DrawZoomControls(CDC& dc, const CRect& content);
	int FloorY(int floor, const CRect& plot) const;
	int FloorY(double floor, const CRect& plot) const;
	int FloorAtY(int y, const CRect& plot) const;
	int FloorLabelStep() const;
	bool IsLargeScaleMode() const;
	CString GroupName(int groupIndex) const;
	CString VisibleFloorText() const;
	double VisualFloor(int elevatorId) const;
	void AdvanceVisualPositions(std::chrono::steady_clock::time_point now);
	void InitializeVisualPositions(std::chrono::steady_clock::time_point now);
	void UpdateVisualTargets();
	void FitAllFloors();
	void ZoomIn();
	void ZoomOut();
	void SetVisibleFloorCount(int floorCount);
	void SelectElevator(int elevatorId);
};

#pragma once

#include "Core/CommonTypes.h"

#include <afxwin.h>

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

	std::shared_ptr<const SimulationUISnapshot> m_snapshot;
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
	int FloorAtY(int y, const CRect& plot) const;
	int FloorLabelStep() const;
	CString GroupName(int groupIndex) const;
	CString VisibleFloorText() const;
	void FitAllFloors();
	void ZoomIn();
	void ZoomOut();
	void SetVisibleFloorCount(int floorCount);
	void SelectElevator(int elevatorId);
};

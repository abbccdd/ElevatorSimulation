#pragma once

#include "Core/CommonTypes.h"

#include <afxwin.h>

#include <memory>
#include <vector>

class ElevatorBuildingView : public CWnd
{
public:
	BOOL Create(CWnd* parent, UINT controlId);
	void SetSnapshot(std::shared_ptr<const SimulationUISnapshot> snapshot);

protected:
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* dc);
	afx_msg void OnLButtonUp(UINT flags, CPoint point);
	afx_msg BOOL OnSetCursor(CWnd* window, UINT hitTest, UINT message);
	DECLARE_MESSAGE_MAP()

private:
	static constexpr int DetailedElevatorLimit = 12;
	static constexpr int ElevatorsPerGroup = 10;

	std::shared_ptr<const SimulationUISnapshot> m_snapshot;
	int m_selectedGroup = -1;
	std::vector<CRect> m_groupHitRects;
	CRect m_backHitRect;

	void DrawView(CDC& dc, const CRect& client);
	void DrawDetailed(CDC& dc, const CRect& content, int firstElevator, int elevatorCount);
	void DrawOverview(CDC& dc, const CRect& content);
	void DrawFloorScale(CDC& dc, const CRect& plot) const;
	int FloorY(int floor, const CRect& plot) const;
	int FloorLabelStep() const;
	CString GroupName(int groupIndex) const;
};

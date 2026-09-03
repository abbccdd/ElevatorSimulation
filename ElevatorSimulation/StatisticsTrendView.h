#pragma once

#include <cstddef>
#include <vector>

struct StatisticsTrendPoint
{
	double time = 0.0;
	std::size_t waitingCount = 0;
	std::size_t arrivedCount = 0;
	double averageWaitingTime = 0.0;
};

class StatisticsTrendView : public CWnd
{
	DECLARE_DYNAMIC(StatisticsTrendView)

public:
	bool Create(CWnd* parent, UINT controlId);
	void SetTrendPoints(const std::vector<StatisticsTrendPoint>& points);

protected:
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* dc);
	DECLARE_MESSAGE_MAP()

private:
	std::vector<StatisticsTrendPoint> m_points;

	void DrawChart(CDC& dc, const CRect& bounds, const wchar_t* title, COLORREF color,
		double (*valueOf)(const StatisticsTrendPoint&), bool integerValues = false) const;
	void DrawOverview(CDC& dc, const CRect& bounds) const;
};

#pragma once

#include "Core/CommonTypes.h"

#include <vector>

class FloorCoverageView : public CWnd
{
    DECLARE_DYNAMIC(FloorCoverageView)

public:
    bool Create(CWnd* parent, UINT controlId);
    void SetCoverage(const std::vector<FloorCoverageSnapshot>& coverage);

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* dc);
    afx_msg BOOL OnMouseWheel(UINT flags, short delta, CPoint point);
    afx_msg void OnVScroll(UINT code, UINT position, CScrollBar* scrollBar);
    afx_msg void OnSize(UINT type, int width, int height);
    afx_msg void OnMouseMove(UINT flags, CPoint point);
    afx_msg void OnLButtonDown(UINT flags, CPoint point);
    afx_msg LRESULT OnMouseLeave(WPARAM wParam, LPARAM lParam);
    DECLARE_MESSAGE_MAP()

private:
    static constexpr int HeaderHeight = 106;
    static constexpr int RowHeight = 22;

    std::vector<FloorCoverageSnapshot> m_coverage;
    int m_scrollOffset = 0;
    int m_hoveredIndex = -1;
    int m_selectedIndex = -1;
    bool m_trackingMouse = false;

    void UpdateScrollBar();
    void ScrollTo(int offset);
    int IndexAt(CPoint point) const;
};

enum class FloorHeatmapMetric
{
    RequestCount,
    AverageWait,
    MaxWait
};

class FloorTrafficHeatmapView : public CWnd
{
    DECLARE_DYNAMIC(FloorTrafficHeatmapView)

public:
    bool Create(CWnd* parent, UINT controlId);
    void SetStatistics(const std::vector<FloorTrafficStatistics>& statistics);

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* dc);
    afx_msg BOOL OnMouseWheel(UINT flags, short delta, CPoint point);
    afx_msg void OnVScroll(UINT code, UINT position, CScrollBar* scrollBar);
    afx_msg void OnSize(UINT type, int width, int height);
    afx_msg void OnLButtonDown(UINT flags, CPoint point);
    DECLARE_MESSAGE_MAP()

private:
    static constexpr int HeaderHeight = 88;
    static constexpr int RowHeight = 22;

    std::vector<FloorTrafficStatistics> m_statistics;
    FloorHeatmapMetric m_metric = FloorHeatmapMetric::RequestCount;
    int m_scrollOffset = 0;
    CRect m_metricButtons[3];

    void UpdateScrollBar();
    void ScrollTo(int offset);
    double MetricValue(const FloorTrafficStatistics& statistics) const;
};

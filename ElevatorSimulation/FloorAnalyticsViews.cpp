#include "pch.h"
#include "FloorAnalyticsViews.h"

#include <algorithm>
#include <cmath>

IMPLEMENT_DYNAMIC(FloorCoverageView, CWnd)
IMPLEMENT_DYNAMIC(FloorTrafficHeatmapView, CWnd)

BEGIN_MESSAGE_MAP(FloorCoverageView, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_MOUSEWHEEL()
    ON_WM_VSCROLL()
    ON_WM_SIZE()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONDOWN()
    ON_MESSAGE(WM_MOUSELEAVE, &FloorCoverageView::OnMouseLeave)
END_MESSAGE_MAP()

BEGIN_MESSAGE_MAP(FloorTrafficHeatmapView, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_MOUSEWHEEL()
    ON_WM_VSCROLL()
    ON_WM_SIZE()
    ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()

namespace
{
    COLORREF Blend(COLORREF start, COLORREF end, double ratio)
    {
        ratio = (std::max)(0.0, (std::min)(1.0, ratio));
        const auto channel = [ratio](BYTE left, BYTE right)
        {
            return static_cast<BYTE>(std::lround(left + (right - left) * ratio));
        };
        return RGB(channel(GetRValue(start), GetRValue(end)),
            channel(GetGValue(start), GetGValue(end)),
            channel(GetBValue(start), GetBValue(end)));
    }

    void DrawBorder(CDC& dc, const CRect& bounds, COLORREF color)
    {
        CPen pen(PS_SOLID, 1, color);
        CPen* oldPen = dc.SelectObject(&pen);
        CBrush* oldBrush = static_cast<CBrush*>(dc.SelectStockObject(NULL_BRUSH));
        dc.Rectangle(bounds);
        dc.SelectObject(oldBrush);
        dc.SelectObject(oldPen);
    }

    CString EtaText(double eta)
    {
        CString text;
        if (std::isfinite(eta)) text.Format(L"%.2f s", eta);
        else text = L"无可行运力";
        return text;
    }
}

bool FloorCoverageView::Create(CWnd* parent, UINT controlId)
{
    const CString className = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW,
        ::LoadCursor(nullptr, IDC_ARROW), reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1), nullptr);
    return CreateEx(WS_EX_CLIENTEDGE, className, L"未来运力覆盖",
        WS_CHILD | WS_CLIPSIBLINGS | WS_VSCROLL, CRect(), parent, controlId) != FALSE;
}

void FloorCoverageView::SetCoverage(const std::vector<FloorCoverageSnapshot>& coverage)
{
    m_coverage = coverage;
    if (m_selectedIndex >= static_cast<int>(m_coverage.size())) m_selectedIndex = -1;
    if (m_hoveredIndex >= static_cast<int>(m_coverage.size())) m_hoveredIndex = -1;
    UpdateScrollBar();
    Invalidate(FALSE);
}

BOOL FloorCoverageView::OnEraseBkgnd(CDC*)
{
    return TRUE;
}

void FloorCoverageView::OnSize(UINT type, int width, int height)
{
    CWnd::OnSize(type, width, height);
    UpdateScrollBar();
}

void FloorCoverageView::UpdateScrollBar()
{
    if (GetSafeHwnd() == nullptr) return;
    CRect client;
    GetClientRect(&client);
    const int visibleRows = (std::max)(1, (client.Height() - HeaderHeight) / RowHeight);
    const int maximumOffset = (std::max)(0, static_cast<int>(m_coverage.size()) - visibleRows);
    m_scrollOffset = (std::min)(m_scrollOffset, maximumOffset);
    SCROLLINFO info{};
    info.cbSize = sizeof(info);
    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin = 0;
    info.nMax = (std::max)(0, static_cast<int>(m_coverage.size()) - 1);
    info.nPage = static_cast<UINT>(visibleRows);
    info.nPos = m_scrollOffset;
    SetScrollInfo(SB_VERT, &info, TRUE);
}

void FloorCoverageView::ScrollTo(int offset)
{
    CRect client;
    GetClientRect(&client);
    const int visibleRows = (std::max)(1, (client.Height() - HeaderHeight) / RowHeight);
    const int maximumOffset = (std::max)(0, static_cast<int>(m_coverage.size()) - visibleRows);
    const int next = (std::max)(0, (std::min)(maximumOffset, offset));
    if (next == m_scrollOffset) return;
    m_scrollOffset = next;
    SetScrollPos(SB_VERT, m_scrollOffset, TRUE);
    Invalidate(FALSE);
}

BOOL FloorCoverageView::OnMouseWheel(UINT, short delta, CPoint)
{
    ScrollTo(m_scrollOffset - (delta / WHEEL_DELTA) * 3);
    return TRUE;
}

void FloorCoverageView::OnVScroll(UINT code, UINT position, CScrollBar*)
{
    int next = m_scrollOffset;
    CRect client;
    GetClientRect(&client);
    const int visibleRows = (std::max)(1, (client.Height() - HeaderHeight) / RowHeight);
    switch (code)
    {
    case SB_LINEUP: --next; break;
    case SB_LINEDOWN: ++next; break;
    case SB_PAGEUP: next -= visibleRows; break;
    case SB_PAGEDOWN: next += visibleRows; break;
    case SB_THUMBPOSITION:
    case SB_THUMBTRACK: next = static_cast<int>(position); break;
    case SB_TOP: next = 0; break;
    case SB_BOTTOM: next = static_cast<int>(m_coverage.size()); break;
    default: return;
    }
    ScrollTo(next);
}

int FloorCoverageView::IndexAt(CPoint point) const
{
    if (point.y < HeaderHeight) return -1;
    const int displayRow = m_scrollOffset + (point.y - HeaderHeight) / RowHeight;
    const int index = static_cast<int>(m_coverage.size()) - 1 - displayRow;
    return index >= 0 && index < static_cast<int>(m_coverage.size()) ? index : -1;
}

void FloorCoverageView::OnMouseMove(UINT flags, CPoint point)
{
    CWnd::OnMouseMove(flags, point);
    if (!m_trackingMouse)
    {
        TRACKMOUSEEVENT event{ sizeof(event), TME_LEAVE, m_hWnd, 0 };
        ::TrackMouseEvent(&event);
        m_trackingMouse = true;
    }
    const int next = IndexAt(point);
    if (next != m_hoveredIndex)
    {
        m_hoveredIndex = next;
        Invalidate(FALSE);
    }
}

LRESULT FloorCoverageView::OnMouseLeave(WPARAM, LPARAM)
{
    m_trackingMouse = false;
    if (m_hoveredIndex != -1)
    {
        m_hoveredIndex = -1;
        Invalidate(FALSE);
    }
    return 0;
}

void FloorCoverageView::OnLButtonDown(UINT flags, CPoint point)
{
    CWnd::OnLButtonDown(flags, point);
    const int next = IndexAt(point);
    if (next != m_selectedIndex)
    {
        m_selectedIndex = next;
        Invalidate(FALSE);
    }
}

void FloorCoverageView::OnPaint()
{
    CPaintDC paintDc(this);
    CRect client;
    GetClientRect(&client);
    if (client.IsRectEmpty()) return;

    CDC dc;
    dc.CreateCompatibleDC(&paintDc);
    CBitmap bitmap;
    bitmap.CreateCompatibleBitmap(&paintDc, client.Width(), client.Height());
    CBitmap* oldBitmap = dc.SelectObject(&bitmap);
    dc.FillSolidRect(client, RGB(246, 248, 251));
    dc.SetBkMode(TRANSPARENT);
    if (GetFont() != nullptr) dc.SelectObject(GetFont());

    CRect header(client.left, client.top, client.right, HeaderHeight);
    dc.FillSolidRect(header, RGB(255, 255, 255));
    dc.SetTextColor(RGB(35, 43, 55));
    CRect title(12, 7, client.right - 10, 29);
    dc.DrawTextW(L"未来运力覆盖 / Coverage", title,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    int weakestFloor = InvalidFloor;
    int highestDemandFloor = InvalidFloor;
    double weakestEta = -1.0;
    double highestDemand = -1.0;
    double weightedEta = 0.0;
    double totalDemand = 0.0;
    bool weightedFinite = true;
    double maximumFiniteEta = 0.0;
    for (const auto& floor : m_coverage)
    {
        if (floor.demandWeight <= 0.0) continue;
        if (highestDemandFloor == InvalidFloor || floor.demandWeight > highestDemand)
        {
            highestDemandFloor = floor.floor;
            highestDemand = floor.demandWeight;
        }
        if (weakestFloor == InvalidFloor || !std::isfinite(floor.coverageEta) ||
            (std::isfinite(weakestEta) && floor.coverageEta > weakestEta))
        {
            weakestFloor = floor.floor;
            weakestEta = floor.coverageEta;
        }
        totalDemand += floor.demandWeight;
        if (std::isfinite(floor.coverageEta))
        {
            weightedEta += floor.demandWeight * floor.coverageEta;
            maximumFiniteEta = (std::max)(maximumFiniteEta, floor.coverageEta);
        }
        else
        {
            weightedFinite = false;
        }
    }

    CString summary;
    if (weakestFloor == InvalidFloor)
    {
        summary = L"暂无需求权重";
    }
    else
    {
        CString averageText = weightedFinite && totalDemand > 0.0 ?
            EtaText(weightedEta / totalDemand) : CString(L"无可行运力");
        summary.Format(L"最弱覆盖 %dF  |  最高需求 %dF  |  加权平均 %s",
            weakestFloor, highestDemandFloor, averageText.GetString());
    }
    dc.SetTextColor(RGB(66, 76, 90));
    CRect summaryRect(12, 30, client.right - 10, 52);
    dc.DrawTextW(summary, summaryRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    CString detail = L"移入或点选楼层查看当前预测；R 表示正在前往该层的软再平衡目标。";
    const int detailIndex = m_selectedIndex >= 0 ? m_selectedIndex : m_hoveredIndex;
    if (detailIndex >= 0 && detailIndex < static_cast<int>(m_coverage.size()))
    {
        const auto& selected = m_coverage[static_cast<std::size_t>(detailIndex)];
        const CString eta = EtaText(selected.coverageEta);
        detail.Format(L"%dF  需求权重 %.4f  Coverage ETA %s%s", selected.floor,
            selected.demandWeight, eta.GetString(), selected.hasRepositionTarget ? L"  [R]" : L"");
    }
    dc.SetTextColor(RGB(79, 91, 106));
    CRect detailRect(12, 52, client.right - 10, 78);
    dc.DrawTextW(detail, detailRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    const int valueRight = client.right - 10;
    const int etaLeft = valueRight - 82;
    const int demandLeft = etaLeft - 82;
    dc.SetTextColor(RGB(102, 111, 123));
    dc.DrawTextW(L"楼层", CRect(10, 80, 52, HeaderHeight), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    dc.DrawTextW(L"需求权重", CRect(demandLeft, 80, etaLeft - 4, HeaderHeight),
        DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    dc.DrawTextW(L"ETA", CRect(etaLeft, 80, valueRight, HeaderHeight),
        DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    const double etaScale = (std::max)(1.0, maximumFiniteEta);
    const int visibleRows = (std::max)(0, (client.Height() - HeaderHeight) / RowHeight + 1);
    for (int row = 0; row < visibleRows; ++row)
    {
        const int displayRow = m_scrollOffset + row;
        const int index = static_cast<int>(m_coverage.size()) - 1 - displayRow;
        if (index < 0) break;
        const auto& floor = m_coverage[static_cast<std::size_t>(index)];
        CRect rowRect(0, HeaderHeight + row * RowHeight, client.right,
            HeaderHeight + (row + 1) * RowHeight);
        const bool highlighted = index == m_selectedIndex || index == m_hoveredIndex;
        dc.FillSolidRect(rowRect, highlighted ? RGB(232, 240, 250) :
            (row % 2 == 0 ? RGB(250, 251, 253) : RGB(245, 247, 250)));

        CString floorText;
        floorText.Format(L"%dF%s", floor.floor, floor.hasRepositionTarget ? L" R" : L"");
        dc.SetTextColor(floor.hasRepositionTarget ? RGB(34, 92, 156) : RGB(43, 51, 62));
        dc.DrawTextW(floorText, CRect(10, rowRect.top, 58, rowRect.bottom),
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        CRect barArea(61, rowRect.top + 5, demandLeft - 8, rowRect.bottom - 5);
        if (barArea.Width() > 0)
        {
            dc.FillSolidRect(barArea, RGB(231, 235, 240));
            const double ratio = std::isfinite(floor.coverageEta) ?
                floor.coverageEta / etaScale : 1.0;
            CRect bar = barArea;
            bar.right = bar.left + static_cast<int>(std::lround(barArea.Width() *
                (std::max)(0.02, (std::min)(1.0, ratio))));
            dc.FillSolidRect(bar, std::isfinite(floor.coverageEta) ?
                Blend(RGB(64, 145, 184), RGB(210, 67, 56), ratio) : RGB(186, 45, 45));
        }

        CString value;
        value.Format(L"%.4f", floor.demandWeight);
        dc.SetTextColor(RGB(55, 64, 75));
        dc.DrawTextW(value, CRect(demandLeft, rowRect.top, etaLeft - 4, rowRect.bottom),
            DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        value = std::isfinite(floor.coverageEta) ? EtaText(floor.coverageEta) : CString(L"不可达");
        dc.SetTextColor(std::isfinite(floor.coverageEta) ? RGB(55, 64, 75) : RGB(165, 38, 38));
        dc.DrawTextW(value, CRect(etaLeft, rowRect.top, valueRight, rowRect.bottom),
            DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    paintDc.BitBlt(0, 0, client.Width(), client.Height(), &dc, 0, 0, SRCCOPY);
    dc.SelectObject(oldBitmap);
}

bool FloorTrafficHeatmapView::Create(CWnd* parent, UINT controlId)
{
    const CString className = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW,
        ::LoadCursor(nullptr, IDC_ARROW), reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1), nullptr);
    return CreateEx(WS_EX_CLIENTEDGE, className, L"楼层热力图",
        WS_CHILD | WS_CLIPSIBLINGS | WS_VSCROLL, CRect(), parent, controlId) != FALSE;
}

void FloorTrafficHeatmapView::SetStatistics(
    const std::vector<FloorTrafficStatistics>& statistics)
{
    m_statistics = statistics;
    UpdateScrollBar();
    Invalidate(FALSE);
}

BOOL FloorTrafficHeatmapView::OnEraseBkgnd(CDC*)
{
    return TRUE;
}

void FloorTrafficHeatmapView::OnSize(UINT type, int width, int height)
{
    CWnd::OnSize(type, width, height);
    UpdateScrollBar();
}

void FloorTrafficHeatmapView::UpdateScrollBar()
{
    if (GetSafeHwnd() == nullptr) return;
    CRect client;
    GetClientRect(&client);
    const int visibleRows = (std::max)(1, (client.Height() - HeaderHeight) / RowHeight);
    const int maximumOffset = (std::max)(0, static_cast<int>(m_statistics.size()) - visibleRows);
    m_scrollOffset = (std::min)(m_scrollOffset, maximumOffset);
    SCROLLINFO info{};
    info.cbSize = sizeof(info);
    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin = 0;
    info.nMax = (std::max)(0, static_cast<int>(m_statistics.size()) - 1);
    info.nPage = static_cast<UINT>(visibleRows);
    info.nPos = m_scrollOffset;
    SetScrollInfo(SB_VERT, &info, TRUE);
}

void FloorTrafficHeatmapView::ScrollTo(int offset)
{
    CRect client;
    GetClientRect(&client);
    const int visibleRows = (std::max)(1, (client.Height() - HeaderHeight) / RowHeight);
    const int maximumOffset = (std::max)(0, static_cast<int>(m_statistics.size()) - visibleRows);
    const int next = (std::max)(0, (std::min)(maximumOffset, offset));
    if (next == m_scrollOffset) return;
    m_scrollOffset = next;
    SetScrollPos(SB_VERT, m_scrollOffset, TRUE);
    Invalidate(FALSE);
}

BOOL FloorTrafficHeatmapView::OnMouseWheel(UINT, short delta, CPoint)
{
    ScrollTo(m_scrollOffset - (delta / WHEEL_DELTA) * 3);
    return TRUE;
}

void FloorTrafficHeatmapView::OnVScroll(UINT code, UINT position, CScrollBar*)
{
    int next = m_scrollOffset;
    CRect client;
    GetClientRect(&client);
    const int visibleRows = (std::max)(1, (client.Height() - HeaderHeight) / RowHeight);
    switch (code)
    {
    case SB_LINEUP: --next; break;
    case SB_LINEDOWN: ++next; break;
    case SB_PAGEUP: next -= visibleRows; break;
    case SB_PAGEDOWN: next += visibleRows; break;
    case SB_THUMBPOSITION:
    case SB_THUMBTRACK: next = static_cast<int>(position); break;
    case SB_TOP: next = 0; break;
    case SB_BOTTOM: next = static_cast<int>(m_statistics.size()); break;
    default: return;
    }
    ScrollTo(next);
}

double FloorTrafficHeatmapView::MetricValue(const FloorTrafficStatistics& statistics) const
{
    if (m_metric == FloorHeatmapMetric::RequestCount)
        return static_cast<double>(statistics.generatedCount);
    if (m_metric == FloorHeatmapMetric::AverageWait)
        return statistics.boardedCount == 0 ? 0.0 :
            statistics.totalWaitingTime / static_cast<double>(statistics.boardedCount);
    return statistics.maxWaitingTime;
}

void FloorTrafficHeatmapView::OnLButtonDown(UINT flags, CPoint point)
{
    CWnd::OnLButtonDown(flags, point);
    for (int index = 0; index < 3; ++index)
    {
        if (!m_metricButtons[index].PtInRect(point)) continue;
        m_metric = static_cast<FloorHeatmapMetric>(index);
        Invalidate(FALSE);
        return;
    }
}

void FloorTrafficHeatmapView::OnPaint()
{
    CPaintDC paintDc(this);
    CRect client;
    GetClientRect(&client);
    if (client.IsRectEmpty()) return;

    CDC dc;
    dc.CreateCompatibleDC(&paintDc);
    CBitmap bitmap;
    bitmap.CreateCompatibleBitmap(&paintDc, client.Width(), client.Height());
    CBitmap* oldBitmap = dc.SelectObject(&bitmap);
    dc.FillSolidRect(client, RGB(246, 248, 251));
    dc.SetBkMode(TRANSPARENT);
    if (GetFont() != nullptr) dc.SelectObject(GetFont());

    dc.FillSolidRect(CRect(client.left, client.top, client.right, HeaderHeight), RGB(255, 255, 255));
    dc.SetTextColor(RGB(35, 43, 55));
    dc.DrawTextW(L"楼层热力图", CRect(12, 5, client.right - 10, 28),
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    const wchar_t* labels[] = { L"请求总数", L"平均等待", L"最大等待" };
    const int gap = 5;
    const int buttonLeft = 10;
    const int buttonRight = client.right - 10;
    const int buttonWidth = (std::max)(1, (buttonRight - buttonLeft - gap * 2) / 3);
    for (int index = 0; index < 3; ++index)
    {
        const int left = buttonLeft + index * (buttonWidth + gap);
        const int right = index == 2 ? buttonRight : left + buttonWidth;
        m_metricButtons[index].SetRect(left, 31, right, 58);
        const bool selected = static_cast<int>(m_metric) == index;
        dc.FillSolidRect(m_metricButtons[index], selected ? RGB(48, 105, 171) : RGB(242, 245, 249));
        DrawBorder(dc, m_metricButtons[index], selected ? RGB(39, 86, 142) : RGB(205, 213, 223));
        dc.SetTextColor(selected ? RGB(255, 255, 255) : RGB(68, 78, 91));
        dc.DrawTextW(labels[index], m_metricButtons[index],
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    }

    dc.SetTextColor(RGB(102, 111, 123));
    dc.DrawTextW(L"楼层", CRect(10, 63, 54, HeaderHeight),
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    dc.DrawTextW(L"当前指标", CRect(client.right - 92, 63, client.right - 10, HeaderHeight),
        DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    double maximum = 0.0;
    for (const auto& statistics : m_statistics)
        maximum = (std::max)(maximum, MetricValue(statistics));
    const double scale = (std::max)(1.0, maximum);
    const int visibleRows = (std::max)(0, (client.Height() - HeaderHeight) / RowHeight + 1);
    for (int row = 0; row < visibleRows; ++row)
    {
        const int displayRow = m_scrollOffset + row;
        const int index = static_cast<int>(m_statistics.size()) - 1 - displayRow;
        if (index < 0) break;
        const auto& statistics = m_statistics[static_cast<std::size_t>(index)];
        CRect rowRect(0, HeaderHeight + row * RowHeight, client.right,
            HeaderHeight + (row + 1) * RowHeight);
        dc.FillSolidRect(rowRect, row % 2 == 0 ? RGB(250, 251, 253) : RGB(245, 247, 250));

        CString floorText;
        floorText.Format(L"%dF", statistics.floor);
        dc.SetTextColor(RGB(43, 51, 62));
        dc.DrawTextW(floorText, CRect(10, rowRect.top, 52, rowRect.bottom),
            DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        const double value = MetricValue(statistics);
        const double ratio = value / scale;
        CRect barArea(54, rowRect.top + 5, client.right - 100, rowRect.bottom - 5);
        if (barArea.Width() > 0)
        {
            dc.FillSolidRect(barArea, RGB(231, 235, 240));
            CRect bar = barArea;
            bar.right = bar.left + static_cast<int>(std::lround(barArea.Width() *
                (std::max)(0.0, (std::min)(1.0, ratio))));
            if (bar.right > bar.left)
                dc.FillSolidRect(bar, Blend(RGB(66, 139, 190), RGB(210, 67, 56), ratio));
        }

        CString valueText;
        if (m_metric == FloorHeatmapMetric::RequestCount)
            valueText.Format(L"%llu", static_cast<unsigned long long>(statistics.generatedCount));
        else
            valueText.Format(L"%.2f s", value);
        dc.SetTextColor(RGB(55, 64, 75));
        dc.DrawTextW(valueText, CRect(client.right - 94, rowRect.top,
            client.right - 10, rowRect.bottom),
            DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    paintDc.BitBlt(0, 0, client.Width(), client.Height(), &dc, 0, 0, SRCCOPY);
    dc.SelectObject(oldBitmap);
}

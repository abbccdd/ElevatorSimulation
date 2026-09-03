#include "pch.h"
#include "StatisticsTrendView.h"

#include <algorithm>
#include <cmath>

IMPLEMENT_DYNAMIC(StatisticsTrendView, CWnd)

BEGIN_MESSAGE_MAP(StatisticsTrendView, CWnd)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

namespace
{
	double WaitingValue(const StatisticsTrendPoint& point)
	{
		return static_cast<double>(point.waitingCount);
	}

	double ArrivedValue(const StatisticsTrendPoint& point)
	{
		return static_cast<double>(point.arrivedCount);
	}

	double AverageWaitValue(const StatisticsTrendPoint& point)
	{
		return point.averageWaitingTime;
	}
}

bool StatisticsTrendView::Create(CWnd* parent, UINT controlId)
{
	const CString className = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW,
		::LoadCursor(nullptr, IDC_ARROW), reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1), nullptr);
	return CreateEx(WS_EX_CLIENTEDGE, className, L"统计趋势",
		WS_CHILD | WS_CLIPSIBLINGS, CRect(), parent, controlId) != FALSE;
}

void StatisticsTrendView::SetTrendPoints(const std::vector<StatisticsTrendPoint>& points)
{
	m_points = points;
	Invalidate(FALSE);
}

BOOL StatisticsTrendView::OnEraseBkgnd(CDC*)
{
	return TRUE;
}

void StatisticsTrendView::OnPaint()
{
	CPaintDC paintDc(this);
	CRect client;
	GetClientRect(&client);
	if (client.IsRectEmpty()) return;

	CDC bufferDc;
	bufferDc.CreateCompatibleDC(&paintDc);
	CBitmap bitmap;
	bitmap.CreateCompatibleBitmap(&paintDc, client.Width(), client.Height());
	CBitmap* oldBitmap = bufferDc.SelectObject(&bitmap);
	bufferDc.FillSolidRect(client, RGB(250, 251, 253));
	bufferDc.SetBkMode(TRANSPARENT);
	if (GetFont() != nullptr) bufferDc.SelectObject(GetFont());

	if (m_points.empty())
	{
		bufferDc.SetTextColor(RGB(92, 101, 113));
		bufferDc.DrawTextW(L"暂无趋势数据\r\n启动仿真后按模型时间低频采样",
			client, DT_CENTER | DT_VCENTER | DT_WORDBREAK | DT_NOPREFIX);
	}
	else
	{
		const int margin = 12;
		const int gap = 10;
		const int chartHeight = (client.Height() - margin * 2 - gap * 2) / 3;
		DrawChart(bufferDc, CRect(margin, margin, client.right - margin, margin + chartHeight),
			L"等待人数", RGB(208, 74, 72), WaitingValue);
		DrawChart(bufferDc, CRect(margin, margin + chartHeight + gap,
			client.right - margin, margin + chartHeight * 2 + gap),
			L"累计到达人数", RGB(45, 126, 78), ArrivedValue);
		DrawChart(bufferDc, CRect(margin, margin + chartHeight * 2 + gap * 2,
			client.right - margin, client.bottom - margin),
			L"平均等待时间 (s)", RGB(40, 105, 180), AverageWaitValue);
	}

	paintDc.BitBlt(0, 0, client.Width(), client.Height(), &bufferDc, 0, 0, SRCCOPY);
	bufferDc.SelectObject(oldBitmap);
}

void StatisticsTrendView::DrawChart(CDC& dc, const CRect& bounds, const wchar_t* title,
	COLORREF color, double (*valueOf)(const StatisticsTrendPoint&)) const
{
	dc.FillSolidRect(bounds, RGB(255, 255, 255));
	CPen borderPen(PS_SOLID, 1, RGB(210, 216, 224));
	CPen* oldPen = dc.SelectObject(&borderPen);
	dc.Rectangle(bounds);

	const double latestValue = valueOf(m_points.back());
	double dataMaximum = 0.0;
	for (const auto& point : m_points)
		dataMaximum = (std::max)(dataMaximum, valueOf(point));
	const double scaleMaximum = (std::max)(1.0, dataMaximum);

	CString heading;
	heading.Format(L"%s    当前 %.2f    最大 %.2f", title, latestValue, dataMaximum);
	dc.SetTextColor(RGB(35, 42, 52));
	CRect headingRect = bounds;
	headingRect.DeflateRect(10, 5, 10, 0);
	dc.DrawTextW(heading, headingRect, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

	CRect plot = bounds;
	plot.DeflateRect(48, 28, 12, 24);
	if (plot.Width() <= 1 || plot.Height() <= 1)
	{
		dc.SelectObject(oldPen);
		return;
	}

	CPen gridPen(PS_DOT, 1, RGB(224, 228, 234));
	dc.SelectObject(&gridPen);
	for (int part = 0; part <= 4; ++part)
	{
		const int y = plot.bottom - MulDiv(plot.Height(), part, 4);
		dc.MoveTo(plot.left, y);
		dc.LineTo(plot.right, y);
		const int x = plot.left + MulDiv(plot.Width(), part, 4);
		dc.MoveTo(x, plot.top);
		dc.LineTo(x, plot.bottom);
	}

	dc.SetTextColor(RGB(90, 99, 111));
	CString scale;
	scale.Format(L"%.1f", scaleMaximum);
	CRect topScale(bounds.left + 5, plot.top - 8, plot.left - 4, plot.top + 10);
	dc.DrawTextW(scale, topScale, DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX);
	CRect zeroScale(bounds.left + 5, plot.bottom - 9, plot.left - 4, plot.bottom + 9);
	dc.DrawTextW(L"0", zeroScale, DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX);

	const double lastTime = (std::max)(m_points.back().time, 0.5);
	CRect timeRect(plot.left, plot.bottom + 4, plot.right, bounds.bottom - 2);
	dc.DrawTextW(L"0 s", timeRect, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
	CString endTimeLabel;
	endTimeLabel.Format(L"%.1f s", m_points.back().time);
	dc.DrawTextW(endTimeLabel, timeRect, DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX);

	CPen dataPen(PS_SOLID, 2, color);
	dc.SelectObject(&dataPen);
	bool first = true;
	for (const auto& point : m_points)
	{
		const int x = plot.left + static_cast<int>(std::lround(
			plot.Width() * (std::max)(0.0, point.time) / lastTime));
		const int y = plot.bottom - static_cast<int>(std::lround(
			plot.Height() * valueOf(point) / scaleMaximum));
		if (first)
		{
			dc.MoveTo(x, y);
			first = false;
		}
		else
		{
			dc.LineTo(x, y);
		}
	}
	dc.SelectObject(oldPen);
}

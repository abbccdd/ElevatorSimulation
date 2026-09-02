#include "pch.h"
#include "ElevatorBuildingView.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr COLORREF BackgroundColor = RGB(250, 251, 253);
	constexpr COLORREF SurfaceColor = RGB(255, 255, 255);
	constexpr COLORREF TextColor = RGB(30, 41, 59);
	constexpr COLORREF MutedTextColor = RGB(100, 116, 139);
	constexpr COLORREF FloorLineColor = RGB(226, 232, 240);
	constexpr COLORREF MajorLineColor = RGB(203, 213, 225);
	constexpr COLORREF AccentColor = RGB(37, 99, 235);
	constexpr COLORREF AccentFillColor = RGB(219, 234, 254);
	constexpr COLORREF ActivityColor = RGB(190, 24, 93);

	const wchar_t* DirectionText(Direction direction)
	{
		switch (direction)
		{
		case Direction::Up: return L"↑";
		case Direction::Down: return L"↓";
		default: return L"Idle";
		}
	}
}

BEGIN_MESSAGE_MAP(ElevatorBuildingView, CWnd)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_LBUTTONUP()
	ON_WM_SETCURSOR()
END_MESSAGE_MAP()

BOOL ElevatorBuildingView::Create(CWnd* parent, UINT controlId)
{
	const CString className = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW,
		::LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr);
	return CreateEx(WS_EX_CLIENTEDGE, className, L"实时电梯群控视图",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP, CRect(), parent, controlId);
}

void ElevatorBuildingView::SetSnapshot(std::shared_ptr<const SimulationUISnapshot> snapshot)
{
	const std::size_t previousElevatorCount = m_snapshot ? m_snapshot->elevators.size() : 0;
	m_snapshot = std::move(snapshot);
	if (!m_snapshot || m_snapshot->elevators.size() <= DetailedElevatorLimit ||
		m_snapshot->elevators.size() != previousElevatorCount)
	{
		m_selectedGroup = -1;
	}
	Invalidate(FALSE);
}

void ElevatorBuildingView::OnPaint()
{
	CPaintDC paintDc(this);
	CRect client;
	GetClientRect(&client);
	if (client.IsRectEmpty()) return;

	CDC bufferDc;
	bufferDc.CreateCompatibleDC(&paintDc);
	CBitmap bufferBitmap;
	bufferBitmap.CreateCompatibleBitmap(&paintDc, client.Width(), client.Height());
	CBitmap* previousBitmap = bufferDc.SelectObject(&bufferBitmap);
	DrawView(bufferDc, client);
	paintDc.BitBlt(0, 0, client.Width(), client.Height(), &bufferDc, 0, 0, SRCCOPY);
	bufferDc.SelectObject(previousBitmap);
}

BOOL ElevatorBuildingView::OnEraseBkgnd(CDC*)
{
	return TRUE;
}

void ElevatorBuildingView::DrawView(CDC& dc, const CRect& client)
{
	dc.FillSolidRect(client, BackgroundColor);
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(TextColor);
	if (GetFont() != nullptr) dc.SelectObject(GetFont());
	m_groupHitRects.clear();
	m_backHitRect.SetRectEmpty();

	if (!m_snapshot)
	{
		CRect messageRect = client;
		dc.SetTextColor(MutedTextColor);
		dc.DrawText(L"等待仿真 Snapshot…", messageRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		return;
	}

	CRect content = client;
	content.DeflateRect(10, 8);
	const int elevatorCount = static_cast<int>(m_snapshot->elevators.size());
	if (elevatorCount <= DetailedElevatorLimit)
	{
		DrawDetailed(dc, content, 0, elevatorCount);
	}
	else if (m_selectedGroup >= 0)
	{
		const int firstElevator = m_selectedGroup * ElevatorsPerGroup;
		const int count = (std::min)(ElevatorsPerGroup, elevatorCount - firstElevator);
		DrawDetailed(dc, content, firstElevator, count);
	}
	else
	{
		DrawOverview(dc, content);
	}
}

int ElevatorBuildingView::FloorY(int floor, const CRect& plot) const
{
	const int floorCount = m_snapshot->config.floorCount;
	const double position = static_cast<double>(floor - 1) / static_cast<double>(floorCount - 1);
	return plot.bottom - static_cast<int>(std::lround(position * plot.Height()));
}

int ElevatorBuildingView::FloorLabelStep() const
{
	const int floorCount = m_snapshot->config.floorCount;
	if (floorCount <= 30) return 1;
	if (floorCount <= 40) return 2;
	if (floorCount <= 60) return 5;
	return 10;
}

void ElevatorBuildingView::DrawFloorScale(CDC& dc, const CRect& plot) const
{
	const int floorCount = m_snapshot->config.floorCount;
	std::vector<std::size_t> upWaiting(static_cast<std::size_t>(floorCount + 1), 0);
	std::vector<std::size_t> downWaiting(static_cast<std::size_t>(floorCount + 1), 0);
	for (const auto& floor : m_snapshot->floors)
	{
		upWaiting[static_cast<std::size_t>(floor.floorNumber)] = floor.upWaitingCount;
		downWaiting[static_cast<std::size_t>(floor.floorNumber)] = floor.downWaitingCount;
	}
	for (const auto& hallCall : m_snapshot->hallCalls)
	{
		auto& waiting = hallCall.direction == Direction::Up
			? upWaiting[static_cast<std::size_t>(hallCall.floorNumber)]
			: downWaiting[static_cast<std::size_t>(hallCall.floorNumber)];
		waiting = (std::max)(waiting, hallCall.waitingCount);
	}

	const int labelStep = FloorLabelStep();
	for (int floor = 1; floor <= floorCount; ++floor)
	{
		const std::size_t upCount = upWaiting[static_cast<std::size_t>(floor)];
		const std::size_t downCount = downWaiting[static_cast<std::size_t>(floor)];
		const bool active = upCount > 0 || downCount > 0;
		const bool major = floor == 1 || floor == floorCount || floor % labelStep == 0;
		const bool drawLine = floorCount <= 60 || major || active;
		if (!drawLine) continue;

		const int y = FloorY(floor, plot);
		CPen pen(PS_SOLID, active ? 2 : 1,
			active ? ActivityColor : (major ? MajorLineColor : FloorLineColor));
		CPen* previousPen = dc.SelectObject(&pen);
		dc.MoveTo(plot.left, y);
		dc.LineTo(plot.right, y);
		dc.SelectObject(previousPen);

		if (!major && !active) continue;
		CString label;
		label.Format(L"%dF", floor);
		if (upCount > 0)
		{
			CString count;
			count.Format(L"  ↑%zu", upCount);
			label += count;
		}
		if (downCount > 0)
		{
			CString count;
			count.Format(L"  ↓%zu", downCount);
			label += count;
		}
		CRect labelRect(plot.left - 106, y - 10, plot.left - 7, y + 10);
		dc.SetTextColor(active ? ActivityColor : MutedTextColor);
		dc.DrawText(label, labelRect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
	}
	dc.SetTextColor(TextColor);
}

void ElevatorBuildingView::DrawDetailed(CDC& dc, const CRect& content,
	int firstElevator, int elevatorCount)
{
	CString title;
	if (m_snapshot->elevators.size() <= DetailedElevatorLimit)
	{
		title.Format(L"%d 层 · %d 台电梯 · 详细模式", m_snapshot->config.floorCount,
			elevatorCount);
	}
	else
	{
		m_backHitRect.SetRect(content.left, content.top, content.left + 112, content.top + 30);
		dc.FillSolidRect(m_backHitRect, AccentFillColor);
		dc.Draw3dRect(m_backHitRect, AccentColor, AccentColor);
		CRect backText = m_backHitRect;
		dc.SetTextColor(AccentColor);
		dc.DrawText(L"← 返回总览", backText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		const int lastElevator = firstElevator + elevatorCount;
		title.Format(L"%s · E%d-E%d · 详细模式", GroupName(m_selectedGroup),
			firstElevator + 1, lastElevator);
	}

	CRect titleRect(content.left + (m_backHitRect.IsRectEmpty() ? 0 : 124),
		content.top, content.right, content.top + 30);
	dc.SetTextColor(TextColor);
	dc.DrawText(title, titleRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

	CRect plot(content.left + 108, content.top + 48, content.right - 8, content.bottom - 28);
	DrawFloorScale(dc, plot);
	const int shaftWidth = plot.Width() / elevatorCount;
	for (int localIndex = 0; localIndex < elevatorCount; ++localIndex)
	{
		const auto& elevator = m_snapshot->elevators[
			static_cast<std::size_t>(firstElevator + localIndex)];
		const int shaftLeft = plot.left + localIndex * shaftWidth;
		const int shaftRight = localIndex + 1 == elevatorCount
			? plot.right : shaftLeft + shaftWidth;
		const int centerX = (shaftLeft + shaftRight) / 2;

		CRect elevatorLabel(shaftLeft, content.top + 30, shaftRight, content.top + 48);
		CString name;
		name.Format(L"E%d", elevator.id + 1);
		dc.DrawText(name, elevatorLabel, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

		CPen shaftPen(PS_SOLID, 1, MajorLineColor);
		CPen* previousPen = dc.SelectObject(&shaftPen);
		dc.MoveTo(centerX, plot.top);
		dc.LineTo(centerX, plot.bottom);
		dc.SelectObject(previousPen);

		const int carWidth = (std::max)(26, (std::min)(64, shaftRight - shaftLeft - 8));
		const int carHeight = 42;
		int carTop = FloorY(elevator.currentFloor, plot) - carHeight / 2;
		carTop = (std::max)(static_cast<int>(plot.top),
			(std::min)(carTop, static_cast<int>(plot.bottom) - carHeight));
		CRect car(centerX - carWidth / 2, carTop,
			centerX + carWidth / 2, carTop + carHeight);
		dc.FillSolidRect(car, AccentFillColor);
		dc.Draw3dRect(car, AccentColor, AccentColor);
		CString carText;
		carText.Format(L"%dF %s\n%d/%d", elevator.currentFloor,
			DirectionText(elevator.direction), elevator.passengerCount, elevator.capacity);
		CRect carTextRect = car;
		dc.SetTextColor(TextColor);
		dc.DrawText(carText, carTextRect, DT_CENTER | DT_VCENTER);
	}
}

CString ElevatorBuildingView::GroupName(int groupIndex) const
{
	CString name;
	if (groupIndex < 26)
		name.Format(L"%c组", L'A' + groupIndex);
	else
		name.Format(L"第%d组", groupIndex + 1);
	return name;
}

void ElevatorBuildingView::DrawOverview(CDC& dc, const CRect& content)
{
	const int elevatorCount = static_cast<int>(m_snapshot->elevators.size());
	const int groupCount = (elevatorCount + ElevatorsPerGroup - 1) / ElevatorsPerGroup;
	CRect titleRect(content.left, content.top, content.right, content.top + 30);
	CString title;
	title.Format(L"%d 层 · %d 台电梯 · %d 个分组    点击分组查看完整井道",
		m_snapshot->config.floorCount, elevatorCount, groupCount);
	dc.DrawText(title, titleRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

	CRect plot(content.left + 108, content.top + 52, content.right - 8, content.bottom - 28);
	const int groupWidth = plot.Width() / groupCount;
	m_groupHitRects.reserve(static_cast<std::size_t>(groupCount));
	for (int groupIndex = 0; groupIndex < groupCount; ++groupIndex)
	{
		const int left = plot.left + groupIndex * groupWidth;
		const int right = groupIndex + 1 == groupCount ? plot.right : left + groupWidth;
		CRect groupRect(left + 3, content.top + 34, right - 3, plot.bottom + 8);
		dc.FillSolidRect(groupRect, SurfaceColor);
		m_groupHitRects.push_back(groupRect);
	}
	DrawFloorScale(dc, plot);

	for (int groupIndex = 0; groupIndex < groupCount; ++groupIndex)
	{
		const int firstElevator = groupIndex * ElevatorsPerGroup;
		const int count = (std::min)(ElevatorsPerGroup, elevatorCount - firstElevator);
		const CRect groupRect = m_groupHitRects[static_cast<std::size_t>(groupIndex)];
		dc.Draw3dRect(groupRect, MajorLineColor, MajorLineColor);

		CString groupLabel;
		groupLabel.Format(L"%s\nE%d-E%d", GroupName(groupIndex), firstElevator + 1,
			firstElevator + count);
		CRect labelRect(groupRect.left + 2, groupRect.top + 2,
			groupRect.right - 2, plot.top - 2);
		dc.SetTextColor(AccentColor);
		dc.DrawText(groupLabel, labelRect, DT_CENTER | DT_VCENTER);

		const int markerAreaLeft = groupRect.left + 7;
		const int markerAreaWidth = (std::max)(1, static_cast<int>(groupRect.Width()) - 14);
		for (int localIndex = 0; localIndex < count; ++localIndex)
		{
			const auto& elevator = m_snapshot->elevators[
				static_cast<std::size_t>(firstElevator + localIndex)];
			const int markerX = count == 1 ? groupRect.CenterPoint().x
				: markerAreaLeft + localIndex * markerAreaWidth / (count - 1);
			const int markerY = FloorY(elevator.currentFloor, plot);
			CRect marker(markerX - 3, markerY - 4, markerX + 4, markerY + 5);
			dc.FillSolidRect(marker, AccentColor);
		}
	}
	dc.SetTextColor(TextColor);
}

void ElevatorBuildingView::OnLButtonUp(UINT flags, CPoint point)
{
	SetFocus();
	if (m_selectedGroup >= 0 && m_backHitRect.PtInRect(point))
	{
		m_selectedGroup = -1;
		Invalidate(FALSE);
	}
	else if (m_selectedGroup < 0)
	{
		for (std::size_t index = 0; index < m_groupHitRects.size(); ++index)
		{
			if (!m_groupHitRects[index].PtInRect(point)) continue;
			m_selectedGroup = static_cast<int>(index);
			Invalidate(FALSE);
			break;
		}
	}
	CWnd::OnLButtonUp(flags, point);
}

BOOL ElevatorBuildingView::OnSetCursor(CWnd* window, UINT hitTest, UINT message)
{
	CPoint point;
	GetCursorPos(&point);
	ScreenToClient(&point);
	if ((m_selectedGroup >= 0 && m_backHitRect.PtInRect(point)) ||
		std::any_of(m_groupHitRects.begin(), m_groupHitRects.end(),
			[point](const CRect& rect) { return rect.PtInRect(point); }))
	{
		::SetCursor(::LoadCursor(nullptr, IDC_HAND));
		return TRUE;
	}
	return CWnd::OnSetCursor(window, hitTest, message);
}

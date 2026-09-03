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
	const auto now = std::chrono::steady_clock::now();
	const std::size_t previousElevatorCount = m_snapshot ? m_snapshot->elevators.size() : 0;
	const int previousFloorCount = m_snapshot ? m_snapshot->config.floorCount : 0;
	const bool configurationChanged = !m_snapshot || !snapshot ||
		m_snapshot->config.floorCount != snapshot->config.floorCount ||
		m_snapshot->elevators.size() != snapshot->elevators.size();
	const bool simulationRestarted = m_snapshot && snapshot &&
		(snapshot->currentTime < m_snapshot->currentTime ||
			(snapshot->state == SimulationState::Ready &&
				m_snapshot->state != SimulationState::Ready));
	if (!configurationChanged && !simulationRestarted)
		AdvanceVisualPositions(now);
	m_snapshot = std::move(snapshot);
	if (!m_snapshot)
	{
		m_elevatorVisualStates.clear();
		m_animationClockInitialized = false;
		Invalidate(FALSE);
		return;
	}
	if (m_snapshot->elevators.size() <= DetailedElevatorLimit ||
		m_snapshot->elevators.size() != previousElevatorCount)
	{
		m_selectedGroup = -1;
	}
	if (m_snapshot->config.floorCount != previousFloorCount)
		FitAllFloors();
	if (configurationChanged || simulationRestarted)
		InitializeVisualPositions(now);
	else
		UpdateVisualTargets();
	if (!m_snapshot->elevators.empty())
	{
		const bool selectedElevatorExists = std::any_of(m_snapshot->elevators.begin(),
			m_snapshot->elevators.end(), [this](const ElevatorSnapshot& elevator)
			{
				return elevator.id == m_selectedElevatorId;
			});
		if (!selectedElevatorExists)
			m_selectedElevatorId = InvalidElevatorId;
	}
	Invalidate(FALSE);
}

int ElevatorBuildingView::GetSelectedElevatorId() const noexcept
{
	return m_selectedElevatorId;
}

void ElevatorBuildingView::OnPaint()
{
	CPaintDC paintDc(this);
	CRect client;
	GetClientRect(&client);
	if (client.IsRectEmpty()) return;
	AdvanceVisualPositions(std::chrono::steady_clock::now());

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
	m_elevatorHitAreas.clear();
	m_backHitRect.SetRectEmpty();
	m_zoomFitHitRect.SetRectEmpty();
	m_zoomInHitRect.SetRectEmpty();
	m_zoomOutHitRect.SetRectEmpty();
	m_floorScaleHitRect.SetRectEmpty();

	if (!m_snapshot)
	{
		CRect messageRect = client;
		dc.SetTextColor(MutedTextColor);
		dc.DrawText(L"等待仿真 Snapshot…", messageRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		return;
	}

	CRect content = client;
	content.DeflateRect(10, 8);
	DrawZoomControls(dc, content);
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
	return FloorY(static_cast<double>(floor), plot);
}

int ElevatorBuildingView::FloorY(double floor, const CRect& plot) const
{
	const double position = (floor - static_cast<double>(m_visibleFloorMin)) /
		static_cast<double>(m_visibleFloorMax - m_visibleFloorMin);
	return plot.bottom - static_cast<int>(std::lround(position * plot.Height()));
}

int ElevatorBuildingView::FloorAtY(int y, const CRect& plot) const
{
	const double position = static_cast<double>(plot.bottom - y) /
		static_cast<double>(plot.Height());
	const int floor = m_visibleFloorMin + static_cast<int>(std::lround(
		position * (m_visibleFloorMax - m_visibleFloorMin)));
	return (std::max)(m_visibleFloorMin, (std::min)(floor, m_visibleFloorMax));
}

int ElevatorBuildingView::FloorLabelStep() const
{
	const int visibleFloorCount = m_visibleFloorMax - m_visibleFloorMin + 1;
	if (visibleFloorCount <= 30) return 1;
	if (visibleFloorCount <= 40) return 2;
	if (visibleFloorCount <= 60) return 5;
	return 10;
}

bool ElevatorBuildingView::IsLargeScaleMode() const
{
	return m_snapshot->config.floorCount > 80 || m_snapshot->elevators.size() > 30;
}

double ElevatorBuildingView::VisualFloor(int elevatorId) const
{
	return m_elevatorVisualStates[static_cast<std::size_t>(elevatorId)].visualFloor;
}

void ElevatorBuildingView::AdvanceVisualPositions(std::chrono::steady_clock::time_point now)
{
	if (!m_animationClockInitialized)
	{
		m_lastAnimationUpdate = now;
		m_animationClockInitialized = true;
		return;
	}

	const double elapsedSeconds = std::chrono::duration<double>(
		now - m_lastAnimationUpdate).count();
	m_lastAnimationUpdate = now;
	if (!m_snapshot || m_snapshot->state == SimulationState::Paused ||
		m_snapshot->state == SimulationState::Ready ||
		m_snapshot->state == SimulationState::Uninitialized)
	{
		return;
	}

	for (auto& visualState : m_elevatorVisualStates)
	{
		const double remaining = visualState.targetFloor - visualState.visualFloor;
		const double step = visualState.speedFloorsPerSecond * elapsedSeconds;
		if (std::abs(remaining) <= step)
		{
			visualState.visualFloor = visualState.targetFloor;
			visualState.speedFloorsPerSecond = 0.0;
		}
		else
		{
			visualState.visualFloor += std::copysign(step, remaining);
		}
	}
}

void ElevatorBuildingView::InitializeVisualPositions(
	std::chrono::steady_clock::time_point now)
{
	m_elevatorVisualStates.assign(m_snapshot->elevators.size(), ElevatorVisualState{});
	for (const auto& elevator : m_snapshot->elevators)
	{
		auto& visualState = m_elevatorVisualStates[static_cast<std::size_t>(elevator.id)];
		visualState.visualFloor = static_cast<double>(elevator.currentFloor);
		visualState.targetFloor = visualState.visualFloor;
	}
	m_lastAnimationUpdate = now;
	m_animationClockInitialized = true;
}

void ElevatorBuildingView::UpdateVisualTargets()
{
	for (const auto& elevator : m_snapshot->elevators)
	{
		auto& visualState = m_elevatorVisualStates[static_cast<std::size_t>(elevator.id)];
		const double targetFloor = static_cast<double>(elevator.currentFloor);
		if (visualState.targetFloor == targetFloor) continue;

		visualState.targetFloor = targetFloor;
		const double distance = std::abs(targetFloor - visualState.visualFloor);
		const double durationSeconds = distance <= 1.0
			? 0.15 : (std::max)(0.06, 0.15 / distance);
		visualState.speedFloorsPerSecond = distance / durationSeconds;
	}
}

void ElevatorBuildingView::DrawFloorScale(CDC& dc, const CRect& plot) const
{
	const int visibleFloorCount = m_visibleFloorMax - m_visibleFloorMin + 1;
	std::vector<std::size_t> upWaiting(static_cast<std::size_t>(visibleFloorCount), 0);
	std::vector<std::size_t> downWaiting(static_cast<std::size_t>(visibleFloorCount), 0);
	for (int floor = m_visibleFloorMin; floor <= m_visibleFloorMax; ++floor)
	{
		const auto& floorSnapshot = m_snapshot->floors[static_cast<std::size_t>(floor - 1)];
		const std::size_t index = static_cast<std::size_t>(floor - m_visibleFloorMin);
		upWaiting[index] = floorSnapshot.upWaitingCount;
		downWaiting[index] = floorSnapshot.downWaitingCount;
	}
	for (const auto& hallCall : m_snapshot->hallCalls)
	{
		if (hallCall.floorNumber < m_visibleFloorMin ||
			hallCall.floorNumber > m_visibleFloorMax)
		{
			continue;
		}
		const std::size_t index = static_cast<std::size_t>(
			hallCall.floorNumber - m_visibleFloorMin);
		auto& waiting = hallCall.direction == Direction::Up
			? upWaiting[index] : downWaiting[index];
		waiting = (std::max)(waiting, hallCall.waitingCount);
	}

	const int labelStep = FloorLabelStep();
	for (int floor = m_visibleFloorMin; floor <= m_visibleFloorMax; ++floor)
	{
		const std::size_t index = static_cast<std::size_t>(floor - m_visibleFloorMin);
		const std::size_t upCount = upWaiting[index];
		const std::size_t downCount = downWaiting[index];
		const bool active = upCount > 0 || downCount > 0;
		const bool major = floor == m_visibleFloorMin || floor == m_visibleFloorMax ||
			floor % labelStep == 0;
		const bool drawLine = visibleFloorCount <= 60 || major || active;
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

void ElevatorBuildingView::DrawZoomControls(CDC& dc, const CRect& content)
{
	const int top = content.top;
	const int right = content.right;
	m_zoomOutHitRect.SetRect(right - 54, top, right, top + 30);
	m_zoomInHitRect.SetRect(right - 112, top, right - 58, top + 30);
	m_zoomFitHitRect.SetRect(right - 184, top, right - 116, top + 30);

	const bool fit = m_visibleFloorMin == 1 &&
		m_visibleFloorMax == m_snapshot->config.floorCount;
	const CRect buttons[] = { m_zoomFitHitRect, m_zoomInHitRect, m_zoomOutHitRect };
	const wchar_t* labels[] = { L"全楼 / Fit", L"放大 +", L"缩小 -" };
	for (int index = 0; index < 3; ++index)
	{
		const bool active = index == 0 && fit;
		dc.FillSolidRect(buttons[index], active ? AccentFillColor : SurfaceColor);
		dc.Draw3dRect(buttons[index], active ? AccentColor : MajorLineColor,
			active ? AccentColor : MajorLineColor);
		CRect textRect = buttons[index];
		dc.SetTextColor(active ? AccentColor : TextColor);
		dc.DrawText(labels[index], textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
	}
	dc.SetTextColor(TextColor);
}

CString ElevatorBuildingView::VisibleFloorText() const
{
	CString text;
	text.Format(L"%dF-%dF", m_visibleFloorMin, m_visibleFloorMax);
	return text;
}

void ElevatorBuildingView::FitAllFloors()
{
	m_visibleFloorMin = 1;
	m_visibleFloorMax = m_snapshot->config.floorCount;
	m_zoomCenterFloor = (m_visibleFloorMin + m_visibleFloorMax) / 2;
}

void ElevatorBuildingView::SetVisibleFloorCount(int floorCount)
{
	const int totalFloorCount = m_snapshot->config.floorCount;
	floorCount = (std::max)((std::min)(MinimumVisibleFloorCount, totalFloorCount),
		(std::min)(floorCount, totalFloorCount));
	int minimum = m_zoomCenterFloor - (floorCount - 1) / 2;
	int maximum = minimum + floorCount - 1;
	if (minimum < 1)
	{
		maximum += 1 - minimum;
		minimum = 1;
	}
	if (maximum > totalFloorCount)
	{
		minimum -= maximum - totalFloorCount;
		maximum = totalFloorCount;
	}
	m_visibleFloorMin = minimum;
	m_visibleFloorMax = maximum;
	m_zoomCenterFloor = (minimum + maximum + 1) / 2;
}

void ElevatorBuildingView::ZoomIn()
{
	const int currentFloorCount = m_visibleFloorMax - m_visibleFloorMin + 1;
	SetVisibleFloorCount((currentFloorCount + 1) / 2);
}

void ElevatorBuildingView::ZoomOut()
{
	const int currentFloorCount = m_visibleFloorMax - m_visibleFloorMin + 1;
	SetVisibleFloorCount(currentFloorCount * 2);
}

void ElevatorBuildingView::SelectElevator(int elevatorId)
{
	if (m_selectedElevatorId == elevatorId) return;
	m_selectedElevatorId = elevatorId;
	Invalidate(FALSE);
	GetTopLevelParent()->SendMessage(WM_ELEVATOR_SELECTION_CHANGED,
		static_cast<WPARAM>(elevatorId), 0);
}

void ElevatorBuildingView::DrawDetailed(CDC& dc, const CRect& content,
	int firstElevator, int elevatorCount)
{
	CString title;
	if (m_snapshot->elevators.size() <= DetailedElevatorLimit)
	{
		title.Format(L"%s · %d 台电梯 · 详细模式", VisibleFloorText(), elevatorCount);
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
		title.Format(L"%s · E%d-E%d · %s · 详细模式", GroupName(m_selectedGroup),
			firstElevator + 1, lastElevator, VisibleFloorText());
	}

	CRect titleRect(content.left + (m_backHitRect.IsRectEmpty() ? 0 : 124),
		content.top, m_zoomFitHitRect.left - 8, content.top + 30);
	dc.SetTextColor(TextColor);
	dc.DrawText(title, titleRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

	CRect plot(content.left + 108, content.top + 48, content.right - 8, content.bottom - 28);
	m_floorScaleHitRect.SetRect(content.left, plot.top, plot.left, plot.bottom);
	DrawFloorScale(dc, plot);
	const bool largeScaleMode = IsLargeScaleMode();
	const int shaftWidth = plot.Width() / elevatorCount;
	for (int localIndex = 0; localIndex < elevatorCount; ++localIndex)
	{
		const auto& elevator = m_snapshot->elevators[
			static_cast<std::size_t>(firstElevator + localIndex)];
		const int shaftLeft = plot.left + localIndex * shaftWidth;
		const int shaftRight = localIndex + 1 == elevatorCount
			? plot.right : shaftLeft + shaftWidth;
		const int centerX = (shaftLeft + shaftRight) / 2;
		const bool selected = elevator.id == m_selectedElevatorId;
		CRect shaftHitRect(shaftLeft + 2, content.top + 30, shaftRight - 2, plot.bottom);
		m_elevatorHitAreas.push_back({ elevator.id, shaftHitRect });
		if (selected)
		{
			CPen selectionPen(PS_SOLID, 2, AccentColor);
			CPen* previousPen = dc.SelectObject(&selectionPen);
			CBrush* previousBrush = static_cast<CBrush*>(dc.SelectStockObject(NULL_BRUSH));
			dc.Rectangle(shaftHitRect);
			dc.SelectObject(previousBrush);
			dc.SelectObject(previousPen);
		}

		CRect elevatorLabel(shaftLeft, content.top + 30, shaftRight, content.top + 48);
		CString name;
		name.Format(L"E%d", elevator.id + 1);
		dc.SetTextColor(selected ? AccentColor : TextColor);
		dc.DrawText(name, elevatorLabel, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

		CPen shaftPen(PS_SOLID, 1, MajorLineColor);
		CPen* previousPen = dc.SelectObject(&shaftPen);
		dc.MoveTo(centerX, plot.top);
		dc.LineTo(centerX, plot.bottom);
		dc.SelectObject(previousPen);
		const double visualFloor = VisualFloor(elevator.id);
		if (visualFloor < static_cast<double>(m_visibleFloorMin) ||
			visualFloor > static_cast<double>(m_visibleFloorMax))
		{
			continue;
		}

		const int carWidth = (std::max)(26, (std::min)(64, shaftRight - shaftLeft - 8));
		const int carHeight = 42;
		int carTop = FloorY(visualFloor, plot) - carHeight / 2;
		carTop = (std::max)(static_cast<int>(plot.top),
			(std::min)(carTop, static_cast<int>(plot.bottom) - carHeight));
		CRect car(centerX - carWidth / 2, carTop,
			centerX + carWidth / 2, carTop + carHeight);
		dc.FillSolidRect(car, selected ? AccentColor : AccentFillColor);
		dc.Draw3dRect(car, AccentColor, AccentColor);
		CString carText;
		const wchar_t* movementText = DirectionText(elevator.direction);
		if (selected && elevator.state == ElevatorState::Boarding)
			movementText = L"Boarding";
		else if (selected && elevator.state == ElevatorState::Alighting)
			movementText = L"Alighting";
		if (largeScaleMode && !selected)
			carText = movementText;
		else
			carText.Format(L"%dF %s\n%d/%d", elevator.currentFloor,
				movementText, elevator.passengerCount, elevator.capacity);
		CRect carTextRect = car;
		dc.SetTextColor(selected ? SurfaceColor : TextColor);
		dc.DrawText(carText, carTextRect, DT_CENTER | DT_VCENTER);
	}
	dc.SetTextColor(TextColor);
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
	CRect titleRect(content.left, content.top, m_zoomFitHitRect.left - 8, content.top + 30);
	CString title;
	title.Format(L"%s · %d 台电梯 · %d 个分组    点击分组查看完整井道",
		VisibleFloorText(), elevatorCount, groupCount);
	dc.DrawText(title, titleRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

	CRect plot(content.left + 108, content.top + 52, content.right - 8, content.bottom - 28);
	m_floorScaleHitRect.SetRect(content.left, plot.top, plot.left, plot.bottom);
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
			const double visualFloor = VisualFloor(elevator.id);
			if (visualFloor < static_cast<double>(m_visibleFloorMin) ||
				visualFloor > static_cast<double>(m_visibleFloorMax))
			{
				continue;
			}
			const int markerX = count == 1 ? groupRect.CenterPoint().x
				: markerAreaLeft + localIndex * markerAreaWidth / (count - 1);
			const int markerY = FloorY(visualFloor, plot);
			const bool selected = elevator.id == m_selectedElevatorId;
			CRect marker(markerX - (selected ? 5 : 3), markerY - (selected ? 6 : 4),
				markerX + (selected ? 6 : 4), markerY + (selected ? 7 : 5));
			dc.FillSolidRect(marker, selected ? ActivityColor : AccentColor);
		}
	}
	dc.SetTextColor(TextColor);
}

void ElevatorBuildingView::OnLButtonUp(UINT flags, CPoint point)
{
	SetFocus();
	if (m_zoomFitHitRect.PtInRect(point))
	{
		FitAllFloors();
		Invalidate(FALSE);
	}
	else if (m_zoomInHitRect.PtInRect(point))
	{
		ZoomIn();
		Invalidate(FALSE);
	}
	else if (m_zoomOutHitRect.PtInRect(point))
	{
		ZoomOut();
		Invalidate(FALSE);
	}
	else if (m_selectedGroup >= 0 && m_backHitRect.PtInRect(point))
	{
		m_selectedGroup = -1;
		Invalidate(FALSE);
	}
	else if (m_selectedGroup < 0 && !m_groupHitRects.empty())
	{
		for (std::size_t index = 0; index < m_groupHitRects.size(); ++index)
		{
			if (!m_groupHitRects[index].PtInRect(point)) continue;
			m_selectedGroup = static_cast<int>(index);
			Invalidate(FALSE);
			break;
		}
	}
	else
	{
		for (const auto& hitArea : m_elevatorHitAreas)
		{
			if (!hitArea.rect.PtInRect(point)) continue;
			SelectElevator(hitArea.elevatorId);
			CWnd::OnLButtonUp(flags, point);
			return;
		}
	}
	if (m_floorScaleHitRect.PtInRect(point))
		m_zoomCenterFloor = FloorAtY(point.y, m_floorScaleHitRect);
	CWnd::OnLButtonUp(flags, point);
}

BOOL ElevatorBuildingView::OnSetCursor(CWnd* window, UINT hitTest, UINT message)
{
	CPoint point;
	GetCursorPos(&point);
	ScreenToClient(&point);
	if (m_zoomFitHitRect.PtInRect(point) || m_zoomInHitRect.PtInRect(point) ||
		m_zoomOutHitRect.PtInRect(point) || m_floorScaleHitRect.PtInRect(point) ||
		(m_selectedGroup >= 0 && m_backHitRect.PtInRect(point)) ||
		std::any_of(m_groupHitRects.begin(), m_groupHitRects.end(),
			[point](const CRect& rect) { return rect.PtInRect(point); }) ||
		std::any_of(m_elevatorHitAreas.begin(), m_elevatorHitAreas.end(),
			[point](const ElevatorHitArea& area) { return area.rect.PtInRect(point); }))
	{
		::SetCursor(::LoadCursor(nullptr, IDC_HAND));
		return TRUE;
	}
	return CWnd::OnSetCursor(window, hitTest, message);
}

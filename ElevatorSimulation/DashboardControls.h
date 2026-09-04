#pragma once

#include "Resource.h"

#include <afxcmn.h>
#include <afxwin.h>
#include <cstddef>
#include <cwchar>

class DashboardStatTitle : public CStatic
{
protected:
    LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override
    {
        if (message == WM_ERASEBKGND) return TRUE;
        if (message != WM_PAINT)
            return CStatic::WindowProc(message, wParam, lParam);

        CPaintDC paintDc(this);
        CRect client;
        GetClientRect(&client);
        if (client.IsRectEmpty()) return 0;

        CDC dc;
        dc.CreateCompatibleDC(&paintDc);
        CBitmap bitmap;
        bitmap.CreateCompatibleBitmap(&paintDc, client.Width(), client.Height());
        CBitmap* oldBitmap = dc.SelectObject(&bitmap);

        dc.FillSolidRect(client, ::GetSysColor(COLOR_3DFACE));
        dc.SetBkMode(TRANSPARENT);
        if (GetFont() != nullptr) dc.SelectObject(GetFont());

        static const wchar_t* Chinese[] = {
            L"总生成", L"等待中", L"乘梯中", L"已到达", L"平均等待", L"最大等待"
        };
        static const wchar_t* English[] = {
            L"Generated", L"Waiting", L"Riding", L"Arrived", L"Avg Wait", L"Max Wait"
        };

        const int index = GetDlgCtrlID() - IDC_STAT_TITLE_FIRST;
        if (index >= 0 && index < 6)
        {
            const int split = static_cast<int>(client.top) + client.Height() / 2;
            CRect top = client;
            CRect bottom = client;
            top.bottom = split + 1;
            bottom.top = split - 1;

            dc.SetTextColor(RGB(45, 52, 62));
            dc.DrawTextW(Chinese[index], top,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            dc.SetTextColor(RGB(105, 112, 122));
            dc.DrawTextW(English[index], bottom,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }

        paintDc.BitBlt(0, 0, client.Width(), client.Height(), &dc, 0, 0, SRCCOPY);
        dc.SelectObject(oldBitmap);
        return 0;
    }
};

class HallCallDashboardList : public CListCtrl
{
protected:
    LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override
    {
        const LRESULT result = CListCtrl::WindowProc(message, wParam, lParam);
        if (message == WM_PAINT && GetSafeHwnd() != nullptr)
        {
            CClientDC dc(this);
            DrawSummary(dc);
        }
        return result;
    }

private:
    void DrawSummary(CDC& dc)
    {
        CRect client;
        GetClientRect(&client);
        if (client.Width() < 180 || client.Height() < 220) return;

        int contentTop = 10;
        if (CHeaderCtrl* header = GetHeaderCtrl())
        {
            CRect headerRect;
            header->GetWindowRect(&headerRect);
            ScreenToClient(&headerRect);
            const int candidate = static_cast<int>(headerRect.bottom) + 10;
            if (candidate > contentTop) contentTop = candidate;
        }

        const int itemCount = GetItemCount();
        if (itemCount > 0)
        {
            CRect lastItem;
            if (GetItemRect(itemCount - 1, &lastItem, LVIR_BOUNDS))
            {
                const int candidate = static_cast<int>(lastItem.bottom) + 16;
                if (candidate > contentTop) contentTop = candidate;
            }
        }

        const int clientBottom = static_cast<int>(client.bottom);
        if (clientBottom - contentTop < 170) return;

        int cardBottom = contentTop + 198;
        if (cardBottom > clientBottom - 12) cardBottom = clientBottom - 12;
        CRect card;
        card.SetRect(static_cast<int>(client.left) + 10, contentTop,
            static_cast<int>(client.right) - 10, cardBottom);
        if (card.Height() < 160) return;

        dc.FillSolidRect(card, RGB(248, 250, 252));
        CPen borderPen(PS_SOLID, 1, RGB(210, 216, 224));
        CPen* oldPen = dc.SelectObject(&borderPen);
        dc.Rectangle(card);
        dc.SelectObject(oldPen);
        dc.SetBkMode(TRANSPARENT);
        if (GetFont() != nullptr) dc.SelectObject(GetFont());

        CRect titleRect = card;
        titleRect.DeflateRect(12, 8, 12, 0);
        titleRect.bottom = titleRect.top + 24;
        dc.SetTextColor(RGB(35, 42, 52));
        dc.DrawTextW(L"外呼概览 / Hall Call Summary", titleRect,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        std::size_t totalWaiting = 0;
        int assigned = 0;
        for (int row = 0; row < itemCount; ++row)
        {
            totalWaiting += static_cast<std::size_t>(_wtoi(GetItemText(row, 2)));
            const CString owner = GetItemText(row, 3);
            if (!owner.IsEmpty() && owner != L"未分配") ++assigned;
        }
        const int unassigned = itemCount - assigned;

        CString traffic = L"--";
        if (CWnd* parent = GetParent())
        {
            if (CWnd* trafficControl = parent->GetDlgItem(IDC_COMBO_TRAFFIC_PATTERN))
            {
                CString value;
                trafficControl->GetWindowTextW(value);
                if (!value.IsEmpty()) traffic = value;
            }
        }

        const int bodyTop = static_cast<int>(titleRect.bottom) + 8;
        const int half = card.Width() / 2;
        CRect left;
        left.SetRect(static_cast<int>(card.left) + 12, bodyTop,
            static_cast<int>(card.left) + half - 4, bodyTop + 62);
        CRect right;
        right.SetRect(static_cast<int>(card.left) + half + 4, bodyTop,
            static_cast<int>(card.right) - 12, bodyTop + 62);

        CString leftText;
        leftText.Format(L"当前外呼  %d\r\n等待乘客  %zu", itemCount, totalWaiting);
        CString rightText;
        rightText.Format(L"已分配  %d\r\n未分配  %d", assigned, unassigned);
        dc.SetTextColor(RGB(58, 66, 77));
        dc.DrawTextW(leftText, left, DT_LEFT | DT_TOP | DT_NOPREFIX);
        dc.DrawTextW(rightText, right, DT_LEFT | DT_TOP | DT_NOPREFIX);

        CRect trafficRect;
        trafficRect.SetRect(static_cast<int>(card.left) + 12, bodyTop + 68,
            static_cast<int>(card.right) - 12, bodyTop + 94);
        CString trafficText;
        trafficText.Format(L"当前客流：%s", traffic.GetString());
        dc.SetTextColor(RGB(45, 83, 128));
        dc.DrawTextW(trafficText, trafficRect,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        CRect hintRect;
        hintRect.SetRect(static_cast<int>(card.left) + 12, bodyTop + 100,
            static_cast<int>(card.right) - 12, static_cast<int>(card.bottom) - 8);
        dc.SetTextColor(RGB(112, 120, 130));
        dc.DrawTextW(L"提示：点击任一外呼，可查看 ETA / Cost 候选及当前归属说明。",
            hintRect, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
    }
};

class DashboardRightTabs : public CTabCtrl
{
};

class ElevatorDetailDashboard : public CStatic
{
protected:
    LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override
    {
        if (message == WM_WINDOWPOSCHANGING)
        {
            WINDOWPOS* position = reinterpret_cast<WINDOWPOS*>(lParam);
            if (position != nullptr && (position->flags & SWP_NOSIZE) == 0)
            {
                if (CWnd* parent = GetParent())
                {
                    CRect parentClient;
                    parent->GetClientRect(&parentClient);
                    const int available = static_cast<int>(parentClient.bottom) - position->y - 24;
                    if (available > position->cy) position->cy = available;
                }
            }
            return CStatic::WindowProc(message, wParam, lParam);
        }
        if (message == WM_SETTEXT)
        {
            const wchar_t* incoming = reinterpret_cast<const wchar_t*>(lParam);
            const CString next = incoming != nullptr ? incoming : L"";
            if (next != m_sourceText)
            {
                m_sourceText = next;
                Invalidate(FALSE);
            }
            return TRUE;
        }
        if (message == WM_GETTEXTLENGTH)
            return static_cast<LRESULT>(m_sourceText.GetLength());
        if (message == WM_GETTEXT)
        {
            if (lParam == 0 || wParam == 0) return 0;
            wchar_t* buffer = reinterpret_cast<wchar_t*>(lParam);
            const int capacity = static_cast<int>(wParam);
            const int count = m_sourceText.GetLength() < capacity - 1 ?
                m_sourceText.GetLength() : capacity - 1;
            if (count > 0) std::wmemcpy(buffer, m_sourceText.GetString(), count);
            buffer[count] = L'\0';
            return count;
        }
        if (message == WM_ERASEBKGND)
            return TRUE;
        if (message == WM_LBUTTONUP)
        {
            const int x = static_cast<short>(LOWORD(lParam));
            const int y = static_cast<short>(HIWORD(lParam));
            if (m_backRect.PtInRect(CPoint(x, y)))
            {
                ReturnToHallCalls();
                return 0;
            }
        }
        if (message == WM_SETCURSOR)
        {
            CPoint point;
            ::GetCursorPos(&point);
            ScreenToClient(&point);
            if (m_backRect.PtInRect(point))
            {
                ::SetCursor(::LoadCursor(nullptr, IDC_HAND));
                return TRUE;
            }
        }
        if (message == WM_PAINT)
        {
            PaintDashboard();
            return 0;
        }
        return CStatic::WindowProc(message, wParam, lParam);
    }

private:
    CString m_sourceText;
    CRect m_backRect;

    static CString ExtractField(const CString& source, const wchar_t* label)
    {
        const int labelPosition = source.Find(label);
        if (labelPosition < 0) return L"--";
        const int start = labelPosition + static_cast<int>(std::wcslen(label));
        int end = source.Find(L"\r\n", start);
        if (end < 0) end = source.GetLength();
        CString value = source.Mid(start, end - start);
        value.Trim();
        return value.IsEmpty() ? CString(L"--") : value;
    }

    void PaintDashboard()
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

        dc.FillSolidRect(client, ::GetSysColor(COLOR_3DFACE));
        dc.SetBkMode(TRANSPARENT);
        if (GetFont() != nullptr) dc.SelectObject(GetFont());

        const CString floor = ExtractField(m_sourceText, L"当前楼层：");
        const CString direction = ExtractField(m_sourceText, L"方向：");
        const CString state = ExtractField(m_sourceText, L"状态：");
        const CString load = ExtractField(m_sourceText, L"载客：");

        m_backRect.SetRect(static_cast<int>(client.left) + 4,
            static_cast<int>(client.top) + 4,
            static_cast<int>(client.right) - 4,
            static_cast<int>(client.top) + 38);
        dc.FillSolidRect(m_backRect, RGB(245, 248, 252));
        CPen buttonPen(PS_SOLID, 1, RGB(194, 204, 218));
        CPen* oldPen = dc.SelectObject(&buttonPen);
        dc.Rectangle(m_backRect);
        dc.SelectObject(oldPen);
        dc.SetTextColor(RGB(48, 89, 145));
        dc.DrawTextW(L"←  返回 Hall Call / 外呼列表", m_backRect,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        CRect sectionTitle;
        sectionTitle.SetRect(static_cast<int>(client.left) + 4,
            static_cast<int>(m_backRect.bottom) + 15,
            static_cast<int>(client.right) - 4,
            static_cast<int>(m_backRect.bottom) + 40);
        dc.SetTextColor(RGB(35, 42, 52));
        dc.DrawTextW(L"实时状态 / Live Status", sectionTitle,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        const int gap = 8;
        const int left = static_cast<int>(client.left) + 4;
        const int right = static_cast<int>(client.right) - 4;
        const int cellWidth = (right - left - gap) / 2;
        const int firstTop = static_cast<int>(sectionTitle.bottom) + 6;
        const int cellHeight = 64;

        DrawMetric(dc, CRect(left, firstTop,
            left + cellWidth, firstTop + cellHeight), L"当前楼层", floor);
        DrawMetric(dc, CRect(left + cellWidth + gap, firstTop,
            right, firstTop + cellHeight), L"运行方向", direction);
        DrawMetric(dc, CRect(left, firstTop + cellHeight + gap,
            left + cellWidth, firstTop + cellHeight * 2 + gap), L"运行状态", state);
        DrawMetric(dc, CRect(left + cellWidth + gap, firstTop + cellHeight + gap,
            right, firstTop + cellHeight * 2 + gap), L"载客情况", load);

        int cardTop = firstTop + cellHeight * 2 + gap + 14;
        CRect taskCard(left, cardTop, right, cardTop + 86);
        DrawCardBorder(dc, taskCard);
        CRect taskText = taskCard;
        taskText.DeflateRect(10, 8, 10, 6);
        CString task;
        if (state.CompareNoCase(L"Idle") == 0)
            task = L"当前任务\r\n空闲待命，等待新的群控分配。";
        else if (state.Find(L"Moving") >= 0)
            task = L"当前任务\r\n正在执行 LOOK 方向保持运行；新请求不会打断当前楼层间动作。";
        else if (state.CompareNoCase(L"Boarding") == 0)
            task = L"当前任务\r\n正在执行乘客登梯服务。";
        else if (state.CompareNoCase(L"Alighting") == 0)
            task = L"当前任务\r\n正在执行乘客离梯服务。";
        else
            task = L"当前任务\r\n正在处理当前停站服务。";
        dc.SetTextColor(RGB(55, 64, 75));
        dc.DrawTextW(task, taskText, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);

        cardTop = static_cast<int>(taskCard.bottom) + 10;
        CRect groupCard(left, cardTop, right, cardTop + 104);
        DrawCardBorder(dc, groupCard);
        CRect groupText = groupCard;
        groupText.DeflateRect(10, 8, 10, 6);
        dc.SetTextColor(RGB(55, 64, 75));
        dc.DrawTextW(
            L"群控参与\r\n该电梯作为候选参与事件级 ETA / Cost 评分。实际外呼归属还会受到联合调度、动态改派与滞回策略影响。",
            groupText, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);

        CString traffic = L"--";
        if (CWnd* parent = GetParent())
        {
            if (CWnd* trafficControl = parent->GetDlgItem(IDC_COMBO_TRAFFIC_PATTERN))
            {
                CString value;
                trafficControl->GetWindowTextW(value);
                if (!value.IsEmpty()) traffic = value;
            }
        }
        cardTop = static_cast<int>(groupCard.bottom) + 10;
        CRect trafficRect(left, cardTop, right, cardTop + 48);
        CString trafficText;
        trafficText.Format(L"当前客流模式：%s", traffic.GetString());
        dc.SetTextColor(RGB(45, 83, 128));
        dc.DrawTextW(trafficText, trafficRect,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        paintDc.BitBlt(0, 0, client.Width(), client.Height(), &dc, 0, 0, SRCCOPY);
        dc.SelectObject(oldBitmap);
    }

    static void DrawCardBorder(CDC& dc, const CRect& bounds)
    {
        dc.FillSolidRect(bounds, RGB(248, 250, 252));
        CPen pen(PS_SOLID, 1, RGB(216, 222, 230));
        CPen* oldPen = dc.SelectObject(&pen);
        dc.Rectangle(bounds);
        dc.SelectObject(oldPen);
    }

    static void DrawMetric(CDC& dc, const CRect& bounds,
        const wchar_t* label, const CString& value)
    {
        dc.FillSolidRect(bounds, RGB(250, 251, 253));
        CPen pen(PS_SOLID, 1, RGB(218, 224, 232));
        CPen* oldPen = dc.SelectObject(&pen);
        dc.Rectangle(bounds);
        dc.SelectObject(oldPen);

        CRect labelRect = bounds;
        labelRect.DeflateRect(8, 6, 8, 0);
        labelRect.bottom = labelRect.top + 20;
        dc.SetTextColor(RGB(102, 111, 123));
        dc.DrawTextW(label, labelRect,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        CRect valueRect = bounds;
        valueRect.DeflateRect(8, 25, 8, 5);
        dc.SetTextColor(RGB(32, 40, 51));
        dc.DrawTextW(value, valueRect,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    void ReturnToHallCalls()
    {
        CWnd* parent = GetParent();
        if (parent == nullptr) return;
        CWnd* tabsWindow = parent->GetDlgItem(IDC_TAB_RIGHT);
        if (tabsWindow == nullptr) return;

        CTabCtrl* tabs = static_cast<CTabCtrl*>(tabsWindow);
        tabs->SetCurSel(0);

        NMHDR notification{};
        notification.hwndFrom = tabs->GetSafeHwnd();
        notification.idFrom = IDC_TAB_RIGHT;
        notification.code = TCN_SELCHANGE;
        parent->SendMessage(WM_NOTIFY, IDC_TAB_RIGHT,
            reinterpret_cast<LPARAM>(&notification));
    }
};

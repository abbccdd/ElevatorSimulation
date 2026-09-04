#pragma once

#include "Resource.h"

#include <afxcmn.h>
#include <afxwin.h>
#include <cwchar>

// Large, dense right-side elevator detail panel.
// Keeps the original dialog data flow: SetWindowText updates a local read-only cache.
// The control owns all painting to avoid native Static text flicker.
class ElevatorDetailDashboardLarge : public CStatic
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
                    const int available = static_cast<int>(parentClient.bottom) - position->y - 20;
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
            const int length = m_sourceText.GetLength();
            const int count = length < capacity - 1 ? length : capacity - 1;
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
    CFont m_buttonFont;
    CFont m_sectionFont;
    CFont m_labelFont;
    CFont m_valueFont;
    CFont m_bodyFont;
    bool m_fontsReady = false;

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

    void EnsureFonts()
    {
        if (m_fontsReady) return;

        LOGFONT base{};
        CFont* current = GetFont();
        if (current != nullptr && current->GetSafeHandle() != nullptr)
            current->GetLogFont(&base);
        else
            ::GetObject(::GetStockObject(DEFAULT_GUI_FONT), sizeof(LOGFONT), &base);

        auto makeFont = [&base](CFont& font, int numerator, int denominator, LONG weight)
        {
            LOGFONT lf = base;
            lf.lfHeight = base.lfHeight * numerator / denominator;
            if (lf.lfHeight == 0) lf.lfHeight = -14;
            lf.lfWeight = weight;
            font.CreateFontIndirect(&lf);
        };

        makeFont(m_buttonFont, 6, 5, FW_SEMIBOLD);
        makeFont(m_sectionFont, 8, 5, FW_BOLD);
        makeFont(m_labelFont, 6, 5, FW_SEMIBOLD);
        makeFont(m_valueFont, 9, 5, FW_BOLD);
        makeFont(m_bodyFont, 6, 5, FW_NORMAL);
        m_fontsReady = true;
    }

    static int ParseLoadCount(const CString& load, int& capacity)
    {
        capacity = 0;
        const int slash = load.Find(L'/');
        if (slash < 0) return 0;
        CString left = load.Left(slash);
        CString right = load.Mid(slash + 1);
        left.Trim();
        right.Trim();
        capacity = _wtoi(right);
        return _wtoi(left);
    }

    void PaintDashboard()
    {
        CPaintDC paintDc(this);
        CRect client;
        GetClientRect(&client);
        if (client.IsRectEmpty()) return;

        EnsureFonts();

        CDC dc;
        dc.CreateCompatibleDC(&paintDc);
        CBitmap bitmap;
        bitmap.CreateCompatibleBitmap(&paintDc, client.Width(), client.Height());
        CBitmap* oldBitmap = dc.SelectObject(&bitmap);

        dc.FillSolidRect(client, RGB(247, 249, 252));
        dc.SetBkMode(TRANSPARENT);

        const CString floor = ExtractField(m_sourceText, L"当前楼层：");
        const CString direction = ExtractField(m_sourceText, L"方向：");
        const CString state = ExtractField(m_sourceText, L"状态：");
        const CString load = ExtractField(m_sourceText, L"载客：");

        const int left = static_cast<int>(client.left) + 6;
        const int right = static_cast<int>(client.right) - 6;
        const int bottom = static_cast<int>(client.bottom) - 6;
        const int gap = 10;

        m_backRect.SetRect(left, static_cast<int>(client.top) + 5, right,
            static_cast<int>(client.top) + 49);
        dc.FillSolidRect(m_backRect, RGB(239, 245, 253));
        DrawBorder(dc, m_backRect, RGB(180, 198, 220));
        CFont* oldFont = dc.SelectObject(&m_buttonFont);
        dc.SetTextColor(RGB(38, 82, 142));
        dc.DrawTextW(L"←  返回 Hall Call / 外呼列表", m_backRect,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        CRect sectionTitle(left, static_cast<int>(m_backRect.bottom) + 9,
            right, static_cast<int>(m_backRect.bottom) + 43);
        dc.SelectObject(&m_sectionFont);
        dc.SetTextColor(RGB(29, 38, 51));
        dc.DrawTextW(L"电梯实时详情 / Live Detail", sectionTitle,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        const int gridTop = static_cast<int>(sectionTitle.bottom) + 7;
        const int cellGap = 8;
        const int cellWidth = (right - left - cellGap) / 2;
        int cellHeight = 78;
        if (client.Height() > 620) cellHeight = 86;

        DrawMetric(dc, CRect(left, gridTop, left + cellWidth, gridTop + cellHeight),
            L"当前楼层", floor);
        DrawMetric(dc, CRect(left + cellWidth + cellGap, gridTop,
            right, gridTop + cellHeight), L"运行方向", direction);
        DrawMetric(dc, CRect(left, gridTop + cellHeight + cellGap,
            left + cellWidth, gridTop + cellHeight * 2 + cellGap),
            L"运行状态", state);
        DrawMetric(dc, CRect(left + cellWidth + cellGap, gridTop + cellHeight + cellGap,
            right, gridTop + cellHeight * 2 + cellGap),
            L"载客情况", load);

        int y = gridTop + cellHeight * 2 + cellGap + gap;
        const int loadBarHeight = 68;
        CRect loadCard(left, y, right, y + loadBarHeight);
        DrawLoadCard(dc, loadCard, load);
        y = static_cast<int>(loadCard.bottom) + gap;

        const int trafficHeight = 54;
        const int bottomTrafficTop = bottom - trafficHeight;
        const int usableForText = bottomTrafficTop - y - gap;
        int taskHeight = usableForText * 43 / 100;
        if (taskHeight < 88) taskHeight = 88;
        int groupHeight = usableForText - taskHeight - gap;
        if (groupHeight < 100)
        {
            groupHeight = 100;
            taskHeight = usableForText - groupHeight - gap;
            if (taskHeight < 76) taskHeight = 76;
        }

        CRect taskCard(left, y, right, y + taskHeight);
        DrawCardBorder(dc, taskCard);
        DrawTaskText(dc, taskCard, state);

        y = static_cast<int>(taskCard.bottom) + gap;
        CRect groupCard(left, y, right, bottomTrafficTop);
        DrawCardBorder(dc, groupCard);
        DrawGroupText(dc, groupCard);

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
        CRect trafficRect(left, bottomTrafficTop + gap / 2, right, bottom);
        dc.FillSolidRect(trafficRect, RGB(239, 245, 253));
        DrawBorder(dc, trafficRect, RGB(202, 214, 230));
        dc.SelectObject(&m_bodyFont);
        dc.SetTextColor(RGB(43, 82, 132));
        CString trafficText;
        trafficText.Format(L"当前客流模式：%s", traffic.GetString());
        dc.DrawTextW(trafficText, trafficRect,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        dc.SelectObject(oldFont);
        paintDc.BitBlt(0, 0, client.Width(), client.Height(), &dc, 0, 0, SRCCOPY);
        dc.SelectObject(oldBitmap);
    }

    void DrawMetric(CDC& dc, const CRect& bounds,
        const wchar_t* label, const CString& value)
    {
        dc.FillSolidRect(bounds, RGB(255, 255, 255));
        DrawBorder(dc, bounds, RGB(212, 220, 230));

        CRect labelRect = bounds;
        labelRect.DeflateRect(10, 7, 10, 0);
        labelRect.bottom = labelRect.top + 24;
        dc.SelectObject(&m_labelFont);
        dc.SetTextColor(RGB(91, 102, 116));
        dc.DrawTextW(label, labelRect,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        CRect valueRect = bounds;
        valueRect.DeflateRect(10, 29, 10, 7);
        dc.SelectObject(&m_valueFont);
        dc.SetTextColor(RGB(24, 33, 45));
        dc.DrawTextW(value, valueRect,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    void DrawLoadCard(CDC& dc, const CRect& bounds, const CString& load)
    {
        dc.FillSolidRect(bounds, RGB(255, 255, 255));
        DrawBorder(dc, bounds, RGB(212, 220, 230));

        int capacity = 0;
        const int count = ParseLoadCount(load, capacity);
        int percent = 0;
        if (capacity > 0)
        {
            percent = count * 100 / capacity;
            if (percent < 0) percent = 0;
            if (percent > 100) percent = 100;
        }

        CRect title = bounds;
        title.DeflateRect(10, 6, 10, 0);
        title.bottom = title.top + 24;
        dc.SelectObject(&m_labelFont);
        dc.SetTextColor(RGB(91, 102, 116));
        CString heading;
        heading.Format(L"载客率 / Occupancy    %d%%", percent);
        dc.DrawTextW(heading, title,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        CRect bar = bounds;
        bar.DeflateRect(12, 34, 12, 12);
        dc.FillSolidRect(bar, RGB(229, 234, 241));
        CRect fill = bar;
        fill.right = fill.left + bar.Width() * percent / 100;
        if (fill.right > fill.left)
            dc.FillSolidRect(fill, RGB(70, 119, 181));
        DrawBorder(dc, bar, RGB(199, 208, 220));
    }

    void DrawTaskText(CDC& dc, const CRect& bounds, const CString& state)
    {
        CString description;
        if (state.CompareNoCase(L"Idle") == 0)
            description = L"空闲待命。当前没有正在执行的楼层间动作，等待群控系统分配新的外呼或内部目标。";
        else if (state.Find(L"Moving") >= 0)
            description = L"正在执行 LOOK 方向保持运行。当前楼层间动作完成前，新请求不会强制改变运动方向。";
        else if (state.CompareNoCase(L"Boarding") == 0)
            description = L"正在执行乘客登梯服务。完成当前乘客登梯后再进入下一状态。";
        else if (state.CompareNoCase(L"Alighting") == 0)
            description = L"正在执行乘客离梯服务。完成当前乘客离梯后继续处理停站流程。";
        else
            description = L"正在处理当前停站服务，并将在服务完成后根据 LOOK 路线继续运行。";

        CRect text = bounds;
        text.DeflateRect(12, 9, 12, 8);
        dc.SelectObject(&m_labelFont);
        dc.SetTextColor(RGB(38, 48, 61));
        CRect heading = text;
        heading.bottom = heading.top + 27;
        dc.DrawTextW(L"当前任务 / Current Task", heading,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        CRect body = text;
        body.top = heading.bottom + 4;
        dc.SelectObject(&m_bodyFont);
        dc.SetTextColor(RGB(67, 77, 90));
        dc.DrawTextW(description, body,
            DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
    }

    void DrawGroupText(CDC& dc, const CRect& bounds)
    {
        CRect text = bounds;
        text.DeflateRect(12, 9, 12, 8);
        dc.SelectObject(&m_labelFont);
        dc.SetTextColor(RGB(38, 48, 61));
        CRect heading = text;
        heading.bottom = heading.top + 27;
        dc.DrawTextW(L"群控策略 / Dispatch Context", heading,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        CRect body = text;
        body.top = heading.bottom + 4;
        dc.SelectObject(&m_bodyFont);
        dc.SetTextColor(RGB(67, 77, 90));
        dc.DrawTextW(
            L"• 事件级 ETA / Cost：基于当前不可变快照计算候选评分\r\n"
            L"• LOOK：已有方向与任务未完成时保持运行方向\r\n"
            L"• Joint Dispatch：多个外呼可进行有限联合分配\r\n"
            L"• Reassignment：满足收益阈值后允许动态改派\r\n"
            L"• DeferredCapacity：容量不足请求保留并持续重新评估",
            body, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
    }

    static void DrawCardBorder(CDC& dc, const CRect& bounds)
    {
        dc.FillSolidRect(bounds, RGB(255, 255, 255));
        DrawBorder(dc, bounds, RGB(212, 220, 230));
    }

    static void DrawBorder(CDC& dc, const CRect& bounds, COLORREF color)
    {
        CPen pen(PS_SOLID, 1, color);
        CPen* oldPen = dc.SelectObject(&pen);
        CBrush* oldBrush = static_cast<CBrush*>(dc.SelectStockObject(NULL_BRUSH));
        dc.Rectangle(bounds);
        if (oldBrush != nullptr) dc.SelectObject(oldBrush);
        dc.SelectObject(oldPen);
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

#pragma once

#include "Resource.h"

#include <afxcmn.h>
#include <afxwin.h>
#include <cstddef>

// 底部 KPI 的双语标题。保留原有动态创建/布局方式，只负责绘制，
// 避免窄卡片下英文 Static 文本被裁剪或出现异常显示。
class DashboardStatTitle : public CStatic
{
protected:
    LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override
    {
        if (message != WM_PAINT)
            return CStatic::WindowProc(message, wParam, lParam);

        CPaintDC dc(this);
        CRect client;
        GetClientRect(&client);
        dc.FillSolidRect(client, ::GetSysColor(COLOR_3DFACE));
        dc.SetBkMode(TRANSPARENT);

        CFont* font = GetFont();
        CFont* oldFont = font != nullptr ? dc.SelectObject(font) : nullptr;
        dc.SetTextColor(RGB(45, 52, 62));

        static const wchar_t* Chinese[] = {
            L"总生成", L"等待中", L"乘梯中", L"已到达", L"平均等待", L"最大等待"
        };
        static const wchar_t* English[] = {
            L"Generated", L"Waiting", L"Riding", L"Arrived", L"Avg Wait", L"Max Wait"
        };

        const int index = GetDlgCtrlID() - IDC_STAT_TITLE_FIRST;
        if (index >= 0 && index < 6)
        {
            CRect top = client;
            CRect bottom = client;
            const int split = static_cast<int>(client.top) + client.Height() / 2;
            top.bottom = split + 1;
            bottom.top = split - 1;

            dc.DrawTextW(Chinese[index], top,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            dc.SetTextColor(RGB(105, 112, 122));
            dc.DrawTextW(English[index], bottom,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }

        if (oldFont != nullptr) dc.SelectObject(oldFont);
        return 0;
    }
};

// Hall Call 列表在请求较少时会留下大面积空白。
// 这个控件直接利用列表空白区域绘制只读群控摘要；当请求较多、空间不足时自动隐藏。
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
            const int headerBottom = static_cast<int>(headerRect.bottom) + 10;
            if (headerBottom > contentTop) contentTop = headerBottom;
        }

        const int itemCount = GetItemCount();
        if (itemCount > 0)
        {
            CRect lastItem;
            if (GetItemRect(itemCount - 1, &lastItem, LVIR_BOUNDS))
            {
                const int itemBottom = static_cast<int>(lastItem.bottom) + 16;
                if (itemBottom > contentTop) contentTop = itemBottom;
            }
        }

        const int clientBottom = static_cast<int>(client.bottom);
        const int clientLeft = static_cast<int>(client.left);
        const int clientRight = static_cast<int>(client.right);

        // 列表本身优先；只有确实有较大空白时才显示摘要。
        if (clientBottom - contentTop < 170) return;

        const int preferredBottom = contentTop + 198;
        const int availableBottom = clientBottom - 12;
        const int cardBottom = preferredBottom < availableBottom ? preferredBottom : availableBottom;

        CRect card;
        card.SetRect(clientLeft + 10, contentTop, clientRight - 10, cardBottom);
        if (card.Height() < 160) return;

        dc.FillSolidRect(card, RGB(248, 250, 252));
        CPen borderPen(PS_SOLID, 1, RGB(210, 216, 224));
        CPen* oldPen = dc.SelectObject(&borderPen);
        dc.SelectStockObject(NULL_BRUSH);
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

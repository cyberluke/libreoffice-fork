/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <svx/writer2027typesystempresetlist.hxx>
#include <svx/writer2027contrast.hxx>
#include <svx/writer2027log.hxx>

#include <vcl/commandevent.hxx>
#include <vcl/event.hxx>
#include <vcl/font.hxx>
#include <vcl/settings.hxx>
#include <vcl/svapp.hxx>

#include <algorithm>
#include <cmath>
#include <utility>

namespace svx::writer2027
{

namespace
{
constexpr tools::Long ROW_H = 48;          // logical px (spec 13)
constexpr tools::Long PAD_H = 12;          // left/right inset
} // namespace

Writer2027TypeSystemPresetList::Writer2027TypeSystemPresetList(weld::DrawingArea& rArea)
    : m_xArea(rArea)
{
    m_xArea.connect_draw(LINK(this, Writer2027TypeSystemPresetList, DrawHdl));
    m_xArea.connect_mouse_press(LINK(this, Writer2027TypeSystemPresetList, MousePressHdl));
    m_xArea.connect_mouse_move(LINK(this, Writer2027TypeSystemPresetList, MouseMoveHdl));
    m_xArea.connect_key_press(LINK(this, Writer2027TypeSystemPresetList, KeyHdl));
    m_xArea.connect_command(LINK(this, Writer2027TypeSystemPresetList, CommandHdl));
}

Writer2027TypeSystemPresetList::~Writer2027TypeSystemPresetList() = default;

double Writer2027TypeSystemPresetList::GetScale() const
{
    return Application::GetDefaultDevice() ? Application::GetDefaultDevice()->GetDPIScaleFactor()
                                           : 1.0;
}

tools::Long Writer2027TypeSystemPresetList::RowHeightPx() const
{
    return std::max<tools::Long>(
        static_cast<tools::Long>(std::lround(ROW_H * GetScale())), 1);
}

tools::Long Writer2027TypeSystemPresetList::GetRowHeightPx() const
{
    return RowHeightPx();
}

void Writer2027TypeSystemPresetList::SetRows(std::vector<TypeSystemPresetListRow> aRows)
{
    maRows = std::move(aRows);
    // Keep highlight/current consistent with the new set.
    int nSel = -1;
    for (size_t i = 0; i < maRows.size(); ++i)
    {
        if (!maSelectedId.isEmpty() && maRows[i].maPresetId == maSelectedId)
            nSel = static_cast<int>(i);
    }
    if (nSel < 0 && !maCurrentId.isEmpty())
    {
        for (size_t i = 0; i < maRows.size(); ++i)
            if (maRows[i].maPresetId == maCurrentId)
                nSel = static_cast<int>(i);
    }
    mnSelectedIndex = (nSel >= 0) ? nSel : (maRows.empty() ? -1 : 0);
    if (mnSelectedIndex >= 0 && mnSelectedIndex < static_cast<int>(maRows.size()))
        maSelectedId = maRows[mnSelectedIndex].maPresetId;
    m_xArea.queue_draw();
}

void Writer2027TypeSystemPresetList::SetCurrentPreset(const OUString& rPresetId)
{
    maCurrentId = rPresetId;
    for (auto& rRow : maRows)
        rRow.mbCurrent = (rRow.maPresetId == rPresetId);
    m_xArea.queue_draw();
}

void Writer2027TypeSystemPresetList::SetViewportSize(const tools::Long nWidthPx,
                                                     const tools::Long nHeightPx)
{
    mnWidthPx = nWidthPx;
    mnHeightPx = nHeightPx;
    m_xArea.set_size_request(static_cast<int>(mnWidthPx), static_cast<int>(mnHeightPx));
    m_xArea.queue_draw();
}

int Writer2027TypeSystemPresetList::IndexAt(const Point& rPoint) const
{
    const tools::Long nRowH = RowHeightPx();
    if (nRowH <= 0 || rPoint.Y() < 0)
        return -1;
    const int nIndex = static_cast<int>(rPoint.Y() / nRowH);
    if (nIndex < 0 || nIndex >= static_cast<int>(maRows.size()))
        return -1;
    return nIndex;
}

int Writer2027TypeSystemPresetList::HitTestAt(const Point& rPoint) const
{
    return IndexAt(rPoint);
}

void Writer2027TypeSystemPresetList::SelectIndex(int nIndex)
{
    if (nIndex < 0 || nIndex >= static_cast<int>(maRows.size()))
        return;
    const OUString aNewId = maRows[nIndex].maPresetId;
    if (aNewId == maSelectedId)
        return;
    maSelectedId = aNewId;
    mnSelectedIndex = nIndex;
    if (m_aChangedHdl.IsSet())
        m_aChangedHdl.Call(aNewId);
    m_xArea.queue_draw();
}

IMPL_LINK(Writer2027TypeSystemPresetList, DrawHdl, weld::DrawingArea::draw_args, aPayload, void)
{
    try
    {
        vcl::RenderContext& rCtx = aPayload.first;
        const tools::Rectangle& rRect = aPayload.second;
        Paint(rCtx, rRect);
    }
    catch (const css::uno::Exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027TypeSystemPresetList::Draw", rEx);
    }
    catch (const std::exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027TypeSystemPresetList::Draw", rEx);
    }
    catch (...)
    {
        svx::writer2027::Writer2027LogUnknownException("Writer2027TypeSystemPresetList::Draw");
    }
}

void Writer2027TypeSystemPresetList::Paint(vcl::RenderContext& rCtx,
                                           const tools::Rectangle& rRect)
{
    const StyleSettings& rStyle = Application::GetSettings().GetStyleSettings();
    const Color aWindowColor = rStyle.GetWindowColor();

    rCtx.Push(vcl::PushFlags::FILLCOLOR | vcl::PushFlags::LINECOLOR);
    rCtx.SetFillColor(aWindowColor);
    rCtx.SetLineColor(aWindowColor);
    rCtx.DrawRect(rRect);
    rCtx.Pop();

    const tools::Long nRowH = RowHeightPx();
    for (size_t i = 0; i < maRows.size(); ++i)
    {
        const tools::Long nTop = static_cast<tools::Long>(i) * nRowH;
        const tools::Rectangle aRowRect(0, nTop, rRect.GetWidth() - 1,
                                        nTop + nRowH - 1);
        const bool bSelected = (static_cast<int>(i) == mnSelectedIndex);
        const bool bHover = (static_cast<int>(i) == mnHoverIndex);
        PaintRow(rCtx, aRowRect, maRows[i], bSelected, bHover);
    }
}

void Writer2027TypeSystemPresetList::PaintRow(vcl::RenderContext& rCtx,
                                              const tools::Rectangle& rRowRect,
                                              const TypeSystemPresetListRow& rRow,
                                              bool bSelected, bool bHover)
{
    const StyleSettings& rStyle = Application::GetSettings().GetStyleSettings();
    const Color aWindowColor = rStyle.GetWindowColor();

    rCtx.Push(vcl::PushFlags::CLIPREGION);
    rCtx.IntersectClipRegion(rRowRect);

    // Background by state: restrained accent, never a full OS blue slab (spec 13).
    Color aBackground = aWindowColor;
    if (bSelected)
        aBackground.Merge(rStyle.GetHighlightColor(), 70);
    else if (bHover)
        aBackground.Merge(rStyle.GetHighlightColor(), 40);
    if (bSelected || bHover)
    {
        rCtx.Push(vcl::PushFlags::FILLCOLOR | vcl::PushFlags::LINECOLOR);
        rCtx.SetFillColor(aBackground);
        rCtx.SetLineColor(aBackground);
        rCtx.DrawRect(rRowRect);
        rCtx.Pop();
    }

    // Left rail for the selected row (spec 13 "left rail" state).
    if (bSelected)
    {
        const Color aRail = rStyle.GetHighlightColor();
        rCtx.Push(vcl::PushFlags::FILLCOLOR | vcl::PushFlags::LINECOLOR);
        rCtx.SetFillColor(aRail);
        rCtx.SetLineColor(aRail);
        const tools::Long nRailW = std::max<tools::Long>(
            static_cast<tools::Long>(std::lround(3 * GetScale())), 1);
        rCtx.DrawRect(tools::Rectangle(0, rRowRect.Top(), nRailW - 1, rRowRect.Bottom()));
        rCtx.Pop();
    }

    // Foreground: title text with selection-aware contrast (spec 6).
    const Color aTextColor = rStyle.GetWindowTextColor();
    const Color aTitle
        = svx::writer2027::Writer2027EnsureTextContrast(aTextColor, aBackground, aTextColor, 4.5);

    rCtx.Push(vcl::PushFlags::FONT | vcl::PushFlags::TEXTCOLOR);
    rCtx.SetTextColor(aTitle);
    const tools::Long nTextX = std::max<tools::Long>(
        static_cast<tools::Long>(std::lround(PAD_H * GetScale())), 4);
    rCtx.DrawText(Point(nTextX, rRowRect.Top()
                                      + (rRowRect.GetHeight() - rCtx.GetTextHeight()) / 2),
                  rRow.maDisplayName);
    rCtx.Pop();

    // "Current" marker (spec 13) at the right if this is the applied preset.
    if (rRow.mbCurrent)
    {
        rCtx.Push(vcl::PushFlags::TEXTCOLOR);
        rCtx.SetTextColor(aTitle);
        const OUString aMarker = u"●"_ustr;
        const tools::Long nMkW
            = rCtx.GetTextWidth(aMarker);
        rCtx.DrawText(Point(rRowRect.GetWidth() - nMkW - nTextX,
                            rRowRect.Top()
                                + (rRowRect.GetHeight() - rCtx.GetTextHeight()) / 2),
                      aMarker);
        rCtx.Pop();
    }

    rCtx.Pop(); // row clip
}

IMPL_LINK(Writer2027TypeSystemPresetList, MousePressHdl, const MouseEvent&, rEvent, bool)
{
    try
    {
        if (!rEvent.IsLeft())
            return false;
        const int nIndex = IndexAt(rEvent.GetPosPixel());
        if (nIndex < 0)
            return false;
        // spec 14: left click only previews (updates selection), does NOT
        // mutate the document. Apply happens via the Apply button / Enter.
        SelectIndex(nIndex);
        return true;
    }
    catch (const css::uno::Exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027TypeSystemPresetList::MousePress", rEx);
    }
    catch (const std::exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027TypeSystemPresetList::MousePress", rEx);
    }
    catch (...)
    {
        svx::writer2027::Writer2027LogUnknownException("Writer2027TypeSystemPresetList::MousePress");
    }
    return false;
}

IMPL_LINK(Writer2027TypeSystemPresetList, MouseMoveHdl, const MouseEvent&, rEvent, bool)
{
    try
    {
        const int nHover = IndexAt(rEvent.GetPosPixel());
        if (nHover != mnHoverIndex)
        {
            mnHoverIndex = nHover;
            m_xArea.queue_draw();
        }
    }
    catch (const css::uno::Exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027TypeSystemPresetList::MouseMove", rEx);
    }
    catch (const std::exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027TypeSystemPresetList::MouseMove", rEx);
    }
    catch (...)
    {
        svx::writer2027::Writer2027LogUnknownException("Writer2027TypeSystemPresetList::MouseMove");
    }
    return false;
}

IMPL_LINK(Writer2027TypeSystemPresetList, KeyHdl, const KeyEvent&, rKEvt, bool)
{
    try
    {
        const vcl::KeyCode& rKeyCode = rKEvt.GetKeyCode();
        const bool bPlain = !rKeyCode.IsMod1() && !rKeyCode.IsMod2() && !rKeyCode.IsMod3();
        if (!bPlain)
            return false;
        if (rKeyCode.GetCode() == KEY_DOWN || rKeyCode.GetCode() == KEY_UP)
        {
            const int nDelta = (rKeyCode.GetCode() == KEY_DOWN) ? 1 : -1;
            const int nCount = static_cast<int>(maRows.size());
            const int nNext = std::clamp(mnSelectedIndex + nDelta, 0, nCount - 1);
            SelectIndex(nNext);
            return true;
        }
        if (rKeyCode.GetCode() == KEY_HOME || rKeyCode.GetCode() == KEY_END)
        {
            const int nTarget = (rKeyCode.GetCode() == KEY_HOME)
                                    ? 0
                                    : static_cast<int>(maRows.size()) - 1;
            SelectIndex(nTarget);
            return true;
        }
        if (rKeyCode.GetCode() == KEY_RETURN)
        {
            if (m_aActivateHdl.IsSet() && !maSelectedId.isEmpty())
                m_aActivateHdl.Call(maSelectedId);
            return true;
        }
        if (rKeyCode.GetCode() == KEY_ESCAPE)
            return true; // popup framework closes on Escape
    }
    catch (const css::uno::Exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027TypeSystemPresetList::Key", rEx);
    }
    catch (const std::exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027TypeSystemPresetList::Key", rEx);
    }
    catch (...)
    {
        svx::writer2027::Writer2027LogUnknownException("Writer2027TypeSystemPresetList::Key");
    }
    return false;
}

IMPL_LINK(Writer2027TypeSystemPresetList, CommandHdl, const CommandEvent&, rCEvt, bool)
{
    (void)rCEvt;
    return false;
}

} // namespace svx::writer2027

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
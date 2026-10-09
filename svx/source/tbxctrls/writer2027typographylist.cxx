/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <svx/writer2027typographylist.hxx>
#include <svx/writer2027log.hxx>

#include <svtools/ctrltool.hxx> // FontList

#include <vcl/commandevent.hxx>
#include <vcl/event.hxx>
#include <vcl/font.hxx>
#include <vcl/metric.hxx>
#include <vcl/settings.hxx>
#include <vcl/svapp.hxx>
#include <vcl/vclenum.hxx>

#include <algorithm>
#include <cmath>

namespace svx::writer2027
{

namespace
{
// Semantic row heights, logical px (remediation spec 8), scaled by UI DPI.
constexpr tools::Long ROW_H_HEADER = 36;
constexpr tools::Long ROW_H_FONT = 84;
constexpr tools::Long ROW_H_NAV = 44; // Category / Legacy / Back
constexpr tools::Long ROW_H_NORESULTS = 48;

// Font-row layout tokens, logical px (remediation spec 9).
constexpr tools::Long PAD_H = 12;
constexpr tools::Long PAD_V = 8;
constexpr tools::Long SPECIMEN_W = 72;
constexpr tools::Long SPECIMEN_PAD = 8;
constexpr tools::Long SPECIMEN_GAP = 12;
constexpr tools::Long TITLE_H = 18;
constexpr tools::Long META_H = 13;
constexpr tools::Long LINE_GAP = 3;
constexpr tools::Long SCROLLBAR_W = 8;

tools::Long lcl_Scale(tools::Long nLogical, double fScale)
{
    return std::max<tools::Long>(static_cast<tools::Long>(std::lround(nLogical * fScale)), 1);
}

// Real glyph-bounds fit (remediation spec 10): choose a font size whose actual
// "Aa" text bounds fit inside the specimen box. Bounded loop, never infinite.
tools::Long lcl_FitSpecimenFont(vcl::RenderContext& rCtx, const FontMetric& rMetric,
                                const tools::Rectangle& rSpecimenRect, vcl::Font& rOutFont)
{
    const tools::Long nAvailableW = std::max<tools::Long>(rSpecimenRect.GetWidth(), 1);
    const tools::Long nAvailableH = std::max<tools::Long>(rSpecimenRect.GetHeight(), 1);
    double fCandidate = std::max<tools::Long>(nAvailableH * 0.62, 8);

    for (int i = 0; i < 8; ++i)
    {
        vcl::Font aFont(rMetric);
        aFont.SetFontSize(Size(0, static_cast<tools::Long>(fCandidate)));
        rCtx.Push(vcl::PushFlags::FONT);
        rCtx.SetFont(aFont);
        const tools::Long nGlyphW = rCtx.GetTextWidth(u"Aa"_ustr);
        const tools::Long nGlyphH = rCtx.GetTextHeight();
        rCtx.Pop();

        if (nGlyphW <= nAvailableW && nGlyphH <= nAvailableH)
        {
            rOutFont = aFont;
            return static_cast<tools::Long>(fCandidate);
        }

        const double fScaleW = static_cast<double>(nAvailableW) / nGlyphW;
        const double fScaleH = static_cast<double>(nAvailableH) / nGlyphH;
        const double fNext = std::floor(fCandidate * std::min(fScaleW, fScaleH) * 0.98);
        if (fNext >= fCandidate || fNext < 1)
            break;
        fCandidate = fNext;
    }

    rOutFont = vcl::Font(rMetric);
    rOutFont.SetFontSize(Size(0, 10));
    return 10;
}

bool lcl_IsPlainKey(const KeyEvent& rKEvt, sal_uInt16 nCode)
{
    const vcl::KeyCode& rKeyCode = rKEvt.GetKeyCode();
    return rKeyCode.GetCode() == nCode && !rKeyCode.IsMod1() && !rKeyCode.IsMod2()
           && !rKeyCode.IsMod3();
}
} // namespace

Writer2027TypographyList::Writer2027TypographyList(weld::DrawingArea& rArea)
    : m_xArea(rArea)
{
    m_xArea.connect_draw(LINK(this, Writer2027TypographyList, DrawHdl));
    m_xArea.connect_mouse_press(LINK(this, Writer2027TypographyList, MousePressHdl));
    m_xArea.connect_mouse_move(LINK(this, Writer2027TypographyList, MouseMoveHdl));
    m_xArea.connect_key_press(LINK(this, Writer2027TypographyList, KeyHdl));
    m_xArea.connect_command(LINK(this, Writer2027TypographyList, CommandHdl));
}

Writer2027TypographyList::~Writer2027TypographyList() = default;

double Writer2027TypographyList::GetScale() const
{
    return Application::GetDefaultDevice() ? Application::GetDefaultDevice()->GetDPIScaleFactor()
                                           : 1.0;
}

tools::Long Writer2027TypographyList::GetRowHeightPx(FontPickerModel::RowKind eKind,
                                                     double fScale)
{
    switch (eKind)
    {
        case FontPickerModel::RowKind::Header:
            return lcl_Scale(ROW_H_HEADER, fScale);
        case FontPickerModel::RowKind::Font:
            return lcl_Scale(ROW_H_FONT, fScale);
        case FontPickerModel::RowKind::NoResults:
            return lcl_Scale(ROW_H_NORESULTS, fScale);
        case FontPickerModel::RowKind::Category:
        case FontPickerModel::RowKind::Legacy:
        case FontPickerModel::RowKind::Back:
            return lcl_Scale(ROW_H_NAV, fScale);
    }
    return lcl_Scale(ROW_H_NAV, fScale);
}

void Writer2027TypographyList::SetRows(const std::vector<FontPickerModel::Row>* pRows)
{
    mpRows = pRows;
}

void Writer2027TypographyList::SetFontList(const FontList* pFontList)
{
    mpFontList = pFontList;
}

void Writer2027TypographyList::SetCurrentFamily(const OUString& rFamily)
{
    maCurrentFamily = rFamily;
    // Keep the current family in sync: find it if visible.
    if (mpRows)
    {
        for (size_t i = 0; i < mpRows->size(); ++i)
        {
            if ((*mpRows)[i].meKind == FontPickerModel::RowKind::Font
                && (*mpRows)[i].maText == maCurrentFamily)
            {
                mnSelectedIndex = static_cast<int>(i);
                break;
            }
        }
    }
}

void Writer2027TypographyList::RebuildLayout()
{
    const double fScale = GetScale();
    maLayout.clear();
    mnScrollOffsetPx = 0;
    mnHoverIndex = -1;
    mnSelectedIndex = -1;

    if (!mpRows)
    {
        mnContentHeightPx = 0;
        return;
    }

    tools::Long nY = 0;
    for (size_t i = 0; i < mpRows->size(); ++i)
    {
        const FontPickerModel::Row& rRow = (*mpRows)[i];
        const tools::Long nRowH = GetRowHeightPx(rRow.meKind, fScale);
        const tools::Long nRowW = std::max<tools::Long>(mnWidthPx, lcl_Scale(400, fScale));

        TypographyRowLayout aLayout;
        aLayout.mnModelIndex = static_cast<int>(i);
        aLayout.maRowRect = tools::Rectangle(0, nY, nRowW - 1, nY + nRowH - 1);

        if (rRow.meKind == FontPickerModel::RowKind::Font)
        {
            // specimen | gap | title / meta
            const tools::Long nPadH = lcl_Scale(PAD_H, fScale);
            const tools::Long nPadV = lcl_Scale(PAD_V, fScale);
            const tools::Long nSpecimenW = lcl_Scale(SPECIMEN_W, fScale);
            const tools::Long nGap = lcl_Scale(SPECIMEN_GAP, fScale);
            const tools::Long nInnerPad = lcl_Scale(SPECIMEN_PAD, fScale);
            const tools::Long nSpecimenH = nRowH - 2 * nPadV;

            aLayout.maSpecimenRect = tools::Rectangle(
                nPadH + nInnerPad, nPadV + nInnerPad,
                nPadH + nSpecimenW - nInnerPad, nPadV + nSpecimenH - nInnerPad);
            const tools::Long nTitleTop = nY + nPadV;
            const tools::Long nMetaTop = nTitleTop + lcl_Scale(TITLE_H, fScale)
                                         + lcl_Scale(LINE_GAP, fScale);
            aLayout.maTitleRect = tools::Rectangle(
                nPadH + nSpecimenW + nGap, nTitleTop,
                nRowW - nPadH - 1, nTitleTop + lcl_Scale(TITLE_H, fScale) - 1);
            aLayout.maMetaRect = tools::Rectangle(
                nPadH + nSpecimenW + nGap, nMetaTop,
                nRowW - nPadH - 1, nMetaTop + lcl_Scale(META_H, fScale) - 1);
        }

        maLayout.push_back(aLayout);
        nY += nRowH;
    }

    mnContentHeightPx = nY;
    if (mnSelectedIndex >= 0 && mnSelectedIndex < static_cast<int>(maLayout.size()))
        EnsureSelectedVisible();
}

void Writer2027TypographyList::SetViewportSize(const tools::Long nWidthPx,
                                               const tools::Long nHeightPx)
{
    mnWidthPx = nWidthPx;
    mnViewportHeightPx = nHeightPx;
    if (mnScrollOffsetPx > MaxContentOffset())
        mnScrollOffsetPx = MaxContentOffset();
    m_xArea.set_size_request(static_cast<int>(mnWidthPx), static_cast<int>(mnViewportHeightPx));
    m_xArea.queue_draw();
}

int Writer2027TypographyList::HitTest(const Point& rPoint) const
{
    // Point is in viewport space; rows are laid out in content space.
    const tools::Long nContentY = rPoint.Y() + mnScrollOffsetPx;
    for (size_t i = 0; i < maLayout.size(); ++i)
    {
        const tools::Rectangle& rRow = maLayout[i].maRowRect;
        if (rRow.Top() <= nContentY && nContentY <= rRow.Bottom())
            return static_cast<int>(i);
    }
    return -1;
}

void Writer2027TypographyList::ScrollBy(tools::Long nDeltaPx)
{
    const tools::Long nOld = mnScrollOffsetPx;
    mnScrollOffsetPx = std::clamp<tools::Long>(mnScrollOffsetPx + nDeltaPx, 0,
                                               MaxContentOffset());
    if (mnScrollOffsetPx != nOld)
        m_xArea.queue_draw();
}

tools::Long Writer2027TypographyList::MaxContentOffset() const
{
    return std::max<tools::Long>(mnContentHeightPx - mnViewportHeightPx, 0);
}

void Writer2027TypographyList::EnsureSelectedVisible()
{
    if (mnSelectedIndex < 0 || mnSelectedIndex >= static_cast<int>(maLayout.size()))
        return;
    const tools::Rectangle& rRow = maLayout[mnSelectedIndex].maRowRect;
    const tools::Long nViewBottom = mnScrollOffsetPx + mnViewportHeightPx;
    if (rRow.Top() < mnScrollOffsetPx)
        mnScrollOffsetPx = rRow.Top();
    else if (rRow.Bottom() > nViewBottom)
        mnScrollOffsetPx = rRow.Bottom() - mnViewportHeightPx + 1;
    mnScrollOffsetPx = std::clamp<tools::Long>(mnScrollOffsetPx, 0, MaxContentOffset());
}

bool Writer2027TypographyList::IsSelectable(const FontPickerModel::Row& rRow) const
{
    switch (rRow.meKind)
    {
        case FontPickerModel::RowKind::Font:
        case FontPickerModel::RowKind::Category:
        case FontPickerModel::RowKind::Back:
        case FontPickerModel::RowKind::Legacy:
            return true;
        case FontPickerModel::RowKind::Header:
        case FontPickerModel::RowKind::NoResults:
            return false;
    }
    return false;
}

int Writer2027TypographyList::GetNextSelectableIndex(int nFrom, int nDelta) const
{
    if (!mpRows || mpRows->empty())
        return -1;
    if (nFrom < 0 || nFrom >= static_cast<int>(mpRows->size())
        || !IsSelectable((*mpRows)[nFrom]))
    {
        nFrom = (nDelta > 0) ? -1 : static_cast<int>(mpRows->size());
    }
    for (int n = nFrom + nDelta;; n += nDelta)
    {
        if (n < 0 || n >= static_cast<int>(mpRows->size()))
            return -1;
        if (IsSelectable((*mpRows)[n]))
            return n;
    }
}

void Writer2027TypographyList::SelectIndex(int nIndex, bool bScroll)
{
    if (nIndex < 0 || nIndex >= static_cast<int>(maLayout.size()))
        return;
    mnSelectedIndex = nIndex;
    if (bScroll)
    {
        EnsureSelectedVisible();
        m_xArea.queue_draw();
    }
    else
        m_xArea.queue_draw();
}

void Writer2027TypographyList::SelectFirstSelectable()
{
    const int nFirst = GetNextSelectableIndex(-1, 1);
    if (nFirst >= 0)
        SelectIndex(nFirst, true);
}

void Writer2027TypographyList::ActivateRow(int nIndex)
{
    if (!mpRows || nIndex < 0 || nIndex >= static_cast<int>(mpRows->size()))
        return;
    const FontPickerModel::Row& rRow = (*mpRows)[nIndex];
    switch (rRow.meKind)
    {
        case FontPickerModel::RowKind::Font:
            if (m_aSelectHdl.IsSet())
                m_aSelectHdl.Call(rRow.maText);
            break;
        case FontPickerModel::RowKind::Back:
        case FontPickerModel::RowKind::Category:
        case FontPickerModel::RowKind::Legacy:
            if (m_aActivateHdl.IsSet())
            {
                if (m_aActivateHdl.Call(rRow))
                {
                    // The owner rebuilt the model; rebuild layout + reselect.
                    RebuildLayout();
                    const int nFirst = GetNextSelectableIndex(-1, 1);
                    if (nFirst >= 0)
                        SelectIndex(nFirst, false);
                    m_xArea.queue_draw();
                }
            }
            break;
        case FontPickerModel::RowKind::Header:
        case FontPickerModel::RowKind::NoResults:
            break; // inert
    }
}

IMPL_LINK(Writer2027TypographyList, DrawHdl, weld::DrawingArea::draw_args, aPayload, void)
{
    try
    {
        vcl::RenderContext& rCtx = aPayload.first;
        const tools::Rectangle& rRect = aPayload.second;
        svx::writer2027::Writer2027LogMessage(
            "fontpopup.paint",
            OUString::Concat(u"draw w=") + OUString::number(rRect.GetWidth())
                + u" h=" + OUString::number(rRect.GetHeight()));
        Paint(rCtx, rRect);
    }
    catch (const css::uno::Exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027TypographyList::Draw", rEx);
    }
    catch (const std::exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027TypographyList::Draw", rEx);
    }
    catch (...)
    {
        svx::writer2027::Writer2027LogUnknownException("Writer2027TypographyList::Draw");
    }
}

void Writer2027TypographyList::Paint(vcl::RenderContext& rCtx, const tools::Rectangle& rRect)
{
    const StyleSettings& rStyle = Application::GetSettings().GetStyleSettings();
    const Color aWindowColor = rStyle.GetWindowColor();

    // Background.
    rCtx.Push(vcl::PushFlags::FILLCOLOR | vcl::PushFlags::LINECOLOR);
    rCtx.SetFillColor(aWindowColor);
    rCtx.SetLineColor(aWindowColor);
    rCtx.DrawRect(rRect);
    rCtx.Pop();

    if (!mpRows)
        return;

    // Visible rows only.
    const tools::Long nViewBottom = mnScrollOffsetPx + mnViewportHeightPx;
    for (size_t i = 0; i < maLayout.size(); ++i)
    {
        const TypographyRowLayout& rLayout = maLayout[i];
        const tools::Rectangle& rRow = rLayout.maRowRect;
        if (rRow.Bottom() < mnScrollOffsetPx || rRow.Top() > nViewBottom)
            continue;
        if (rLayout.mnModelIndex < 0
            || rLayout.mnModelIndex >= static_cast<int>(mpRows->size()))
            continue;
        const bool bSelected = (mnSelectedIndex == rLayout.mnModelIndex);
        const bool bHover = (mnHoverIndex == rLayout.mnModelIndex);
        PaintRow(rCtx, rLayout, (*mpRows)[rLayout.mnModelIndex], bSelected, bHover);
    }

    PaintScrollBar(rCtx, rRect);
}

void Writer2027TypographyList::PaintScrollBar(vcl::RenderContext& rCtx,
                                              const tools::Rectangle& rRect)
{
    if (mnContentHeightPx <= mnViewportHeightPx)
        return;
    const StyleSettings& rStyle = Application::GetSettings().GetStyleSettings();
    const Color aMuted = rStyle.GetWindowTextColor();
    Color aTrack(aMuted);
    aTrack.Merge(rStyle.GetWindowColor(), 200);

    const tools::Long nTrackX = rRect.Right() - lcl_Scale(SCROLLBAR_W, GetScale());
    const tools::Long nTrackTop = rRect.Top();
    const tools::Long nTrackBottom = rRect.Bottom();

    rCtx.Push(vcl::PushFlags::FILLCOLOR | vcl::PushFlags::LINECOLOR);
    rCtx.SetLineColor(aTrack);
    rCtx.SetFillColor(aTrack);
    rCtx.DrawRect(tools::Rectangle(nTrackX, nTrackTop, rRect.Right(), nTrackBottom));

    // Thumb.
    const double fVisible = static_cast<double>(mnViewportHeightPx) / mnContentHeightPx;
    const tools::Long nThumbH
        = std::max<tools::Long>(lcl_Scale(24, GetScale()),
                                static_cast<tools::Long>(nTrackBottom - nTrackTop) * fVisible);
    const double fPos = static_cast<double>(mnScrollOffsetPx)
                        / std::max<tools::Long>(mnContentHeightPx - mnViewportHeightPx, 1);
    const tools::Long nThumbTop
        = nTrackTop + static_cast<tools::Long>((nTrackBottom - nTrackTop - nThumbH) * fPos);

    Color aThumb(aMuted);
    aThumb.Merge(rStyle.GetWindowColor(), 120);
    rCtx.SetLineColor(aThumb);
    rCtx.SetFillColor(aThumb);
    rCtx.DrawRect(tools::Rectangle(nTrackX, nThumbTop, rRect.Right(), nThumbTop + nThumbH));
    rCtx.Pop();
}

void Writer2027TypographyList::PaintRow(vcl::RenderContext& rCtx,
                                        const TypographyRowLayout& rLayout,
                                        const FontPickerModel::Row& rRow, bool bSelected,
                                        bool bHover)
{
    // Translate content-space rect into viewport space.
    const tools::Rectangle aRowRect(rLayout.maRowRect);
    const tools::Rectangle aViewRect(
        Point(aRowRect.Left(), aRowRect.Top() - mnScrollOffsetPx),
        Size(aRowRect.GetWidth(), aRowRect.GetHeight()));

    // Hard clip to this row: nothing may paint outside it (spec 11).
    rCtx.Push(vcl::PushFlags::CLIPREGION);
    rCtx.IntersectClipRegion(aViewRect);

    const StyleSettings& rStyle = Application::GetSettings().GetStyleSettings();
    const Color aWindowColor = rStyle.GetWindowColor();
    const Color aTextColor = rStyle.GetWindowTextColor();
    Color aMuted = aTextColor;
    aMuted.Merge(rStyle.GetFieldColor(), 110);

    // Selection / hover background (restrained, never the OS slab).
    if (bSelected)
    {
        Color aSel(aWindowColor);
        aSel.Merge(rStyle.GetHighlightColor(), 80);
        rCtx.Push(vcl::PushFlags::FILLCOLOR | vcl::PushFlags::LINECOLOR);
        rCtx.SetFillColor(aSel);
        rCtx.SetLineColor(aSel);
        rCtx.DrawRect(aViewRect);
        rCtx.Pop();
    }
    else if (bHover)
    {
        Color aHov(aWindowColor);
        aHov.Merge(rStyle.GetHighlightColor(), 45);
        rCtx.Push(vcl::PushFlags::FILLCOLOR | vcl::PushFlags::LINECOLOR);
        rCtx.SetFillColor(aHov);
        rCtx.SetLineColor(aHov);
        rCtx.DrawRect(aViewRect);
        rCtx.Pop();
    }

    const tools::Long nX = aViewRect.Left() + lcl_Scale(PAD_H, GetScale());

    switch (rRow.meKind)
    {
        case FontPickerModel::RowKind::Header:
        {
            vcl::Font aFont(rCtx.GetFont());
            aFont.SetWeight(WEIGHT_BOLD);
            rCtx.Push(vcl::PushFlags::FONT | vcl::PushFlags::TEXTCOLOR);
            rCtx.SetFont(aFont);
            rCtx.SetTextColor(aMuted);
            rCtx.DrawText(Point(nX, aViewRect.Top()
                                          + (aViewRect.GetHeight() - rCtx.GetTextHeight()) / 2),
                          rRow.maText);
            rCtx.Pop();
            break;
        }
        case FontPickerModel::RowKind::Back:
        case FontPickerModel::RowKind::Category:
        case FontPickerModel::RowKind::Legacy:
        {
            rCtx.Push(vcl::PushFlags::TEXTCOLOR);
            rCtx.SetTextColor(aTextColor);
            rCtx.DrawText(Point(nX, aViewRect.Top()
                                        + (aViewRect.GetHeight() - rCtx.GetTextHeight()) / 2),
                          rRow.maText);
            rCtx.Pop();
            break;
        }
        case FontPickerModel::RowKind::NoResults:
        {
            rCtx.Push(vcl::PushFlags::TEXTCOLOR);
            rCtx.SetTextColor(aMuted);
            rCtx.DrawText(Point(nX, aViewRect.Top()
                                        + (aViewRect.GetHeight() - rCtx.GetTextHeight()) / 2),
                          rRow.maText);
            rCtx.Pop();
            break;
        }
        case FontPickerModel::RowKind::Font:
            PaintFontRow(rCtx, rLayout, rRow, bSelected, bHover);
            break;
    }

    rCtx.Pop(); // row clip
}

void Writer2027TypographyList::PaintFontRow(vcl::RenderContext& rCtx,
                                            const TypographyRowLayout& rLayout,
                                            const FontPickerModel::Row& rRow, bool /*bSelected*/,
                                            bool /*bHover*/)
{
    const tools::Long nScroll = mnScrollOffsetPx;
    const StyleSettings& rStyle = Application::GetSettings().GetStyleSettings();
    const Color aTextColor = rStyle.GetWindowTextColor();
    Color aMuted = aTextColor;
    aMuted.Merge(rStyle.GetFieldColor(), 110);

    // Specimen: measured fit + hard clip (spec 10). Nested scope restores the
    // row clip before the text scope (the fix from remediation phase 1).
    FontMetric aMetric;
    const bool bPreview = mpFontList && mpFontList->IsAvailable(rRow.maText);
    if (bPreview)
        aMetric = mpFontList->Get(rRow.maText, WEIGHT_NORMAL, ITALIC_NONE);

    const tools::Rectangle aSpecimenView(rLayout.maSpecimenRect.Left(),
                                         rLayout.maSpecimenRect.Top() - nScroll,
                                         rLayout.maSpecimenRect.Right(),
                                         rLayout.maSpecimenRect.Bottom() - nScroll);
    if (bPreview)
    {
        rCtx.Push(vcl::PushFlags::CLIPREGION);
        rCtx.IntersectClipRegion(aSpecimenView);

        vcl::Font aPreviewFont;
        const tools::Long nFitH
            = lcl_FitSpecimenFont(rCtx, aMetric, aSpecimenView, aPreviewFont);
        if (nFitH > 0)
        {
            rCtx.Push(vcl::PushFlags::FONT | vcl::PushFlags::TEXTCOLOR);
            rCtx.SetFont(aPreviewFont);
            rCtx.SetTextColor(aTextColor);
            const tools::Long nGlyphW = rCtx.GetTextWidth(u"Aa"_ustr);
            const tools::Long nGlyphH = rCtx.GetTextHeight();
            rCtx.DrawText(Point(aSpecimenView.Left()
                                    + (aSpecimenView.GetWidth() - nGlyphW) / 2,
                                aSpecimenView.Top()
                                    + (aSpecimenView.GetHeight() - nGlyphH) / 2),
                          u"Aa"_ustr);
            rCtx.Pop();
        }
        rCtx.Pop(); // specimen clip -> row clip
    }

    // Text scope: title + metadata, clipped to the text band.
    const tools::Rectangle aTitleView(rLayout.maTitleRect.Left(),
                                      rLayout.maTitleRect.Top() - nScroll,
                                      rLayout.maTitleRect.Right(),
                                      rLayout.maTitleRect.Bottom() - nScroll);
    rCtx.Push(vcl::PushFlags::CLIPREGION);
    rCtx.IntersectClipRegion(aTitleView);
    rCtx.Push(vcl::PushFlags::TEXTCOLOR);
    rCtx.SetTextColor(aTextColor);
    rCtx.DrawText(Point(aTitleView.Left(), aTitleView.Top()), rRow.maText);
    rCtx.Pop();
    rCtx.Pop(); // title clip

    if (!rRow.maMeta.isEmpty())
    {
        const tools::Rectangle aMetaView(rLayout.maMetaRect.Left(),
                                         rLayout.maMetaRect.Top() - nScroll,
                                         rLayout.maMetaRect.Right(),
                                         rLayout.maMetaRect.Bottom() - nScroll);
        rCtx.Push(vcl::PushFlags::CLIPREGION);
        rCtx.IntersectClipRegion(aMetaView);
        rCtx.Push(vcl::PushFlags::TEXTCOLOR);
        rCtx.SetTextColor(aMuted);
        rCtx.DrawText(Point(aMetaView.Left(), aMetaView.Top()), rRow.maMeta);
        rCtx.Pop();
        rCtx.Pop(); // meta clip
    }
}

IMPL_LINK(Writer2027TypographyList, MousePressHdl, const MouseEvent&, rEvent, bool)
{
    svx::writer2027::Writer2027LogMessage("fontpopup.mousepress",
                                          OUString::Concat(u"x=")
                                              + OUString::number(rEvent.GetPosPixel().X())
                                              + u" y="
                                              + OUString::number(rEvent.GetPosPixel().Y()));
    if (!rEvent.IsLeft())
        return false;

    try
    {
        const int nIndex = HitTest(rEvent.GetPosPixel());
        if (nIndex < 0)
            return false;
        SelectIndex(nIndex, true);
        ActivateRow(nIndex);
        return true;
    }
    catch (const css::uno::Exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027TypographyList::MousePress", rEx);
    }
    catch (const std::exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027TypographyList::MousePress", rEx);
    }
    catch (...)
    {
        svx::writer2027::Writer2027LogUnknownException("Writer2027TypographyList::MousePress");
    }
    return false;
}

IMPL_LINK(Writer2027TypographyList, MouseMoveHdl, const MouseEvent&, rEvent, bool)
{
    try
    {
        const int nHover = HitTest(rEvent.GetPosPixel());
        if (nHover != mnHoverIndex)
        {
            mnHoverIndex = nHover;
            m_xArea.queue_draw();
        }
    }
    catch (const css::uno::Exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027TypographyList::MouseMove", rEx);
    }
    catch (const std::exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027TypographyList::MouseMove", rEx);
    }
    catch (...)
    {
        svx::writer2027::Writer2027LogUnknownException("Writer2027TypographyList::MouseMove");
    }
    return false;
}

IMPL_LINK(Writer2027TypographyList, KeyHdl, const KeyEvent&, rKEvt, bool)
{
    try
    {
        if (lcl_IsPlainKey(rKEvt, KEY_UP))
        {
            const int nTarget = GetNextSelectableIndex(mnSelectedIndex, -1);
            if (nTarget >= 0)
                SelectIndex(nTarget, true);
            return true;
        }
        if (lcl_IsPlainKey(rKEvt, KEY_DOWN))
        {
            const int nTarget = GetNextSelectableIndex(mnSelectedIndex, 1);
            if (nTarget >= 0)
                SelectIndex(nTarget, true);
            return true;
        }
        if (lcl_IsPlainKey(rKEvt, KEY_PAGEUP))
        {
            const int nPage = 8;
            const int nTarget = GetNextSelectableIndex(mnSelectedIndex, -nPage);
            if (nTarget >= 0)
                SelectIndex(nTarget, true);
            return true;
        }
        if (lcl_IsPlainKey(rKEvt, KEY_PAGEDOWN))
        {
            const int nPage = 8;
            const int nTarget = GetNextSelectableIndex(mnSelectedIndex, nPage);
            if (nTarget >= 0)
                SelectIndex(nTarget, true);
            return true;
        }
        if (lcl_IsPlainKey(rKEvt, KEY_HOME))
        {
            const int nTarget = GetNextSelectableIndex(-1, 1);
            if (nTarget >= 0)
                SelectIndex(nTarget, true);
            return true;
        }
        if (lcl_IsPlainKey(rKEvt, KEY_END))
        {
            const int nTarget = GetNextSelectableIndex(static_cast<int>(maLayout.size()), -1);
            if (nTarget >= 0)
                SelectIndex(nTarget, true);
            return true;
        }
        if (lcl_IsPlainKey(rKEvt, KEY_RETURN))
        {
            ActivateRow(mnSelectedIndex);
            return true;
        }
        if (lcl_IsPlainKey(rKEvt, KEY_ESCAPE))
            return true; // popup framework closes on Escape
    }
    catch (const css::uno::Exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027TypographyList::Key", rEx);
    }
    catch (const std::exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027TypographyList::Key", rEx);
    }
    catch (...)
    {
        svx::writer2027::Writer2027LogUnknownException("Writer2027TypographyList::Key");
    }
    return false;
}

IMPL_LINK(Writer2027TypographyList, CommandHdl, const CommandEvent&, rCEvt, bool)
{
    try
    {
        if (rCEvt.GetCommand() == CommandEventId::Wheel)
        {
            const CommandWheelData* pWheel = rCEvt.GetWheelData();
            if (pWheel && !pWheel->IsHorz() && !pWheel->IsDeltaPixel()
                && pWheel->GetModifier() == 0)
            {
                const tools::Long nStep = lcl_Scale(ROW_H_NAV, GetScale());
                ScrollBy(pWheel->GetDelta() > 0 ? -nStep : nStep);
                return true;
            }
        }
    }
    catch (const css::uno::Exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027TypographyList::Command", rEx);
    }
    catch (const std::exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027TypographyList::Command", rEx);
    }
    catch (...)
    {
        svx::writer2027::Writer2027LogUnknownException("Writer2027TypographyList::Command");
    }
    return false;
}

} // namespace svx::writer2027

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
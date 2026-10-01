/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <svx/writer2027fontpopup.hxx>
#include <svx/writer2027typography.hxx>

#include <svtools/ctrltool.hxx>

#include <vcl/event.hxx>
#include <vcl/font.hxx>
#include <vcl/metric.hxx>
#include <vcl/settings.hxx>
#include <vcl/svapp.hxx>
#include <vcl/vclenum.hxx>
#include <vcl/window.hxx>
#include <vcl/weld/Entry.hxx>
#include <vcl/weld/TreeView.hxx>
#include <vcl/weld/weldutils.hxx>

#include <algorithm>

namespace svx::writer2027
{

namespace
{

// Target desktop geometry (logical px, scaled by the UI DPI factor).
constexpr int POPUP_WIDTH = 400;
constexpr int POPUP_WIDTH_MIN = 340;
constexpr int ROW_HEIGHT_FONT = 54;   // font rows: 52-58 logical px
constexpr int ROW_HEIGHT_HEADER = 30; // section headers: 28-34 logical px
constexpr int ROW_HEIGHT_NAV = 34;    // back / category / legacy rows
constexpr int PREVIEW_BLOCK_WIDTH = 48;
constexpr int ROW_MARGIN = 10;

bool lcl_IsPlainKey(const KeyEvent& rKEvt, sal_uInt16 nCode)
{
    const vcl::KeyCode& rKeyCode = rKEvt.GetKeyCode();
    return rKeyCode.GetCode() == nCode && !rKeyCode.IsMod1() && !rKeyCode.IsMod2()
           && !rKeyCode.IsMod3();
}

} // namespace

Writer2027FontPopup::Writer2027FontPopup(FontPickerModel& rModel)
    : mrModel(rModel)
{
}

Writer2027FontPopup::~Writer2027FontPopup() = default;

void Writer2027FontPopup::Open(const FontList* pFontList, const OUString& rCurrentFamily,
                               const OUString& rInitialQuery, vcl::Window& rAnchorWin)
{
    mpFontList = pFontList;
    maCurrentFamily = rCurrentFamily;

    // Build the popover once; reuse the instance across opens (no tear-down
    // churn, deterministic focus behaviour).
    if (!m_xBuilder)
    {
        tools::Rectangle aInitRect(Point(0, 0), rAnchorWin.GetSizePixel());
        weld::Window* pInitParent = weld::GetPopupParent(rAnchorWin, aInitRect);
        m_xBuilder = Application::CreateBuilder(pInitParent, u"svx/ui/writer2027fontpopup.ui"_ustr);
        m_xPopup = m_xBuilder->weld_popover(u"Writer2027FontPopup"_ustr);
        m_xSearch = m_xBuilder->weld_entry(u"search"_ustr);
        m_xRows = m_xBuilder->weld_tree_view(u"rows"_ustr);

        m_xPopup->set_accessible_name(u"Font picker"_ustr);
        m_xPopup->connect_closed(LINK(this, Writer2027FontPopup, PopupClosedHdl));

        m_xRows->set_selection_mode(SelectionMode::Single);
        m_xRows->set_column_custom_renderer(0, true);
        m_xRows->connect_custom_get_size(LINK(this, Writer2027FontPopup, RowGetSizeHdl));
        m_xRows->connect_custom_render(LINK(this, Writer2027FontPopup, RowRenderHdl));
        m_xRows->connect_key_press(LINK(this, Writer2027FontPopup, TreeKeyHdl));
        m_xRows->connect_selection_changed(LINK(this, Writer2027FontPopup, TreeSelectionHdl));
        m_xRows->connect_mouse_press(LINK(this, Writer2027FontPopup, TreeMousePressHdl));

        m_xSearch->connect_changed(LINK(this, Writer2027FontPopup, SearchChangedHdl));
        m_xSearch->connect_key_press(LINK(this, Writer2027FontPopup, SearchKeyHdl));
    }

    // Search state lives only in the popup; the closed font-name control is
    // never overwritten.
    m_xSearch->set_text(rInitialQuery);
    maQuery = rInitialQuery;
    RebuildRows();

    // Geometry: independent popup width; height capped to the active
    // monitor's work area (physical pixels from the anchor frame).
    const double fScale = Application::GetDefaultDevice()
                              ? Application::GetDefaultDevice()->GetDPIScaleFactor()
                              : 1.0;
    const AbsoluteScreenPixelRectangle aScreenRect = rAnchorWin.GetDesktopRectPixel();
    const tools::Long nWorkW = aScreenRect.GetWidth();
    const tools::Long nWorkH = aScreenRect.GetHeight();
    const int nMargin = 24;
    int nPopupWidth = POPUP_WIDTH;
    if (nWorkW - 2 * nMargin < POPUP_WIDTH_MIN)
        nPopupWidth = std::max(POPUP_WIDTH_MIN, static_cast<int>(nWorkW - 2 * nMargin));

    tools::Long nContentH = 0;
    for (const auto& rRow : mrModel.GetRows())
    {
        switch (rRow.meKind)
        {
            case FontPickerModel::RowKind::Header:
                nContentH += ROW_HEIGHT_HEADER;
                break;
            case FontPickerModel::RowKind::Font:
                nContentH += ROW_HEIGHT_FONT;
                break;
            case FontPickerModel::RowKind::Back:
            case FontPickerModel::RowKind::Category:
            case FontPickerModel::RowKind::Legacy:
            case FontPickerModel::RowKind::NoResults:
                nContentH += ROW_HEIGHT_NAV;
                break;
        }
    }
    const tools::Long nMaxPopupH = nWorkH - 2 * nMargin;
    const tools::Long nMinPopupH = 3 * ROW_HEIGHT_NAV;
    const tools::Long nTargetH = std::clamp(nContentH, nMinPopupH, std::max(nMinPopupH, nMaxPopupH));
    const int nPopupH = static_cast<int>(nTargetH * fScale);
    m_xRows->set_size_request(nPopupWidth, nPopupH);

    tools::Rectangle aRect(Point(0, 0), rAnchorWin.GetSizePixel());
    weld::Window* pParent = weld::GetPopupParent(rAnchorWin, aRect);
    mbOpen = true;
    m_xPopup->popup_at_rect(pParent, aRect, weld::Placement::Under);
    m_xPopup->resize_to_request();

    // Focus the search field: typing filters immediately, Down moves into
    // the list, Enter applies the best match.
    m_xSearch->grab_focus();
}

void Writer2027FontPopup::Close()
{
    if (!mbOpen)
        return;
    m_xPopup->popdown(); // PopupClosedHdl fires via signal_closed
}

void Writer2027FontPopup::SetCurrentFamily(const FontList* pFontList,
                                           const OUString& rCurrentFamily)
{
    if (!IsOpen())
        return;
    mpFontList = pFontList;
    maCurrentFamily = rCurrentFamily;
    RebuildRows();
}

void Writer2027FontPopup::RebuildRows()
{
    if (!m_xRows || !mpFontList)
        return;

    mrModel.Rebuild(mpFontList, maCurrentFamily, maQuery);
    const std::vector<FontPickerModel::Row>& rRows = mrModel.GetRows();

    mbInternalMove = true;
    m_xRows->freeze();
    m_xRows->clear();
    if (!rRows.empty())
    {
        m_xRows->bulk_insert_for_each(
            static_cast<int>(rRows.size()),
            [this, &rRows](weld::TreeIter& rIter, int nIndex) {
                const FontPickerModel::Row& rRow = rRows[nIndex];
                m_xRows->set_text(rIter, rRow.maText);
                m_xRows->set_id(rIter, rRow.maId);
            });

        // Initial selection: the active family when visible, otherwise the
        // first selectable row. Headers/NoResults rows never get selected.
        int nSelect = -1;
        for (size_t i = 0; i < rRows.size(); ++i)
        {
            if (rRows[i].meKind == FontPickerModel::RowKind::Font
                && rRows[i].maText == maCurrentFamily)
            {
                nSelect = static_cast<int>(i);
                break;
            }
        }
        if (nSelect < 0)
            nSelect = GetNextSelectableIndex(-1, 1);
        if (nSelect >= 0)
        {
            m_xRows->set_cursor(nSelect);
            m_xRows->select(nSelect);
            if (auto xIter = m_xRows->get_iterator(nSelect))
                m_xRows->scroll_to_row(*xIter);
            mnLastSelectedIndex = nSelect;
        }
    }
    m_xRows->thaw();
    mbInternalMove = false;
}

bool Writer2027FontPopup::IsSelectableRow(const FontPickerModel::Row& rRow) const
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

int Writer2027FontPopup::GetNextSelectableIndex(int nFrom, int nDelta) const
{
    const std::vector<FontPickerModel::Row>& rRows = mrModel.GetRows();
    if (rRows.empty())
        return -1;
    if (nFrom < 0 || nFrom >= static_cast<int>(rRows.size())
        || !IsSelectableRow(rRows[nFrom]))
    {
        // No valid cursor: start at the boundary in the travel direction.
        nFrom = (nDelta > 0) ? -1 : static_cast<int>(rRows.size());
    }
    for (int n = nFrom + nDelta;; n += nDelta)
    {
        if (n < 0 || n >= static_cast<int>(rRows.size()))
            return -1;
        if (IsSelectableRow(rRows[n]))
            return n;
    }
}

void Writer2027FontPopup::SelectRowIndex(int nIndex, bool bScroll)
{
    if (nIndex < 0 || nIndex >= m_xRows->n_children())
        return;
    mbInternalMove = true;
    m_xRows->set_cursor(nIndex);
    m_xRows->select(nIndex);
    if (bScroll)
    {
        if (auto xIter = m_xRows->get_iterator(nIndex))
            m_xRows->scroll_to_row(*xIter);
    }
    mnLastSelectedIndex = nIndex;
    mbInternalMove = false;
}

void Writer2027FontPopup::MoveCursor(int nDelta)
{
    const int nFrom = m_xRows->get_cursor_index();
    const int nTarget = GetNextSelectableIndex(nFrom, nDelta);
    if (nTarget >= 0)
        SelectRowIndex(nTarget, true);
}

void Writer2027FontPopup::MoveCursorPage(int nDelta)
{
    int nPage = 8;
    int nVisible = 0;
    m_xRows->visible_foreach(
        [&nVisible](weld::TreeIter&) {
            ++nVisible;
            return false;
        });
    if (nVisible > 0)
        nPage = nVisible;
    MoveCursor(nDelta * nPage);
}

void Writer2027FontPopup::MoveCursorHomeOrEnd(bool bHome)
{
    const std::vector<FontPickerModel::Row>& rRows = mrModel.GetRows();
    if (rRows.empty())
        return;
    const int nBoundary = bHome ? -1 : static_cast<int>(rRows.size());
    const int nTarget = GetNextSelectableIndex(nBoundary, bHome ? 1 : -1);
    if (nTarget >= 0)
        SelectRowIndex(nTarget, true);
}

void Writer2027FontPopup::ApplyFamily(const OUString& rFamily)
{
    if (!rFamily.isEmpty())
        m_aSelectHdl.Call(rFamily);
    Close();
}

void Writer2027FontPopup::ActivateRow(const FontPickerModel::Row& rRow)
{
    switch (rRow.meKind)
    {
        case FontPickerModel::RowKind::Font:
            ApplyFamily(rRow.maText);
            break;
        case FontPickerModel::RowKind::Back:
            mrModel.GoBack();
            RebuildRows();
            break;
        case FontPickerModel::RowKind::Category:
            mrModel.EnterCategory(rRow.meCategory);
            RebuildRows();
            break;
        case FontPickerModel::RowKind::Legacy:
            mrModel.EnterLegacy();
            RebuildRows();
            break;
        case FontPickerModel::RowKind::Header:
        case FontPickerModel::RowKind::NoResults:
            break; // inert
    }
}

void Writer2027FontPopup::HandleSearchEscape()
{
    if (!maQuery.isEmpty())
    {
        // First Escape clears the query (LibreOffice convention), the list
        // returns to the grouped root via the changed handler.
        m_xSearch->set_text(OUString());
    }
    else
        Close();
}

void Writer2027FontPopup::HandleTreeEscape()
{
    if (!maQuery.isEmpty())
    {
        m_xSearch->set_text(OUString());
        m_xSearch->grab_focus();
    }
    else
        Close();
}

IMPL_LINK_NOARG(Writer2027FontPopup, SearchChangedHdl, weld::TextWidget&, void)
{
    maQuery = m_xSearch->get_text();
    if (!IsOpen())
        return;
    RebuildRows();
}

IMPL_LINK(Writer2027FontPopup, SearchKeyHdl, const KeyEvent&, rKEvt, bool)
{
    if (lcl_IsPlainKey(rKEvt, KEY_DOWN))
    {
        m_xRows->grab_focus();
        MoveCursor(1);
        return true;
    }
    if (lcl_IsPlainKey(rKEvt, KEY_RETURN))
    {
        if (!maQuery.isEmpty())
        {
            const std::vector<FontPickerModel::Row>& rRows = mrModel.GetRows();
            for (const auto& rRow : rRows)
            {
                if (rRow.meKind == FontPickerModel::RowKind::Font)
                {
                    ApplyFamily(rRow.maText);
                    break;
                }
            }
        }
        else
            m_xRows->grab_focus();
        return true;
    }
    if (lcl_IsPlainKey(rKEvt, KEY_ESCAPE))
    {
        HandleSearchEscape();
        return true;
    }
    if (rKEvt.GetKeyCode().GetCode() == KEY_TAB)
    {
        m_xRows->grab_focus();
        return true;
    }
    return false;
}

IMPL_LINK(Writer2027FontPopup, TreeKeyHdl, const KeyEvent&, rKEvt, bool)
{
    const vcl::KeyCode& rKeyCode = rKEvt.GetKeyCode();
    if (lcl_IsPlainKey(rKEvt, KEY_UP))
    {
        MoveCursor(-1);
        return true;
    }
    if (lcl_IsPlainKey(rKEvt, KEY_DOWN))
    {
        MoveCursor(1);
        return true;
    }
    if (lcl_IsPlainKey(rKEvt, KEY_PAGEUP))
    {
        MoveCursorPage(-1);
        return true;
    }
    if (lcl_IsPlainKey(rKEvt, KEY_PAGEDOWN))
    {
        MoveCursorPage(1);
        return true;
    }
    if (lcl_IsPlainKey(rKEvt, KEY_HOME))
    {
        MoveCursorHomeOrEnd(true);
        return true;
    }
    if (lcl_IsPlainKey(rKEvt, KEY_END))
    {
        MoveCursorHomeOrEnd(false);
        return true;
    }
    if (lcl_IsPlainKey(rKEvt, KEY_RETURN))
    {
        const int nSel = m_xRows->get_selected_index();
        const std::vector<FontPickerModel::Row>& rRows = mrModel.GetRows();
        if (nSel >= 0 && nSel < static_cast<int>(rRows.size()))
            ActivateRow(rRows[nSel]);
        return true;
    }
    if (lcl_IsPlainKey(rKEvt, KEY_ESCAPE))
    {
        HandleTreeEscape();
        return true;
    }
    if (rKeyCode.GetCode() == KEY_TAB)
    {
        // Cyclic Tab/Shift+Tab between the search field and the list.
        m_xSearch->grab_focus();
        return true;
    }
    return false;
}

IMPL_LINK_NOARG(Writer2027FontPopup, TreeSelectionHdl, weld::ItemView&, rItemView, void)
{
    if (mbInternalMove)
        return;

    weld::TreeView& rTreeView = dynamic_cast<weld::TreeView&>(rItemView);
    const int nSel = rTreeView.get_selected_index();
    const std::vector<FontPickerModel::Row>& rRows = mrModel.GetRows();
    if (nSel < 0 || nSel >= static_cast<int>(rRows.size()))
        return;

    const FontPickerModel::Row& rRow = rRows[nSel];
    if (IsSelectableRow(rRow))
    {
        mnLastSelectedIndex = nSel;
        return;
    }

    // A header / no-results row can never keep selected state: restore the
    // previous selection immediately (clicking them does nothing).
    if (mnLastSelectedIndex >= 0 && mnLastSelectedIndex < static_cast<int>(rRows.size())
        && IsSelectableRow(rRows[mnLastSelectedIndex]))
    {
        SelectRowIndex(mnLastSelectedIndex, false);
        return;
    }
    int nFallback = GetNextSelectableIndex(nSel, 1);
    if (nFallback < 0)
        nFallback = GetNextSelectableIndex(nSel, -1);
    if (nFallback >= 0)
        SelectRowIndex(nFallback, false);
}

IMPL_LINK(Writer2027FontPopup, TreeMousePressHdl, const MouseEvent&, rEvent, bool)
{
    if (mbInternalMove || !rEvent.IsLeft())
        return false;

    // Runs after the list processed the click. Acting here (instead of on
    // selection change) also covers re-clicks on the already-selected row.
    std::unique_ptr<weld::TreeIter> xIter
        = m_xRows->get_dest_row_at_pos(rEvent.GetPosPixel(), false, false);
    if (!xIter)
        return false;

    const int nIndex = m_xRows->get_iter_index_in_parent(*xIter);
    const std::vector<FontPickerModel::Row>& rRows = mrModel.GetRows();
    if (nIndex < 0 || nIndex >= static_cast<int>(rRows.size()))
        return false;

    const FontPickerModel::Row& rRow = rRows[nIndex];
    switch (rRow.meKind)
    {
        case FontPickerModel::RowKind::Font:
            // Single click applies - the classic font-dropdown convention.
            ApplyFamily(rRow.maText);
            return true;
        case FontPickerModel::RowKind::Back:
        case FontPickerModel::RowKind::Category:
        case FontPickerModel::RowKind::Legacy:
            ActivateRow(rRow); // in-place navigation, popup stays open
            return true;
        case FontPickerModel::RowKind::Header:
        case FontPickerModel::RowKind::NoResults:
            return true; // inert
    }
    return false;
}

IMPL_LINK(Writer2027FontPopup, RowGetSizeHdl, weld::TreeView::get_size_args, aPayload, Size)
{
    const double fScale = aPayload.first.GetDPIScaleFactor();
    const FontPickerModel::Row* pRow = mrModel.FindRow(aPayload.second);
    if (!pRow)
        return Size(200, ROW_HEIGHT_NAV * fScale);
    switch (pRow->meKind)
    {
        case FontPickerModel::RowKind::Header:
            return Size(200, ROW_HEIGHT_HEADER * fScale);
        case FontPickerModel::RowKind::Font:
            return Size(200, ROW_HEIGHT_FONT * fScale);
        case FontPickerModel::RowKind::Back:
        case FontPickerModel::RowKind::Category:
        case FontPickerModel::RowKind::Legacy:
        case FontPickerModel::RowKind::NoResults:
            return Size(200, ROW_HEIGHT_NAV * fScale);
    }
    return Size(200, ROW_HEIGHT_NAV * fScale);
}

IMPL_LINK(Writer2027FontPopup, RowRenderHdl, weld::TreeView::render_args, aPayload, void)
{
    vcl::RenderContext& rCtx = std::get<0>(aPayload);
    const tools::Rectangle& rRect = std::get<1>(aPayload);
    const bool bSelected = std::get<2>(aPayload);
    const OUString& rId = std::get<3>(aPayload);

    const FontPickerModel::Row* pRow = mrModel.FindRow(rId);
    if (!pRow)
        return;

    // Full-row rect: the custom render cell starts at the first tab, extend
    // it to the right edge of the list.
    const tools::Rectangle aRowRect(
        rRect.TopLeft(),
        Size(rCtx.GetOutputSize().Width() - rRect.Left(), rRect.GetHeight()));

    const StyleSettings& rStyleSettings = Application::GetSettings().GetStyleSettings();
    const Color aWindowColor = rStyleSettings.GetWindowColor();
    const Color aTextColor = rStyleSettings.GetWindowTextColor();
    Color aMutedColor = aTextColor;
    aMutedColor.Merge(rStyleSettings.GetFieldColor(), 110);

    const double fScale = rCtx.GetDPIScaleFactor();

    if (pRow->meKind == FontPickerModel::RowKind::Font && bSelected)
    {
        // Restrained selection: a subtle tint, never the OS slab.
        Color aSelColor(aWindowColor);
        aSelColor.Merge(rStyleSettings.GetHighlightColor(), 80);
        rCtx.Push(vcl::PushFlags::FILLCOLOR | vcl::PushFlags::LINECOLOR);
        rCtx.SetFillColor(aSelColor);
        rCtx.SetLineColor(aSelColor);
        rCtx.DrawRect(aRowRect);
        rCtx.Pop();
    }

    const tools::Long nX = rRect.Left() + ROW_MARGIN * fScale;

    switch (pRow->meKind)
    {
        case FontPickerModel::RowKind::Header:
        {
            vcl::Font aHeaderFont(rCtx.GetFont());
            aHeaderFont.SetWeight(WEIGHT_BOLD);
            rCtx.Push(vcl::PushFlags::FONT | vcl::PushFlags::TEXTCOLOR | vcl::PushFlags::LINECOLOR);
            rCtx.SetFont(aHeaderFont);
            rCtx.SetTextColor(aMutedColor);
            rCtx.DrawText(Point(nX, rRect.Top() + (rRect.GetHeight() - rCtx.GetTextHeight()) / 2),
                          pRow->maText);
            // subtle divider under the section label
            rCtx.SetLineColor(aMutedColor);
            rCtx.DrawLine(Point(rRect.Left() + 4 * fScale, rRect.Bottom() - 1),
                          Point(rRect.Right(), rRect.Bottom() - 1));
            rCtx.Pop();
            break;
        }
        case FontPickerModel::RowKind::Back:
        case FontPickerModel::RowKind::Category:
        case FontPickerModel::RowKind::Legacy:
        {
            rCtx.Push(vcl::PushFlags::TEXTCOLOR);
            rCtx.SetTextColor(aTextColor);
            rCtx.DrawText(Point(nX, rRect.Top() + (rRect.GetHeight() - rCtx.GetTextHeight()) / 2),
                          pRow->maText);
            rCtx.Pop();
            break;
        }
        case FontPickerModel::RowKind::NoResults:
        {
            rCtx.Push(vcl::PushFlags::TEXTCOLOR);
            rCtx.SetTextColor(aMutedColor);
            rCtx.DrawText(Point(nX, rRect.Top() + (rRect.GetHeight() - rCtx.GetTextHeight()) / 2),
                          pRow->maText);
            rCtx.Pop();
            break;
        }
        case FontPickerModel::RowKind::Font:
        {
            // Preview glyph with the candidate font, family label and
            // metadata with the UI font (readable even for display-heavy
            // faces), dispatch name is always the installed family.
            FontMetric aMetric;
            bool bPreviewFont = mpFontList && mpFontList->IsAvailable(pRow->maText);
            if (bPreviewFont)
                aMetric = mpFontList->Get(pRow->maText, WEIGHT_NORMAL, ITALIC_NONE);

            const tools::Long nPreviewCenter
                = rRect.Left() + (PREVIEW_BLOCK_WIDTH + ROW_MARGIN) * fScale / 2;

            if (bPreviewFont)
            {
                vcl::Font aPreviewFont(aMetric);
                aPreviewFont.SetFontSize(rCtx.GetFont().GetFontSize());
                rCtx.Push(vcl::PushFlags::FONT | vcl::PushFlags::TEXTCOLOR);
                rCtx.SetFont(aPreviewFont);
                rCtx.SetTextColor(aTextColor);
                rCtx.DrawText(
                    Point(nPreviewCenter - rCtx.GetTextWidth(u"Aa"_ustr) / 2,
                          rRect.Top() + (rRect.GetHeight() - rCtx.GetTextHeight()) / 2),
                    u"Aa"_ustr);
                rCtx.Pop();
            }

            const tools::Long nTextX = nX + PREVIEW_BLOCK_WIDTH * fScale;
            const tools::Long nHalf = rRect.GetHeight() / 2;
            rCtx.Push(vcl::PushFlags::TEXTCOLOR);
            rCtx.SetTextColor(aTextColor);
            rCtx.DrawText(Point(nTextX, rRect.Top() + nHalf / 2 - rCtx.GetTextHeight() / 2),
                          pRow->maText);
            if (!pRow->maMeta.isEmpty())
            {
                rCtx.SetTextColor(aMutedColor);
                rCtx.DrawText(Point(nTextX, rRect.Top() + nHalf + nHalf / 2
                                                - rCtx.GetTextHeight() / 2),
                              pRow->maMeta);
            }
            rCtx.Pop();
            break;
        }
    }
}

IMPL_LINK_NOARG(Writer2027FontPopup, PopupClosedHdl, weld::Popover&, void)
{
    mbOpen = false;
    mnLastSelectedIndex = -1;
    // Search state is popup-only; the closed font-name control is untouched.
    m_xSearch->set_text(OUString());
    maQuery.clear();
    m_aCloseHdl.Call(*this);
}

} // namespace svx::writer2027

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
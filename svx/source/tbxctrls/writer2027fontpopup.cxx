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
#include <svx/writer2027visual.hxx>
#include <svx/writer2027log.hxx>

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
#include <vcl/weld/Window.hxx>
#include <vcl/weld/weldutils.hxx>

#include <algorithm>

namespace svx::writer2027
{

namespace
{
// Target desktop geometry (logical px, scaled by the UI DPI factor).
//
// This is a Typography Browser, not a narrow combo dropdown. Geometry comes
// from the central visual constitution (writer2027visual.hxx): a shared
// AdaptivePopoverGeometry policy plus semantic tokens. The browser renders a
// specimen-first row (reserved preview column + family label + metadata line)
// on a wide, golden-ratio-informed body; height is capped to a fraction of the
// work area so it never becomes a full-screen column.
constexpr auto POPUP_GEOMETRY
    = svx::writer2027::AdaptivePopoverGeometry(/*min*/ 560, /*preferred*/ 640, /*max*/ 760);
constexpr int PREVIEW_BLOCK_WIDTH = 72; // reserved specimen column
constexpr int ROW_MARGIN = svx::writer2027::Spacing::M;
constexpr int ROW_TEXT_START = ROW_MARGIN + PREVIEW_BLOCK_WIDTH; // name/meta x-band

bool lcl_IsPlainKey(const KeyEvent& rKEvt, sal_uInt16 nCode)
{
    const vcl::KeyCode& rKeyCode = rKEvt.GetKeyCode();
    return rKeyCode.GetCode() == nCode && !rKeyCode.IsMod1() && !rKeyCode.IsMod2()
           && !rKeyCode.IsMod3();
}

// spec 9.2: choose a font size whose actual "Aa" text bounds fit inside the
// specimen box. Real bounds measurement, scale-down-to-fit, bounded loop.
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

        // Fit to whichever axis is tighter, with a small safety factor so the
        // final glyph never clips due to rounding (spec 9.2).
        const double fScaleW = static_cast<double>(nAvailableW) / nGlyphW;
        const double fScaleH = static_cast<double>(nAvailableH) / nGlyphH;
        const double fNext = std::floor(fCandidate * std::min(fScaleW, fScaleH) * 0.98);
        if (fNext >= fCandidate || fNext < 1)
            break; // no further meaningful shrink
        fCandidate = fNext;
    }

    // Minimum-size fallback if nothing above truly fit (keeps the glyph on
    // screen but never lets it exceed the specimen clip).
    rOutFont = vcl::Font(rMetric);
    rOutFont.SetFontSize(Size(0, 10));
    return 10;
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
    // Re-entrancy guard: the popup instance is reused and the notebookbar
    // button can be clicked while it is already open. Rebuilding + repopping
    // an already-shown popover wedges the weld layer (hang), which reads as a
    // crash to the user. Refresh state and re-focus instead.
    if (mbOpen)
    {
        maCurrentFamily = rCurrentFamily;
        m_xSearch->set_text(rInitialQuery);
        maQuery = rInitialQuery;
        m_xSearch->grab_focus();
        return;
    }

    try
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
            m_xRows->connect_mouse_move(LINK(this, Writer2027FontPopup, TreeMouseMoveHdl));

            m_xSearch->connect_changed(LINK(this, Writer2027FontPopup, SearchChangedHdl));
            m_xSearch->connect_key_press(LINK(this, Writer2027FontPopup, SearchKeyHdl));
        }

        // Search state lives only in the popup; the closed font-name control is
        // never overwritten.
        m_xSearch->set_text(rInitialQuery);
        maQuery = rInitialQuery;
        // spec 5 / remediation phase 1: build the MODEL first so the geometry
        // pass below (which stores mnPopupContentWidthPx) sees the real rows
        // before the tree is populated/layout out.
        RebuildModel();

    // Geometry: a self-sizing Typography Browser driven by the shared
    // AdaptivePopoverGeometry policy (single source of truth). Width follows
    // the widest row (name + metadata + reserved specimen band) clamped to a
    // 4K-first golden-ratio-informed band; height follows content but is
    // capped to a fraction of the work area. All values logical px.
    const double fScale = Application::GetDefaultDevice()
                              ? Application::GetDefaultDevice()->GetDPIScaleFactor()
                              : 1.0;
    const AbsoluteScreenPixelRectangle aScreenRect = rAnchorWin.GetDesktopRectPixel();
    const tools::Long nWorkW = aScreenRect.GetWidth();
    const tools::Long nWorkH = aScreenRect.GetHeight();
    const tools::Long nLogW = static_cast<tools::Long>(nWorkW / fScale);

// Content floor: widest font label OR its description line + reserved
    // specimen column + margins. Measuring only the font name let the
    // description (maMeta) overflow the popup on long entries.
    tools::Long nWidestRow = 0;
    const float fCharW = m_xRows->get_approximate_digit_width();
    for (const auto& rRow : mrModel.GetRows())
    {
        if (rRow.meKind == FontPickerModel::RowKind::Font)
        {
            const float fTextW
                = std::max(rRow.maText.getLength(), rRow.maMeta.getLength()) * fCharW;
            nWidestRow = std::max(nWidestRow,
                                  static_cast<tools::Long>(fTextW + ROW_TEXT_START));
        }
    }
    const int nContentW
        = static_cast<int>(nWidestRow) + 2 * POPUP_GEOMETRY.nWorkMargin;
    // set_size_request consumes the tree's DEVICE pixels (same space as the
    // measure callback / get_height_rows), so the policy band (logical px)
    // must be scaled by fScale to device px; the work-area bound is nWorkW.
    const int nPopupWidth = static_cast<int>(POPUP_GEOMETRY.clampWidth(nContentW, nLogW) * fScale);

    // Content-driven height, capped to ~68% of the work area (65-75vh).
    // Row heights derive from the tree's own text metrics (same basis as the
    // custom-measure callback), so the popup request stays consistent with the
    // rows the tree actually lays out at any DPI.
    const tools::Long nTextH = std::max<tools::Long>(m_xRows->get_text_height(), 12);
    // Single source of truth for the row-measure callback: rows must report the
    // same width the popup actually lays them out at (device px). Without this
    // a font row reports a stale hard-coded width and the custom-rendered rows
    // drift from the tree's layout on re-open or at another DPI.
    mnPopupContentWidthPx
        = std::max<tools::Long>(nPopupWidth - 2 * ROW_MARGIN, nTextH * 8);
    tools::Long nContentH = 0;
    for (const auto& rRow : mrModel.GetRows())
    {
        switch (rRow.meKind)
        {
            case FontPickerModel::RowKind::Header:
                nContentH += nTextH * 2;
                break;
            case FontPickerModel::RowKind::Font:
                nContentH += nTextH * 5;
                break;
            case FontPickerModel::RowKind::Back:
            case FontPickerModel::RowKind::Category:
            case FontPickerModel::RowKind::Legacy:
            case FontPickerModel::RowKind::NoResults:
                nContentH += nTextH * 3;
                break;
        }
    }
    const tools::Long nMinPopupH = 5 * nTextH * 3;
    // Content and rows live in the tree's own device space (see the measure
    // callback); the work-area cap must use the same space, so nWorkH not nLogH.
    const tools::Long nPopupH = POPUP_GEOMETRY.clampHeight(nContentH, nMinPopupH, nWorkH);
    m_xRows->set_size_request(nPopupWidth, static_cast<int>(nPopupH));

    // spec 5 / remediation phase 1: only now, with the final geometry stored
    // (mnPopupContentWidthPx non-zero), populate the rows so the row-measure
    // callback never sees a zero/stale width during first layout.
    PopulateRows();

    tools::Rectangle aRect(Point(0, 0), rAnchorWin.GetSizePixel());
    weld::Window* pParent = weld::GetPopupParent(rAnchorWin, aRect);
    mbOpen = true;
    m_xPopup->popup_at_rect(pParent, aRect, weld::Placement::Under);
    m_xPopup->resize_to_request();

    // Focus the search field: typing filters immediately, Down moves into
    // the list, Enter applies the best match.
    m_xSearch->grab_focus();
    }
    catch (const css::uno::Exception& rEx)
    {
        mbOpen = false;
        svx::writer2027::Writer2027LogException("Writer2027FontPopup::Open", rEx);
    }
    catch (const std::exception& rEx)
    {
        mbOpen = false;
        svx::writer2027::Writer2027LogException("Writer2027FontPopup::Open", rEx);
    }
    catch (...)
    {
        mbOpen = false;
        svx::writer2027::Writer2027LogUnknownException("Writer2027FontPopup::Open");
    }
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

void Writer2027FontPopup::RebuildModel()
{
    if (!m_xRows || !mpFontList)
        return;
    // spec 5 / remediation phase 1: model build is separate from tree
    // population so Open() can compute the final popup geometry (and therefore
    // mnPopupContentWidthPx, which the row-measure callback consumes during
    // layout) BEFORE any row is inserted into the tree. Populating rows with a
    // zero width first is the same category of bug as the old Type System
    // geometry ordering.
    mrModel.Rebuild(mpFontList, maCurrentFamily, maQuery);
    mnHoverIndex = -1;
}

void Writer2027FontPopup::PopulateRows()
{
    if (!m_xRows || !mpFontList)
        return;

    const std::vector<FontPickerModel::Row>& rRows = mrModel.GetRows();

    // Keep the whole population + selection under mbInternalMove so no
    // selection_changed signal from inside this rebuild can re-enter
    // TreeSelectionHdl while the tree is inconsistent.
    mbInternalMove = true;
    m_xRows->freeze();
    m_xRows->clear();
    if (!rRows.empty())
    {
        m_xRows->bulk_insert_for_each(
            static_cast<int>(rRows.size()),
            [this, &rRows](weld::TreeIter& rIter, int nIndex) {
                const FontPickerModel::Row& rRow = rRows[nIndex];
                // set_text with an explicit column: the weld default (col == -1)
                // routes through SvTreeListBox::SetEntryText, which on this
                // custom-rendered tree dereferences a dangling SvLBoxString item
                // and crashes. The explicit-column path populates the row item
                // directly and is safe. (Writer 2027 font-dropdown crash fix.)
                m_xRows->set_text(rIter, rRow.maText, 0);
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
            // Set the cursor but do NOT call select() here: the initial
            // selection must be applied only once the popup has been laid out
            // and shown. Programmatically selecting during population makes
            // the weld layer fire selection_changed while the SvTreeListEntry
            // set is transient, and the handler can then re-layout a freed
            // entry (Writer 2027 font-dropdown crash). The selection tint is
            // drawn by the custom renderer from mnLastSelectedIndex.
            m_xRows->set_cursor(nSelect);
            if (auto xIter = m_xRows->get_iterator(nSelect))
                m_xRows->scroll_to_row(*xIter);
            mnLastSelectedIndex = nSelect;
        }
    }
    m_xRows->thaw();
    mbInternalMove = false;
}

void Writer2027FontPopup::RebuildRows()
{
    RebuildModel();
    PopulateRows();
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

IMPL_LINK(Writer2027FontPopup, TreeSelectionHdl, weld::ItemView&, rItemView, void)
{
    if (mbInternalMove)
        return;

    weld::TreeView& rTreeView = dynamic_cast<weld::TreeView&>(rItemView);
    const int nSel = rTreeView.get_selected_index();
    const std::vector<FontPickerModel::Row>& rRows = mrModel.GetRows();
    if (nSel < 0 || nSel >= static_cast<int>(rRows.size()))
        return;

    // Stale-signal guard: a selection_changed delivery is only trustworthy
    // when the tree currently exposes exactly the model's rows. If the counts
    // differ a RebuildRows() clear/repopulate is in progress (or just finished
    // with a freed/partial set of SvTreeListEntry nodes) - dereferencing the
    // selection there could touch a freed entry's text buffer. Drop it.
    if (rTreeView.n_children() != static_cast<int>(rRows.size()))
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

IMPL_LINK(Writer2027FontPopup, TreeMouseMoveHdl, const MouseEvent&, rEvent, bool)
{
    // Track the hovered row so the custom renderer can draw hover feedback
    // (the native combo-list highlight). Only repaint when the hovered row
    // actually changed, otherwise mouse moves over the same row would repaint
    // on every pixel.
    const std::vector<FontPickerModel::Row>& rRows = mrModel.GetRows();
    int nHover = -1;
    if (std::unique_ptr<weld::TreeIter> xIter
        = m_xRows->get_dest_row_at_pos(rEvent.GetPosPixel(), false, false))
    {
        const int nIndex = m_xRows->get_iter_index_in_parent(*xIter);
        if (nIndex >= 0 && nIndex < static_cast<int>(rRows.size())
            && IsSelectableRow(rRows[nIndex]))
            nHover = nIndex;
    }
    if (nHover != mnHoverIndex)
    {
        mnHoverIndex = nHover;
        m_xRows->queue_draw();
    }
    return false;
}

IMPL_LINK(Writer2027FontPopup, RowGetSizeHdl, weld::TreeView::get_size_args, aPayload, Size)
{
    // Row heights MUST be derived from the render context metrics (the same
    // device the paint callback draws into) — this is the FmFilterNavigator
    // convention. The VCL tree stores the returned height verbatim
    // (SvLBoxString::InitViewData -> mnHeight) and lays out / hit-tests with
    // it in DEVICE pixels; fixed logical constants collapse the rows on
    // high-DPI (4K@200%) and make the two text lines collide.
    const vcl::RenderContext& rCtx = aPayload.first;
    const tools::Long nTextH = rCtx.GetTextHeight();
    const tools::Long nBaseH = std::max<tools::Long>(nTextH, 12);

    const FontPickerModel::Row* pRow = mrModel.FindRow(aPayload.second);
    if (!pRow)
        return Size(mnPopupContentWidthPx, nBaseH * 3);
    switch (pRow->meKind)
    {
        case FontPickerModel::RowKind::Header:
            return Size(mnPopupContentWidthPx, nBaseH * 2);
        case FontPickerModel::RowKind::Font:
            // name line + meta line + specimen + paddings
            return Size(mnPopupContentWidthPx, nBaseH * 5);
        case FontPickerModel::RowKind::Back:
        case FontPickerModel::RowKind::Category:
        case FontPickerModel::RowKind::Legacy:
        case FontPickerModel::RowKind::NoResults:
            return Size(mnPopupContentWidthPx, nBaseH * 3);
    }
    return Size(mnPopupContentWidthPx, nBaseH * 3);
}

IMPL_LINK(Writer2027FontPopup, RowRenderHdl, weld::TreeView::render_args, aPayload, void)
{
    vcl::RenderContext& rCtx = std::get<0>(aPayload);
    const tools::Rectangle& rRect = std::get<1>(aPayload);
    const OUString& rId = std::get<3>(aPayload);

    // A paint-time exception in a custom renderer must never take down the
    // application: log it and render nothing for that row instead.
    try
    {
        RowRender(rCtx, rRect, rId);
    }
    catch (const css::uno::Exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027FontPopup::RowRender", rEx);
    }
    catch (const std::exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027FontPopup::RowRender", rEx);
    }
    catch (...)
    {
        svx::writer2027::Writer2027LogUnknownException("Writer2027FontPopup::RowRender");
    }
}

void Writer2027FontPopup::RowRender(vcl::RenderContext& rCtx, const tools::Rectangle& rRect,
                                    const OUString& rId)
{
    const FontPickerModel::Row* pRow = mrModel.FindRow(rId);
    if (!pRow)
        return;

    // The selection tint is driven by mnLastSelectedIndex (the "current row").
    // Because the initial select() is intentionally not called during row
    // population (it crashed the weld layer on a not-yet-laid-out tree), the
    // tree's own "selected" flag can be stale/absent here; our current-row
    // index is the source of truth for the highlighted row. The hover tint is
    // driven by mnHoverIndex (updated by TreeMouseMoveHdl).
    const std::vector<FontPickerModel::Row>& rModelRows = mrModel.GetRows();
    const bool bIsCurrentRow
        = (mnLastSelectedIndex >= 0 && mnLastSelectedIndex < static_cast<int>(rModelRows.size())
           && rModelRows[mnLastSelectedIndex].maId == rId);
    const bool bIsHoverRow
        = (mnHoverIndex >= 0 && mnHoverIndex < static_cast<int>(rModelRows.size())
           && rModelRows[mnHoverIndex].maId == rId);

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

    if (pRow->meKind == FontPickerModel::RowKind::Font)
    {
        if (bIsCurrentRow)
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
        else if (bIsHoverRow)
        {
            // Hover feedback: a lighter tint than the selection slab.
            Color aHoverColor(aWindowColor);
            aHoverColor.Merge(rStyleSettings.GetHighlightColor(), 45);
            rCtx.Push(vcl::PushFlags::FILLCOLOR | vcl::PushFlags::LINECOLOR);
            rCtx.SetFillColor(aHoverColor);
            rCtx.SetLineColor(aHoverColor);
            rCtx.DrawRect(aRowRect);
            rCtx.Pop();
        }
    }

    const tools::Long nX = rRect.Left() + ROW_MARGIN;

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
            rCtx.DrawLine(Point(rRect.Left() + 4, rRect.Bottom() - 1),
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
            // Typography row: reserved specimen block (never under the text),
            // family label on line 1, metadata (style/mood) on line 2.
            FontMetric aMetric;
            bool bPreviewFont = mpFontList && mpFontList->IsAvailable(pRow->maText);
            if (bPreviewFont)
                aMetric = mpFontList->Get(pRow->maText, WEIGHT_NORMAL, ITALIC_NONE);

            // The whole row is hard-clipped first so even a pathological
            // glyph can never paint into a neighbouring row (spec 9.3). The
            // specimen uses its own rect with explicit padding.
            rCtx.Push(vcl::PushFlags::CLIPREGION);
            rCtx.IntersectClipRegion(aRowRect);

            if (bPreviewFont)
            {
                // Specimen box: vertically centered within the row, with
                // explicit padding so the glyph never touches the row edges.
                const tools::Rectangle aSpecimenRect(
                    rRect.Left() + 4,
                    rRect.Top() + 4,
                    rRect.Left() + ROW_MARGIN + PREVIEW_BLOCK_WIDTH - 4,
                    rRect.Bottom() - 4);
                // Specimen scope: a NESTED clip so the specimen can never leak
                // into the text band, and — critically — a matching Pop() that
                // restores the row clip BEFORE the text scope below. Without
                // the Pop() the active clip stays (row ∩ specimen), and the
                // later text IntersectClipRegion(aTextBand) becomes
                // row ∩ specimen ∩ textBand == empty, clipping the family name
                // and metadata out entirely (the "Aa/Aa/Aa" screenshot).
                rCtx.Push(vcl::PushFlags::CLIPREGION);
                rCtx.IntersectClipRegion(aSpecimenRect);

                // Real glyph-bounds fit (spec 9.2): choose a font size whose
                // actual "Aa" text bounds fit inside the specimen box, then
                // draw centred on the box. Never assume nominal font size.
                vcl::Font aPreviewFont;
                const size_t nFitH
                    = lcl_FitSpecimenFont(rCtx, aMetric, aSpecimenRect, aPreviewFont);
                if (nFitH > 0)
                {
                    rCtx.Push(vcl::PushFlags::FONT | vcl::PushFlags::TEXTCOLOR);
                    rCtx.SetFont(aPreviewFont);
                    rCtx.SetTextColor(aTextColor);
                    const tools::Long nGlyphW = rCtx.GetTextWidth(u"Aa"_ustr);
                    const tools::Long nGlyphH = rCtx.GetTextHeight();
                    rCtx.DrawText(
                        Point(aSpecimenRect.Left()
                                  + (aSpecimenRect.GetWidth() - nGlyphW) / 2,
                              aSpecimenRect.Top()
                                  + (aSpecimenRect.GetHeight() - nGlyphH) / 2),
                        u"Aa"_ustr);
                    rCtx.Pop();
                }
                rCtx.Pop(); // pop the specimen clip, restoring the row clip
            }

            const tools::Long nTextX = rRect.Left() + ROW_TEXT_START;
            const tools::Long nLineH = std::max<tools::Long>(rCtx.GetTextHeight(), 12);
            const tools::Long nL1Top
                = rRect.Top() + (rRect.GetHeight() - 2 * nLineH) / 2 - nLineH / 2;
            const tools::Long nL2Top = nL1Top + nLineH + 2;

            // Text band (name + metadata) clipped to the right portion of the
            // row so long descriptions ellipsize instead of overflowing.
            {
                rCtx.Push(vcl::PushFlags::TEXTCOLOR | vcl::PushFlags::CLIPREGION);
                rCtx.SetTextColor(aTextColor);
                rCtx.IntersectClipRegion(tools::Rectangle(
                    Point(nTextX, rRect.Top()),
                    Size(aRowRect.GetWidth() - (nTextX - rRect.Left()) - ROW_MARGIN,
                         rRect.GetHeight())));
                rCtx.DrawText(Point(nTextX, nL1Top), pRow->maText);
                if (!pRow->maMeta.isEmpty())
                {
                    rCtx.SetTextColor(aMutedColor);
                    rCtx.DrawText(Point(nTextX, nL2Top), pRow->maMeta);
                }
                rCtx.Pop();
            }
            rCtx.Pop(); // pop the row clip
            break;
        }
    }
}

IMPL_LINK_NOARG(Writer2027FontPopup, PopupClosedHdl, weld::Popover&, void)
{
    mbOpen = false;
    mnLastSelectedIndex = -1;
    mnHoverIndex = -1;
    // Search state is popup-only; the closed font-name control is untouched.
    m_xSearch->set_text(OUString());
    maQuery.clear();
    m_aCloseHdl.Call(*this);
}

} // namespace svx::writer2027

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
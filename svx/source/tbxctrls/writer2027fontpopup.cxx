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
#include <svx/writer2027typographylist.hxx>
#include <svx/writer2027visual.hxx>
#include <svx/writer2027log.hxx>

#include <vcl/event.hxx>
#include <vcl/svapp.hxx>
#include <vcl/window.hxx>
#include <vcl/weld/Entry.hxx>
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
    = svx::writer2027::AdaptivePopoverGeometry(/*min*/ 600, /*preferred*/ 680, /*max*/ 800);
constexpr int ROW_MARGIN = svx::writer2027::Spacing::M;

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
    // Re-entrancy guard: the popup instance is reused and the notebookbar
    // button can be clicked while it is already open. Rebuilding + repopping
    // an already-shown popover wedges the weld layer (hang), which reads as a
    // crash to the user. Refresh state and re-focus instead.
    if (mbOpen)
    {
        maCurrentFamily = rCurrentFamily;
        m_xSearch->set_text(rInitialQuery);
        maQuery = rInitialQuery;
        RebuildModel();
        RebuildList();
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
            m_xBuilder
                = Application::CreateBuilder(pInitParent, u"svx/ui/writer2027fontpopup.ui"_ustr);
            m_xPopup = m_xBuilder->weld_popover(u"Writer2027FontPopup"_ustr);
            m_xSearch = m_xBuilder->weld_entry(u"search"_ustr);
            // The custom drawing list owns layout/paint/hit-test/scroll (spec 6).
            m_xList = std::make_unique<Writer2027TypographyList>(
                *m_xBuilder->weld_drawing_area(u"rows"_ustr));

            m_xPopup->set_accessible_name(u"Font picker"_ustr);
            m_xPopup->connect_closed(LINK(this, Writer2027FontPopup, PopupClosedHdl));

            m_xList->SetFontList(mpFontList);
            m_xList->SetCurrentFamily(maCurrentFamily);
            m_xList->connect_select(LINK(this, Writer2027FontPopup, ApplyFamily));
            m_xList->connect_activate(LINK(this, Writer2027FontPopup, ListActivateHdl));

            m_xSearch->connect_changed(LINK(this, Writer2027FontPopup, SearchChangedHdl));
            m_xSearch->connect_key_press(LINK(this, Writer2027FontPopup, SearchKeyHdl));
        }

        // Search state lives only in the popup; the closed font-name control is
        // never overwritten.
        m_xSearch->set_text(rInitialQuery);
        maQuery = rInitialQuery;
        // spec 5: build the MODEL first so the geometry pass below sees the
        // real rows before the list is laid out.
        RebuildModel();

        // Geometry: a self-sizing Typography Browser driven by the shared
        // AdaptivePopoverGeometry policy (single source of truth). Width
        // follows the widest row (name + metadata + reserved specimen band)
        // clamped to a 4K-first band; height follows content but is capped to
        // a fraction of the work area. All values logical px.
        const double fScale = Application::GetDefaultDevice()
                                  ? Application::GetDefaultDevice()->GetDPIScaleFactor()
                                  : 1.0;
        const AbsoluteScreenPixelRectangle aScreenRect = rAnchorWin.GetDesktopRectPixel();
        const tools::Long nWorkW = aScreenRect.GetWidth();
        const tools::Long nWorkH = aScreenRect.GetHeight();
        const tools::Long nLogW = static_cast<tools::Long>(nWorkW / fScale);

        // Content floor: widest font label OR its description line + reserved
        // specimen column + margins.
        tools::Long nWidestRow = 0;
        const float fCharW = m_xSearch->get_approximate_digit_width();
        for (const auto& rRow : mrModel.GetRows())
        {
            if (rRow.meKind == FontPickerModel::RowKind::Font)
            {
                const float fTextW
                    = std::max(rRow.maText.getLength(), rRow.maMeta.getLength()) * fCharW;
                nWidestRow = std::max(nWidestRow,
                                      static_cast<tools::Long>(fTextW + ROW_MARGIN * 2 + 120));
            }
        }
        const int nContentW = static_cast<int>(nWidestRow) + 2 * POPUP_GEOMETRY.nWorkMargin;
        // set_size_request consumes DEVICE pixels; the policy band (logical px)
        // must be scaled by fScale to device px; the work-area bound is nWorkW.
        const int nPopupWidth
            = static_cast<int>(POPUP_GEOMETRY.clampWidth(nContentW, nLogW) * fScale);

        // Content-driven height from semantic row heights (spec 8), capped to
        // ~68% of the work area (65-75vh).
        tools::Long nContentH = 0;
        for (const auto& rRow : mrModel.GetRows())
            nContentH += Writer2027TypographyList::GetRowHeightPx(rRow.meKind, fScale);
        const tools::Long nMinPopupH = 5 * Writer2027TypographyList::GetRowHeightPx(
                                                   FontPickerModel::RowKind::Font, fScale);
        const tools::Long nPopupH = POPUP_GEOMETRY.clampHeight(nContentH, nMinPopupH, nWorkH);

        // The list's viewport = popup height minus the search field (44 lp +
        // spacing). Single source of truth for the row width (device px).
        mnPopupContentWidthPx = nPopupWidth - 2 * ROW_MARGIN;
        const tools::Long nSearchH = 44 * fScale + 6 * fScale;
        m_xList->SetViewportSize(std::max<tools::Long>(mnPopupContentWidthPx, 1),
                                 std::max<tools::Long>(nPopupH - nSearchH, 1));
        RebuildList();

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
    RebuildModel();
    RebuildList();
}

void Writer2027FontPopup::RebuildModel()
{
    if (!m_xList || !mpFontList)
        return;
    // Model build is separate from list layout so geometry (and therefore
    // mnPopupContentWidthPx) is final before any row is laid out.
    mrModel.Rebuild(mpFontList, maCurrentFamily, maQuery);
}

void Writer2027FontPopup::RebuildList()
{
    if (!m_xList || !mpFontList)
        return;
    m_xList->SetRows(&mrModel.GetRows());
    m_xList->SetFontList(mpFontList);
    m_xList->SetCurrentFamily(maCurrentFamily);
    m_xList->RebuildLayout();
    m_xList->QueueDraw();
}

IMPL_LINK(Writer2027FontPopup, ApplyFamily, const OUString&, rFamily, void)
{
    if (!rFamily.isEmpty())
        m_aSelectHdl.Call(rFamily);
    Close();
}

IMPL_LINK(Writer2027FontPopup, ListActivateHdl, const FontPickerModel::Row&, rRow, bool)
{
    switch (rRow.meKind)
    {
        case FontPickerModel::RowKind::Back:
            mrModel.GoBack();
            RebuildModel();
            RebuildList();
            return true;
        case FontPickerModel::RowKind::Category:
            mrModel.EnterCategory(rRow.meCategory);
            RebuildModel();
            RebuildList();
            return true;
        case FontPickerModel::RowKind::Legacy:
            mrModel.EnterLegacy();
            RebuildModel();
            RebuildList();
            return true;
        case FontPickerModel::RowKind::Font:
        case FontPickerModel::RowKind::Header:
        case FontPickerModel::RowKind::NoResults:
            break; // fonts handled by connect_select; Header/NoResults inert
    }
    return false;
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

IMPL_LINK_NOARG(Writer2027FontPopup, SearchChangedHdl, weld::TextWidget&, void)
{
    maQuery = m_xSearch->get_text();
    if (!IsOpen())
        return;
    RebuildModel();
    RebuildList();
}

IMPL_LINK(Writer2027FontPopup, SearchKeyHdl, const KeyEvent&, rKEvt, bool)
{
    if (lcl_IsPlainKey(rKEvt, KEY_DOWN))
    {
        // Move into the list and select the first selectable row.
        m_xList->GrabFocus();
        m_xList->SelectFirstSelectable();
        return true;
    }
    if (lcl_IsPlainKey(rKEvt, KEY_RETURN))
    {
        if (!maQuery.isEmpty())
        {
            // Apply the best (first font) match.
            for (const auto& rRow : mrModel.GetRows())
            {
                if (rRow.meKind == FontPickerModel::RowKind::Font)
                {
                    ApplyFamily(rRow.maText);
                    break;
                }
            }
        }
        else
            m_xList->GrabFocus();
        return true;
    }
    if (lcl_IsPlainKey(rKEvt, KEY_ESCAPE))
    {
        HandleSearchEscape();
        return true;
    }
    if (rKEvt.GetKeyCode().GetCode() == KEY_TAB)
    {
        m_xList->GrabFocus();
        return true;
    }
    return false;
}

IMPL_LINK(Writer2027FontPopup, PopupClosedHdl, weld::Popover&, /*rPopover*/, void)
{
    mbOpen = false;
    if (m_aCloseHdl.IsSet())
        m_aCloseHdl.Call(*this);
}

} // namespace svx::writer2027

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
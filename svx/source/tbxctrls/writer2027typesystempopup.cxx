/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <svx/writer2027typesystempopup.hxx>
#include <svx/writer2027typesystem.hxx>
#include <svx/writer2027visual.hxx>
#include <svx/writer2027log.hxx>

#include <svtools/ctrltool.hxx>

#include <vcl/event.hxx>
#include <vcl/font.hxx>
#include <vcl/settings.hxx>
#include <vcl/svapp.hxx>
#include <vcl/vclenum.hxx>
#include <vcl/window.hxx>
#include <vcl/weld/TreeView.hxx>
#include <vcl/weld/Window.hxx>
#include <vcl/weld/weldutils.hxx>

#include <svx/dialmgr.hxx>
#include <svx/strings.hrc>

#include <algorithm>

namespace svx::writer2027
{

namespace
{

// Target desktop geometry (logical px, scaled by the UI DPI factor). Uses the
// central visual constitution + shared AdaptivePopoverGeometry policy. The
// preferred width is deliberately compact so the type-system picker reads as a
// focused popover anchored to the button, not as a full-width side panel.
constexpr auto POPUP_GEOMETRY
    = svx::writer2027::AdaptivePopoverGeometry(/*min*/ 360, /*preferred*/ 420, /*max*/ 520);
constexpr int ROW_MARGIN = svx::writer2027::Spacing::M;

bool lcl_IsPlainKey(const KeyEvent& rKEvt, sal_uInt16 nCode)
{
    const vcl::KeyCode& rKeyCode = rKEvt.GetKeyCode();
    return rKeyCode.GetCode() == nCode && !rKeyCode.IsMod1() && !rKeyCode.IsMod2()
           && !rKeyCode.IsMod3();
}

// Card content height in units of the base text height nH. This MUST mirror
// the vertical layout in Writer2027TypeSystemPopup::RowRender exactly (same
// sequence of drawn lines and the same per-line advances), otherwise the tree
// row height and the painted content disagree and the card's sample lines
// overlap / clip on high-DPI displays. Each drawn line advances the cursor by
// the line-height of the font actually used for that line (≈ the font size as
// a multiple of nH), plus half a step of breathing room before the heading.
tools::Long lcl_TypeSystemCardHeight(tools::Long nH, bool bHasMissingBlock)
{
    tools::Long nY = nH / 2; // leading offset
    nY += nH * 11 / 10;      // 1. preset name (font 11/10)
    nY += nH * 19 / 10;      // 2. heading sample (font 19/10, begins after a half step)
    nY += nH / 2;            //    breathing room before the heading line
    nY += nH * 95 / 100;     // 3. body sample (font 95/100)
    nY += nH * 85 / 100;     // 4. pairing line (font 85/100)
    if (bHasMissingBlock)
    {
        nY += nH / 2;         //    breathing room before the missing block
        nY += nH * 80 / 100;  // 5. "N fonts missing" note
        nY += nH * 80 / 100;  // 6. "Using: …" line
    }
    nY += nH; // bottom breathing room
    return nY;
}

} // namespace

Writer2027TypeSystemPopup::Writer2027TypeSystemPopup() = default;

Writer2027TypeSystemPopup::~Writer2027TypeSystemPopup() = default;

void Writer2027TypeSystemPopup::Open(const FontList* pFontList, const OUString& rCurrentPresetId,
                                     vcl::Window& rAnchorWin, tools::Rectangle rAnchorRect)
{
    svx::writer2027::Writer2027LogMessage("Writer2027TypeSystemPopup::Open", u"entered"_ustr);

    // Re-entrancy guard: the popup is a reusable instance and the notebookbar
    // button can be clicked while it is already open. Rebuilding the rows and
    // calling popup_at_rect on an already-shown popover wedges the weld layer
    // (hang, not a clean no-op). Just refresh the preset id and re-focus.
    if (mbOpen)
    {
        maCurrentPresetId = rCurrentPresetId;
        m_xRows->grab_focus();
        return;
    }

    try
    {
        // Install the OS-level crash capture so a hard fault inside the
        // popup (build/render) still produces a backtrace in
        // %TEMP%/writer2027_crash.log (see Writer2027InstallCrashCapture).
        svx::writer2027::Writer2027InstallCrashCapture();

        mpFontList = pFontList;
        maCurrentPresetId = rCurrentPresetId;

        // Build the popover once; reuse the instance across opens.
        if (!m_xBuilder)
        {
            tools::Rectangle aInitRect(Point(0, 0), rAnchorWin.GetSizePixel());
            weld::Window* pInitParent = weld::GetPopupParent(rAnchorWin, aInitRect);
            m_xBuilder = Application::CreateBuilder(pInitParent,
                                                    u"svx/ui/writer2027typesystempopup.ui"_ustr);
            m_xPopup = m_xBuilder->weld_popover(u"Writer2027TypeSystemPopup"_ustr);
            m_xRows = m_xBuilder->weld_tree_view(u"rows"_ustr);

            m_xPopup->set_accessible_name(SvxResId(STR_WRITER2027_TYPESYSTEM_POPUP_TITLE));
            m_xPopup->connect_closed(LINK(this, Writer2027TypeSystemPopup, PopupClosedHdl));

            m_xRows->set_selection_mode(SelectionMode::Single);
            m_xRows->connect_key_press(LINK(this, Writer2027TypeSystemPopup, TreeKeyHdl));
            m_xRows->connect_selection_changed(LINK(this, Writer2027TypeSystemPopup, TreeSelectionHdl));
            m_xRows->connect_mouse_press(LINK(this, Writer2027TypeSystemPopup, TreeMousePressHdl));
        }

    // Geometry: shared AdaptivePopoverGeometry policy (single source of truth).
    // Computed BEFORE RebuildRows so the tree already has the intended popup
    // dimensions (and row content width) when it measures/lays out its rows;
    // otherwise the rows first measure with the stale zero width and the text
    // renders clipped inside a too-narrow cell.
    const double fScale = Application::GetDefaultDevice()
                              ? Application::GetDefaultDevice()->GetDPIScaleFactor()
                              : 1.0;
    const AbsoluteScreenPixelRectangle aScreenRect = rAnchorWin.GetDesktopRectPixel();
    const tools::Long nWorkW = aScreenRect.GetWidth();
    const tools::Long nWorkH = aScreenRect.GetHeight();
    const tools::Long nLogW = static_cast<tools::Long>(nWorkW / fScale);

    const tools::Long nTextH = std::max<tools::Long>(m_xRows->get_text_height(), 12);
    // Plain single-line rows (the custom multi-line card renderer was removed):
    // one text-height of vertical space per row, plus a little breathing room.
    tools::Long nContentH = 0;
    for (const auto& rId : maRowIds)
        nContentH += nTextH * 2;
    const tools::Long nMinPopupH = nTextH * 4;
    const int nPopupWidth
        = static_cast<int>(POPUP_GEOMETRY.clampWidth(nTextH * 40, nLogW) * fScale);
    const tools::Long nPopupH = POPUP_GEOMETRY.clampHeight(nContentH, nMinPopupH, nWorkH);
    // Row content width = the popup's content band (device px). This is what
    // the custom row-measure callback returns as the row width so text is never
    // clipped to a narrow default cell. Must be set before RebuildRows.
    mnPopupContentWidth
        = std::max<tools::Long>(POPUP_GEOMETRY.nMinWidth * 2, nPopupWidth - 2 * ROW_MARGIN);
    m_xRows->set_size_request(nPopupWidth, static_cast<int>(nPopupH));
    svx::writer2027::Writer2027LogMessage(
        "Writer2027TypeSystemPopup::Open",
        OUString::Concat(u"geometry: fScale=") + OUString::number(fScale)
            + u" nTextH=" + OUString::number(nTextH) + u" popupW=" + OUString::number(nPopupWidth)
            + u" popupH=" + OUString::number(nPopupH) + u" contentW="
            + OUString::number(mnPopupContentWidth));

    RebuildRows();

    // Anchor under the invoking toolbar button (rAnchorRect), NOT the whole
    // document window — anchoring to the window is what made the popover land
    // as a "sidebar" at the window top-left.
    weld::Window* pParent = weld::GetPopupParent(rAnchorWin, rAnchorRect);
    mbOpen = true;
    m_xPopup->popup_at_rect(pParent, rAnchorRect, weld::Placement::Under);
    m_xPopup->resize_to_request();

    // Focus the list; the current preset (or the first preset) is selected.
    if (m_xRows->n_children() > 0)
    {
        int nIndex = 0;
        if (!maCurrentPresetId.isEmpty())
        {
            for (size_t i = 0; i < maRowIds.size(); ++i)
                if (maRowIds[i] == u"p:"_ustr + maCurrentPresetId)
                {
                    nIndex = static_cast<int>(i);
                    break;
                }
        }
        else if (!IsSelectableIndex(0))
            nIndex = GetNextSelectableIndex(0, 1);
        SelectRowIndex(nIndex, true);
    }
    m_xRows->grab_focus();
    }
    catch (const css::uno::Exception& rEx)
    {
        mbOpen = false;
        svx::writer2027::Writer2027LogException("Writer2027TypeSystemPopup::Open", rEx);
    }
    catch (const std::exception& rEx)
    {
        mbOpen = false;
        svx::writer2027::Writer2027LogException("Writer2027TypeSystemPopup::Open", rEx);
    }
    catch (...)
    {
        mbOpen = false;
        svx::writer2027::Writer2027LogUnknownException("Writer2027TypeSystemPopup::Open");
    }
}

void Writer2027TypeSystemPopup::Close()
{
    if (!mbOpen)
        return;
    m_xPopup->popdown(); // PopupClosedHdl fires via signal_closed
}

void Writer2027TypeSystemPopup::RebuildRows()
{
    maRowIds.clear();
    maResolved.clear();

    const Writer2027TypeSystemCatalog& rCatalog = Writer2027TypeSystemCatalog::Get();
    maResolved.reserve(rCatalog.GetPresets().size());

    if (maCurrentPresetId.isEmpty())
        maRowIds.push_back(u"custom"_ustr); // inert current-state row

    for (const auto& rPreset : rCatalog.GetPresets())
    {
        maRowIds.push_back(u"p:"_ustr + rPreset.maId);
        maResolved.push_back(ResolveTypeSystem(rPreset, mpFontList));
    }

    m_xRows->clear();
    if (!maRowIds.empty())
    {
        m_xRows->bulk_insert_for_each(
            static_cast<int>(maRowIds.size()),
            [this](weld::TreeIter& rIter, int nIndex) {
                const OUString& rId = maRowIds[nIndex];
                OUString aText;
                if (rId == u"custom"_ustr)
                    aText = SvxResId(STR_WRITER2027_TYPESYSTEM_CUSTOM);
                else
                {
                    const TypeSystemPreset* pPreset
                        = Writer2027TypeSystemCatalog::Get().FindPreset(rId.copy(2));
                    if (pPreset)
                    {
                        aText = SvxResId(pPreset->maNameResId);
                        // Expose the pairing to assistive technologies through
                        // the row text: name, then resolved heading + body.
                        // maRowIds may contain the inert "custom" row before
                        // the presets, so translate the tree index into the
                        // matching maResolved slot (which holds one entry per
                        // preset only) instead of indexing maResolved directly.
                        const int nPresetIndex
                            = (maCurrentPresetId.isEmpty()) ? nIndex - 1 : nIndex;
                        if (nPresetIndex >= 0
                            && nPresetIndex < static_cast<int>(maResolved.size()))
                        {
                            const ResolvedTypeSystem& rResolved = maResolved[nPresetIndex];
                            const OUString aHeading
                                = rResolved.Get(TypeSystemFontRole::Heading).maFamily;
                            const OUString aBody
                                = rResolved.Get(TypeSystemFontRole::Body).maFamily;
                            if (!aHeading.isEmpty() && !aBody.isEmpty())
                                aText += u" — "_ustr + aHeading + u" + "_ustr + aBody;
                        }
                    }
                }
                // set_text with an explicit column: the default (col == -1)
                // goes through SvTreeListBox::SetEntryText and crashes on a
                // custom-rendered tree (dangling SvLBoxString item). Use the
                // safe per-column path. (Writer 2027 popup crash fix.)
                m_xRows->set_text(rIter, aText, 0);
                m_xRows->set_id(rIter, rId);
            });
    }
}

void Writer2027TypeSystemPopup::ApplyPreset(const OUString& rPresetId)
{
    if (rPresetId.isEmpty() || rPresetId == u"custom"_ustr)
        return;
    m_aSelectHdl.Call(rPresetId);
    Close();
}

void Writer2027TypeSystemPopup::MoveCursor(int nDelta)
{
    const int nCount = m_xRows->n_children();
    if (nCount == 0)
        return;
    int nIndex = m_xRows->get_cursor_index();
    if (nIndex < 0)
        nIndex = (nDelta > 0) ? -1 : nCount;
    const int nNext = GetNextSelectableIndex(nIndex, nDelta);
    if (nNext >= 0)
        SelectRowIndex(nNext, true);
}

int Writer2027TypeSystemPopup::GetNextSelectableIndex(int nFrom, int nDelta) const
{
    const int nCount = m_xRows->n_children();
    for (int i = nFrom + nDelta; i >= 0 && i < nCount; i += nDelta)
        if (IsSelectableIndex(i))
            return i;
    return -1;
}

bool Writer2027TypeSystemPopup::IsSelectableIndex(int nIndex) const
{
    return nIndex >= 0 && nIndex < static_cast<int>(maRowIds.size())
           && maRowIds[nIndex] != u"custom"_ustr;
}

void Writer2027TypeSystemPopup::SelectRowIndex(int nIndex, bool bScroll)
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

IMPL_LINK(Writer2027TypeSystemPopup, TreeKeyHdl, const KeyEvent&, rKEvt, bool)
{
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
    if (lcl_IsPlainKey(rKEvt, KEY_HOME))
    {
        const int nNext = GetNextSelectableIndex(-1, 1);
        if (nNext >= 0)
            SelectRowIndex(nNext, true);
        return true;
    }
    if (lcl_IsPlainKey(rKEvt, KEY_END))
    {
        const int nNext = GetNextSelectableIndex(m_xRows->n_children(), -1);
        if (nNext >= 0)
            SelectRowIndex(nNext, true);
        return true;
    }
    if (lcl_IsPlainKey(rKEvt, KEY_RETURN))
    {
        const int nSel = m_xRows->get_selected_index();
        if (IsSelectableIndex(nSel))
            ApplyPreset(maRowIds[nSel].copy(2));
        return true;
    }
    if (lcl_IsPlainKey(rKEvt, KEY_ESCAPE))
    {
        Close();
        return true;
    }
    return false;
}

IMPL_LINK(Writer2027TypeSystemPopup, TreeSelectionHdl, weld::ItemView&, rItemView, void)
{
    if (mbInternalMove)
        return;

    weld::TreeView& rTreeView = dynamic_cast<weld::TreeView&>(rItemView);
    const int nSel = rTreeView.get_selected_index();
    if (IsSelectableIndex(nSel))
    {
        mnLastSelectedIndex = nSel;
        return;
    }

    // The inert "Custom" row can never keep selected state: restore the
    // previous selection immediately.
    if (mnLastSelectedIndex >= 0 && IsSelectableIndex(mnLastSelectedIndex))
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

IMPL_LINK(Writer2027TypeSystemPopup, TreeMousePressHdl, const MouseEvent&, rEvent, bool)
{
    if (mbInternalMove || !rEvent.IsLeft())
        return false;

    // Runs after the list processed the click: covers re-clicks on the
    // already-selected row too.
    std::unique_ptr<weld::TreeIter> xIter
        = m_xRows->get_dest_row_at_pos(rEvent.GetPosPixel(), false, false);
    if (!xIter)
        return false;

    const int nIndex = m_xRows->get_iter_index_in_parent(*xIter);
    if (IsSelectableIndex(nIndex))
    {
        ApplyPreset(maRowIds[nIndex].copy(2));
        return true;
    }
    return false; // inert row: let the selection handler restore state
}

IMPL_LINK(Writer2027TypeSystemPopup, RowGetSizeHdl, weld::TreeView::get_size_args, aPayload, Size)
{
    // Row heights MUST be derived from the render context metrics (the same
    // device the paint callback draws into) — this is the FmFilterNavigator
    // convention. The VCL tree stores the returned height verbatim
    // (SvLBoxString::InitViewData -> mnHeight) and lays out / hit-tests with
    // it in DEVICE pixels; fixed logical constants collapse the cards on
    // high-DPI (4K@200%) and make the stacked sample lines overlap.
    const vcl::RenderContext& rCtx = aPayload.first;
    const tools::Long nTextH = rCtx.GetTextHeight();
    const tools::Long nBaseH = std::max<tools::Long>(nTextH, 12);

    const OUString& rId = aPayload.second;
    if (rId == u"custom"_ustr)
        return Size(mnPopupContentWidth, nBaseH * 3);

    // Preset card: name + heading/body samples + pairing (+ optional missing
    // block). The height must fit the painted content exactly (see
    // lcl_TypeSystemCardHeight mirror) so the sample lines never overlap; the
    // width equals the popup content band so text is never clipped.
    const Writer2027TypeSystemCatalog& rCatalog = Writer2027TypeSystemCatalog::Get();
    const TypeSystemPreset* pPreset = rCatalog.FindPreset(rId.copy(2));
    const size_t nRowIdx
        = std::find(maRowIds.begin(), maRowIds.end(), rId) - maRowIds.begin();
    const size_t nPresetIndex = (maCurrentPresetId.isEmpty()) ? nRowIdx - 1 : nRowIdx;
    const bool bHasMissing
        = pPreset && nPresetIndex < maResolved.size()
          && maResolved[nPresetIndex].GetMissingCount() > 0;

    const tools::Long nRowW
        = std::max<tools::Long>(mnPopupContentWidth, POPUP_GEOMETRY.nMinWidth * 2);
    return Size(nRowW, lcl_TypeSystemCardHeight(nBaseH, bHasMissing));
}

IMPL_LINK(Writer2027TypeSystemPopup, RowRenderHdl, weld::TreeView::render_args, aPayload, void)
{
    vcl::RenderContext& rCtx = std::get<0>(aPayload);
    const tools::Rectangle& rRect = std::get<1>(aPayload);
    const bool bSelected = std::get<2>(aPayload);
    const OUString& rId = std::get<3>(aPayload);

    // A paint-time exception in a custom renderer must never take down the
    // application: log it and render nothing for that row instead.
    try
    {
        RowRender(rCtx, rRect, bSelected, rId);
    }
    catch (const css::uno::Exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027TypeSystemPopup::RowRender", rEx);
    }
    catch (const std::exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027TypeSystemPopup::RowRender", rEx);
    }
    catch (...)
    {
        svx::writer2027::Writer2027LogUnknownException("Writer2027TypeSystemPopup::RowRender");
    }
}

void Writer2027TypeSystemPopup::RowRender(vcl::RenderContext& rCtx, const tools::Rectangle& rRect,
                                          bool bSelected, const OUString& rId)
{
    if (rId == u"custom"_ustr)
    {
        // Inert "Custom" state row: the document's styles match no preset.
        const StyleSettings& rStyleSettings = Application::GetSettings().GetStyleSettings();
        Color aMutedColor = rStyleSettings.GetWindowTextColor();
        aMutedColor.Merge(rStyleSettings.GetFieldColor(), 110);
        rCtx.Push(vcl::PushFlags::TEXTCOLOR);
        rCtx.SetTextColor(aMutedColor);
        const tools::Long nX = rRect.Left() + ROW_MARGIN;
        rCtx.DrawText(Point(nX, rRect.Top() + (rRect.GetHeight() - rCtx.GetTextHeight()) / 2),
                      SvxResId(STR_WRITER2027_TYPESYSTEM_CUSTOM));
        rCtx.Pop();
        return;
    }

    const Writer2027TypeSystemCatalog& rCatalog = Writer2027TypeSystemCatalog::Get();
    const TypeSystemPreset* pPreset = rCatalog.FindPreset(rId.copy(2));
    // maRowIds may lead with the inert "custom" row; maResolved holds exactly
    // one entry per preset, so the preset index is the row index minus the
    // leading custom row. Guarded: an out-of-range preset just renders nothing.
    const size_t nRowIdx
        = std::find(maRowIds.begin(), maRowIds.end(), rId) - maRowIds.begin();
    const size_t nPresetIndex = (maCurrentPresetId.isEmpty()) ? nRowIdx - 1 : nRowIdx;
    if (!pPreset || nPresetIndex >= maResolved.size())
        return;
    const ResolvedTypeSystem& rResolved = maResolved[nPresetIndex];

    const StyleSettings& rStyleSettings = Application::GetSettings().GetStyleSettings();
    const Color aWindowColor = rStyleSettings.GetWindowColor();
    const Color aTextColor = rStyleSettings.GetWindowTextColor();
    Color aMutedColor = aTextColor;
    aMutedColor.Merge(rStyleSettings.GetFieldColor(), 110);

    // All layout derives from the render context's OWN pixel metrics: the
    // tree measures rows and hit-tests in the same device pixels, so using
    // GetTextHeight() here keeps measure/render consistent at ANY DPI and
    // guarantees fonts are never zero-sized (a 0-size font AVs on GDI).
    const tools::Long nTextH = std::max<tools::Long>(rCtx.GetTextHeight(), 12);
    const tools::Long nX = rRect.Left() + ROW_MARGIN;

    if (bSelected)
    {
        // Restrained selection: a subtle tint, never the OS slab.
        Color aSelColor(aWindowColor);
        aSelColor.Merge(rStyleSettings.GetHighlightColor(), 80);
        rCtx.Push(vcl::PushFlags::FILLCOLOR | vcl::PushFlags::LINECOLOR);
        rCtx.SetFillColor(aSelColor);
        rCtx.SetLineColor(aSelColor);
        rCtx.DrawRect(tools::Rectangle(rRect.TopLeft(),
                                       Size(rCtx.GetOutputSize().Width() - rRect.Left(),
                                            rRect.GetHeight())));
        rCtx.Pop();
    }

    const bool bCurrent = (maCurrentPresetId == pPreset->maId);

    // Vertical cursor: every drawn line advances by the line-height of the font
    // actually used for it (the render context's GetTextHeight after SetFont),
    // so enlarged sample fonts never collide with the following line and the
    // painted content always fits the row height produced by
    // RowGetSizeHdl / lcl_TypeSystemCardHeight (same metric basis).
    tools::Long nY = rRect.Top() + nTextH / 2;

    // 1. Preset name (bold; "✓" marks the currently active system).
    {
        vcl::Font aNameFont(rCtx.GetFont());
        aNameFont.SetWeight(WEIGHT_BOLD);
        aNameFont.SetFontSize(Size(0, nTextH * 11 / 10));
        rCtx.Push(vcl::PushFlags::FONT | vcl::PushFlags::TEXTCOLOR);
        rCtx.SetFont(aNameFont);
        rCtx.SetTextColor(aTextColor);
        OUString aName = SvxResId(pPreset->maNameResId);
        if (bCurrent)
            aName = u"✓ "_ustr + aName;
        rCtx.DrawText(Point(nX, nY), aName);
        nY += rCtx.GetTextHeight();
        rCtx.Pop();
    }

    // 2. Heading sample in the resolved heading family.
    {
        vcl::Font aSampleFont(rCtx.GetFont());
        aSampleFont.SetFontSize(Size(0, nTextH * 19 / 10));
        aSampleFont.SetWeight(static_cast<FontWeight>(pPreset->mnHeadingWeight));
        const OUString& rFamily
            = rResolved.Get(TypeSystemFontRole::Heading).maFamily;
        if (mpFontList && !rFamily.isEmpty() && mpFontList->IsAvailable(rFamily))
            aSampleFont.SetFamilyName(rFamily);
        rCtx.Push(vcl::PushFlags::FONT | vcl::PushFlags::TEXTCOLOR);
        rCtx.SetFont(aSampleFont);
        rCtx.SetTextColor(aTextColor);
        nY += nTextH / 2; // breathing room before the large sample
        rCtx.DrawText(Point(nX, nY), u"A Better Way to Write"_ustr);
        nY += rCtx.GetTextHeight();
        rCtx.Pop();
    }

    // 3. Body sample in the resolved body family.
    {
        vcl::Font aSampleFont(rCtx.GetFont());
        aSampleFont.SetFontSize(Size(0, nTextH * 95 / 100));
        const OUString& rFamily = rResolved.Get(TypeSystemFontRole::Body).maFamily;
        if (mpFontList && !rFamily.isEmpty() && mpFontList->IsAvailable(rFamily))
            aSampleFont.SetFamilyName(rFamily);
        rCtx.Push(vcl::PushFlags::FONT | vcl::PushFlags::TEXTCOLOR);
        rCtx.SetFont(aSampleFont);
        rCtx.SetTextColor(aTextColor);
        rCtx.DrawText(Point(nX, nY),
                      u"This is a short body line showing the pairing."_ustr);
        nY += rCtx.GetTextHeight();
        rCtx.Pop();
    }

    // 4. Pairing line: resolved heading + body families.
    {
        vcl::Font aPairFont(rCtx.GetFont());
        aPairFont.SetFontSize(Size(0, nTextH * 85 / 100));
        rCtx.Push(vcl::PushFlags::FONT | vcl::PushFlags::TEXTCOLOR);
        rCtx.SetFont(aPairFont);
        rCtx.SetTextColor(aMutedColor);
        const OUString aHeading = rResolved.Get(TypeSystemFontRole::Heading).maFamily;
        const OUString aBody = rResolved.Get(TypeSystemFontRole::Body).maFamily;
        OUString aPairing;
        if (!aHeading.isEmpty() && !aBody.isEmpty())
            aPairing = aHeading + u" + "_ustr + aBody;
        else if (!aHeading.isEmpty())
            aPairing = aHeading;
        else
            aPairing = aBody;
        rCtx.DrawText(Point(nX, nY), aPairing);
        nY += rCtx.GetTextHeight();
        rCtx.Pop();
    }

    // 5. Missing-font block: explicit, never silent.
    const int nMissing = rResolved.GetMissingCount();
    if (nMissing > 0)
    {
        vcl::Font aNoteFont(rCtx.GetFont());
        aNoteFont.SetFontSize(Size(0, nTextH * 80 / 100));
        rCtx.Push(vcl::PushFlags::FONT | vcl::PushFlags::TEXTCOLOR);
        rCtx.SetFont(aNoteFont);
        rCtx.SetTextColor(aMutedColor);

        nY += nTextH / 2; // breathing room before the missing block

        OUString aMissing = SvxResId(STR_WRITER2027_TYPESYSTEM_FONTS_MISSING);
        aMissing = aMissing.replaceFirst(u"%1"_ustr, OUString::number(nMissing));
        rCtx.DrawText(Point(nX, nY), aMissing);
        nY += rCtx.GetTextHeight();

        OUString aUsing;
        for (TypeSystemFontRole eRole :
             { TypeSystemFontRole::Heading, TypeSystemFontRole::Body, TypeSystemFontRole::Mono,
               TypeSystemFontRole::Display })
        {
            const TypeSystemResolvedRole& rRole = rResolved.Get(eRole);
            if (rRole.mbFallbackUsed && !rRole.maFamily.isEmpty())
            {
                if (!aUsing.isEmpty())
                    aUsing += u" + "_ustr;
                aUsing += rRole.maFamily;
            }
        }
        if (!aUsing.isEmpty())
        {
            OUString aUsingLine = SvxResId(STR_WRITER2027_TYPESYSTEM_USING_FONTS);
            aUsingLine = aUsingLine.replaceFirst(u"%1"_ustr, aUsing);
            rCtx.DrawText(Point(nX, nY), aUsingLine);
            nY += rCtx.GetTextHeight();
        }
        rCtx.Pop();
    }
}

IMPL_LINK_NOARG(Writer2027TypeSystemPopup, PopupClosedHdl, weld::Popover&, void)
{
    mbOpen = false;
    mnLastSelectedIndex = -1;
    m_aCloseHdl.Call(*this);
}

} // namespace svx::writer2027

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
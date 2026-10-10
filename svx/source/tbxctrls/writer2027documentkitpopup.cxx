/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <svx/writer2027documentkitpopup.hxx>
#include <svx/writer2027blocks.hxx>
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

#include <svx/dialmgr.hxx>
#include <svx/strings.hrc>

#include <algorithm>

namespace svx::writer2027
{

namespace
{

constexpr int ROW_MARGIN = svx::writer2027::Spacing::M;

bool lcl_IsPlainKey(const KeyEvent& rKEvt, sal_uInt16 nCode)
{
    const vcl::KeyCode& rKeyCode = rKEvt.GetKeyCode();
    return rKeyCode.GetCode() == nCode && !rKeyCode.IsMod1() && !rKeyCode.IsMod2()
           && !rKeyCode.IsMod3();
}

/// Draw a text clipped to a maximum width (append "…" when truncated).
void lcl_DrawClippedText(vcl::RenderContext& rCtx, const Point& rPos, const OUString& rText,
                         tools::Long nMaxWidth)
{
    if (rCtx.GetTextWidth(rText) <= nMaxWidth)
    {
        rCtx.DrawText(rPos, rText);
        return;
    }
    OUString aText = rText;
    while (!aText.isEmpty() && rCtx.GetTextWidth(aText + u"…"_ustr) > nMaxWidth)
        aText = aText.copy(0, aText.getLength() - 1);
    rCtx.DrawText(rPos, aText + u"…"_ustr);
}

/** Tiny composition sketch for a kit card: a muted rule + three composition
    bars that suggest the kit's rhythm (hero-ish, editorial-ish, dense). */
void lcl_DrawKitPreview(vcl::RenderContext& rCtx, const tools::Rectangle& rRect,
                        const DocumentKit& rKit, const Color& rText, const Color& rMuted,
                        const Color& rLine)
{
    tools::Long nX = rRect.Left();
    tools::Long nY = rRect.Top();
    // Logical px: the weld custom-render layer owns DPI scaling.
    auto S = [](tools::Long n) { return n; };

    const bool bEditorial = (rKit.maId == u"editorial"_ustr || rKit.maId == u"creative-agency"_ustr);
    const bool bDense = (rKit.maId == u"executive"_ustr || rKit.maId == u"research"_ustr);

    if (bEditorial)
    {
        rCtx.Push(vcl::PushFlags::FILLCOLOR | vcl::PushFlags::LINECOLOR);
        rCtx.SetFillColor(rText);
        rCtx.SetLineColor(rText);
        rCtx.DrawRect(tools::Rectangle(Point(nX + S(0), nY + S(0)), Size(S(80), S(12))));
        rCtx.Pop();
        rCtx.Push(vcl::PushFlags::FILLCOLOR | vcl::PushFlags::LINECOLOR);
        rCtx.SetFillColor(rMuted);
        rCtx.SetLineColor(rMuted);
        rCtx.DrawRect(tools::Rectangle(Point(nX + S(12), nY + S(18)), Size(S(64), S(4))));
        rCtx.DrawRect(tools::Rectangle(Point(nX + S(12), nY + S(26)), Size(S(56), S(4))));
        rCtx.DrawRect(tools::Rectangle(Point(nX + S(12), nY + S(34)), Size(S(60), S(4))));
        rCtx.Pop();
    }
    else if (bDense)
    {
        rCtx.Push(vcl::PushFlags::FILLCOLOR | vcl::PushFlags::LINECOLOR);
        rCtx.SetFillColor(rText);
        rCtx.SetLineColor(rText);
        rCtx.DrawRect(tools::Rectangle(Point(nX + S(0), nY + S(0)), Size(S(52), S(7))));
        rCtx.Pop();
        rCtx.Push(vcl::PushFlags::FILLCOLOR | vcl::PushFlags::LINECOLOR);
        rCtx.SetFillColor(rMuted);
        rCtx.SetLineColor(rMuted);
        for (int i = 0; i < 4; ++i)
            rCtx.DrawRect(tools::Rectangle(Point(nX + S(0), nY + S(12) + S(7) * i),
                                           Size(S(78), S(3))));
        rCtx.Pop();
    }
    else
    {
        rCtx.Push(vcl::PushFlags::FILLCOLOR | vcl::PushFlags::LINECOLOR);
        rCtx.SetFillColor(rText);
        rCtx.SetLineColor(rText);
        rCtx.DrawRect(tools::Rectangle(Point(nX + S(0), nY + S(0)), Size(S(64), S(9))));
        rCtx.DrawRect(tools::Rectangle(Point(nX + S(0), nY + S(13)), Size(S(40), S(4))));
        rCtx.Pop();
        rCtx.Push(vcl::PushFlags::FILLCOLOR | vcl::PushFlags::LINECOLOR);
        rCtx.SetFillColor(rMuted);
        rCtx.SetLineColor(rMuted);
        rCtx.DrawRect(tools::Rectangle(Point(nX + S(0), nY + S(24)), Size(S(70), S(3))));
        rCtx.DrawRect(tools::Rectangle(Point(nX + S(0), nY + S(31)), Size(S(60), S(3))));
        rCtx.Pop();
    }

    // Hairline frame around the preview area.
    rCtx.Push(vcl::PushFlags::LINECOLOR);
    rCtx.SetLineColor(rLine);
    rCtx.DrawRect(tools::Rectangle(rRect.TopLeft(),
                                   Size(rRect.GetWidth() - 1, rRect.GetHeight() - 1)));
    rCtx.Pop();
}

} // namespace

Writer2027DocumentKitPopup::Writer2027DocumentKitPopup(
    const css::uno::Reference<css::frame::XFrame>& xFrame, weld::Widget* pParent)
    : WeldToolbarPopup(xFrame, pParent, u"svx/ui/writer2027documentkitpopup.ui"_ustr,
                       u"Writer2027DocumentKitPopup"_ustr)
    , m_xRows(m_xBuilder->weld_tree_view(u"rows"_ustr))
{
    m_xRows->set_selection_mode(SelectionMode::Single);
    m_xRows->set_column_custom_renderer(0, true);
    m_xRows->connect_custom_get_size(LINK(this, Writer2027DocumentKitPopup, RowGetSizeHdl));
    m_xRows->connect_custom_render(LINK(this, Writer2027DocumentKitPopup, RowRenderHdl));
    m_xRows->connect_key_press(LINK(this, Writer2027DocumentKitPopup, TreeKeyHdl));
    m_xRows->connect_selection_changed(LINK(this, Writer2027DocumentKitPopup, TreeSelectionHdl));
    m_xRows->connect_mouse_press(LINK(this, Writer2027DocumentKitPopup, TreeMousePressHdl));

    RebuildRows();
    if (m_xRows->n_children() > 0)
        SelectRowIndex(0, true);
}

Writer2027DocumentKitPopup::~Writer2027DocumentKitPopup() = default;

void Writer2027DocumentKitPopup::GrabFocus()
{
    if (m_xRows)
        m_xRows->grab_focus();
}

void Writer2027DocumentKitPopup::RebuildRows()
{
    maRowIds.clear();

    const Writer2027DocumentKitCatalog& rCatalog = Writer2027DocumentKitCatalog::Get();
    for (const auto& rKit : rCatalog.GetKits())
        maRowIds.push_back(u"k:"_ustr + rKit.maId);

    m_xRows->clear();
    if (!maRowIds.empty())
    {
        m_xRows->bulk_insert_for_each(
            static_cast<int>(maRowIds.size()),
            [this](weld::TreeIter& rIter, int nIndex) {
                const OUString& rId = maRowIds[nIndex];
                const DocumentKit* pKit = Writer2027DocumentKitCatalog::Get().FindKit(rId.copy(2));
                if (pKit)
                {
                    // Expose name + description to assistive technologies. Explicit
                    // column: the col==-1 default crashes SvTreeListBox::
                    // SetEntryText on custom-rendered rows.
                    m_xRows->set_text(rIter, SvxResId(pKit->maNameResId) + u" — "_ustr
                                                 + SvxResId(pKit->maDescriptionResId),
                                     0);
                }
                m_xRows->set_id(rIter, rId);
            });
    }
}

void Writer2027DocumentKitPopup::ApplyKit(const OUString& rKitId)
{
    if (rKitId.isEmpty() || !rKitId.startsWith(u"k:"_ustr))
        return;
    if (m_aSelectHdl.IsSet())
        m_aSelectHdl.Call(rKitId.copy(2));
}

void Writer2027DocumentKitPopup::MoveCursor(int nDelta)
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

int Writer2027DocumentKitPopup::GetNextSelectableIndex(int nFrom, int nDelta) const
{
    const int nCount = m_xRows->n_children();
    for (int i = nFrom + nDelta; i >= 0 && i < nCount; i += nDelta)
        return i;
    return -1;
}

void Writer2027DocumentKitPopup::SelectRowIndex(int nIndex, bool bScroll)
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

IMPL_LINK(Writer2027DocumentKitPopup, TreeKeyHdl, const KeyEvent&, rKEvt, bool)
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
        if (nSel >= 0)
            ApplyKit(maRowIds[nSel]);
        return true;
    }
    if (lcl_IsPlainKey(rKEvt, KEY_ESCAPE))
    {
        // The framework closes the popover on Escape; nothing more to do.
        return true;
    }
    return false;
}

IMPL_LINK(Writer2027DocumentKitPopup, TreeSelectionHdl, weld::ItemView&, rItemView, void)
{
    if (mbInternalMove)
        return;
    weld::TreeView& rTreeView = dynamic_cast<weld::TreeView&>(rItemView);
    const int nSel = rTreeView.get_selected_index();
    if (nSel >= 0)
        mnLastSelectedIndex = nSel;
}

IMPL_LINK(Writer2027DocumentKitPopup, TreeMousePressHdl, const MouseEvent&, rEvent, bool)
{
    if (mbInternalMove || !rEvent.IsLeft())
        return false;

    std::unique_ptr<weld::TreeIter> xIter
        = m_xRows->get_dest_row_at_pos(rEvent.GetPosPixel(), false, false);
    if (!xIter)
        return false;

    const int nIndex = m_xRows->get_iter_index_in_parent(*xIter);
    if (nIndex >= 0)
    {
        ApplyKit(maRowIds[nIndex]);
        return true;
    }
    return false;
}

IMPL_LINK(Writer2027DocumentKitPopup, RowGetSizeHdl, weld::TreeView::get_size_args, aPayload, Size)
{
    const vcl::RenderContext& rCtx = aPayload.first;
    const tools::Long nTextH = rCtx.GetTextHeight();
    const tools::Long nBaseH = std::max<tools::Long>(nTextH, 12);
    return Size(200, nBaseH * 9);
}

IMPL_LINK(Writer2027DocumentKitPopup, RowRenderHdl, weld::TreeView::render_args, aPayload, void)
{
    vcl::RenderContext& rCtx = std::get<0>(aPayload);
    const tools::Rectangle& rRect = std::get<1>(aPayload);
    const bool bSelected = std::get<2>(aPayload);
    const OUString& rId = std::get<3>(aPayload);

    try
    {
        RowRender(rCtx, rRect, bSelected, rId);
    }
    catch (const css::uno::Exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027DocumentKitPopup::RowRender", rEx);
    }
    catch (const std::exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027DocumentKitPopup::RowRender", rEx);
    }
    catch (...)
    {
        svx::writer2027::Writer2027LogUnknownException("Writer2027DocumentKitPopup::RowRender");
    }
}

void Writer2027DocumentKitPopup::RowRender(vcl::RenderContext& rCtx,
                                           const tools::Rectangle& rRect, bool bSelected,
                                           const OUString& rId)
{
    const DocumentKit* pKit = Writer2027DocumentKitCatalog::Get().FindKit(rId.copy(2));
    if (!pKit)
        return;

    const StyleSettings& rStyleSettings = Application::GetSettings().GetStyleSettings();
    const Color aWindowColor = rStyleSettings.GetWindowColor();
    const Color aTextColor = rStyleSettings.GetWindowTextColor();
    Color aMutedColor = aTextColor;
    aMutedColor.Merge(rStyleSettings.GetFieldColor(), 110);
    Color aLineColor = aMutedColor;
    aLineColor.Merge(aWindowColor, 140);

    const tools::Long nTextH = std::max<tools::Long>(rCtx.GetTextHeight(), 12);

    const tools::Long nX = rRect.Left() + ROW_MARGIN;

    if (bSelected)
    {
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

    // 1. Miniature composition preview (left).
    {
        const tools::Long nPrevH = std::max<tools::Long>(nTextH * 5, 60);
        const tools::Long nPrevW = nPrevH * 4 / 5;
        tools::Rectangle aPrevRect(Point(nX, rRect.Top() + (rRect.GetHeight() - nPrevH) / 2),
                                   Size(nPrevW, nPrevH));
        rCtx.Push(vcl::PushFlags::FILLCOLOR | vcl::PushFlags::LINECOLOR | vcl::PushFlags::TEXTCOLOR
                  | vcl::PushFlags::FONT);
        lcl_DrawKitPreview(rCtx, aPrevRect, *pKit, aTextColor, aMutedColor, aLineColor);
        rCtx.Pop();
    }

    const tools::Long nTextX = nX + nTextH * 10;
    const tools::Long nTextMaxW = rRect.GetWidth() - (nTextX - rRect.Left()) - ROW_MARGIN;

    // 2. Kit name (bold).
    {
        vcl::Font aNameFont(rCtx.GetFont());
        aNameFont.SetWeight(WEIGHT_BOLD);
        aNameFont.SetFontSize(Size(0, nTextH * 11 / 10));
        rCtx.Push(vcl::PushFlags::FONT | vcl::PushFlags::TEXTCOLOR);
        rCtx.SetFont(aNameFont);
        rCtx.SetTextColor(aTextColor);
        lcl_DrawClippedText(rCtx, Point(nTextX, rRect.Top() + nTextH / 2),
                            SvxResId(pKit->maNameResId), nTextMaxW);
        rCtx.Pop();
    }

    // 3. Recommended Type System association.
    if (!pKit->maTypeSystemId.isEmpty())
    {
        const TypeSystemPreset* pTypeSystem
            = Writer2027TypeSystemCatalog::Get().FindPreset(pKit->maTypeSystemId);
        if (pTypeSystem)
        {
            vcl::Font aTsFont(rCtx.GetFont());
            aTsFont.SetFontSize(Size(0, nTextH * 9 / 10));
            rCtx.Push(vcl::PushFlags::FONT | vcl::PushFlags::TEXTCOLOR);
            rCtx.SetFont(aTsFont);
            rCtx.SetTextColor(aMutedColor);
            OUString aTs = SvxResId(STR_WRITER2027_KIT_TYPESYSTEM_LABEL);
            aTs = aTs.replaceFirst(u"%1"_ustr, SvxResId(pTypeSystem->maNameResId));
            lcl_DrawClippedText(rCtx, Point(nTextX, rRect.Top() + nTextH * 5 / 2), aTs,
                                nTextMaxW);
            rCtx.Pop();
        }
    }

    // 4. Description.
    {
        vcl::Font aDescFont(rCtx.GetFont());
        aDescFont.SetFontSize(Size(0, nTextH * 9 / 10));
        rCtx.Push(vcl::PushFlags::FONT | vcl::PushFlags::TEXTCOLOR);
        rCtx.SetFont(aDescFont);
        rCtx.SetTextColor(aTextColor);
        lcl_DrawClippedText(rCtx, Point(nTextX, rRect.Top() + nTextH * 9 / 2),
                            SvxResId(pKit->maDescriptionResId), nTextMaxW);
        rCtx.Pop();
    }

    // 5. Recommended use (muted).
    {
        vcl::Font aUseFont(rCtx.GetFont());
        aUseFont.SetFontSize(Size(0, nTextH * 8 / 10));
        rCtx.Push(vcl::PushFlags::FONT | vcl::PushFlags::TEXTCOLOR);
        rCtx.SetFont(aUseFont);
        rCtx.SetTextColor(aLineColor);
        lcl_DrawClippedText(rCtx, Point(nTextX, rRect.Top() + nTextH * 13 / 2),
                            SvxResId(pKit->maUseResId), nTextMaxW);
        rCtx.Pop();
    }
}

} // namespace svx::writer2027

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <svx/writer2027blockgallerypopup.hxx>
#include <svx/writer2027blocks.hxx>
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

/** Lightweight miniature preview painter. Pure vector drawing with theme
    colors: no raster assets, no full-page rendering, crisp on HiDPI. */
class PreviewPainter
{
public:
    PreviewPainter(vcl::RenderContext& rCtx, const tools::Rectangle& rRect)
        : mrCtx(rCtx)
        , mnX(rRect.Left())
        , mnY(rRect.Top())
        , mnW(rRect.GetWidth())
        , mnH(rRect.GetHeight())
    {
    }

    // Logical px: the weld custom-render layer owns DPI scaling.
    tools::Long S(tools::Long n) const { return n; }

    void Bar(tools::Long nX, tools::Long nY, tools::Long nW, tools::Long nH, const Color& rColor)
    {
        mrCtx.Push(vcl::PushFlags::FILLCOLOR | vcl::PushFlags::LINECOLOR);
        mrCtx.SetFillColor(rColor);
        mrCtx.SetLineColor(rColor);
        mrCtx.DrawRect(tools::Rectangle(Point(mnX + S(nX), mnY + S(nY)), Size(S(nW), S(nH))));
        mrCtx.Pop();
    }

    void Rule(tools::Long nX, tools::Long nY, tools::Long nW, const Color& rColor)
    {
        Bar(nX, nY, nW, 1, rColor);
    }

    void Text(tools::Long nX, tools::Long nY, const OUString& rText, const Color& rColor,
              double fFontScale)
    {
        // Derive from the render context text height (never zero): the old
        // GetFont().GetFontSize().Height() could be 0 in a fresh paint
        // context, and SetFontSize(Size(0,0)) + DrawText AVs on GDI.
        const tools::Long nCtxHeight = std::max<tools::Long>(mrCtx.GetTextHeight(), 12);
        vcl::Font aFont(mrCtx.GetFont());
        aFont.SetFontSize(Size(0, nCtxHeight * fFontScale));
        mrCtx.Push(vcl::PushFlags::FONT | vcl::PushFlags::TEXTCOLOR);
        mrCtx.SetFont(aFont);
        mrCtx.SetTextColor(rColor);
        mrCtx.DrawText(Point(mnX + S(nX), mnY + S(nY)), rText);
        mrCtx.Pop();
    }

    void Outline(tools::Long nX, tools::Long nY, tools::Long nW, tools::Long nH,
                 const Color& rColor)
    {
        mrCtx.Push(vcl::PushFlags::LINECOLOR);
        mrCtx.SetLineColor(rColor);
        mrCtx.DrawRect(tools::Rectangle(Point(mnX + S(nX), mnY + S(nY)), Size(S(nW), S(nH))));
        mrCtx.Pop();
    }

    vcl::RenderContext& mrCtx;
    tools::Long mnX, mnY, mnW, mnH;
};

void lcl_DrawPreview(vcl::RenderContext& rCtx, const tools::Rectangle& rRect,
                     const EditorialBlockDefinition& rBlock, const Color& rText,
                     const Color& rMuted, const Color& rLine)
{
    PreviewPainter aP(rCtx, rRect);

    if (rBlock.maId == u"hero"_ustr)
    {
        aP.Text(0, 2, u"CATEGORY"_ustr, rMuted, 0.6);
        aP.Bar(0, 14, 80, 14, rText);
        aP.Rule(0, 34, 64, rMuted);
        aP.Rule(0, 42, 44, rMuted);
        aP.Rule(0, 56, 40, rLine);
        aP.Rule(0, 60, 30, rLine);
        aP.Rule(0, 64, 20, rLine);
        aP.Outline(0, 72, 90, 18, rLine);
    }
    else if (rBlock.maId == u"section-intro"_ustr)
    {
        aP.Text(0, 2, u"01"_ustr, rMuted, 0.7);
        aP.Bar(0, 18, 76, 12, rText);
        aP.Rule(0, 40, 66, rMuted);
        aP.Rule(0, 48, 56, rMuted);
    }
    else if (rBlock.maId == u"pull-quote"_ustr)
    {
        aP.Text(0, 2, u"\u201C"_ustr, rText, 2.2);
        aP.Rule(8, 28, 72, rMuted);
        aP.Rule(8, 36, 64, rMuted);
        aP.Rule(10, 48, 40, rLine);
    }
    else if (rBlock.maId == u"key-stat"_ustr)
    {
        aP.Text(0, 2, u"72%"_ustr, rText, 1.8);
        aP.Rule(0, 30, 44, rMuted);
        aP.Rule(0, 40, 32, rLine);
    }
    else if (rBlock.maId == u"image-caption"_ustr)
    {
        aP.Outline(0, 0, 90, 52, rLine);
        aP.Bar(40, 24, 10, 6, rLine);
        aP.Rule(0, 62, 70, rMuted);
        aP.Rule(0, 70, 46, rLine);
    }
    else if (rBlock.maId == u"two-column"_ustr)
    {
        aP.Rule(0, 0, 88, rLine);
        aP.Rule(0, 4, 88, rLine);
        const tools::Long nColW = 42;
        for (tools::Long nCol = 0; nCol < 2; ++nCol)
        {
            const tools::Long nX = nCol * (nColW + 4);
            aP.Rule(nX, 12, nColW, rMuted);
            aP.Rule(nX, 20, nColW - 6, rMuted);
            aP.Rule(nX, 28, nColW - 10, rMuted);
            aP.Rule(nX, 36, nColW - 4, rMuted);
        }
        aP.Rule(0, 48, 88, rLine);
    }
    else if (rBlock.maId == u"callout"_ustr)
    {
        aP.Bar(0, 0, 3, 66, rLine);
        aP.Bar(6, 2, 60, 8, rText);
        aP.Rule(6, 18, 72, rMuted);
        aP.Rule(6, 26, 64, rMuted);
        aP.Rule(6, 34, 50, rMuted);
    }
    else if (rBlock.maId == u"research-note"_ustr)
    {
        aP.Text(0, 0, u"RESEARCH NOTE"_ustr, rMuted, 0.55);
        aP.Bar(0, 12, 56, 8, rText);
        aP.Rule(0, 28, 72, rMuted);
        aP.Rule(0, 36, 60, rMuted);
        aP.Rule(0, 50, 40, rLine);
    }
    else if (rBlock.maId == u"code"_ustr)
    {
        aP.Text(0, 0, u"code"_ustr, rMuted, 0.55);
        aP.Bar(0, 10, 88, 44, rLine);
        aP.Rule(6, 18, 46, rText);
        aP.Rule(6, 26, 56, rText);
        aP.Rule(6, 34, 40, rText);
        aP.Rule(6, 42, 50, rText);
    }
    else if (rBlock.maId == u"closing-statement"_ustr)
    {
        aP.Bar(0, 4, 70, 14, rText);
        aP.Rule(0, 26, 44, rMuted);
        aP.Rule(0, 34, 30, rLine);
    }
}

} // namespace

Writer2027BlockGalleryPopup::Writer2027BlockGalleryPopup(
    const css::uno::Reference<css::frame::XFrame>& xFrame, weld::Widget* pParent)
    : WeldToolbarPopup(xFrame, pParent, u"svx/ui/writer2027blockgallerypopup.ui"_ustr,
                       u"Writer2027BlockGalleryPopup"_ustr)
    , m_xSearch(m_xBuilder->weld_entry(u"search"_ustr))
    , m_xRows(m_xBuilder->weld_tree_view(u"rows"_ustr))
{
    m_xRows->set_selection_mode(SelectionMode::Single);
    m_xRows->set_column_custom_renderer(0, true);
    m_xRows->connect_custom_get_size(LINK(this, Writer2027BlockGalleryPopup, RowGetSizeHdl));
    m_xRows->connect_custom_render(LINK(this, Writer2027BlockGalleryPopup, RowRenderHdl));
    m_xRows->connect_key_press(LINK(this, Writer2027BlockGalleryPopup, TreeKeyHdl));
    m_xRows->connect_selection_changed(LINK(this, Writer2027BlockGalleryPopup, TreeSelectionHdl));
    m_xRows->connect_mouse_press(LINK(this, Writer2027BlockGalleryPopup, TreeMousePressHdl));

    if (m_xSearch)
    {
        m_xSearch->connect_changed(LINK(this, Writer2027BlockGalleryPopup, SearchChangedHdl));
        m_xSearch->connect_activate(LINK(this, Writer2027BlockGalleryPopup, SearchActivateHdl));
        m_xSearch->connect_key_press(LINK(this, Writer2027BlockGalleryPopup, SearchKeyHdl));
    }

    RebuildRows();
    if (m_xRows->n_children() > 0)
    {
        int nIndex = GetNextSelectableIndex(-1, 1);
        if (nIndex >= 0)
            SelectRowIndex(nIndex, true);
    }
}

Writer2027BlockGalleryPopup::~Writer2027BlockGalleryPopup() = default;

void Writer2027BlockGalleryPopup::GrabFocus()
{
    if (m_xSearch)
        m_xSearch->grab_focus();
}

void Writer2027BlockGalleryPopup::RebuildRows()
{
    maRowIds.clear();

    const Writer2027EditorialBlockCatalog& rCatalog = Writer2027EditorialBlockCatalog::Get();

    // Filtered blocks in display order. Within the Recommended group the
    // active kit's recommended blocks come first (session-level aid, never
    // a whole-document match).
    const DocumentKit* pKit = Writer2027DocumentKitCatalog::Get().FindKit(maActiveKitId);

    auto matches = [this](const EditorialBlockDefinition& rBlock) {
        if (maSearchText.isEmpty())
            return true;
        const OUString aSearchLower = maSearchText.toAsciiLowerCase();
        const OUString aName = SvxResId(rBlock.maNameResId).toAsciiLowerCase();
        const OUString aDesc = SvxResId(rBlock.maDescriptionResId).toAsciiLowerCase();
        return aName.indexOf(aSearchLower) >= 0 || aDesc.indexOf(aSearchLower) >= 0;
    };

    std::vector<const EditorialBlockDefinition*> aOrdered;

    if (pKit)
    {
        for (const auto& rBlockId : pKit->maRecommendedBlocks)
        {
            const EditorialBlockDefinition* pBlock = rCatalog.FindBlock(rBlockId);
            if (pBlock && pBlock->meCategory == EditorialBlockCategory::Recommended && matches(*pBlock))
                aOrdered.push_back(pBlock);
        }
    }
    for (const auto& rBlock : rCatalog.GetBlocks())
    {
        if (rBlock.meCategory != EditorialBlockCategory::Recommended || !matches(rBlock))
            continue;
        const bool bAlready = std::any_of(
            aOrdered.begin(), aOrdered.end(),
            [&rBlock](const EditorialBlockDefinition* pB) { return pB->maId == rBlock.maId; });
        if (!bAlready)
            aOrdered.push_back(&rBlock);
    }

    for (EditorialBlockCategory eCat : { EditorialBlockCategory::Editorial,
                                         EditorialBlockCategory::Technical })
    {
        for (const auto& rBlock : rCatalog.GetBlocks())
            if (rBlock.meCategory == eCat && matches(rBlock))
                aOrdered.push_back(&rBlock);
    }

    EditorialBlockCategory eCurrent = static_cast<EditorialBlockCategory>(-1);
    for (const auto* pBlock : aOrdered)
    {
        if (pBlock->meCategory != eCurrent)
        {
            eCurrent = pBlock->meCategory;
            switch (eCurrent)
            {
                case EditorialBlockCategory::Recommended:
                    maRowIds.push_back(u"h:recommended"_ustr);
                    break;
                case EditorialBlockCategory::Editorial:
                    maRowIds.push_back(u"h:editorial"_ustr);
                    break;
                case EditorialBlockCategory::Technical:
                    maRowIds.push_back(u"h:technical"_ustr);
                    break;
            }
        }
        maRowIds.push_back(u"b:"_ustr + pBlock->maId);
    }

    m_xRows->clear();
    if (!maRowIds.empty())
    {
        m_xRows->bulk_insert_for_each(
            static_cast<int>(maRowIds.size()),
            [this](weld::TreeIter& rIter, int nIndex) {
                const OUString& rId = maRowIds[nIndex];
                OUString aText;
                if (rId == u"h:recommended"_ustr)
                    aText = SvxResId(STR_WRITER2027_GALLERY_CATEGORY_RECOMMENDED);
                else if (rId == u"h:editorial"_ustr)
                    aText = SvxResId(STR_WRITER2027_GALLERY_CATEGORY_EDITORIAL);
                else if (rId == u"h:technical"_ustr)
                    aText = SvxResId(STR_WRITER2027_GALLERY_CATEGORY_TECHNICAL);
                else
                {
                    const EditorialBlockDefinition* pBlock
                        = Writer2027EditorialBlockCatalog::Get().FindBlock(rId.copy(2));
                    if (pBlock)
                    {
                        aText = SvxResId(pBlock->maNameResId) + u" — "_ustr
                                + SvxResId(pBlock->maDescriptionResId);
                    }
                }
                m_xRows->set_text(rIter, aText, 0);
                m_xRows->set_id(rIter, rId);
            });
    }
}

void Writer2027BlockGalleryPopup::ApplyBlock(const OUString& rBlockId)
{
    if (rBlockId.isEmpty() || !rBlockId.startsWith(u"b:"_ustr))
        return;
    if (m_aSelectHdl.IsSet())
        m_aSelectHdl.Call(rBlockId.copy(2));
}

void Writer2027BlockGalleryPopup::MoveCursor(int nDelta)
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

int Writer2027BlockGalleryPopup::GetNextSelectableIndex(int nFrom, int nDelta) const
{
    const int nCount = m_xRows->n_children();
    for (int i = nFrom + nDelta; i >= 0 && i < nCount; i += nDelta)
        if (IsSelectableIndex(i))
            return i;
    return -1;
}

bool Writer2027BlockGalleryPopup::IsHeaderRow(int nIndex) const
{
    return nIndex >= 0 && nIndex < static_cast<int>(maRowIds.size())
           && maRowIds[nIndex].startsWith(u"h:"_ustr);
}

bool Writer2027BlockGalleryPopup::IsSelectableIndex(int nIndex) const
{
    return !IsHeaderRow(nIndex) && nIndex < m_xRows->n_children();
}

void Writer2027BlockGalleryPopup::SelectRowIndex(int nIndex, bool bScroll)
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

IMPL_LINK(Writer2027BlockGalleryPopup, SearchChangedHdl, weld::TextWidget&, rWidget, void)
{
    maSearchText = rWidget.get_text();
    RebuildRows();
}

IMPL_LINK(Writer2027BlockGalleryPopup, SearchActivateHdl, weld::Entry&, /*rEntry*/, bool)
{
    const int nSel = m_xRows->get_selected_index();
    if (IsSelectableIndex(nSel))
        ApplyBlock(maRowIds[nSel]);
    return true;
}

IMPL_LINK(Writer2027BlockGalleryPopup, SearchKeyHdl, const KeyEvent&, rKEvt, bool)
{
    if (lcl_IsPlainKey(rKEvt, KEY_ESCAPE))
        return true; // framework closes the popover on Escape
    if (lcl_IsPlainKey(rKEvt, KEY_DOWN))
    {
        if (m_xRows->n_children() > 0)
        {
            m_xRows->grab_focus();
            MoveCursor(1);
        }
        return true;
    }
    return false;
}

IMPL_LINK(Writer2027BlockGalleryPopup, TreeKeyHdl, const KeyEvent&, rKEvt, bool)
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
            ApplyBlock(maRowIds[nSel]);
        return true;
    }
    return false;
}

IMPL_LINK(Writer2027BlockGalleryPopup, TreeSelectionHdl, weld::ItemView&, rItemView, void)
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

IMPL_LINK(Writer2027BlockGalleryPopup, TreeMousePressHdl, const MouseEvent&, rEvent, bool)
{
    if (mbInternalMove || !rEvent.IsLeft())
        return false;

    std::unique_ptr<weld::TreeIter> xIter
        = m_xRows->get_dest_row_at_pos(rEvent.GetPosPixel(), false, false);
    if (!xIter)
        return false;

    const int nIndex = m_xRows->get_iter_index_in_parent(*xIter);
    if (IsSelectableIndex(nIndex))
    {
        ApplyBlock(maRowIds[nIndex]);
        return true;
    }
    return false;
}

IMPL_LINK(Writer2027BlockGalleryPopup, RowGetSizeHdl, weld::TreeView::get_size_args, aPayload, Size)
{
    const vcl::RenderContext& rCtx = aPayload.first;
    const tools::Long nTextH = rCtx.GetTextHeight();
    const tools::Long nBaseH = std::max<tools::Long>(nTextH, 12);

    const OUString& rId = aPayload.second;
    if (rId.startsWith(u"h:"_ustr))
        return Size(200, nBaseH * 3);
    return Size(200, nBaseH * 8);
}

IMPL_LINK(Writer2027BlockGalleryPopup, RowRenderHdl, weld::TreeView::render_args, aPayload, void)
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
        svx::writer2027::Writer2027LogException("Writer2027BlockGalleryPopup::RowRender", rEx);
    }
    catch (const std::exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027BlockGalleryPopup::RowRender", rEx);
    }
    catch (...)
    {
        svx::writer2027::Writer2027LogUnknownException("Writer2027BlockGalleryPopup::RowRender");
    }
}

void Writer2027BlockGalleryPopup::RowRender(vcl::RenderContext& rCtx,
                                            const tools::Rectangle& rRect, bool bSelected,
                                            const OUString& rId)
{
    const StyleSettings& rStyleSettings = Application::GetSettings().GetStyleSettings();
    const Color aWindowColor = rStyleSettings.GetWindowColor();
    const Color aTextColor = rStyleSettings.GetWindowTextColor();
    Color aMutedColor = aTextColor;
    aMutedColor.Merge(rStyleSettings.GetFieldColor(), 110);
    Color aLineColor = aMutedColor;
    aLineColor.Merge(aWindowColor, 140);

    const tools::Long nTextH = std::max<tools::Long>(rCtx.GetTextHeight(), 12);

    const tools::Long nX = rRect.Left() + ROW_MARGIN;

    if (rId.startsWith(u"h:"_ustr))
    {
        const OUString aLabel
            = rId == u"h:recommended"_ustr   ? SvxResId(STR_WRITER2027_GALLERY_CATEGORY_RECOMMENDED)
              : rId == u"h:editorial"_ustr   ? SvxResId(STR_WRITER2027_GALLERY_CATEGORY_EDITORIAL)
                                             : SvxResId(STR_WRITER2027_GALLERY_CATEGORY_TECHNICAL);
        vcl::Font aLabelFont(rCtx.GetFont());
        aLabelFont.SetFontSize(Size(0, nTextH * 9 / 10));
        rCtx.Push(vcl::PushFlags::FONT | vcl::PushFlags::TEXTCOLOR);
        rCtx.SetFont(aLabelFont);
        rCtx.SetTextColor(aMutedColor);
        rCtx.DrawText(Point(nX, rRect.Top() + (rRect.GetHeight() - rCtx.GetTextHeight()) / 2),
                      aLabel);
        rCtx.Pop();
        return;
    }

    const EditorialBlockDefinition* pBlock
        = Writer2027EditorialBlockCatalog::Get().FindBlock(rId.copy(2));
    if (!pBlock)
        return;

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

    {
        const tools::Long nPrevH = std::max<tools::Long>(nTextH * 5, 60);
        const tools::Long nPrevW = nPrevH * 3 / 2;
        const tools::Long nPrevX = nX;
        const tools::Long nPrevY = rRect.Top() + (rRect.GetHeight() - nPrevH) / 2;
        tools::Rectangle aPrevRect(Point(nPrevX, nPrevY), Size(nPrevW, nPrevH));
        rCtx.Push(vcl::PushFlags::FILLCOLOR | vcl::PushFlags::LINECOLOR | vcl::PushFlags::TEXTCOLOR
                  | vcl::PushFlags::FONT);
        lcl_DrawPreview(rCtx, aPrevRect, *pBlock, aTextColor, aMutedColor, aLineColor);
        rCtx.Pop();
    }

    const tools::Long nTextX = nX + nTextH * 11;
    const tools::Long nTextMaxW = rRect.GetWidth() - (nTextX - rRect.Left()) - ROW_MARGIN;

    {
        vcl::Font aNameFont(rCtx.GetFont());
        aNameFont.SetWeight(WEIGHT_BOLD);
        aNameFont.SetFontSize(Size(0, nTextH * 11 / 10));
        rCtx.Push(vcl::PushFlags::FONT | vcl::PushFlags::TEXTCOLOR);
        rCtx.SetFont(aNameFont);
        rCtx.SetTextColor(aTextColor);
        lcl_DrawClippedText(rCtx, Point(nTextX, rRect.Top() + nTextH / 2),
                            SvxResId(pBlock->maNameResId), nTextMaxW);
        rCtx.Pop();
    }

    {
        vcl::Font aDescFont(rCtx.GetFont());
        aDescFont.SetFontSize(Size(0, nTextH * 9 / 10));
        rCtx.Push(vcl::PushFlags::FONT | vcl::PushFlags::TEXTCOLOR);
        rCtx.SetFont(aDescFont);
        rCtx.SetTextColor(aMutedColor);
        lcl_DrawClippedText(rCtx, Point(nTextX, rRect.Top() + nTextH * 5 / 2),
                            SvxResId(pBlock->maDescriptionResId), nTextMaxW);
        rCtx.Pop();
    }

    {
        vcl::Font aHintFont(rCtx.GetFont());
        aHintFont.SetFontSize(Size(0, nTextH * 8 / 10));
        rCtx.Push(vcl::PushFlags::FONT | vcl::PushFlags::TEXTCOLOR);
        rCtx.SetFont(aHintFont);
        rCtx.SetTextColor(aLineColor);
        OUString aHint;
        switch (pBlock->meInsertionPolicy)
        {
            case EditorialBlockInsertionPolicy::AtCursor:
                aHint = SvxResId(STR_WRITER2027_BLOCK_HINT_AT_CURSOR);
                break;
            case EditorialBlockInsertionPolicy::NewParagraph:
                aHint = SvxResId(STR_WRITER2027_BLOCK_HINT_NEW_PARAGRAPH);
                break;
            case EditorialBlockInsertionPolicy::NewSection:
                aHint = SvxResId(STR_WRITER2027_BLOCK_HINT_NEW_SECTION);
                break;
            case EditorialBlockInsertionPolicy::NewPage:
            case EditorialBlockInsertionPolicy::ReplaceEmptyDocument:
                aHint = SvxResId(STR_WRITER2027_BLOCK_HINT_NEW_PAGE);
                break;
        }
        if (!aHint.isEmpty())
            lcl_DrawClippedText(rCtx, Point(nTextX, rRect.Top() + nTextH * 9 / 2), aHint,
                                nTextMaxW);
        rCtx.Pop();
    }
}

} // namespace svx::writer2027

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
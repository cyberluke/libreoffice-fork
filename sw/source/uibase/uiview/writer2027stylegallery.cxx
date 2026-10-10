/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <writer2027stylegallery.hxx>
#include <writer2027typographymanager.hxx>

#include <svx/writer2027contrast.hxx>
#include <svx/writer2027log.hxx>

#include <IDocumentStylePoolAccess.hxx>
#include <doc.hxx>
#include <poolfmt.hxx>
#include <format.hxx>
#include <fmtcol.hxx>
#include <SwStyleNameMapper.hxx>

#include <editeng/fontitem.hxx>
#include <editeng/fhgtitem.hxx>
#include <editeng/wghtitem.hxx>
#include <svl/itemset.hxx>

#include <svtools/ctrltool.hxx> // FontList

#include <vcl/font.hxx>
#include <vcl/metric.hxx>
#include <vcl/settings.hxx>
#include <vcl/svapp.hxx>

#include <algorithm>
#include <cmath>

namespace sw::writer2027stylegallery
{

namespace
{
constexpr tools::Long CARD_W_LP = 124; // spec 36: 116-132 logical px
constexpr tools::Long CARD_H_LP = 56;  // spec 36: 54-62
constexpr tools::Long CARD_GAP_LP = 6;
constexpr tools::Long PAD_LP = 4;
constexpr tools::Long SCROLL_STEP_LP = 48;
} // namespace

Writer2027StyleGallery::Writer2027StyleGallery(vcl::Window* pParent, WinBits nStyle)
    : Window(pParent, nStyle)
{
}

Writer2027StyleGallery::~Writer2027StyleGallery() = default;

double Writer2027StyleGallery::Scale() const
{
    return Application::GetDefaultDevice() ? Application::GetDefaultDevice()->GetDPIScaleFactor()
                                           : 1.0;
}

tools::Long Writer2027StyleGallery::CardWidthPx() const
{
    return std::max<tools::Long>(static_cast<tools::Long>(std::lround(CARD_W_LP * Scale())), 1);
}

tools::Long Writer2027StyleGallery::CardHeightPx() const
{
    return std::max<tools::Long>(static_cast<tools::Long>(std::lround(CARD_H_LP * Scale())), 20);
}

tools::Long Writer2027StyleGallery::GetCardWidthPx() const
{
    return CardWidthPx();
}

void Writer2027StyleGallery::SetFontList(const FontList* pFontList)
{
    mpFontList = pFontList;
    Invalidate();
}

void Writer2027StyleGallery::SetItems(std::vector<StyleGalleryItem> aItems)
{
    maItems = std::move(aItems);
    if (mnScrollOffsetPx > GetMaxScrollOffset())
        mnScrollOffsetPx = std::max<tools::Long>(GetMaxScrollOffset(), 0);
    for (auto& rItem : maItems)
        rItem.mbCurrent = (rItem.mnPoolId == mnCurrentPoolId);
    Invalidate();
}

void Writer2027StyleGallery::SetCurrentStyle(int nCurrentPoolId)
{
    mnCurrentPoolId = nCurrentPoolId;
    for (auto& rItem : maItems)
        rItem.mbCurrent = (rItem.mnPoolId == mnCurrentPoolId);
    Invalidate();
}

void Writer2027StyleGallery::ScrollBy(tools::Long nDeltaPx)
{
    const tools::Long nOld = mnScrollOffsetPx;
    mnScrollOffsetPx
        = std::clamp<tools::Long>(mnScrollOffsetPx + nDeltaPx, 0, GetMaxScrollOffset());
    if (mnScrollOffsetPx != nOld)
        Invalidate();
}

tools::Long Writer2027StyleGallery::GetMaxScrollOffset() const
{
    const tools::Long nGap
        = std::max<tools::Long>(static_cast<tools::Long>(std::lround(CARD_GAP_LP * Scale())), 1);
    const tools::Long nSizeW = GetSizePixel().Width();
    const tools::Long nContentW
        = static_cast<tools::Long>(maItems.size()) * CardWidthPx()
          + static_cast<tools::Long>(std::max<size_t>(maItems.size(), 1) - 1) * nGap
          + 2 * std::max<tools::Long>(
                  static_cast<tools::Long>(std::lround(PAD_LP * Scale())), 1);
    return std::max<tools::Long>(nContentW - nSizeW, 0);
}

int Writer2027StyleGallery::IndexAtInternal(const Point& rPoint) const
{
    const tools::Long nPad
        = std::max<tools::Long>(static_cast<tools::Long>(std::lround(PAD_LP * Scale())), 1);
    const tools::Long nGap
        = std::max<tools::Long>(static_cast<tools::Long>(std::lround(CARD_GAP_LP * Scale())), 1);
    // ONE coordinate convention (spec V4 24, Option A): card start positions
    // include the padding; the pointer is in content space by adding scroll.
    const tools::Long nContentX = rPoint.X() + mnScrollOffsetPx;
    const size_t nItem = nContentX / (CardWidthPx() + nGap);
    if (nItem >= maItems.size())
        return -1;
    const tools::Long nCardStart = nPad + static_cast<tools::Long>(nItem) * (CardWidthPx() + nGap);
    if (nContentX < nCardStart)
        return -1; // before the first card's left edge or in a gap
    if (nContentX >= nCardStart + CardWidthPx())
        return -1; // in the trailing gap
    return static_cast<int>(nItem);
}

int Writer2027StyleGallery::IndexAt(const Point& rPoint) const
{
    return IndexAtInternal(rPoint);
}

void Writer2027StyleGallery::Paint(vcl::RenderContext& rOut, const tools::Rectangle& rRect)
{
    const StyleSettings& rStyle = Application::GetSettings().GetStyleSettings();
    const Color aWindowColor = rStyle.GetWindowColor();

    rOut.Push(vcl::PushFlags::FILLCOLOR | vcl::PushFlags::LINECOLOR);
    rOut.SetFillColor(aWindowColor);
    rOut.SetLineColor(aWindowColor);
    rOut.DrawRect(rRect);
    rOut.Pop();

    const tools::Long nPad
        = std::max<tools::Long>(static_cast<tools::Long>(std::lround(PAD_LP * Scale())), 1);
    const tools::Long nGap
        = std::max<tools::Long>(static_cast<tools::Long>(std::lround(CARD_GAP_LP * Scale())), 1);
    const tools::Long nCardH = CardHeightPx();
    const tools::Long nTop = nPad;
    const tools::Long nBottom = nTop + nCardH - 1;

    for (size_t i = 0; i < maItems.size(); ++i)
    {
        const tools::Long nLeft = nPad + static_cast<tools::Long>(i) * (CardWidthPx() + nGap)
                                  - mnScrollOffsetPx;
        if (nLeft + CardWidthPx() < rRect.Left())
            continue;
        if (nLeft > rRect.Right())
            break;

        const tools::Rectangle aCard(nLeft, nTop, nLeft + CardWidthPx() - 1, nBottom);
        const bool bHover = (static_cast<int>(i) == mnHoverIndex);
        const bool bFocus = (static_cast<int>(i) == mnFocusIndex);
        const bool bCurrent = maItems[i].mbCurrent;

        Color aBg = aWindowColor;
        if (bCurrent)
            aBg.Merge(rStyle.GetHighlightColor(), 55);
        else if (bHover)
            aBg.Merge(rStyle.GetHighlightColor(), 35);

        rOut.Push(vcl::PushFlags::CLIPREGION | vcl::PushFlags::FILLCOLOR
                  | vcl::PushFlags::LINECOLOR);
        rOut.IntersectClipRegion(aCard);
        rOut.SetFillColor(aBg);
        // Keyboard focus: a distinct ring/darker rail, separate from hover and
        // current (spec V4 31).
        if (bFocus && !bHover && !bCurrent)
        {
            Color aFocusLine = rStyle.GetHighlightColor();
            aFocusLine.Merge(aBg, 70);
            rOut.SetLineColor(aFocusLine);
        }
        else
            rOut.SetLineColor(aBg);
        rOut.DrawRect(aCard);

        // Label in the actual document style font/family/size (spec 35/56).
        vcl::Font aFont(rOut.GetFont());
        const OUString& rFam = maItems[i].maEffectiveFamily;
        if (!rFam.isEmpty() && mpFontList && mpFontList->IsAvailable(rFam))
            aFont = mpFontList->Get(rFam, WEIGHT_NORMAL, ITALIC_NONE);
        if (maItems[i].maEffectiveHeight > 0)
        {
            const tools::Long nPreviewPx
                = std::min<tools::Long>(CardHeightPx() * 7 / 10,
                                        static_cast<tools::Long>(std::lround(
                                            maItems[i].maEffectiveHeight / 20.0 * Scale())));
            aFont.SetFontSize(Size(0, std::max<tools::Long>(nPreviewPx, 9)));
        }
        if (maItems[i].mnWeight)
            aFont.SetWeight(static_cast<FontWeight>(maItems[i].mnWeight));
        rOut.SetFont(aFont);

        const Color aTextColor = rStyle.GetWindowTextColor();
        const Color aFg = svx::writer2027::Writer2027EnsureTextContrast(aTextColor, aBg, aTextColor,
                                                                        4.5);
        rOut.SetTextColor(aFg);
        const tools::Long nTextX = aCard.Left()
                                   + std::max<tools::Long>(
                                       static_cast<tools::Long>(std::lround(6 * Scale())), 3);
        rOut.DrawText(Point(nTextX, aCard.Top() + (aCard.GetHeight() - rOut.GetTextHeight()) / 2),
                      maItems[i].maDisplayName);
        rOut.Pop();
    }
}

void Writer2027StyleGallery::ActivateAt(int nIndex)
{
    if (nIndex < 0 || nIndex >= static_cast<int>(maItems.size()))
        return;
    const OUString aName = maItems[nIndex].maInternalName;
    if (!aName.isEmpty() && m_aActivateHdl.IsSet())
        m_aActivateHdl.Call(aName);
}

void Writer2027StyleGallery::MouseButtonDown(const MouseEvent& rEvent)
{
    try
    {
        if (rEvent.IsLeft())
            ActivateAt(IndexAtInternal(rEvent.GetPosPixel()));
    }
    catch (const css::uno::Exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027StyleGallery::MouseButtonDown", rEx);
    }
    catch (const std::exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027StyleGallery::MouseButtonDown", rEx);
    }
    catch (...)
    {
        svx::writer2027::Writer2027LogUnknownException("Writer2027StyleGallery::MouseButtonDown");
    }
    Window::MouseButtonDown(rEvent);
}

void Writer2027StyleGallery::MouseMove(const MouseEvent& rEvent)
{
    try
    {
        const int nHover = IndexAtInternal(rEvent.GetPosPixel());
        if (nHover != mnHoverIndex)
        {
            mnHoverIndex = nHover;
            Invalidate();
        }
    }
    catch (const css::uno::Exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027StyleGallery::MouseMove", rEx);
    }
    catch (const std::exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027StyleGallery::MouseMove", rEx);
    }
    catch (...)
    {
        svx::writer2027::Writer2027LogUnknownException("Writer2027StyleGallery::MouseMove");
    }
    Window::MouseMove(rEvent);
}

void Writer2027StyleGallery::KeyInput(const KeyEvent& rEvent)
{
    try
    {
        const vcl::KeyCode& rKeyCode = rEvent.GetKeyCode();
        const bool bPlain = !rKeyCode.IsMod1() && !rKeyCode.IsMod2() && !rKeyCode.IsMod3();
        if (bPlain && (rKeyCode.GetCode() == KEY_LEFT || rKeyCode.GetCode() == KEY_RIGHT))
        {
            const int nDelta = (rKeyCode.GetCode() == KEY_RIGHT) ? 1 : -1;
            if (!maItems.empty())
            {
                mnFocusIndex = (mnFocusIndex < 0)
                                   ? 0
                                   : std::clamp(mnFocusIndex + nDelta, 0,
                                                static_cast<int>(maItems.size()) - 1);
                const tools::Long nPad = std::max<tools::Long>(
                    static_cast<tools::Long>(std::lround(PAD_LP * Scale())), 1);
                const tools::Long nGap = std::max<tools::Long>(
                    static_cast<tools::Long>(std::lround(CARD_GAP_LP * Scale())), 1);
                const tools::Long nCardLeft
                    = nPad + static_cast<tools::Long>(mnFocusIndex) * (CardWidthPx() + nGap);
                if (nCardLeft < mnScrollOffsetPx + nPad)
                    ScrollBy(nCardLeft - (mnScrollOffsetPx + nPad));
                else if (nCardLeft + CardWidthPx()
                         > mnScrollOffsetPx + GetSizePixel().Width() - nPad)
                    ScrollBy(nCardLeft + CardWidthPx()
                             - (mnScrollOffsetPx + GetSizePixel().Width() - nPad));
                Invalidate();
            }
            return;
        }
        if (bPlain && (rKeyCode.GetCode() == KEY_HOME || rKeyCode.GetCode() == KEY_END))
        {
            if (!maItems.empty())
            {
                mnFocusIndex = (rKeyCode.GetCode() == KEY_HOME)
                                   ? 0
                                   : static_cast<int>(maItems.size()) - 1;
                ScrollBy(rKeyCode.GetCode() == KEY_HOME ? -GetMaxScrollOffset()
                                                        : GetMaxScrollOffset());
            }
            return;
        }
        if (bPlain && rKeyCode.GetCode() == KEY_RETURN)
        {
            ActivateAt(mnFocusIndex);
            return;
        }
        if (bPlain && rKeyCode.GetCode() == KEY_ESCAPE)
            return;
    }
    catch (const css::uno::Exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027StyleGallery::KeyInput", rEx);
    }
    catch (const std::exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027StyleGallery::KeyInput", rEx);
    }
    catch (...)
    {
        svx::writer2027::Writer2027LogUnknownException("Writer2027StyleGallery::KeyInput");
    }
    Window::KeyInput(rEvent);
}

void Writer2027StyleGallery::Command(const CommandEvent& rEvent)
{
    try
    {
        if (rEvent.GetCommand() == CommandEventId::Wheel)
        {
            const CommandWheelData* pWheel = rEvent.GetWheelData();
            if (pWheel && pWheel->IsHorz() && !pWheel->IsDeltaPixel()
                && pWheel->GetModifier() == 0)
            {
                const tools::Long nStep = std::max<tools::Long>(
                    static_cast<tools::Long>(std::lround(SCROLL_STEP_LP * Scale())), 1);
                ScrollBy(pWheel->GetDelta() > 0 ? -nStep : nStep);
                return;
            }
        }
    }
    catch (const css::uno::Exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027StyleGallery::Command", rEx);
    }
    catch (const std::exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027StyleGallery::Command", rEx);
    }
    catch (...)
    {
        svx::writer2027::Writer2027LogUnknownException("Writer2027StyleGallery::Command");
    }
    Window::Command(rEvent);
}

std::vector<StyleGalleryItem>
BuildStyleGalleryModel(SwDoc& rDoc, int nCurrentPoolId)
{
    std::vector<StyleGalleryItem> aItems;
    namespace TSMan = sw::writer2027typographymanager;
    for (const auto& rDesc : TSMan::GetWriter2027SemanticStyleDescriptors())
    {
        // The paragraph gallery shows paragraph styles only (Body, H1-H6,
        // Title, Subtitle, Quote, Caption, CodeBlock) - skip character styles.
        if (rDesc.mbCharacterStyle)
            continue;
        const auto ePoolId = static_cast<SwPoolFormatId>(rDesc.mnPoolId);
        SwTextFormatColl* pColl = rDoc.getIDocumentStylePoolAccess().GetTextCollFromPool(ePoolId);
        if (!pColl)
            continue;

        StyleGalleryItem aItem;
        aItem.meRole = static_cast<int>(rDesc.meRole);
        aItem.mnPoolId = static_cast<int>(ePoolId);
        aItem.maDisplayName = rDesc.maUiLabel;
        // Canonical programmatic name for .uno:StyleApply dispatch.
        aItem.maInternalName
            = SwStyleNameMapper::GetProgName(ePoolId, pColl->GetName()).toString();
        const SfxItemSet& rSet = pColl->GetAttrSet();
        aItem.maEffectiveFamily = rSet.Get(RES_CHRATR_FONT).GetFamilyName();
        aItem.maEffectiveHeight
            = static_cast<tools::Long>(rSet.Get(RES_CHRATR_FONTSIZE).GetHeight());
        aItem.mnWeight = rSet.Get(RES_CHRATR_WEIGHT).GetWeight();
        aItem.mbCurrent = (aItem.mnPoolId == nCurrentPoolId);
        aItems.push_back(aItem);
    }
    return aItems;
}

} // namespace sw::writer2027stylegallery

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
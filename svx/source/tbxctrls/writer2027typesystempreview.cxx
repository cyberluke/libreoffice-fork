/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <svx/writer2027typesystempreview.hxx>
#include <svx/writer2027contrast.hxx>
#include <svx/writer2027log.hxx>

#include <svtools/ctrltool.hxx> // FontList

#include <vcl/font.hxx>
#include <vcl/metric.hxx>
#include <vcl/settings.hxx>
#include <vcl/svapp.hxx>

#include <algorithm>
#include <cmath>
#include <utility>

namespace svx::writer2027
{

namespace
{
// Preview hierarchy sizes, logical px (spec 11/52).
constexpr tools::Long PREV_HEADING_LP = 30;
constexpr tools::Long PREV_BODY_LP = 16;
constexpr tools::Long PREV_MONO_LP = 14;
constexpr tools::Long PREV_LABEL_LP = 11;
constexpr tools::Long PREV_GAP_LP = 16;
constexpr tools::Long PAD_LP = 20;
} // namespace

Writer2027TypeSystemPreview::Writer2027TypeSystemPreview(weld::DrawingArea& rArea)
    : m_xArea(rArea)
{
    m_xArea.connect_draw(LINK(this, Writer2027TypeSystemPreview, DrawHdl));
}

Writer2027TypeSystemPreview::~Writer2027TypeSystemPreview() = default;

double Writer2027TypeSystemPreview::Scale() const
{
    return Application::GetDefaultDevice() ? Application::GetDefaultDevice()->GetDPIScaleFactor()
                                           : 1.0;
}

tools::Long Writer2027TypeSystemPreview::ClampLogical(tools::Long nLp) const
{
    return std::max<tools::Long>(
        static_cast<tools::Long>(std::lround(nLp * Scale())), 1);
}

void Writer2027TypeSystemPreview::SetModel(TypeSystemPreviewModel aModel)
{
    maModel = std::move(aModel);
    m_xArea.queue_draw();
}

void Writer2027TypeSystemPreview::SetFontList(const FontList* pFontList)
{
    mpFontList = pFontList;
    m_xArea.queue_draw();
}

void Writer2027TypeSystemPreview::SetViewportSize(const tools::Long nWidthPx,
                                                  const tools::Long nHeightPx)
{
    mnWidthPx = nWidthPx;
    mnHeightPx = nHeightPx;
    m_xArea.set_size_request(static_cast<int>(mnWidthPx), static_cast<int>(mnHeightPx));
    m_xArea.queue_draw();
}

IMPL_LINK(Writer2027TypeSystemPreview, DrawHdl, weld::DrawingArea::draw_args, aPayload, void)
{
    try
    {
        vcl::RenderContext& rCtx = aPayload.first;
        const tools::Rectangle& rRect = aPayload.second;
        Paint(rCtx, rRect);
    }
    catch (const css::uno::Exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027TypeSystemPreview::Draw", rEx);
    }
    catch (const std::exception& rEx)
    {
        svx::writer2027::Writer2027LogException("Writer2027TypeSystemPreview::Draw", rEx);
    }
    catch (...)
    {
        svx::writer2027::Writer2027LogUnknownException("Writer2027TypeSystemPreview::Draw");
    }
}

void Writer2027TypeSystemPreview::PaintSection(vcl::RenderContext& rCtx,
                                               const tools::Rectangle& rSection,
                                               const OUString& rLabel, const OUString& rText,
                                               const OUString& rFamily, sal_uInt16 nWeight,
                                               double fSizeLp)
{
    const StyleSettings& rStyle = Application::GetSettings().GetStyleSettings();
    const Color aBg = rStyle.GetWindowColor();
    const Color aTextColor = rStyle.GetWindowTextColor();
    const tools::Long nX = rSection.Left() + ClampLogical(4);

    // Section label drawn in the UI font.
    rCtx.Push(vcl::PushFlags::CLIPREGION | vcl::PushFlags::FONT | vcl::PushFlags::TEXTCOLOR);
    rCtx.IntersectClipRegion(rSection);
    vcl::Font aLabelFont(rCtx.GetFont());
    aLabelFont.SetFontSize(Size(0, ClampLogical(PREV_LABEL_LP)));
    rCtx.SetFont(aLabelFont);
    const Color aLabel
        = svx::writer2027::Writer2027EnsureTextContrast(aTextColor, aBg, aTextColor, 4.5);
    rCtx.SetTextColor(aLabel);
    rCtx.DrawText(Point(nX, rSection.Top()), rLabel);
    rCtx.Pop();

    // Sample text drawn in the resolved role font (spec 52: "Sample text must
    // use the role font"). Ellipsize/let the row clip bound overflow; the
    // section rect keeps it from overlapping the next section.
    rCtx.Push(vcl::PushFlags::CLIPREGION | vcl::PushFlags::FONT | vcl::PushFlags::TEXTCOLOR);
    rCtx.IntersectClipRegion(tools::Rectangle(rSection.Left(), rSection.Top() + ClampLogical(20),
                                              rSection.Right(), rSection.Bottom()));
    vcl::Font aSampleFont(rCtx.GetFont());
    if (!rFamily.isEmpty() && mpFontList && mpFontList->IsAvailable(rFamily))
        aSampleFont = mpFontList->Get(rFamily, WEIGHT_NORMAL, ITALIC_NONE);
    aSampleFont.SetFontSize(Size(0, ClampLogical(static_cast<tools::Long>(fSizeLp))));
    if (nWeight)
        aSampleFont.SetWeight(static_cast<FontWeight>(nWeight));
    rCtx.SetFont(aSampleFont);
    rCtx.SetTextColor(aTextColor);
    rCtx.DrawText(Point(nX, rSection.Top() + ClampLogical(20)), rText);
    rCtx.Pop();
}

void Writer2027TypeSystemPreview::Paint(vcl::RenderContext& rCtx,
                                        const tools::Rectangle& rRect)
{
    const StyleSettings& rStyle = Application::GetSettings().GetStyleSettings();
    const Color aWindowColor = rStyle.GetWindowColor();

    rCtx.Push(vcl::PushFlags::FILLCOLOR | vcl::PushFlags::LINECOLOR);
    rCtx.SetFillColor(aWindowColor);
    rCtx.SetLineColor(aWindowColor);
    rCtx.DrawRect(rRect);
    rCtx.Pop();

    const tools::Long nPad = ClampLogical(PAD_LP);
    const tools::Long nGap = ClampLogical(PREV_GAP_LP);
    const tools::Long nLeft = rRect.Left() + nPad;
    const tools::Long nRight = rRect.Right() - nPad;
    tools::Long nY = rRect.Top() + nPad;

    // Preset name (UI font, bold-ish) - a section header.
    if (!maModel.maDisplayName.isEmpty())
    {
        rCtx.Push(vcl::PushFlags::FONT | vcl::PushFlags::TEXTCOLOR);
        vcl::Font aNameFont(rCtx.GetFont());
        aNameFont.SetWeight(WEIGHT_BOLD);
        aNameFont.SetFontSize(Size(0, ClampLogical(15)));
        rCtx.SetFont(aNameFont);
        rCtx.SetTextColor(rStyle.GetWindowTextColor());
        rCtx.DrawText(Point(nLeft, nY), maModel.maDisplayName);
        rCtx.Pop();
        nY += ClampLogical(34);
    }

    // Heading section.
    tools::Rectangle aHeadingSection(nLeft, nY, nRight, nY + ClampLogical(64));
    PaintSection(rCtx, aHeadingSection,
                 u"Heading · "_ustr + (maModel.maHeadingFamily.isEmpty()
                                            ? u"—"_ustr
                                            : maModel.maHeadingFamily),
                 maModel.maHeadingSample, maModel.maHeadingFamily, maModel.mnHeadingWeight,
                 PREV_HEADING_LP);
    nY += ClampLogical(64) + nGap;

    // Body section.
    tools::Rectangle aBodySection(nLeft, nY, nRight, nY + ClampLogical(48));
    PaintSection(rCtx, aBodySection,
                 u"Body · "_ustr
                     + (maModel.maBodyFamily.isEmpty() ? u"—"_ustr : maModel.maBodyFamily),
                 maModel.maBodySample, maModel.maBodyFamily, 0, PREV_BODY_LP);
    nY += ClampLogical(48) + nGap;

    // Code section.
    tools::Rectangle aCodeSection(nLeft, nY, nRight, nY + ClampLogical(40));
    PaintSection(rCtx, aCodeSection,
                 u"Code · "_ustr
                     + (maModel.maMonoFamily.isEmpty() ? u"—"_ustr : maModel.maMonoFamily),
                 maModel.maMonoSample, maModel.maMonoFamily, 0, PREV_MONO_LP);
    nY += ClampLogical(40) + nGap;

    // Scale line (UI font).
    if (!maModel.maScaleLabelText.isEmpty())
    {
        rCtx.Push(vcl::PushFlags::FONT | vcl::PushFlags::TEXTCOLOR);
        vcl::Font aFont(rCtx.GetFont());
        aFont.SetFontSize(Size(0, ClampLogical(PREV_LABEL_LP)));
        rCtx.SetFont(aFont);
        rCtx.SetTextColor(rStyle.GetWindowTextColor());
        rCtx.DrawText(Point(nLeft, nY), u"Scale · "_ustr + maModel.maScaleLabelText);
        rCtx.Pop();
        nY += ClampLogical(24);
    }

    // Fallback line (UI font).
    if (!maModel.maFallbackText.isEmpty())
    {
        rCtx.Push(vcl::PushFlags::FONT | vcl::PushFlags::TEXTCOLOR);
        vcl::Font aFont(rCtx.GetFont());
        aFont.SetFontSize(Size(0, ClampLogical(PREV_LABEL_LP)));
        rCtx.SetFont(aFont);
        rCtx.SetTextColor(rStyle.GetWindowTextColor());
        rCtx.DrawText(Point(nLeft, nY), u"Fallbacks · "_ustr + maModel.maFallbackText);
        rCtx.Pop();
    }
}

} // namespace svx::writer2027

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
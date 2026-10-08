/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <writer2027view.hxx>

#include <writer2027.hxx>
#include <doc.hxx>
#include <IDocumentStylePoolAccess.hxx>
#include <pagedesc.hxx>
#include <format.hxx>
#include <poolfmt.hxx>
#include <viewsh.hxx>
#include <viewopt.hxx>

#include <editeng/brushitem.hxx>

#include <algorithm>
#include <vcl/settings.hxx>
#include <vcl/svapp.hxx>

namespace sw::writer2027view
{
namespace
{
// Classic 5 mm border/gap (matches DOCUMENTBORDER / SwViewOption::defGapBetweenPages).
constexpr SwTwips cClassicBorder = 284;
// Target page share of the stage width on wide windows (golden-ratio inspired).
constexpr double cTargetPageShare = 0.62;
// Edit-window width (twips) at which the canvas gutter starts to ramp in (~1267 px).
constexpr tools::Long cRampStartTwips = 19000;
// Edit-window width (twips) at which the full canvas gutter applies (~1667 px).
constexpr tools::Long cRampFullTwips = 25000;
}

bool IsDarkThemeActive()
{
    const StyleSettings& rStyle = Application::GetSettings().GetStyleSettings();
    // high contrast stays untouched so the classic high-contrast presentation wins
    return !rStyle.GetHighContrastMode() && rStyle.GetFaceColor().IsDark();
}

bool IsWriter2027DarkDocument(SwDoc& rDoc)
{
    const SwPageDesc* pDesc = rDoc.getIDocumentStylePoolAccess().GetPageDescFromPool(
        SwPoolFormatId::PAGE_STANDARD, /*bRegardLanguage=*/false);
    if (!pDesc)
        return false;
    const SvxBrushItem* pBrush = pDesc->GetMaster().GetItemIfSet(RES_BACKGROUND, false);
    return pBrush && pBrush->GetColor() == sw::writer2027::PageBackground;
}

bool IsWriter2027CanvasActive(SwDoc& rDoc)
{
    return IsDarkThemeActive() && IsWriter2027DarkDocument(rDoc);
}

SwTwips ComputeCanvasBorder(tools::Long nViewWidth)
{
    if (nViewWidth <= cRampStartTwips)
        return cClassicBorder;

    const SwTwips nWideGutter
        = static_cast<SwTwips>(nViewWidth * (1.0 - cTargetPageShare) / 2.0);
    const double fT = std::clamp(
        (nViewWidth - cRampStartTwips) / static_cast<double>(cRampFullTwips - cRampStartTwips),
        0.0, 1.0);
    return static_cast<SwTwips>(cClassicBorder + (nWideGutter - cClassicBorder) * fT);
}

SwTwips ComputeCanvasGap(tools::Long nViewWidth)
{
    return std::max<SwTwips>(cClassicBorder, nViewWidth / 100);
}

AuthoringMode GetCurrentAuthoringMode(const SwViewShell& rSh)
{
    const SwViewOption* pOpt = rSh.GetViewOptions();
    if (pOpt->getBrowseMode() && pOpt->getDraftView() && !pOpt->IsViewAnyRuler())
        return AuthoringMode::Story;
    if (pOpt->getBrowseMode() && pOpt->getDraftView())
        return AuthoringMode::Flow;
    if (pOpt->getBrowseMode())
        return AuthoringMode::Web;
    return AuthoringMode::Layout;
}

WriterOutputIntent DefaultOutputIntent(bool bIsPDFExport, bool bIsWriter2027DarkDocument)
{
    // PDF export keeps the authored digital appearance; physical print of a
    // Writer 2027 dark document defaults to the paper-friendly transform.
    // Everything else preserves the classic LibreOffice behavior.
    return (!bIsPDFExport && bIsWriter2027DarkDocument) ? WriterOutputIntent::PrintFriendly
                                                        : WriterOutputIntent::DigitalAppearance;
}

std::optional<Color> GetPrintFriendlyColor(const Color& rColor)
{
    // Only the default Writer 2027 digital palette is remapped (exact match).
    // The page fill itself is suppressed by the existing print-page-background
    // seam (PageBackground is a fill, not a text color — a dark page color
    // used as text stays readable on white and is left alone). Explicit user
    // colors never match these constants and stay untouched.
    if (rColor == sw::writer2027::TextPrimary)
        return Color(0x1A, 0x1A, 0x1A); // near-black body/heading text
    if (rColor == sw::writer2027::TextSecondary)
        return Color(0x40, 0x4A, 0x54); // dark gray secondary text
    if (rColor == sw::writer2027::Hairline)
        return Color(0x2B, 0x2F, 0x33); // dark neutral hairline
    if (rColor == sw::writer2027::Link)
        return Color(0x0B, 0x53, 0x94); // dark blue link
    if (rColor == sw::writer2027::LinkVisited)
        return Color(0x5E, 0x35, 0xB1); // dark violet visited link
    return std::nullopt;
}

sal_uInt16 ComputeInitialAuthoringZoom(tools::Long nPageWidthTwips, tools::Long nStageWidthTwips)
{
    if (nPageWidthTwips <= 0 || nStageWidthTwips <= 0)
        return 100;

    // Scale the page so it occupies the golden-ratio stage share (~61.8%).
    // Both inputs are twips, so the ratio is unit-consistent: on a wide 4K
    // stage the page must zoom up substantially (1.6x-2.2x legacy 100% glyph
    // scale); on a compact window the result naturally stays near legacy.
    const double fTargetShare = cTargetPageShare; // 0.62, golden-ratio inspired
    const double fZoom = (nStageWidthTwips * fTargetShare) / nPageWidthTwips;

    // Comfortable authoring band: never below legacy 100%, never absurdly
    // large (matches the 1.6x-2.2x qualitative guidance on wide/high-DPI).
    constexpr double cMinAuthoringZoom = 1.0; // 100%
    constexpr double cMaxAuthoringZoom = 2.6; // 260% ceiling
    const sal_uInt16 nZoom = static_cast<sal_uInt16>(
        std::clamp(fZoom * 100.0, cMinAuthoringZoom * 100.0, cMaxAuthoringZoom * 100.0));
    return nZoom;
}
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
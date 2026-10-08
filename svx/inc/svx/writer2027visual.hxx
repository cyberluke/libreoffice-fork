/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <tools/gen.hxx>
#include <sal/types.h>
#include <algorithm>

namespace svx::writer2027
{
// ===========================================================================
// Writer 2027 Visual Constitution — semantic tokens (single source of truth).
//
// Phase 2 of the Writer 2027 UI recovery replaces scattered per-file magic
// constants with one semantic token system. Small-scale components use the
// ergonomic spacing/control-height scales below; the golden ratio is reserved
// for large region composition (see WorkspaceCompositionPolicy), not for
// every padding or icon size.
//
// All values are LOGICAL pixels. The weld layer scales them by the UI DPI
// factor; do not multiply by DPI here.
// ===========================================================================

// --- Spacing rhythm (4/8/12/16/24/32/48 logical units) --------------------
struct Spacing
{
    static constexpr sal_Int32 XS = 4;
    static constexpr sal_Int32 S = 8;
    static constexpr sal_Int32 M = 12;
    static constexpr sal_Int32 L = 16;
    static constexpr sal_Int32 XL = 24;
    static constexpr sal_Int32 XXL = 32;
    static constexpr sal_Int32 XXXL = 48;
};

// --- Density modes ----------------------------------------------------------
enum class Density
{
    Compact,
    Standard,
    Studio
};

struct ControlHeight
{
    static constexpr sal_Int32 Compact = 34;
    static constexpr sal_Int32 Standard = 40;
    static constexpr sal_Int32 Studio = 48;

    static constexpr sal_Int32 of(Density eDensity)
    {
        switch (eDensity)
        {
            case Density::Compact:
                return Compact;
            case Density::Studio:
                return Studio;
            case Density::Standard:
            default:
                return Standard;
        }
    }
};

// --- Typography roles (UI chrome, not document fonts) -----------------------
struct UIType
{
    static constexpr double CaptionScale = 0.78;
    static constexpr double BodyScale = 1.0;
    static constexpr double LabelScale = 1.05;
    static constexpr double TitleScale = 1.35;
};

// --- Shape / border / elevation ---------------------------------------------
struct Radius
{
    static constexpr sal_Int32 Small = 3;
    static constexpr sal_Int32 Medium = 6;
};

struct Elevation
{
    // Popovers and cards use near-zero decorative shadow (digital surface).
    static constexpr sal_Int32 Popover = 1;
    static constexpr sal_Int32 Card = 1;
};

// --- Panel width classes (central, application-consumable) -------------------
struct PanelWidth
{
    static constexpr sal_Int32 Compact = 220;
    static constexpr sal_Int32 Standard = 300;
    static constexpr sal_Int32 Studio = 360;

    static constexpr sal_Int32 of(Density eDensity)
    {
        switch (eDensity)
        {
            case Density::Compact:
                return Compact;
            case Density::Studio:
                return Studio;
            case Density::Standard:
            default:
                return Standard;
        }
    }
};

// ===========================================================================
// AdaptivePopoverGeometry — shared generic popover sizing policy.
//
// All Writer 2027 custom popovers (Typography Browser, Type System,
// Document Kit, Editorial Block gallery) previously duplicated work-area
// width/height clamping with unrelated constants. This helper centralizes the
// policy: it computes a width clamped into [minimum, maximum] (with a
// content-preferred target and a work-area bound) and a height clamped to a
// fraction of the available work area. It is intentionally generic (no
// Writer-2027-specific naming baked in) so other applications can consume it.
// ===========================================================================
struct AdaptivePopoverGeometry
{
    sal_Int32 nMinWidth;
    sal_Int32 nPreferredWidth;
    sal_Int32 nMaxWidth;
    sal_Int32 nHeightVhPercent; ///< height cap as % of work area (e.g. 68)
    sal_Int32 nWorkMargin;       ///< outer margin from work area (logical px)

    constexpr AdaptivePopoverGeometry(sal_Int32 nMin, sal_Int32 nPreferred, sal_Int32 nMax,
                                      sal_Int32 nVh = 68, sal_Int32 nMargin = 16)
        : nMinWidth(nMin)
        , nPreferredWidth(nPreferred)
        , nMaxWidth(nMax)
        , nHeightVhPercent(nVh)
        , nWorkMargin(nMargin)
    {
    }

    /// Clamp a content/preferred width into the configured band, honoring the
    /// available work area. All inputs/outputs are logical px.
    sal_Int32 clampWidth(sal_Int32 nContentWidth, sal_Int32 nWorkWidthLogical) const
    {
        sal_Int32 nWidth = std::max(nContentWidth, nPreferredWidth);
        nWidth = std::clamp(nWidth, nMinWidth, nMaxWidth);
        const sal_Int32 nWorkBound = std::max<sal_Int32>(nMinWidth, nWorkWidthLogical - 2 * nWorkMargin);
        return std::min(nWidth, nWorkBound);
    }

    /// Compute the popup height for a given content height (logical px):
    /// clamp to [minimum, work-area * vh% - margins].
    sal_Int32 clampHeight(sal_Int32 nContentHeight, sal_Int32 nMinHeight,
                          sal_Int32 nWorkHeightLogical) const
    {
        const sal_Int32 nVhCap = std::max<sal_Int32>(
            nMinHeight, (nWorkHeightLogical * nHeightVhPercent) / 100 - 2 * nWorkMargin);
        return std::clamp(nContentHeight, nMinHeight, nVhCap);
    }

    /// One-call convenience: both dimensions from a screen rect (physical px).
    /// fScale maps logical -> physical (DPIScaleFactor).
    tools::Long clampHeightPhysical(tools::Long nContentHeightLogical, tools::Long nMinHeightLogical,
                                    tools::Long nWorkHeightPhysical, double fScale) const
    {
        const sal_Int32 nWorkHLogical = static_cast<sal_Int32>(nWorkHeightPhysical / fScale);
        return static_cast<tools::Long>(clampHeight(nContentHeightLogical, nMinHeightLogical,
                                                    nWorkHLogical)
                                        * fScale);
    }
};

// ===========================================================================
// WorkspaceCompositionPolicy — golden-ratio region composition (semantic).
//
// The golden ratio guides MAJOR region balance only (document stage vs
// auxiliary space). This is configuration, not a magic-number sprinkling.
// ===========================================================================
struct WorkspaceCompositionPolicy
{
    // Primary relationship: PRIMARY DOCUMENT STAGE ≈ 61.8%,
    // AUXILIARY SPACE ≈ 38.2% on wide/Studio layouts.
    static constexpr double PrimaryStageRatio = 0.618;
    static constexpr double PrimaryAuxRatio = 1.0 - PrimaryStageRatio;

    // Further optional subdivision of the auxiliary region (rail/panel).
    static constexpr double AuxRailRatio = 0.382;
    static constexpr double AuxPanelRatio = 1.0 - AuxRailRatio;

    /// Suggested default stage/aux split on wide layouts (fraction of total).
    static double stageFraction(Density eDensity)
    {
        // Studio/wide: golden-ratio target. Standard: less extreme.
        return eDensity == Density::Studio ? PrimaryStageRatio : 0.62;
    }
};

} // namespace svx::writer2027

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
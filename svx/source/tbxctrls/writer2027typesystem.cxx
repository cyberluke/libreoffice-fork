/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <svx/writer2027typesystem.hxx>
#include <svx/writer2027typography.hxx>

#include <svtools/ctrltool.hxx>
#include <tools/fontenum.hxx>

#include <svx/strings.hrc>

#include <utility>

namespace svx::writer2027
{

namespace
{

// Resolved typographic scales. All sizes are twips (1/20 pt); the ratios
// follow the Phase 6 spec and are rounded to clean point values.
const TypeSystemScale aBalanced = {
    /*mnBody*/ 220, /*mnH1*/ 420, /*mnH2*/ 340, /*mnH3*/ 280, /*mnTitle*/ 540,
    /*mnSubtitle*/ 260, /*mnCaption*/ 190, /*mnQuote*/ 220, /*mnMono*/ 200,
    /*mnLineSpacingPercent*/ 115, /*mnSpaceAfter*/ 150,
    /*H1*/ 300, 150, /*H2*/ 260, 120, /*H3*/ 220, 100,
    /*Title*/ 200, 220, /*mnSubtitleAfter*/ 300,
    /*Quote*/ 240, 150, /*mnCaptionBefore*/ 180
};

const TypeSystemScale aEditorial = {
    /*mnBody*/ 240, /*mnH1*/ 520, /*mnH2*/ 400, /*mnH3*/ 320, /*mnTitle*/ 720,
    /*mnSubtitle*/ 280, /*mnCaption*/ 200, /*mnQuote*/ 240, /*mnMono*/ 210,
    /*mnLineSpacingPercent*/ 130, /*mnSpaceAfter*/ 190,
    /*H1*/ 380, 170, /*H2*/ 320, 140, /*H3*/ 260, 110,
    /*Title*/ 240, 240, /*mnSubtitleAfter*/ 340,
    /*Quote*/ 300, 190, /*mnCaptionBefore*/ 220
};

const TypeSystemScale aCompact = {
    /*mnBody*/ 210, /*mnH1*/ 340, /*mnH2*/ 290, /*mnH3*/ 250, /*mnTitle*/ 420,
    /*mnSubtitle*/ 240, /*mnCaption*/ 180, /*mnQuote*/ 210, /*mnMono*/ 190,
    /*mnLineSpacingPercent*/ 110, /*mnSpaceAfter*/ 120,
    /*H1*/ 240, 100, /*H2*/ 200, 80, /*H3*/ 170, 70,
    /*Title*/ 150, 150, /*mnSubtitleAfter*/ 240,
    /*Quote*/ 190, 110, /*mnCaptionBefore*/ 140
};

const TypeSystemScale aAccessible = {
    /*mnBody*/ 250, /*mnH1*/ 480, /*mnH2*/ 400, /*mnH3*/ 320, /*mnTitle*/ 600,
    /*mnSubtitle*/ 300, /*mnCaption*/ 220, /*mnQuote*/ 250, /*mnMono*/ 220,
    /*mnLineSpacingPercent*/ 135, /*mnSpaceAfter*/ 200,
    /*H1*/ 320, 170, /*H2*/ 280, 130, /*H3*/ 240, 110,
    /*Title*/ 220, 240, /*mnSubtitleAfter*/ 320,
    /*Quote*/ 260, 170, /*mnCaptionBefore*/ 200
};

// Semantic color roles. Values are hex "#RRGGBB" and reuse the canonical
// Writer 2027 digital palette (Phase 1, sw/inc/writer2027.hxx) - the
// application service applies them only to digital-dark documents.
const TypeSystemColorRoles aDigitalSecondaryRoles = {
    /*headingAccent*/ OUString(),
    /*quoteAccent*/ u"#AAB5C0"_ustr,     // TextSecondary
    /*captionSecondary*/ u"#AAB5C0"_ustr, // TextSecondary
    /*linkRole*/ OUString()               // canonical Phase 1 link colors
};

const TypeSystemColorRoles aNoColorRoles = {};

// Curated preset table. Family names are canonical installed family names;
// the fallback chains are deterministic and mirrored in the UI.
struct PresetSeed
{
    const char* pId;
    TranslateId aNameResId;
    const char* pHeading;
    const char* pHeadingFb1;
    const char* pHeadingFb2;
    const char* pBody;
    const char* pBodyFb1;
    const char* pBodyFb2;
    const char* pMono;
    const char* pMonoFb1;
    const char* pMonoFb2;
    const char* pDisplay;
    const char* pDisplayFb1;
    const char* pDisplayFb2;
    TypeSystemScaleId eScale;
    sal_uInt16 nHeadingWeight;
    sal_uInt16 nTitleWeight;
    const TypeSystemColorRoles* pColors;
};

const PresetSeed aPresetSeeds[] = {
    // Modern Product: clean, product, technical, premium, screen-first.
    { "modern-product", STR_WRITER2027_TYPESYSTEM_MODERN_PRODUCT,
      "Neue Montreal", "Inter", nullptr,
      "SAP 72", "Inter", nullptr,
      "SAP 72 Mono", "JetBrains Mono", nullptr,
      "Neue Montreal", "Inter", nullptr,
      TypeSystemScaleId::Balanced, WEIGHT_SEMIBOLD, WEIGHT_SEMIBOLD, &aDigitalSecondaryRoles },
    // Executive: restrained, clear, business, high-trust.
    { "executive", STR_WRITER2027_TYPESYSTEM_EXECUTIVE,
      "Söhne", "Inter", nullptr,
      "SAP 72", "Inter", nullptr,
      "JetBrains Mono", nullptr, nullptr,
      "Söhne", "Inter", nullptr,
      TypeSystemScaleId::Balanced, WEIGHT_SEMIBOLD, WEIGHT_SEMIBOLD, &aDigitalSecondaryRoles },
    // Editorial: magazine, essay, long-form, editorial.
    { "editorial", STR_WRITER2027_TYPESYSTEM_EDITORIAL,
      "Canela", "Instrument Serif", "Source Serif",
      "Söhne", "Inter", nullptr,
      "IBM Plex Mono", "JetBrains Mono", nullptr,
      "Canela", "Instrument Serif", "Source Serif",
      TypeSystemScaleId::Editorial, WEIGHT_MEDIUM, WEIGHT_MEDIUM, &aDigitalSecondaryRoles },
    // Tech: engineering, startup, AI, developer.
    { "tech", STR_WRITER2027_TYPESYSTEM_TECH,
      "Neue Machina", "IBM Plex Sans", nullptr,
      "Inter", nullptr, nullptr,
      "JetBrains Mono", nullptr, nullptr,
      "Neue Machina", "IBM Plex Sans", nullptr,
      TypeSystemScaleId::Balanced, WEIGHT_SEMIBOLD, WEIGHT_SEMIBOLD, &aDigitalSecondaryRoles },
    // Research: scientific, structured, readable, neutral.
    { "research", STR_WRITER2027_TYPESYSTEM_RESEARCH,
      "SAP 72", "IBM Plex Sans", nullptr,
      "SAP 72", "IBM Plex Sans", nullptr,
      "SAP 72 Mono", "IBM Plex Mono", nullptr,
      "SAP 72", "IBM Plex Sans", nullptr,
      TypeSystemScaleId::Balanced, WEIGHT_SEMIBOLD, WEIGHT_SEMIBOLD, &aDigitalSecondaryRoles },
    // Accessible: high legibility, generous spacing, clear hierarchy.
    // OpenDyslexic is offered as the secondary option (fallback chain).
    // Descriptive language only - no medical claims.
    { "accessible", STR_WRITER2027_TYPESYSTEM_ACCESSIBLE,
      "Atkinson Hyperlegible", "OpenDyslexic", nullptr,
      "Atkinson Hyperlegible", "OpenDyslexic", nullptr,
      "JetBrains Mono", nullptr, nullptr,
      "Atkinson Hyperlegible", "OpenDyslexic", nullptr,
      TypeSystemScaleId::Accessible, WEIGHT_BOLD, WEIGHT_BOLD, &aNoColorRoles },
    // Creative Agency: brand, campaign, portfolio, expressive.
    { "creative-agency", STR_WRITER2027_TYPESYSTEM_CREATIVE_AGENCY,
      "Hatton", "Instrument Serif", "Inter",
      "Neue Montreal", "Inter", nullptr,
      "JetBrains Mono", nullptr, nullptr,
      "Hatton", "Druk", "Inter",
      TypeSystemScaleId::Balanced, WEIGHT_BOLD, WEIGHT_BOLD, &aDigitalSecondaryRoles },
};

TypeSystemFontSpec MakeFontSpec(const char* pPreferred, const char* pFb1,
                                const char* pFb2)
{
    TypeSystemFontSpec aSpec;
    if (pPreferred)
        aSpec.maPreferred = OUString::fromUtf8(pPreferred);
    if (pFb1)
        aSpec.maFallback1 = OUString::fromUtf8(pFb1);
    if (pFb2)
        aSpec.maFallback2 = OUString::fromUtf8(pFb2);
    return aSpec;
}

} // namespace

namespace
{

// Phase 6 family aliases: design/marketing names -> the installed family
// name of the same typeface. SAP 72 ships under the family name "72" (and
// 72 Mono under "72 Mono"), so the preset names from the design brief must
// map onto the real installed families. These are aliases of the SAME font,
// not fallbacks: resolving through them is not reported as "missing".
const std::pair<const char*, const char*> aTypeSystemAliases[] = {
    { "SAP 72", "72" },
    { "SAP 72 Mono", "72 Mono" },
};

/** Installed name for a candidate family.

    Reuses the Phase 2 TypographyCatalog alias database: the canonical
    family when installed, otherwise the first installed alias (e.g.
    "Source Serif Pro" for "Source Serif", "Soehne" for "Söhne"). Empty
    when nothing is installed. */
OUString lcl_InstalledName(const FontList* pFontList, const OUString& rFamily)
{
    if (rFamily.isEmpty())
        return OUString();
    if (!pFontList)
        return rFamily;
    if (pFontList->IsAvailable(rFamily))
        return rFamily;

    // Phase 6 aliases (same typeface under its real installed name).
    for (const auto& rAlias : aTypeSystemAliases)
    {
        if (rFamily == OUString::fromUtf8(rAlias.first)
            && pFontList->IsAvailable(OUString::fromUtf8(rAlias.second)))
            return OUString::fromUtf8(rAlias.second);
    }

    const TypographyCatalog& rCatalog = TypographyCatalog::Get();
    const CuratedFont* pCurated = rCatalog.FindFamily(rFamily);
    if (pCurated)
        return rCatalog.GetInstalledName(pFontList, *pCurated);
    return OUString();
}

} // namespace

int ResolvedTypeSystem::GetMissingCount() const
{
    // A role is "missing" when its preferred family is not installed: it
    // either fell back to another family (mbFallbackUsed) or nothing at all
    // could be resolved (mbUnresolved, the style keeps its family).
    int nMissing = 0;
    for (const auto& rRole : maRoles)
        if (!rRole.maFamily.isEmpty() || rRole.mbUnresolved)
        {
            if (rRole.mbFallbackUsed || rRole.mbUnresolved)
                ++nMissing;
        }
    return nMissing;
}

Writer2027TypeSystemCatalog::Writer2027TypeSystemCatalog()
{
    for (const auto& rSeed : aPresetSeeds)
    {
        TypeSystemPreset aPreset;
        aPreset.maId = OUString::fromUtf8(rSeed.pId);
        aPreset.maNameResId = rSeed.aNameResId;
        aPreset.maHeading = MakeFontSpec(rSeed.pHeading, rSeed.pHeadingFb1, rSeed.pHeadingFb2);
        aPreset.maBody = MakeFontSpec(rSeed.pBody, rSeed.pBodyFb1, rSeed.pBodyFb2);
        aPreset.maMono = MakeFontSpec(rSeed.pMono, rSeed.pMonoFb1, rSeed.pMonoFb2);
        aPreset.maDisplay = MakeFontSpec(rSeed.pDisplay, rSeed.pDisplayFb1, rSeed.pDisplayFb2);
        aPreset.meScale = rSeed.eScale;
        aPreset.mnHeadingWeight = rSeed.nHeadingWeight;
        aPreset.mnTitleWeight = rSeed.nTitleWeight;
        if (rSeed.pColors)
            aPreset.maColors = *rSeed.pColors;
        maPresets.push_back(std::move(aPreset));
    }
}

const Writer2027TypeSystemCatalog& Writer2027TypeSystemCatalog::Get()
{
    static const Writer2027TypeSystemCatalog aCatalog;
    return aCatalog;
}

const TypeSystemPreset* Writer2027TypeSystemCatalog::FindPreset(const OUString& rId) const
{
    for (const auto& rPreset : maPresets)
        if (rPreset.maId == rId)
            return &rPreset;
    return nullptr;
}

const TypeSystemScale& Writer2027TypeSystemCatalog::GetScale(TypeSystemScaleId eScale) const
{
    switch (eScale)
    {
        case TypeSystemScaleId::Balanced:
            return aBalanced;
        case TypeSystemScaleId::Editorial:
            return aEditorial;
        case TypeSystemScaleId::Compact:
            return aCompact;
        case TypeSystemScaleId::Accessible:
            return aAccessible;
    }
    return aBalanced;
}

ResolvedTypeSystem ResolveTypeSystem(const TypeSystemPreset& rPreset, const FontList* pFontList)
{
    ResolvedTypeSystem aResolved;

    const TypeSystemFontSpec* pSpecs[4] = { &rPreset.maHeading, &rPreset.maBody, &rPreset.maMono,
                                            &rPreset.maDisplay };

    for (size_t i = 0; i < 4; ++i)
    {
        TypeSystemResolvedRole& rRole = aResolved.maRoles[i];
        const TypeSystemFontSpec& rSpec = *pSpecs[i];
        if (rSpec.IsEmpty())
        {
            rRole.mbUnresolved = true;
            continue;
        }

        auto pick = [&rRole](const OUString& rFamily)
        {
            rRole.maFamily = rFamily;
            rRole.mbFallbackUsed = false;
            rRole.mbUnresolved = false;
        };

        // Preferred: canonical family, then its Phase 2 aliases.
        OUString aInstalled = lcl_InstalledName(pFontList, rSpec.maPreferred);
        if (!aInstalled.isEmpty())
        {
            pick(aInstalled);
            continue;
        }
        // Fallback 1, then fallback 2, through the same alias resolution.
        if (!rSpec.maFallback1.isEmpty())
        {
            aInstalled = lcl_InstalledName(pFontList, rSpec.maFallback1);
            if (!aInstalled.isEmpty())
            {
                pick(aInstalled);
                rRole.mbFallbackUsed = true;
                continue;
            }
        }
        if (!rSpec.maFallback2.isEmpty())
        {
            aInstalled = lcl_InstalledName(pFontList, rSpec.maFallback2);
            if (!aInstalled.isEmpty())
            {
                pick(aInstalled);
                rRole.mbFallbackUsed = true;
                continue;
            }
        }
        rRole.maFamily.clear();
        rRole.mbFallbackUsed = false;
        rRole.mbUnresolved = true;
    }

    return aResolved;
}

} // namespace svx::writer2027

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
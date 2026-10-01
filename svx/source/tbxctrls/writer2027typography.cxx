/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <svx/writer2027typography.hxx>

#include <svtools/ctrltool.hxx>
#include <vcl/metric.hxx>

#include <algorithm>
#include <utility>

namespace svx::writer2027
{

namespace
{

struct CuratedSeed
{
    const char* pFamilyName;
    const char* pDisplayName;
    FontCategory eCategory;
    sal_uInt16 nTags;
    const char* pDescription;
    const char* pAccessibilityNote;
    int nPriority;
};

// Initial curated demo set. Only families that are actually installed are
// ever shown: the catalog never synthesizes or substitutes fonts.
const CuratedSeed aSeeds[] = {
    // Modern / Product
    { "SAP 72", nullptr, FontCategory::Modern, Tag_UI | Tag_Product | Tag_Sans | Tag_Humanist,
      "Modern · Product · UI", nullptr, 10 },
    { "Inter", nullptr, FontCategory::Modern, Tag_Neutral | Tag_Sans, "Modern · Neutral", nullptr,
      20 },
    { "Neue Montreal", nullptr, FontCategory::Modern, Tag_Neutral | Tag_Sans, "Modern · Neutral",
      nullptr, 30 },
    { "Söhne", nullptr, FontCategory::Modern, Tag_Neutral | Tag_Sans | Tag_Humanist,
      "Modern · Neutral", nullptr, 40 },
    { "Roobert", nullptr, FontCategory::Modern, Tag_UI | Tag_Product | Tag_Sans,
      "Modern · Product · UI", nullptr, 50 },
    { "Work Sans", nullptr, FontCategory::Modern, Tag_Sans | Tag_Humanist, "Modern · Humanist",
      nullptr, 60 },
    { "IBM Plex Sans", nullptr, FontCategory::Modern, Tag_Sans | Tag_Humanist, "Modern · Humanist",
      nullptr, 70 },
    // Editorial
    { "Canela", nullptr, FontCategory::Editorial, Tag_Serif | Tag_Editorial, "Editorial · Serif",
      nullptr, 100 },
    { "Tiempos", nullptr, FontCategory::Editorial, Tag_Serif | Tag_Editorial, "Editorial · Serif",
      nullptr, 110 },
    { "Instrument Serif", nullptr, FontCategory::Editorial, Tag_Serif | Tag_Editorial,
      "Editorial · Serif", nullptr, 120 },
    { "Source Serif", nullptr, FontCategory::Editorial, Tag_Serif | Tag_Editorial,
      "Editorial · Serif", nullptr, 130 },
    { "Noto Serif", nullptr, FontCategory::Editorial, Tag_Serif | Tag_Editorial,
      "Editorial · Serif", nullptr, 140 },
    // Display / Brand
    { "Druk", nullptr, FontCategory::Display, Tag_Display | Tag_Sans, "Display · Brand", nullptr,
      200 },
    { "Hatton", nullptr, FontCategory::Display, Tag_Display, "Display · Brand", nullptr, 210 },
    { "Neue Machina", nullptr, FontCategory::Display, Tag_Display, "Display · Brand", nullptr, 220 },
    { "Obviously", nullptr, FontCategory::Display, Tag_Display | Tag_Sans, "Display · Brand",
      nullptr, 230 },
    // Technical / Mono
    { "SAP 72 Mono", nullptr, FontCategory::Technical, Tag_Mono | Tag_Technical | Tag_Sans,
      "Technical · Mono", nullptr, 300 },
    { "JetBrains Mono", nullptr, FontCategory::Technical, Tag_Mono | Tag_Technical,
      "Technical · Mono", nullptr, 310 },
    { "IBM Plex Mono", nullptr, FontCategory::Technical, Tag_Mono | Tag_Technical,
      "Technical · Mono", nullptr, 320 },
    { "Source Code Pro", nullptr, FontCategory::Technical, Tag_Mono | Tag_Technical,
      "Technical · Mono", nullptr, 330 },
    // Accessible
    { "OpenDyslexic", nullptr, FontCategory::Accessible, Tag_Accessible | Tag_DyslexiaFriendly,
      "Accessible · Dyslexia-friendly design", "Dyslexia-friendly design", 400 },
    { "Atkinson Hyperlegible", nullptr, FontCategory::Accessible,
      Tag_Accessible | Tag_HighLegibility, "Accessible · High legibility", "High legibility", 410 },
};

// Family-name aliases across platforms: alias -> canonical family.
const std::pair<const char*, const char*> aAliases[] = {
    { "Source Serif 4", "Source Serif" },
    { "Source Serif Pro", "Source Serif" },
    { "Inter Variable", "Inter" },
    { "Soehne", "Söhne" },
    { "JetBrainsMono Nerd Font", "JetBrains Mono" },
};

// Static priority list for the Recommended section (no AI, no telemetry).
const char* const aRecommended[] = {
    "SAP 72", "Inter", "Atkinson Hyperlegible", "OpenDyslexic", "JetBrains Mono",
    "Instrument Serif",
};

bool lcl_ContainsNoCase(const OUString& rHaystack, const OUString& rNeedle)
{
    return rHaystack.toAsciiLowerCase().indexOf(rNeedle.toAsciiLowerCase()) != -1;
}

void lcl_SortByPriority(std::vector<const CuratedFont*>& rFonts)
{
    std::stable_sort(rFonts.begin(), rFonts.end(),
                     [](const CuratedFont* pA, const CuratedFont* pB)
                     { return pA->mnPriority < pB->mnPriority; });
}

} // namespace

TypographyCatalog::TypographyCatalog()
{
    for (const auto& rSeed : aSeeds)
    {
        CuratedFont aFont;
        aFont.maFamilyName = OUString::fromUtf8(rSeed.pFamilyName);
        aFont.maDisplayName
            = rSeed.pDisplayName ? OUString::fromUtf8(rSeed.pDisplayName) : aFont.maFamilyName;
        aFont.meCategory = rSeed.eCategory;
        aFont.mnTags = rSeed.nTags;
        aFont.maDescription = OUString::fromUtf8(rSeed.pDescription);
        aFont.maAccessibilityNote
            = rSeed.pAccessibilityNote ? OUString::fromUtf8(rSeed.pAccessibilityNote) : OUString();
        aFont.mnPriority = rSeed.nPriority;
        maCuratedFonts.push_back(std::move(aFont));
    }
    for (const auto& rAlias : aAliases)
        maAliases.emplace_back(OUString::fromUtf8(rAlias.first), OUString::fromUtf8(rAlias.second));
    for (const char* pName : aRecommended)
        maRecommended.emplace_back(OUString::fromUtf8(pName));
}

const TypographyCatalog& TypographyCatalog::Get()
{
    static const TypographyCatalog aCatalog;
    return aCatalog;
}

const CuratedFont* TypographyCatalog::FindFamily(const OUString& rFamilyName) const
{
    for (const auto& rFont : maCuratedFonts)
        if (rFont.maFamilyName == rFamilyName)
            return &rFont;
    for (const auto& rAlias : maAliases)
    {
        if (rAlias.first != rFamilyName)
            continue;
        for (const auto& rFont : maCuratedFonts)
            if (rFont.maFamilyName == rAlias.second)
                return &rFont;
    }
    return nullptr;
}

OUString TypographyCatalog::GetInstalledName(const FontList* pFontList,
                                             const CuratedFont& rFont) const
{
    if (pFontList && pFontList->IsAvailable(rFont.maFamilyName))
        return rFont.maFamilyName;
    for (const auto& rAlias : maAliases)
    {
        if (rAlias.second != rFont.maFamilyName)
            continue;
        if (pFontList && pFontList->IsAvailable(rAlias.first))
            return rAlias.first;
    }
    return OUString();
}

OUString TypographyCatalog::GetSectionLabel(FontCategory eCategory)
{
    switch (eCategory)
    {
        case FontCategory::Modern:
            return u"Modern"_ustr;
        case FontCategory::Editorial:
            return u"Editorial"_ustr;
        case FontCategory::Display:
            return u"Display"_ustr;
        case FontCategory::Accessible:
            return u"Accessible"_ustr;
        case FontCategory::Technical:
            return u"Technical / Mono"_ustr;
        case FontCategory::Variable:
            return u"Variable Fonts"_ustr;
        case FontCategory::Legacy:
            return u"Legacy Fonts"_ustr;
    }
    return OUString();
}

const std::vector<FontCategory>& TypographyCatalog::GetSectionOrder()
{
    static const std::vector<FontCategory> aOrder = { FontCategory::Modern,
                                                      FontCategory::Editorial,
                                                      FontCategory::Display,
                                                      FontCategory::Accessible,
                                                      FontCategory::Technical,
                                                      FontCategory::Variable };
    return aOrder;
}

bool TypographyCatalog::MatchesSearch(const CuratedFont& rFont, const OUString& rQuery)
{
    if (rQuery.isEmpty())
        return true;
    if (lcl_ContainsNoCase(rFont.maFamilyName, rQuery)
        || lcl_ContainsNoCase(rFont.maDisplayName, rQuery)
        || lcl_ContainsNoCase(rFont.maDescription, rQuery)
        || lcl_ContainsNoCase(rFont.maAccessibilityNote, rQuery)
        || lcl_ContainsNoCase(GetSectionLabel(rFont.meCategory), rQuery))
    {
        return true;
    }

    static const std::pair<sal_uInt16, OUString> aTagWords[] = {
        { Tag_UI, u"ui"_ustr },
        { Tag_Product, u"product"_ustr },
        { Tag_Swiss, u"swiss"_ustr },
        { Tag_Neutral, u"neutral"_ustr },
        { Tag_Humanist, u"humanist"_ustr },
        { Tag_Serif, u"serif"_ustr },
        { Tag_Sans, u"sans"_ustr },
        { Tag_Mono, u"mono"_ustr },
        { Tag_Display, u"display"_ustr },
        { Tag_Variable, u"variable"_ustr },
        { Tag_Accessible, u"accessible"_ustr },
        { Tag_HighLegibility, u"legibility"_ustr },
        { Tag_DyslexiaFriendly, u"dyslexia"_ustr },
        { Tag_Technical, u"technical"_ustr },
        { Tag_Editorial, u"editorial"_ustr },
    };
    for (const auto& rTag : aTagWords)
        if ((rFont.mnTags & rTag.first) && lcl_ContainsNoCase(rTag.second, rQuery))
            return true;
    return false;
}

bool FontPickerModel::IsInstalled(const FontList* pFontList, const OUString& rFamilyName)
{
    return pFontList && pFontList->IsAvailable(rFamilyName);
}

void FontPickerModel::AddHeader(const OUString& rLabel)
{
    Row aRow;
    aRow.meKind = RowKind::Header;
    aRow.maId = u"h:"_ustr + rLabel;
    aRow.maText = rLabel;
    maRows.push_back(aRow);
}

void FontPickerModel::AddFontRow(const OUString& rFamilyName, const OUString& rMeta,
                                 const CuratedFont* pCurated)
{
    Row aRow;
    aRow.meKind = RowKind::Font;
    aRow.maId = MakeFontId(rFamilyName);
    aRow.maText = rFamilyName;
    aRow.maMeta = rMeta;
    aRow.mpCurated = pCurated;
    aRow.meCategory = pCurated ? pCurated->meCategory : FontCategory::Legacy;
    maRows.push_back(aRow);
}

void FontPickerModel::AddCategoryRow(FontCategory eCategory)
{
    Row aRow;
    aRow.meKind = RowKind::Category;
    aRow.maId = u"c:"_ustr + OUString::number(static_cast<sal_Int32>(eCategory));
    aRow.maText = TypographyCatalog::GetSectionLabel(eCategory) + u"  ▸"_ustr;
    aRow.meCategory = eCategory;
    maRows.push_back(aRow);
}

void FontPickerModel::AddBackRow()
{
    Row aRow;
    aRow.meKind = RowKind::Back;
    aRow.maId = u"b"_ustr;
    aRow.maText = u"‹ All fonts"_ustr;
    maRows.push_back(aRow);
}

void FontPickerModel::AddLegacyRow()
{
    Row aRow;
    aRow.meKind = RowKind::Legacy;
    aRow.maId = u"l"_ustr;
    aRow.maText = u"Legacy Fonts ("_ustr + OUString::number(static_cast<sal_Int64>(mnLegacyCount))
                  + u")  ▸"_ustr;
    maRows.push_back(aRow);
}

void FontPickerModel::Rebuild(const FontList* pFontList, const OUString& rCurrentFamily,
                              const OUString& rQuery)
{
    maQuery = rQuery;
    maCurrentFamily = rCurrentFamily;
    maRows.clear();
    if (!pFontList)
        return;
    if (!maQuery.isEmpty())
        RebuildSearch(pFontList);
    else if (mbLegacyView)
        RebuildLegacy(pFontList);
    else if (mbCategoryView)
        RebuildCategory(pFontList);
    else
        RebuildGrouped(pFontList);
}

void FontPickerModel::RebuildGrouped(const FontList* pFontList)
{
    const TypographyCatalog& rCatalog = TypographyCatalog::Get();

    // Root information architecture (scannable in one second):
    //   Current (pinned only when useful)
    //   Recommended (small, directly visible)
    //   Collections (category drill rows; fonts live behind them)
    //   Legacy (collapsed alphabetical inventory)

    // Pin the active family whenever it would otherwise be hidden behind a
    // category drill or inside the legacy bucket - i.e. whenever the root
    // view does not already surface it directly.
    bool bCurrentVisible = false;
    for (const auto& rFamily : rCatalog.GetRecommendedFamilies())
    {
        const CuratedFont* pFont = rCatalog.FindFamily(rFamily);
        if (pFont && rCatalog.GetInstalledName(pFontList, *pFont) == maCurrentFamily)
        {
            bCurrentVisible = true;
            break;
        }
    }
    if (!bCurrentVisible && !maCurrentFamily.isEmpty()
        && IsInstalled(pFontList, maCurrentFamily))
    {
        AddHeader(u"Current"_ustr);
        AddFontRow(maCurrentFamily, OUString(), nullptr);
    }

    // Recommended: static priority list, installed fonts only. A small set
    // shown directly in the root; the rest of the catalog lives in the
    // Collections drill-ins so the root stays shallow.
    std::vector<const CuratedFont*> aRecommended;
    for (const auto& rFamily : rCatalog.GetRecommendedFamilies())
    {
        const CuratedFont* pFont = rCatalog.FindFamily(rFamily);
        if (pFont && !rCatalog.GetInstalledName(pFontList, *pFont).isEmpty())
            aRecommended.push_back(pFont);
    }
    if (!aRecommended.empty())
    {
        AddHeader(u"Recommended"_ustr);
        for (const CuratedFont* pFont : aRecommended)
            AddFontRow(rCatalog.GetInstalledName(pFontList, *pFont), pFont->maDescription, pFont);
    }

    // Collections: one drill row per non-empty curated category, in display
    // order. Empty categories (e.g. Variable Fonts while capability
    // detection stays deferred) are omitted entirely.
    std::vector<FontCategory> aNonEmptyCategories;
    for (FontCategory eCategory : rCatalog.GetSectionOrder())
    {
        for (const auto& rFont : rCatalog.GetCuratedFonts())
        {
            if (rFont.meCategory == eCategory
                && !rCatalog.GetInstalledName(pFontList, rFont).isEmpty())
            {
                aNonEmptyCategories.push_back(eCategory);
                break;
            }
        }
    }
    if (!aNonEmptyCategories.empty())
    {
        AddHeader(u"Collections"_ustr);
        for (FontCategory eCategory : aNonEmptyCategories)
            AddCategoryRow(eCategory);
    }

    // Legacy bucket: every installed family not surfaced above, collapsed.
    mnLegacyCount = 0;
    for (size_t i = 0; i < pFontList->GetFontNameCount(); ++i)
    {
        const OUString aFamily = pFontList->GetFontName(i).GetFamilyName();
        if (!rCatalog.IsCurated(aFamily))
            ++mnLegacyCount;
    }
    AddHeader(u"Legacy"_ustr);
    AddLegacyRow();
}

void FontPickerModel::RebuildCategory(const FontList* pFontList)
{
    const TypographyCatalog& rCatalog = TypographyCatalog::Get();
    AddBackRow();
    AddHeader(rCatalog.GetSectionLabel(meCategory));

    std::vector<const CuratedFont*> aFonts;
    for (const auto& rFont : rCatalog.GetCuratedFonts())
    {
        if (rFont.meCategory == meCategory
            && !rCatalog.GetInstalledName(pFontList, rFont).isEmpty())
            aFonts.push_back(&rFont);
    }
    lcl_SortByPriority(aFonts);
    for (const CuratedFont* pFont : aFonts)
        AddFontRow(rCatalog.GetInstalledName(pFontList, *pFont), pFont->maDescription, pFont);
}

void FontPickerModel::RebuildLegacy(const FontList* pFontList)
{
    const TypographyCatalog& rCatalog = TypographyCatalog::Get();
    AddBackRow();

    std::vector<OUString> aLegacy;
    for (size_t i = 0; i < pFontList->GetFontNameCount(); ++i)
    {
        const OUString aFamily = pFontList->GetFontName(i).GetFamilyName();
        if (!rCatalog.IsCurated(aFamily))
            aLegacy.push_back(aFamily);
    }
    std::sort(aLegacy.begin(), aLegacy.end());
    for (const auto& rFamily : aLegacy)
        AddFontRow(rFamily, OUString(), nullptr);
}

void FontPickerModel::RebuildSearch(const FontList* pFontList)
{
    const TypographyCatalog& rCatalog = TypographyCatalog::Get();
    AddHeader(u"Search results"_ustr);

    std::vector<const CuratedFont*> aCurated;
    for (const auto& rFont : rCatalog.GetCuratedFonts())
    {
        if (!rCatalog.GetInstalledName(pFontList, rFont).isEmpty()
            && rCatalog.MatchesSearch(rFont, maQuery))
            aCurated.push_back(&rFont);
    }
    lcl_SortByPriority(aCurated);
    for (const CuratedFont* pFont : aCurated)
        AddFontRow(rCatalog.GetInstalledName(pFontList, *pFont), pFont->maDescription, pFont);

    std::vector<OUString> aLegacy;
    for (size_t i = 0; i < pFontList->GetFontNameCount(); ++i)
    {
        const OUString aFamily = pFontList->GetFontName(i).GetFamilyName();
        if (!rCatalog.IsCurated(aFamily) && lcl_ContainsNoCase(aFamily, maQuery))
            aLegacy.push_back(aFamily);
    }
    std::sort(aLegacy.begin(), aLegacy.end());
    for (const auto& rFamily : aLegacy)
        AddFontRow(rFamily, OUString(), nullptr);

    if (aCurated.empty() && aLegacy.empty())
    {
        Row aRow;
        aRow.meKind = RowKind::NoResults;
        aRow.maId = u"n"_ustr;
        aRow.maText = u"No fonts match"_ustr;
        maRows.push_back(aRow);
    }
}

void FontPickerModel::EnterCategory(FontCategory eCategory)
{
    meCategory = eCategory;
    mbCategoryView = true;
    mbLegacyView = false;
    maQuery.clear();
}

void FontPickerModel::EnterLegacy()
{
    mbLegacyView = true;
    mbCategoryView = false;
    maQuery.clear();
}

void FontPickerModel::GoBack()
{
    mbCategoryView = false;
    mbLegacyView = false;
    maQuery.clear();
}

void FontPickerModel::Reset()
{
    mbCategoryView = false;
    mbLegacyView = false;
    maQuery.clear();
}

const FontPickerModel::Row* FontPickerModel::FindRow(const OUString& rId) const
{
    if (rId.isEmpty())
        return nullptr;
    for (const auto& rRow : maRows)
        if (rRow.maId == rId)
            return &rRow;
    return nullptr;
}

} // namespace svx::writer2027

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
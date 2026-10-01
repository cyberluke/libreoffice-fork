/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_SVX_WRITER2027TYPOGRAPHY_HXX
#define INCLUDED_SVX_WRITER2027TYPOGRAPHY_HXX

#include <rtl/ustring.hxx>

#include <utility>
#include <vector>

class FontList;

namespace svx::writer2027
{

// Curated typography categories shown as sections in the Writer 2027 font picker.
enum class FontCategory
{
    Modern,
    Editorial,
    Display,
    Accessible,
    Technical,
    Variable,
    Legacy
};

// Searchable metadata tags for curated families.
enum FontTag : sal_uInt16
{
    Tag_UI = 0x0001,
    Tag_Product = 0x0002,
    Tag_Swiss = 0x0004,
    Tag_Neutral = 0x0008,
    Tag_Humanist = 0x0010,
    Tag_Serif = 0x0020,
    Tag_Sans = 0x0040,
    Tag_Mono = 0x0080,
    Tag_Display = 0x0100,
    Tag_Variable = 0x0200,
    Tag_Accessible = 0x0400,
    Tag_HighLegibility = 0x0800,
    Tag_DyslexiaFriendly = 0x1000,
    Tag_Technical = 0x2000,
    Tag_Editorial = 0x4000
};

/** Metadata for one curated font family.

    The catalog is metadata + presentation only: a family is only ever shown
    when the installed-font enumeration (FontList) actually provides it.
    Nothing is synthesized, substituted, downloaded or otherwise invented.
 */
struct CuratedFont
{
    OUString maFamilyName;        // canonical installed family name
    OUString maDisplayName;       // optional nicer label, falls back to maFamilyName
    FontCategory meCategory;
    sal_uInt16 mnTags;
    OUString maDescription;       // e.g. "Modern · Product · UI"
    OUString maAccessibilityNote; // descriptive only, e.g. "Dyslexia-friendly design"
    int mnPriority;               // lower sorts earlier within a section
};

/** Static, data-driven catalog of curated font families. */
class TypographyCatalog
{
public:
    static const TypographyCatalog& Get();

    const std::vector<CuratedFont>& GetCuratedFonts() const { return maCuratedFonts; }

    /** Exact family-name match, then alias match. Null when unknown. */
    const CuratedFont* FindFamily(const OUString& rFamilyName) const;
    bool IsCurated(const OUString& rFamilyName) const
    {
        return FindFamily(rFamilyName) != nullptr;
    }

    /** Installed name for a curated font: the canonical family when it is
        installed, otherwise the first installed alias. Empty when the font
        is not installed at all - the picker then omits the row, so a
        selection always dispatches a real installed family name. */
    OUString GetInstalledName(const FontList* pFontList, const CuratedFont& rFont) const;

    /** Static priority list for the Recommended section (installed only). */
    const std::vector<OUString>& GetRecommendedFamilies() const { return maRecommended; }

    /** Section header label for a category, e.g. "Technical / Mono". */
    static OUString GetSectionLabel(FontCategory eCategory);
    /** Sections in display order; empty sections are omitted by the model. */
    static const std::vector<FontCategory>& GetSectionOrder();

    /** Case-insensitive match of a query against family/display/tags/description. */
    static bool MatchesSearch(const CuratedFont& rFont, const OUString& rQuery);

private:
    TypographyCatalog();

    std::vector<CuratedFont> maCuratedFonts;
    std::vector<std::pair<OUString, OUString>> maAliases; // alias -> canonical family
    std::vector<OUString> maRecommended;
};

/** View model composing the grouped/curated picker from the FontList.

    Rows are lightweight string records; font objects are only created for
    rows that are actually drawn (see the picker's custom row renderer).
 */
class FontPickerModel
{
public:
    enum class RowKind
    {
        Header,   // section label, inert
        Font,     // selectable font family (curated or legacy)
        Category, // drill into a curated category
        Back,     // return from a drilled view
        Legacy,   // drill into the alphabetical legacy list
        NoResults // search found nothing, inert
    };

    struct Row
    {
        RowKind meKind = RowKind::Header;
        OUString maId;   // stable id used for lookup + custom rendering
        OUString maText; // entry string: family name for fonts, label otherwise
        OUString maMeta; // secondary metadata line for font rows (may be empty)
        FontCategory meCategory = FontCategory::Legacy;
        const CuratedFont* mpCurated = nullptr;
    };

    FontPickerModel() = default;

    /** Rebuild the row list from the installed-font source of truth. */
    void Rebuild(const FontList* pFontList, const OUString& rCurrentFamily,
                 const OUString& rQuery);

    void EnterCategory(FontCategory eCategory);
    void EnterLegacy();
    void GoBack();
    void Reset();

    bool IsCategoryView() const { return mbCategoryView; }
    bool IsLegacyView() const { return mbLegacyView; }
    size_t GetLegacyCount() const { return mnLegacyCount; }
    const std::vector<Row>& GetRows() const { return maRows; }
    const Row* FindRow(const OUString& rId) const;

    static OUString MakeFontId(const OUString& rFamilyName)
    {
        return u"f:"_ustr + rFamilyName;
    }

private:
    void RebuildGrouped(const FontList* pFontList);
    void RebuildCategory(const FontList* pFontList);
    void RebuildLegacy(const FontList* pFontList);
    void RebuildSearch(const FontList* pFontList);

    static bool IsInstalled(const FontList* pFontList, const OUString& rFamilyName);
    void AddHeader(const OUString& rLabel);
    void AddFontRow(const OUString& rFamilyName, const OUString& rMeta,
                    const CuratedFont* pCurated);
    void AddCategoryRow(FontCategory eCategory);
    void AddBackRow();
    void AddLegacyRow();

    std::vector<Row> maRows;
    FontCategory meCategory = FontCategory::Modern;
    bool mbCategoryView = false;
    bool mbLegacyView = false;
    OUString maQuery;
    OUString maCurrentFamily;
    size_t mnLegacyCount = 0;
};

} // namespace svx::writer2027

#endif // INCLUDED_SVX_WRITER2027TYPOGRAPHY_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
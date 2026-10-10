/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_SVX_WRITER2027TYPESYSTEM_HXX
#define INCLUDED_SVX_WRITER2027TYPESYSTEM_HXX

#include <rtl/ustring.hxx>
#include <sal/types.h>

#include <svx/svxdllapi.h>

#include <tools/long.hxx>
#include <unotools/resmgr.hxx>

#include <utility>
#include <vector>

class FontList;

namespace svx::writer2027
{

// Curated typographic scales. Sizes are resolved to absolute twips
// (1/20 pt) at catalog build time: no floating design ratios leak into the
// style engine. The ratios follow the Phase 6 spec (Balanced 1/1.25/1.56/
// 1.95/2.44, Editorial 1/1.30/1.70/2.20/3.00, Compact 1/1.18/1.38/1.62/
// 2.00, Accessible extra-generous) and are then rounded to clean point
// values.
enum class TypeSystemScaleId
{
    Balanced,
    Editorial,
    Compact,
    Accessible
};

enum class TypeSystemFontRole
{
    Heading, // headings 1-3 + the "Heading" base style
    Body,    // Default Paragraph Style (everything inherits from it)
    Mono,    // Preformatted Text + Source Text character style
    Display  // Title (and Title-adjacent display roles)
};

/** One font role of a preset: preferred family plus a deterministic
    fallback chain. Families are matched against the installed-font
    enumeration (FontList) at resolve time; nothing is downloaded or
    synthesized. */
struct TypeSystemFontSpec
{
    OUString maPreferred;
    OUString maFallback1;
    OUString maFallback2;

    bool IsEmpty() const { return maPreferred.isEmpty(); }
};

/** Resolution result for one font role. */
struct TypeSystemResolvedRole
{
    OUString maFamily;       // installed family chosen; empty when nothing resolved
    bool mbFallbackUsed = false;  // a fallback replaced the preferred family
    bool mbUnresolved = false;    // no candidate installed: style stays untouched
};

/** Font resolution result for a whole preset. Deterministic given the
    installed font enumeration. */
struct ResolvedTypeSystem
{
    TypeSystemResolvedRole maRoles[4]; // indexed by TypeSystemFontRole

    const TypeSystemResolvedRole& Get(TypeSystemFontRole eRole) const
    {
        return maRoles[static_cast<size_t>(eRole)];
    }

    TypeSystemResolvedRole& Get(TypeSystemFontRole eRole)
    {
        return maRoles[static_cast<size_t>(eRole)];
    }

    /** Number of roles whose preferred family is not installed. */
    SVXCORE_DLLPUBLIC int GetMissingCount() const;
};

/** Restrained semantic color roles (hex "#RRGGBB", empty = not defined).

    These are *document* color roles on top of the Writer 2027 palette
    (sw/inc/writer2027.hxx, Phase 1). The application service only applies
    them to digital-dark Writer 2027 documents and only ever reuses the
    canonical Phase 1 palette values; light/imported documents keep their
    existing style colors untouched.
 */
struct TypeSystemColorRoles
{
    OUString maHeadingAccent;    // heading emphasis (kept empty in all presets)
    OUString maQuoteAccent;      // Block Quotation foreground
    OUString maCaptionSecondary; // Caption foreground
    OUString maLinkRole;         // link role (kept empty: canonical Phase 1 link colors)
};

/** Resolved typographic scale, all sizes in twips (1/20 pt). */
struct TypeSystemScale
{
    sal_uInt16 mnBody;      // body text size
    sal_uInt16 mnH1;        // Heading 1
    sal_uInt16 mnH2;        // Heading 2
    sal_uInt16 mnH3;        // Heading 3
    sal_uInt16 mnH4;        // Heading 4
    sal_uInt16 mnH5;        // Heading 5
    sal_uInt16 mnH6;        // Heading 6
    sal_uInt16 mnTitle;     // Title
    sal_uInt16 mnSubtitle;  // Subtitle
    sal_uInt16 mnCaption;   // Caption
    sal_uInt16 mnQuote;     // Block Quotation
    sal_uInt16 mnMono;      // Preformatted Text / Source Text
    sal_uInt16 mnLineSpacingPercent; // proportional line spacing, 100 = single
    tools::Long mnSpaceAfter;        // body space-after (twips)
    tools::Long mnH1Before, mnH1After;
    tools::Long mnH2Before, mnH2After;
    tools::Long mnH3Before, mnH3After;
    tools::Long mnH4Before, mnH4After;
    tools::Long mnH5Before, mnH5After;
    tools::Long mnH6Before, mnH6After;
    tools::Long mnTitleBefore, mnTitleAfter;
    tools::Long mnSubtitleAfter;
    tools::Long mnQuoteBefore, mnQuoteAfter;
    tools::Long mnCaptionBefore;
};

/** One curated Type System preset. Pure data: independent from the UI and
    from installed-font enumeration. */
struct TypeSystemPreset
{
    OUString maId;              // stable id, e.g. "editorial"
    TranslateId maNameResId;    // localized name (svx/inc/strings.hrc, NC_)
    TypeSystemFontSpec maHeading;
    TypeSystemFontSpec maBody;
    TypeSystemFontSpec maMono;
    TypeSystemFontSpec maDisplay;
    TypeSystemScaleId meScale = TypeSystemScaleId::Balanced;
    sal_uInt16 mnHeadingWeight; // FontWeight value for headings (500/600/700)
    sal_uInt16 mnTitleWeight;   // FontWeight value for Title
    TypeSystemColorRoles maColors;
    std::vector<OUString> maPersonalityTags; // metadata only (not shown as UI text)
};

/** Static, data-driven catalog of curated Type Systems. */
class SVXCORE_DLLPUBLIC Writer2027TypeSystemCatalog
{
public:
    static const Writer2027TypeSystemCatalog& Get();

    const std::vector<TypeSystemPreset>& GetPresets() const { return maPresets; }

    /** Exact id match. Null when unknown. */
    const TypeSystemPreset* FindPreset(const OUString& rId) const;

    const TypeSystemScale& GetScale(TypeSystemScaleId eScale) const;

private:
    Writer2027TypeSystemCatalog();

    std::vector<TypeSystemPreset> maPresets;
};

/** Resolve a preset against the installed-font enumeration.

    Per role: preferred family when installed; otherwise the first installed
    fallback (flagged as such); otherwise the role is unresolved and the
    style keeps its existing family. Never blocks, never downloads.
 */
SVXCORE_DLLPUBLIC ResolvedTypeSystem ResolveTypeSystem(const TypeSystemPreset& rPreset,
                                     const FontList* pFontList);

} // namespace svx::writer2027

#endif // INCLUDED_SVX_WRITER2027TYPESYSTEM_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
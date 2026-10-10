/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <writer2027typesystem.hxx>
#include <writer2027typographymanager.hxx>

#include <svx/writer2027typesystem.hxx>
#include <svx/writer2027log.hxx>

#include <svtools/ctrltool.hxx>
#include <o3tl/safeint.hxx>
#include <rtl/textenc.h>
#include <tools/color.hxx>
#include <tools/fontenum.hxx>
#include <vcl/metric.hxx>

#include <IDocumentState.hxx>
#include <IDocumentStylePoolAccess.hxx>
#include <IDocumentUndoRedo.hxx>
#include <SwRewriter.hxx>
#include <charfmt.hxx>
#include <doc.hxx>
#include <editeng/colritem.hxx>
#include <editeng/fhgtitem.hxx>
#include <editeng/fontitem.hxx>
#include <editeng/lspcitem.hxx>
#include <editeng/svxenum.hxx>
#include <editeng/ulspitem.hxx>
#include <editeng/wghtitem.hxx>
#include <format.hxx>
#include <hintids.hxx>
#include <poolfmt.hxx>
#include <svl/itemset.hxx>
#include <svl/whichranges.hxx>
#include <swundo.hxx>
#include <fmtcol.hxx>
#include <writer2027.hxx>
#include <writer2027view.hxx>

#include <optional>

namespace sw::writer2027typesystem
{

namespace
{

constexpr sal_uInt16 RES_FONT_WHICHS[] = { RES_CHRATR_FONT, RES_CHRATR_CJK_FONT,
                                           RES_CHRATR_CTL_FONT };
constexpr sal_uInt16 RES_HEIGHT_WHICHS[] = { RES_CHRATR_FONTSIZE, RES_CHRATR_CJK_FONTSIZE,
                                             RES_CHRATR_CTL_FONTSIZE };
constexpr sal_uInt16 RES_WEIGHT_WHICHS[] = { RES_CHRATR_WEIGHT, RES_CHRATR_CJK_WEIGHT,
                                             RES_CHRATR_CTL_WEIGHT };

SfxItemSet lcl_MakeItemSet(SwDoc& rDoc)
{
    return SfxItemSet(rDoc.GetAttrPool(),
                      svl::Items<RES_CHRATR_BEGIN, RES_CHRATR_END - 1, RES_PARATR_BEGIN,
                                 RES_PARATR_END - 1, RES_FRMATR_BEGIN, RES_FRMATR_END - 1>);
}

void lcl_PutFamily(SfxItemSet& rSet, const OUString& rFamily, const FontList* pFontList)
{
    if (rFamily.isEmpty())
        return;

    FontFamily eFam = FAMILY_DONTKNOW;
    OUString aFamilyName = rFamily;
    OUString aStyleName;
    FontPitch ePitch = PITCH_DONTKNOW;
    rtl_TextEncoding eCharset = RTL_TEXTENCODING_DONTKNOW;
    if (pFontList && pFontList->IsAvailable(rFamily))
    {
        FontMetric aMetric = pFontList->Get(rFamily, WEIGHT_NORMAL, ITALIC_NONE);
        eFam = aMetric.GetFamilyType();
        aFamilyName = aMetric.GetFamilyName();
        aStyleName = aMetric.GetStyleName();
        ePitch = aMetric.GetPitch();
        eCharset = aMetric.GetCharSet();
    }
    for (const sal_uInt16 nWhich : RES_FONT_WHICHS)
        rSet.Put(SvxFontItem(eFam, aFamilyName, aStyleName, ePitch, eCharset, nWhich));
}

void lcl_PutSize(SfxItemSet& rSet, sal_uInt16 nTwips)
{
    if (nTwips == 0)
        return;
    for (const sal_uInt16 nWhich : RES_HEIGHT_WHICHS)
        rSet.Put(SvxFontHeightItem(nTwips, 100, nWhich));
}

void lcl_PutWeight(SfxItemSet& rSet, sal_uInt16 nWeight)
{
    if (nWeight == 0)
        return;
    for (const sal_uInt16 nWhich : RES_WEIGHT_WHICHS)
        rSet.Put(SvxWeightItem(static_cast<FontWeight>(nWeight), nWhich));
}

void lcl_PutLineSpacing(SfxItemSet& rSet, sal_uInt16 nPercent)
{
    if (nPercent == 0)
        return;
    SvxLineSpacingItem aLine(LINE_SPACE_DEFAULT_HEIGHT, RES_PARATR_LINESPACING);
    if (nPercent == 100)
        aLine.SetInterLineSpaceRule(SvxInterLineSpaceRule::Off);
    else
        aLine.SetPropLineSpace(nPercent);
    rSet.Put(aLine);
}

void lcl_PutUpperLower(SfxItemSet& rSet, tools::Long nUpper, tools::Long nLower)
{
    if (nUpper <= 0 && nLower <= 0)
        return;
    SvxULSpaceItem aUL(RES_UL_SPACE);
    if (nUpper > 0)
        aUL.SetUpper(o3tl::narrowing<sal_uInt16>(nUpper));
    if (nLower > 0)
        aUL.SetLower(o3tl::narrowing<sal_uInt16>(nLower));
    rSet.Put(aUL);
}

/** Parse "#RRGGBB" into a Color; empty string yields no value. */
std::optional<Color> lcl_ParseHexColor(const OUString& rHex)
{
    if (rHex.isEmpty())
        return std::nullopt;
    if (rHex.getLength() != 7 || rHex[0] != '#')
        return std::nullopt;
    const sal_uInt32 nRed = rHex.copy(1, 2).toUInt32(16);
    const sal_uInt32 nGreen = rHex.copy(3, 2).toUInt32(16);
    const sal_uInt32 nBlue = rHex.copy(5, 2).toUInt32(16);
    return Color(o3tl::narrowing<sal_uInt8>(nRed), o3tl::narrowing<sal_uInt8>(nGreen),
                 o3tl::narrowing<sal_uInt8>(nBlue));
}

void lcl_PutColor(SfxItemSet& rSet, const OUString& rHex)
{
    const std::optional<Color> oColor = lcl_ParseHexColor(rHex);
    if (oColor)
        rSet.Put(SvxColorItem(*oColor, RES_CHRATR_COLOR));
}

void lcl_ApplyFormat(SwDoc& rDoc, SwFormat& rFormat, const SfxItemSet& rSet)
{
    if (rSet.Count() == 0)
        return;
    // SwDoc::ChgFormat re-derives the "actually different" subset internally
    // for the undo record and always applies the target via SetFormatAttr, so
    // the values persist even when the pooled format reported the same item as
    // the preset (the old Differentiate() early-out here dropped legitimate
    // changes such as heading-scale sizes, which silently kept the pool
    // default and broke DetectCurrentTypeSystem round-trip). Idempotence is
    // preserved because ChgFormat with an identical set is a no-op for the
    // document state.
    rDoc.ChgFormat(rFormat, rSet);
}

SwTextFormatColl* lcl_GetTextColl(SwDoc& rDoc, SwPoolFormatId nId)
{
    return rDoc.getIDocumentStylePoolAccess().GetTextCollFromPool(nId);
}

SwCharFormat* lcl_GetCharFormat(SwDoc& rDoc, SwPoolFormatId nId)
{
    return rDoc.getIDocumentStylePoolAccess().GetCharFormatFromPool(nId);
}

} // namespace

namespace
{

namespace TSMan = ::sw::writer2027typographymanager;
using TSSlot = TSMan::TypeSystemScaleSlot;

/** Resolve the scale slot of a semantic descriptor to a concrete size in
    twips from the preset scale. Returns 0 for roles that do not carry a scale
    size (e.g. Inline Code handled by the Mono slot, or slots not in the table). */
sal_uInt16 lcl_SizeForSlot(const svx::writer2027::TypeSystemScale& rScale, TSSlot eSlot)
{
    switch (eSlot)
    {
        case TSSlot::Body:
            return rScale.mnBody;
        case TSSlot::Heading1:
            return rScale.mnH1;
        case TSSlot::Heading2:
            return rScale.mnH2;
        case TSSlot::Heading3:
            return rScale.mnH3;
        case TSSlot::Heading4:
            return rScale.mnH4;
        case TSSlot::Heading5:
            return rScale.mnH5;
        case TSSlot::Heading6:
            return rScale.mnH6;
        case TSSlot::Title:
            return rScale.mnTitle;
        case TSSlot::Subtitle:
            return rScale.mnSubtitle;
        case TSSlot::Quote:
            return rScale.mnQuote;
        case TSSlot::Caption:
            return rScale.mnCaption;
        case TSSlot::Mono:
            return rScale.mnMono;
        case TSSlot::None:
        default:
            return 0;
    }
}

/** Upper/lower spacing (from/before + after) for a scale slot. Empty/zero in
    positions the role keeps its existing spacing from inheritance. */
void lcl_SpacingForSlot(const svx::writer2027::TypeSystemScale& rScale, TSSlot eSlot,
                        tools::Long& rFrom, tools::Long& rAfter)
{
    const auto none = [&rScale]() { return tools::Long(0); };
    switch (eSlot)
    {
        case TSSlot::Heading1:
            rFrom = rScale.mnH1Before;
            rAfter = rScale.mnH1After;
            break;
        case TSSlot::Heading2:
            rFrom = rScale.mnH2Before;
            rAfter = rScale.mnH2After;
            break;
        case TSSlot::Heading3:
            rFrom = rScale.mnH3Before;
            rAfter = rScale.mnH3After;
            break;
        case TSSlot::Heading4:
            rFrom = rScale.mnH4Before;
            rAfter = rScale.mnH4After;
            break;
        case TSSlot::Heading5:
            rFrom = rScale.mnH5Before;
            rAfter = rScale.mnH5After;
            break;
        case TSSlot::Heading6:
            rFrom = rScale.mnH6Before;
            rAfter = rScale.mnH6After;
            break;
        case TSSlot::Title:
            rFrom = rScale.mnTitleBefore;
            rAfter = rScale.mnTitleAfter;
            break;
        case TSSlot::Subtitle:
            rFrom = none();
            rAfter = rScale.mnSubtitleAfter;
            break;
        case TSSlot::Quote:
            rFrom = rScale.mnQuoteBefore;
            rAfter = rScale.mnQuoteAfter;
            break;
        case TSSlot::Caption:
            rFrom = rScale.mnCaptionBefore;
            rAfter = none();
            break;
        default:
            rFrom = none();
            rAfter = none();
            break;
    }
    (void)none;
}

} // namespace

bool ApplyTypeSystem(SwDoc& rDoc, const svx::writer2027::TypeSystemPreset& rPreset,
                     const svx::writer2027::ResolvedTypeSystem& rResolved,
                     const FontList* pFontList)
{
    const svx::writer2027::TypeSystemScale& rScale
        = svx::writer2027::Writer2027TypeSystemCatalog::Get().GetScale(rPreset.meScale);

    // Semantic color roles are only meaningful on the digital-dark Writer
    // 2027 page (Phase 1 palette). Light/imported documents keep their
    // existing style colors untouched.
    const bool bDarkDocument = sw::writer2027view::IsWriter2027DarkDocument(rDoc);

    rDoc.GetIDocumentUndoRedo().StartUndo(SwUndoId::WRITER2027_TYPE_SYSTEM, nullptr);

    // Canonical application order (spec 44):
    //   1. resolve fonts
    //   2. one undo group (already started)
    //   3. materialize semantic styles
    //   4..12 update each semantic style via the canonical map
    // The map is the single source of truth for pool id + font role + scale
    // slot, so apply and detect share one table (spec 19/44/46).

    // Font families per role (from the SAME resolved model as the preview):
    const OUString aHeadingFam
        = rResolved.Get(svx::writer2027::TypeSystemFontRole::Heading).maFamily;
    const OUString aBodyFam = rResolved.Get(svx::writer2027::TypeSystemFontRole::Body).maFamily;
    const OUString aMonoFam = rResolved.Get(svx::writer2027::TypeSystemFontRole::Mono).maFamily;
    const OUString aDisplayFam
        = rResolved.Get(svx::writer2027::TypeSystemFontRole::Display).maFamily;

    for (const auto& rDesc : TSMan::GetWriter2027SemanticStyleDescriptors())
    {
        const auto ePoolId = static_cast<SwPoolFormatId>(rDesc.mnPoolId);
        SfxItemSet aSet = lcl_MakeItemSet(rDoc);

        // Font family + size + weight by the descriptor's roles. Base spacing
        // (body rhythm) is applied on Default Paragraph Style; per-level
        // space-before/after on headings etc.
        const OUString& rFam
            = (rDesc.meFontRole == svx::writer2027::TypeSystemFontRole::Heading)
                  ? aHeadingFam
                  : (rDesc.meFontRole == svx::writer2027::TypeSystemFontRole::Mono)
                        ? aMonoFam
                        : (rDesc.meFontRole == svx::writer2027::TypeSystemFontRole::Display)
                              ? aDisplayFam
                              : aBodyFam;
        lcl_PutFamily(aSet, rFam, pFontList);

        const sal_uInt16 nSize = lcl_SizeForSlot(rScale, rDesc.meScaleSlot);
        lcl_PutSize(aSet, nSize);

        if (rDesc.meScaleSlot == TSSlot::Body || rDesc.meScaleSlot == TSSlot::Quote)
            lcl_PutLineSpacing(aSet, rScale.mnLineSpacingPercent);
        if (rDesc.meScaleSlot == TSSlot::Body && rDesc.meRole
                == TSMan::Writer2027SemanticStyle::DefaultBody)
            lcl_PutUpperLower(aSet, 0, rScale.mnSpaceAfter);

        tools::Long nFrom = 0, nAfter = 0;
        lcl_SpacingForSlot(rScale, rDesc.meScaleSlot, nFrom, nAfter);
        lcl_PutUpperLower(aSet, nFrom, nAfter);

        // Heading weight on the heading base + per-level inherited; Title/
        // Subtitle use the display weight.
        if (rDesc.meFontRole == svx::writer2027::TypeSystemFontRole::Heading)
            lcl_PutWeight(aSet, rPreset.mnHeadingWeight);
        else if (rDesc.meRole == TSMan::Writer2027SemanticStyle::Title
                 || rDesc.meRole == TSMan::Writer2027SemanticStyle::Subtitle)
            lcl_PutWeight(aSet, rPreset.mnTitleWeight);

        // Semantic color roles (digital-dark only).
        if (bDarkDocument)
        {
            switch (rDesc.meRole)
            {
                case TSMan::Writer2027SemanticStyle::Subtitle:
                case TSMan::Writer2027SemanticStyle::Quote:
                    lcl_PutColor(aSet, rPreset.maColors.maQuoteAccent);
                    break;
                case TSMan::Writer2027SemanticStyle::Caption:
                    lcl_PutColor(aSet, rPreset.maColors.maCaptionSecondary);
                    break;
                default:
                    break;
            }
        }

        SwFormat* pFormat = rDesc.mbCharacterStyle
                                ? static_cast<SwFormat*>(
                                      lcl_GetCharFormat(rDoc, ePoolId))
                                : static_cast<SwFormat*>(lcl_GetTextColl(rDoc, ePoolId));
        if (pFormat)
            lcl_ApplyFormat(rDoc, *pFormat, aSet);
        else
        {
            svx::writer2027::Writer2027LogMessage(
                "writer2027typesystem.apply",
                OUString::Concat(u"missing pool format id=")
                    + OUString::number(rDesc.mnPoolId));
        }
    }

    rDoc.GetIDocumentUndoRedo().EndUndo(SwUndoId::WRITER2027_TYPE_SYSTEM, nullptr);
    rDoc.getIDocumentState().SetModified();
    return true;
}

namespace
{

bool lcl_FormatHasFamily(const SwFormat& rFormat, const OUString& rFamily)
{
    if (rFamily.isEmpty())
        return true; // unresolved role: nothing to compare
    return rFormat.GetAttrSet().Get(RES_CHRATR_FONT).GetFamilyName() == rFamily;
}

bool lcl_FormatHasSize(const SwFormat& rFormat, sal_uInt16 nTwips)
{
    return nTwips == 0
           || rFormat.GetAttrSet().Get(RES_CHRATR_FONTSIZE).GetHeight()
                  == static_cast<sal_uInt32>(nTwips);
}

bool lcl_FormatHasLineSpacing(const SwFormat& rFormat, sal_uInt16 nPercent)
{
    const SvxLineSpacingItem& rLine = rFormat.GetAttrSet().Get(RES_PARATR_LINESPACING);
    if (nPercent == 100)
        return rLine.GetInterLineSpaceRule() == SvxInterLineSpaceRule::Off;
    return rLine.GetInterLineSpaceRule() == SvxInterLineSpaceRule::Prop
           && rLine.GetPropLineSpace() == nPercent;
}

bool lcl_FormatHasSpaceAfter(const SwFormat& rFormat, tools::Long nTwips)
{
    return nTwips <= 0
           || rFormat.GetAttrSet().Get(RES_UL_SPACE).GetLower() == nTwips;
}

/** The family applied (by the semantic map) to a given font role of a preset. */
OUString lcl_AppliedFamily(const svx::writer2027::ResolvedTypeSystem& rResolved,
                           svx::writer2027::TypeSystemFontRole eRole)
{
    switch (eRole)
    {
        case svx::writer2027::TypeSystemFontRole::Heading:
            return rResolved.Get(svx::writer2027::TypeSystemFontRole::Heading).maFamily;
        case svx::writer2027::TypeSystemFontRole::Mono:
            return rResolved.Get(svx::writer2027::TypeSystemFontRole::Mono).maFamily;
        case svx::writer2027::TypeSystemFontRole::Display:
            return rResolved.Get(svx::writer2027::TypeSystemFontRole::Display).maFamily;
        case svx::writer2027::TypeSystemFontRole::Body:
        default:
            return rResolved.Get(svx::writer2027::TypeSystemFontRole::Body).maFamily;
    }
}

/** Full-contract detection: every managed semantic style must match the
    resolved preset's family (by font role) and scale size (by slot). This
    shares the SAME canonical map as ApplyTypeSystem (spec 19/46). */
bool lcl_PresetMatches(SwDoc& rDoc, const svx::writer2027::TypeSystemPreset& rPreset,
                       const svx::writer2027::ResolvedTypeSystem& rResolved)
{
    const svx::writer2027::TypeSystemScale& rScale
        = svx::writer2027::Writer2027TypeSystemCatalog::Get().GetScale(rPreset.meScale);

    for (const auto& rDesc : TSMan::GetWriter2027SemanticStyleDescriptors())
    {
        const auto ePoolId = static_cast<SwPoolFormatId>(rDesc.mnPoolId);
        SwFormat* pFormat = rDesc.mbCharacterStyle
                                ? static_cast<SwFormat*>(lcl_GetCharFormat(rDoc, ePoolId))
                                : static_cast<SwFormat*>(lcl_GetTextColl(rDoc, ePoolId));
        // A missing pool style must never dereference: treat as "does not match".
        if (!pFormat)
            return false;

        if (!lcl_FormatHasFamily(*pFormat, lcl_AppliedFamily(rResolved, rDesc.meFontRole)))
            return false;

        const sal_uInt16 nSize = lcl_SizeForSlot(rScale, rDesc.meScaleSlot);
        if (!lcl_FormatHasSize(*pFormat, nSize))
            return false;

        if (rDesc.meScaleSlot == TSSlot::Body && rDesc.meRole
                == TSMan::Writer2027SemanticStyle::DefaultBody)
        {
            if (!lcl_FormatHasLineSpacing(*pFormat, rScale.mnLineSpacingPercent))
                return false;
            if (!lcl_FormatHasSpaceAfter(*pFormat, rScale.mnSpaceAfter))
                return false;
        }
    }
    return true;
}

} // namespace

OUString DetectCurrentTypeSystem(SwDoc& rDoc, const FontList* pFontList)
{
    const svx::writer2027::Writer2027TypeSystemCatalog& rCatalog
        = svx::writer2027::Writer2027TypeSystemCatalog::Get();
    for (const auto& rPreset : rCatalog.GetPresets())
    {
        const svx::writer2027::ResolvedTypeSystem aResolved
            = svx::writer2027::ResolveTypeSystem(rPreset, pFontList);
        // A preset with unresolved roles can never match the document (the
        // styles carry real family names).
        bool bAllResolved = true;
        for (size_t i = 0; i < 4; ++i)
            bAllResolved = bAllResolved && !aResolved.maRoles[i].mbUnresolved;
        if (!bAllResolved)
            continue;
        if (lcl_PresetMatches(rDoc, rPreset, aResolved))
            return rPreset.maId;
    }
    return OUString();
}

} // namespace sw::writer2027typesystem

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
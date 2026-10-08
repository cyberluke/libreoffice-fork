/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <writer2027typesystem.hxx>

#include <svx/writer2027typesystem.hxx>

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

    // 1. Default Paragraph Style: body typography + rhythm. Every built-in
    //    paragraph style derives from it, so this single root change covers
    //    the whole document without flattening inheritance.
    {
        SfxItemSet aSet = lcl_MakeItemSet(rDoc);
        lcl_PutFamily(aSet, rResolved.Get(svx::writer2027::TypeSystemFontRole::Body).maFamily,
                      pFontList);
        lcl_PutSize(aSet, rScale.mnBody);
        lcl_PutLineSpacing(aSet, rScale.mnLineSpacingPercent);
        lcl_PutUpperLower(aSet, 0, rScale.mnSpaceAfter);
        lcl_ApplyFormat(rDoc, *lcl_GetTextColl(rDoc, SwPoolFormatId::COLL_STANDARD), aSet);
    }

    // 2. Heading base: heading family + weight for all heading levels.
    {
        SfxItemSet aSet = lcl_MakeItemSet(rDoc);
        lcl_PutFamily(aSet, rResolved.Get(svx::writer2027::TypeSystemFontRole::Heading).maFamily,
                      pFontList);
        lcl_PutWeight(aSet, rPreset.mnHeadingWeight);
        lcl_ApplyFormat(rDoc, *lcl_GetTextColl(rDoc, SwPoolFormatId::COLL_HEADLINE_BASE), aSet);
    }

    // 3. Heading 1-3: level sizes + heading spacing.
    {
        SfxItemSet aSet = lcl_MakeItemSet(rDoc);
        lcl_PutSize(aSet, rScale.mnH1);
        lcl_PutUpperLower(aSet, rScale.mnH1Before, rScale.mnH1After);
        lcl_ApplyFormat(rDoc, *lcl_GetTextColl(rDoc, SwPoolFormatId::COLL_HEADLINE1), aSet);
    }
    {
        SfxItemSet aSet = lcl_MakeItemSet(rDoc);
        lcl_PutSize(aSet, rScale.mnH2);
        lcl_PutUpperLower(aSet, rScale.mnH2Before, rScale.mnH2After);
        lcl_ApplyFormat(rDoc, *lcl_GetTextColl(rDoc, SwPoolFormatId::COLL_HEADLINE2), aSet);
    }
    {
        SfxItemSet aSet = lcl_MakeItemSet(rDoc);
        lcl_PutSize(aSet, rScale.mnH3);
        lcl_PutUpperLower(aSet, rScale.mnH3Before, rScale.mnH3After);
        lcl_ApplyFormat(rDoc, *lcl_GetTextColl(rDoc, SwPoolFormatId::COLL_HEADLINE3), aSet);
    }

    // 4. Title: display family + title size/weight/spacing.
    {
        SfxItemSet aSet = lcl_MakeItemSet(rDoc);
        lcl_PutFamily(aSet, rResolved.Get(svx::writer2027::TypeSystemFontRole::Display).maFamily,
                      pFontList);
        lcl_PutSize(aSet, rScale.mnTitle);
        lcl_PutWeight(aSet, rPreset.mnTitleWeight);
        lcl_PutUpperLower(aSet, rScale.mnTitleBefore, rScale.mnTitleAfter);
        lcl_ApplyFormat(rDoc, *lcl_GetTextColl(rDoc, SwPoolFormatId::COLL_DOC_TITLE), aSet);
    }

    // 5. Subtitle: smaller display tone; muted foreground on the digital page.
    {
        SfxItemSet aSet = lcl_MakeItemSet(rDoc);
        lcl_PutSize(aSet, rScale.mnSubtitle);
        lcl_PutUpperLower(aSet, 0, rScale.mnSubtitleAfter);
        if (bDarkDocument)
            lcl_PutColor(aSet, rPreset.maColors.maQuoteAccent); // TextSecondary tone
        lcl_ApplyFormat(rDoc, *lcl_GetTextColl(rDoc, SwPoolFormatId::COLL_DOC_SUBTITLE), aSet);
    }

    // 6. Block Quotation: quote size + generous spacing + muted accent.
    {
        SfxItemSet aSet = lcl_MakeItemSet(rDoc);
        lcl_PutSize(aSet, rScale.mnQuote);
        lcl_PutLineSpacing(aSet, rScale.mnLineSpacingPercent);
        lcl_PutUpperLower(aSet, rScale.mnQuoteBefore, rScale.mnQuoteAfter);
        if (bDarkDocument)
            lcl_PutColor(aSet, rPreset.maColors.maQuoteAccent);
        lcl_ApplyFormat(rDoc, *lcl_GetTextColl(rDoc, SwPoolFormatId::COLL_HTML_BLOCKQUOTE),
                        aSet);
    }

    // 7. Caption: caption size + secondary foreground + modest spacing.
    {
        SfxItemSet aSet = lcl_MakeItemSet(rDoc);
        lcl_PutSize(aSet, rScale.mnCaption);
        lcl_PutUpperLower(aSet, rScale.mnCaptionBefore, 0);
        if (bDarkDocument)
            lcl_PutColor(aSet, rPreset.maColors.maCaptionSecondary);
        lcl_ApplyFormat(rDoc, *lcl_GetTextColl(rDoc, SwPoolFormatId::COLL_LABEL), aSet);
    }

    // 8. Preformatted Text: mono family + mono size.
    {
        SfxItemSet aSet = lcl_MakeItemSet(rDoc);
        lcl_PutFamily(aSet, rResolved.Get(svx::writer2027::TypeSystemFontRole::Mono).maFamily,
                      pFontList);
        lcl_PutSize(aSet, rScale.mnMono);
        lcl_PutLineSpacing(aSet, rScale.mnLineSpacingPercent);
        lcl_ApplyFormat(rDoc, *lcl_GetTextColl(rDoc, SwPoolFormatId::COLL_HTML_PRE), aSet);
    }

    // 9. Source Text character style: coherent mono typography for inline code.
    {
        SfxItemSet aSet = lcl_MakeItemSet(rDoc);
        lcl_PutFamily(aSet, rResolved.Get(svx::writer2027::TypeSystemFontRole::Mono).maFamily,
                      pFontList);
        lcl_PutSize(aSet, rScale.mnMono);
        lcl_ApplyFormat(rDoc, *lcl_GetCharFormat(rDoc, SwPoolFormatId::CHR_HTML_CODE), aSet);
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

bool lcl_PresetMatches(SwDoc& rDoc, const svx::writer2027::TypeSystemPreset& rPreset,
                       const svx::writer2027::ResolvedTypeSystem& rResolved)
{
    const svx::writer2027::TypeSystemScale& rScale
        = svx::writer2027::Writer2027TypeSystemCatalog::Get().GetScale(rPreset.meScale);

    SwTextFormatColl* pStandard = lcl_GetTextColl(rDoc, SwPoolFormatId::COLL_STANDARD);
    SwTextFormatColl* pHeadingBase = lcl_GetTextColl(rDoc, SwPoolFormatId::COLL_HEADLINE_BASE);
    SwTextFormatColl* pH1 = lcl_GetTextColl(rDoc, SwPoolFormatId::COLL_HEADLINE1);
    SwTextFormatColl* pH2 = lcl_GetTextColl(rDoc, SwPoolFormatId::COLL_HEADLINE2);
    SwTextFormatColl* pH3 = lcl_GetTextColl(rDoc, SwPoolFormatId::COLL_HEADLINE3);
    SwTextFormatColl* pTitle = lcl_GetTextColl(rDoc, SwPoolFormatId::COLL_DOC_TITLE);
    SwTextFormatColl* pPre = lcl_GetTextColl(rDoc, SwPoolFormatId::COLL_HTML_PRE);

    // A missing pool style must never dereference: treat it as "does not
    // match" rather than crashing the Type System picker.
    if (!pStandard || !pHeadingBase || !pH1 || !pH2 || !pH3 || !pTitle || !pPre)
        return false;

    if (!lcl_FormatHasFamily(*pStandard,
                             rResolved.Get(svx::writer2027::TypeSystemFontRole::Body).maFamily)
        || !lcl_FormatHasSize(*pStandard, rScale.mnBody)
        || !lcl_FormatHasLineSpacing(*pStandard, rScale.mnLineSpacingPercent)
        || !lcl_FormatHasSpaceAfter(*pStandard, rScale.mnSpaceAfter)
        || !lcl_FormatHasFamily(*pHeadingBase,
                                rResolved.Get(svx::writer2027::TypeSystemFontRole::Heading)
                                    .maFamily)
        || !lcl_FormatHasSize(*pH1, rScale.mnH1) || !lcl_FormatHasSize(*pH2, rScale.mnH2)
        || !lcl_FormatHasSize(*pH3, rScale.mnH3) || !lcl_FormatHasSize(*pTitle, rScale.mnTitle)
        || !lcl_FormatHasFamily(
            *pPre, rResolved.Get(svx::writer2027::TypeSystemFontRole::Mono).maFamily))
    {
        return false;
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
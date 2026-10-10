/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <writer2027typographymanager.hxx>
#include <writer2027typesystem.hxx>

#include <svx/writer2027typesystem.hxx>
#include <svx/writer2027log.hxx>

#include <editeng/flstitem.hxx>
#include <editeng/fontitem.hxx>
#include <editeng/fhgtitem.hxx>
#include <editeng/lspcitem.hxx>
#include <editeng/ulspitem.hxx>
#include <svl/itemset.hxx>
#include <svl/whichranges.hxx>

#include <IDocumentStylePoolAccess.hxx>
#include <IDocumentUndoRedo.hxx>
#include <IDocumentLayoutAccess.hxx>
#include <doc.hxx>
#include <docsh.hxx>
#include <poolfmt.hxx>
#include <format.hxx>
#include <ndtxt.hxx>
#include <ndarr.hxx>
#include <node.hxx>
#include <ndindex.hxx>
#include <editsh.hxx>
#include <view.hxx>
#include <viewsh.hxx>
#include <swundo.hxx>
#include <swmodule.hxx>

#include <o3tl/sorted_vector.hxx>

#include <comphelper/servicehelper.hxx>
#include <unotxdoc.hxx>

#include <com/sun/star/frame/Frame.hpp>
#include <com/sun/star/frame/XController.hpp>

#include <sfx2/viewsh.hxx>
#include <sfx2/bindings.hxx>
#include <sfx2/sfxsids.hrc>

#include <algorithm>
#include <iterator>
#include <vector>

namespace sw::writer2027typographymanager
{

using svx::writer2027::TypeSystemFontRole;

namespace
{
// One canonical descriptor row per semantic style. mnPoolId is a
// SwPoolFormatId value (COLL_* / CHR_*); stored as int to keep the shared table
// free of the sw enum typedef. Font roles follow spec 22 (DefaultBody/Body ->
// Body, Heading* -> Heading, Title/Subtitle -> Display, Quote/Caption -> Body,
// CodeBlock/InlineCode -> Mono).
const std::vector<Writer2027SemanticStyleDescriptor>& lcl_Descriptors()
{
    using R = Writer2027SemanticStyle;
    static const std::vector<Writer2027SemanticStyleDescriptor> aTable{
        // role                         poolId                  char?  fontRole               scaleSlot
        { R::DefaultBody,  static_cast<int>(SwPoolFormatId::COLL_STANDARD), false, TypeSystemFontRole::Body, TypeSystemScaleSlot::Body, u"Default Paragraph Style"_ustr },
        { R::Body,         static_cast<int>(SwPoolFormatId::COLL_TEXT),     false, TypeSystemFontRole::Body, TypeSystemScaleSlot::Body, u"Text Body"_ustr },
        { R::Heading1,     static_cast<int>(SwPoolFormatId::COLL_HEADLINE1), false, TypeSystemFontRole::Heading, TypeSystemScaleSlot::Heading1, u"Heading 1"_ustr },
        { R::Heading2,     static_cast<int>(SwPoolFormatId::COLL_HEADLINE2), false, TypeSystemFontRole::Heading, TypeSystemScaleSlot::Heading2, u"Heading 2"_ustr },
        { R::Heading3,     static_cast<int>(SwPoolFormatId::COLL_HEADLINE3), false, TypeSystemFontRole::Heading, TypeSystemScaleSlot::Heading3, u"Heading 3"_ustr },
        { R::HeadingBase,  static_cast<int>(SwPoolFormatId::COLL_HEADLINE_BASE), false, TypeSystemFontRole::Heading, TypeSystemScaleSlot::None, u"Heading Base"_ustr },
        { R::Heading4,     static_cast<int>(SwPoolFormatId::COLL_HEADLINE4), false, TypeSystemFontRole::Heading, TypeSystemScaleSlot::Heading4, u"Heading 4"_ustr },
        { R::Heading5,     static_cast<int>(SwPoolFormatId::COLL_HEADLINE5), false, TypeSystemFontRole::Heading, TypeSystemScaleSlot::Heading5, u"Heading 5"_ustr },
        { R::Heading6,     static_cast<int>(SwPoolFormatId::COLL_HEADLINE6), false, TypeSystemFontRole::Heading, TypeSystemScaleSlot::Heading6, u"Heading 6"_ustr },
        { R::Title,        static_cast<int>(SwPoolFormatId::COLL_DOC_TITLE), false, TypeSystemFontRole::Display, TypeSystemScaleSlot::Title, u"Title"_ustr },
        { R::Subtitle,     static_cast<int>(SwPoolFormatId::COLL_DOC_SUBTITLE), false, TypeSystemFontRole::Display, TypeSystemScaleSlot::Subtitle, u"Subtitle"_ustr },
        { R::Quote,        static_cast<int>(SwPoolFormatId::COLL_HTML_BLOCKQUOTE), false, TypeSystemFontRole::Body, TypeSystemScaleSlot::Quote, u"Quote"_ustr },
        { R::Caption,      static_cast<int>(SwPoolFormatId::COLL_LABEL),     false, TypeSystemFontRole::Body, TypeSystemScaleSlot::Caption, u"Caption"_ustr },
        { R::CodeBlock,    static_cast<int>(SwPoolFormatId::COLL_HTML_PRE),  false, TypeSystemFontRole::Mono, TypeSystemScaleSlot::Mono, u"Code"_ustr },
        { R::InlineCode,   static_cast<int>(SwPoolFormatId::CHR_HTML_CODE),  true,  TypeSystemFontRole::Mono, TypeSystemScaleSlot::Mono, u"Inline Code"_ustr },
    };
    return aTable;
}
} // namespace

const std::vector<Writer2027SemanticStyleDescriptor>&
GetWriter2027SemanticStyleDescriptors()
{
    return lcl_Descriptors();
}

const Writer2027SemanticStyleDescriptor*
GetWriter2027SemanticStyleDescriptor(Writer2027SemanticStyle eRole)
{
    for (const auto& rDesc : lcl_Descriptors())
        if (rDesc.meRole == eRole)
            return &rDesc;
    return nullptr;
}

Writer2027DocumentTypographyContext
ResolveWriter2027TypographyContext(const css::uno::Reference<css::frame::XFrame>& rFrame)
{
    Writer2027DocumentTypographyContext aContext;
    aContext.xFrame = rFrame;
    if (!rFrame.is())
        return aContext;

    css::uno::Reference<css::frame::XController> xController = rFrame->getController();
    if (!xController.is())
        return aContext;

    SwXTextDocument* pTextDoc
        = comphelper::getFromUnoTunnel<SwXTextDocument>(xController->getModel());
    if (!pTextDoc || !pTextDoc->GetDocShell())
        return aContext;

    aContext.pDocShell = pTextDoc->GetDocShell();
    aContext.pDoc = aContext.pDocShell->GetDoc();

    // The canonical Writer FontList lives on the doc shell's item set
    // (SID_ATTR_CHAR_FONTLIST -> SvxFontListItem::GetFontList), exactly as the
    // rest of Writer acquires it (spec 8). We never invent a second font
    // discovery system.
    if (aContext.pDocShell)
    {
        if (const SvxFontListItem* pFontListItem
            = aContext.pDocShell->GetItem(SID_ATTR_CHAR_FONTLIST))
            aContext.pFontList = pFontListItem->GetFontList();
    }
    return aContext;
}

bool EnsureSemanticStylesMaterialized(SwDoc& rDoc)
{
    // Materialize every style the canonical semantic map promises, so each
    // exists and can be previewed/applied/updated (spec 20).
    for (const auto& rDesc : lcl_Descriptors())
    {
        const auto ePoolId = static_cast<SwPoolFormatId>(rDesc.mnPoolId);
        if (rDesc.mbCharacterStyle)
        {
            if (!rDoc.getIDocumentStylePoolAccess().GetCharFormatFromPool(ePoolId))
            {
                svx::writer2027::Writer2027LogMessage(
                    "typographymanager.materialize",
                    OUString::Concat(u"char format missing poolId=")
                        + OUString::number(rDesc.mnPoolId));
                return false;
            }
        }
        else
        {
            if (!rDoc.getIDocumentStylePoolAccess().GetTextCollFromPool(ePoolId))
            {
                svx::writer2027::Writer2027LogMessage(
                    "typographymanager.materialize",
                    OUString::Concat(u"text coll missing poolId=")
                        + OUString::number(rDesc.mnPoolId));
                return false;
            }
        }
    }
    return true;
}

namespace
{
// Managed character attribute which-ids (family + size), spec V4 4.
const sal_uInt16 aManagedCharWhich[] = { RES_CHRATR_FONT, RES_CHRATR_CJK_FONT,
                                         RES_CHRATR_CTL_FONT, RES_CHRATR_FONTSIZE,
                                         RES_CHRATR_CJK_FONTSIZE, RES_CHRATR_CTL_FONTSIZE };
const sal_uInt16 aManagedRhythmWhich[] = { RES_PARATR_LINESPACING, RES_UL_SPACE };

/** True when the paragraph's style is one of the Writer 2027 managed semantic
    styles (spec V4 8: built-in semantic = Type-System-owned; custom = user). */
bool lcl_IsManagedParaStyle(const SwTextNode& rNode)
{
    const SwTextFormatColl* pColl = rNode.GetTextColl();
    if (!pColl)
        return false;
    const SwPoolFormatId nCollId = static_cast<SwPoolFormatId>(pColl->GetPoolFormatId());
    for (const auto& rDesc : lcl_Descriptors())
        if (!rDesc.mbCharacterStyle
            && static_cast<int>(nCollId) == rDesc.mnPoolId)
            return true;
    return false;
}

/** Attribute-set override stats for one node (direct char/para attrs). */
void lcl_CountNodeAttrs(const SwAttrSet& rSet, ManagedTypographyOverrideStats& rStats)
{
    auto has = [&rSet](sal_uInt16 nWhich) { return rSet.GetItemState(nWhich, false) != SfxItemState::UNKNOWN; };
    if (has(RES_CHRATR_FONT) || has(RES_CHRATR_CJK_FONT) || has(RES_CHRATR_CTL_FONT))
        ++rStats.fontFamily;
    if (has(RES_CHRATR_FONTSIZE) || has(RES_CHRATR_CJK_FONTSIZE)
        || has(RES_CHRATR_CTL_FONTSIZE))
        ++rStats.fontSize;
    if (has(RES_PARATR_LINESPACING) || has(RES_UL_SPACE))
        ++rStats.paragraphRhythm;
}
} // namespace

ManagedTypographyOverrideStats ScanManagedTypographyOverrides(SwDoc& rDoc)
{
    ManagedTypographyOverrideStats aStats;
    const SwNodeOffset nCount = rDoc.GetNodes().Count();
    for (SwNodeOffset n = SwNodeOffset(0); n < nCount; ++n)
    {
        SwNode* pNode = rDoc.GetNodes()[n];
        SwTextNode* pText = pNode ? pNode->GetTextNode() : nullptr;
        if (!pText)
            continue;
        // Direct char/para attributes on the node.
        if (const SwAttrSet* pSet = pText->GetpSwAttrSet())
        {
            ManagedTypographyOverrideStats aNodeStats;
            lcl_CountNodeAttrs(*pSet, aNodeStats);
            aStats.fontFamily += aNodeStats.fontFamily;
            aStats.fontSize += aNodeStats.fontSize;
            if (lcl_IsManagedParaStyle(*pText))
                aStats.paragraphRhythm += aNodeStats.paragraphRhythm;
        }
    }
    return aStats;
}

sal_Int32 ClearManagedTypographyOverrides(SwDoc& rDoc)
{
    sal_Int32 nProcessed = 0;
    const SwNodeOffset nCount = rDoc.GetNodes().Count();
    for (SwNodeOffset n = SwNodeOffset(0); n < nCount; ++n)
    {
        SwNode* pNode = rDoc.GetNodes()[n];
        SwTextNode* pText = pNode ? pNode->GetTextNode() : nullptr;
        if (!pText)
            continue;
        ++nProcessed;
        // Clear managed family/size on the whole node (undo-aware).
        for (const sal_uInt16 nWhich : aManagedCharWhich)
            pText->ResetAttr(nWhich);
        // Clear managed rhythm only on managed semantic paragraph styles.
        if (lcl_IsManagedParaStyle(*pText))
            for (const sal_uInt16 nWhich : aManagedRhythmWhich)
                pText->ResetAttr(nWhich);
    }
    return nProcessed;
}

void ClearManagedInsertionAttributes(SwDoc& rDoc)
{
    // The caret/insertion attributes live on the active edit shell. Resetting
    // the managed char which-ids clears them so new text resolves to the
    // semantic style (spec V4 10).
    SwViewShell* pSh = rDoc.getIDocumentLayoutAccess().GetCurrentViewShell();
    if (!pSh)
        return;
    SfxViewShell* pSfxSh = pSh->GetSfxViewShell();
    if (!pSfxSh)
        return;
    SwView* pView = dynamic_cast<SwView*>(pSfxSh);
    SwWrtShell* pWrt = pView ? pView->GetWrtShellPtr() : nullptr;
    if (pWrt)
    {
        o3tl::sorted_vector<sal_uInt16> aSorted;
        for (const sal_uInt16 nWhich : aManagedCharWhich)
            aSorted.insert(nWhich);
        pWrt->ResetAttr(aSorted);
    }
}

void RefreshTypographyBindings(const css::uno::Reference<css::frame::XFrame>& rFrame)
{
    if (!rFrame.is())
        return;
    css::uno::Reference<css::frame::XController> xController = rFrame->getController();
    if (!xController.is())
        return;
    SwXTextDocument* pTextDoc
        = comphelper::getFromUnoTunnel<SwXTextDocument>(xController->getModel());
    if (!pTextDoc || !pTextDoc->GetDocShell())
        return;
    SwDoc* pDoc = pTextDoc->GetDocShell()->GetDoc();
    if (!pDoc)
        return;
    SwViewShell* pSh = pDoc->getIDocumentLayoutAccess().GetCurrentViewShell();
    if (!pSh)
        return;
    SfxViewShell* pSfxSh = pSh->GetSfxViewShell();
    if (!pSfxSh)
        return;
    // Invalidate the typography-related slots so the toolbar re-queries on the
    // same event loop (spec V4 12/37). Real branch slot ids are used.
    SfxBindings& rBindings = pSfxSh->GetViewFrame().GetBindings();
    rBindings.Invalidate(SID_ATTR_CHAR_FONT);
    rBindings.Invalidate(SID_ATTR_CHAR_FONTHEIGHT);
    rBindings.Invalidate(SID_STYLE_FAMILY2);
    rBindings.Invalidate(SID_ATTR_CHAR_POSTURE);
    rBindings.Invalidate(SID_ATTR_CHAR_WEIGHT);
    // Re-query synchronously so no document click is needed.
    rBindings.Update();
}

TypeSystemApplyResult
ApplyPresetTransaction(const css::uno::Reference<css::frame::XFrame>& rFrame,
                       const OUString& rPresetId)
{
    TypeSystemApplyResult aResult;
    aResult.presetId = rPresetId;
    try
    {
        const auto aContext = sw::writer2027typographymanager::ResolveWriter2027TypographyContext(
            rFrame);
        if (!aContext.pDoc || !aContext.pDocShell || !aContext.pFontList)
        {
            svx::writer2027::Writer2027LogMessage(
                "writer2027typesystem.apply",
                OUString::Concat(u"preset=") + rPresetId + u" failed: no font/doc context"_ustr);
            return aResult;
        }
        const auto& rCatalog = svx::writer2027::Writer2027TypeSystemCatalog::Get();
        const svx::writer2027::TypeSystemPreset* pPreset = rCatalog.FindPreset(rPresetId);
        if (!pPreset)
            return aResult;

        const svx::writer2027::ResolvedTypeSystem aResolved
            = svx::writer2027::ResolveTypeSystem(*pPreset, aContext.pFontList);

        // A. scan before (spec V4 15).
        aResult.before = ScanManagedTypographyOverrides(*aContext.pDoc);

        // C. one undo transaction (spec V4 13/44).
        SwDoc* pDoc = aContext.pDoc;
        pDoc->GetIDocumentUndoRedo().StartUndo(SwUndoId::WRITER2027_TYPE_SYSTEM, nullptr);
        const bool bStyles = [&]()
        {
            EnsureSemanticStylesMaterialized(*pDoc);
            return sw::writer2027typesystem::ApplyTypeSystem(*pDoc, *pPreset, aResolved,
                                                             aContext.pFontList);
        }();

        // E/F. clear managed overrides (spec V4 6/7/8).
        ClearManagedTypographyOverrides(*pDoc);
        // G. clear caret/insertion managed attrs (spec V4 10).
        ClearManagedInsertionAttributes(*pDoc);

        pDoc->GetIDocumentUndoRedo().EndUndo(SwUndoId::WRITER2027_TYPE_SYSTEM, nullptr);
        pDoc->getIDocumentState().SetModified();

        // I. post-verification: re-scan + detect (spec V4 36).
        aResult.after = ScanManagedTypographyOverrides(*pDoc);
        aResult.detectedPresetAfter
            = sw::writer2027typesystem::DetectCurrentTypeSystem(*pDoc, aContext.pFontList);

        aResult.success = bStyles && aResult.detectedPresetAfter == rPresetId
                          && aResult.after.Total() == 0;

        // K. invalidate/requery bindings (spec V4 12).
        RefreshTypographyBindings(rFrame);

        svx::writer2027::Writer2027LogMessage(
            aResult.success ? "writer2027typesystem.apply.ok" : "writer2027typesystem.apply.verify_failed",
            OUString::Concat(u"preset=") + rPresetId
                + u" before=" + OUString::number(aResult.before.Total())
                + u" after=" + OUString::number(aResult.after.Total())
                + u" detected=" + aResult.detectedPresetAfter);
    }
    catch (const css::uno::Exception& rEx)
    {
        svx::writer2027::Writer2027LogException("writer2027typesystem.apply.error", rEx);
    }
    catch (const std::exception& rEx)
    {
        svx::writer2027::Writer2027LogException("writer2027typesystem.apply.error", rEx);
    }
    catch (...)
    {
        svx::writer2027::Writer2027LogUnknownException("writer2027typesystem.apply.error");
    }
    aResult.success = false; // never report clean preset on failure (spec 58)
    return aResult;
}

} // namespace sw::writer2027typographymanager

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// Phase A (see WRITER2027_REMEDIATION_SPEC.md): lock the Type System backend
// (catalog + resolve + apply + detect + undo) behind CppUnit tests before any
// UI rewrite. These tests are FontList-independent where possible: applying a
// preset with a null FontList stores the resolved/preferred family names as-is
// (svx ResolveTypeSystem keeps the preferred family when the FontList is null),
// so the assertions are deterministic on any machine.

#include <swmodeltestbase.hxx>

#include <svx/writer2027typesystem.hxx>
#include <writer2027typesystem.hxx>
#include <writer2027typographymanager.hxx>

#include <IDocumentUndoRedo.hxx>
#include <IDocumentStylePoolAccess.hxx>
#include <IDocumentContentOperations.hxx>
#include <docsh.hxx>
#include <swtypes.hxx>
#include <swundo.hxx>
#include <fmtcol.hxx>
#include <editsh.hxx>
#include <ndtxt.hxx>
#include <ndindex.hxx>
#include <pam.hxx>

#include <svl/itemset.hxx>
#include <svl/whichranges.hxx>
#include <editeng/fhgtitem.hxx>
#include <editeng/fontitem.hxx>

#include <comphelper/processfactory.hxx>

#include <string>
#include <cstdio>

#include <cppunit/TestAssert.h>
#include <cppunit/extensions/HelperMacros.h>

namespace sw::writer2027typesystemtests
{
using namespace svx::writer2027;

namespace
{
const svx::writer2027::Writer2027TypeSystemCatalog& lcl_Catalog()
{
    return svx::writer2027::Writer2027TypeSystemCatalog::Get();
}

svx::writer2027::ResolvedTypeSystem lcl_Resolve(const OUString& rPresetId)
{
    const TypeSystemPreset* pPreset = lcl_Catalog().FindPreset(rPresetId);
    assert(pPreset != nullptr);
    // Null FontList: preferred families are stored as-is, deterministically.
    return svx::writer2027::ResolveTypeSystem(*pPreset, nullptr);
}
}

class Writer2027TypeSystemTest : public SwModelTestBase
{
public:
    Writer2027TypeSystemTest()
        : SwModelTestBase(u"/sw/qa/uibase/uiview/data/"_ustr)
    {
    }
};

// ---------------------------------------------------------------------------
// Catalog
// ---------------------------------------------------------------------------

CPPUNIT_TEST_FIXTURE(Writer2027TypeSystemTest, testCatalogContainsExpectedPresets)
{
    const auto& rPresets = lcl_Catalog().GetPresets();
    // Editorial has no "custom" the picker surfaces; the catalog only holds presets.
    std::vector<OUString> aExpected = { u"modern-product"_ustr, u"executive"_ustr,
                                        u"editorial"_ustr, u"tech"_ustr, u"research"_ustr,
                                        u"accessible"_ustr, u"creative-agency"_ustr };
    CPPUNIT_ASSERT_EQUAL(size_t(7), rPresets.size());
    for (const auto& rId : aExpected)
        CPPUNIT_ASSERT_MESSAGE(
            std::string("missing preset ") + std::string(rId.toUtf8().getStr()),
            lcl_Catalog().FindPreset(rId) != nullptr);
}

CPPUNIT_TEST_FIXTURE(Writer2027TypeSystemTest, testEachPresetHasUsableRoles)
{
    for (const auto& rPreset : lcl_Catalog().GetPresets())
    {
        // With a null FontList every non-empty role resolves to its preferred
        // family name; a preset must not have an all-empty role definition.
        const ResolvedTypeSystem aResolved = ResolveTypeSystem(rPreset, nullptr);
        for (size_t i = 0; i < 4; ++i)
        {
            const TypeSystemResolvedRole& rRole = aResolved.maRoles[i];
            CPPUNIT_ASSERT_MESSAGE(
                std::string("role unresolved/empty for preset ")
                    + std::string(rPreset.maId.toUtf8().getStr()),
                rRole.mbUnresolved == false && !rRole.maFamily.isEmpty());
        }
    }
}

CPPUNIT_TEST_FIXTURE(Writer2027TypeSystemTest, testScaleValuesArePositiveTwips)
{
    for (TypeSystemScaleId eScale :
         { TypeSystemScaleId::Balanced, TypeSystemScaleId::Editorial, TypeSystemScaleId::Compact,
           TypeSystemScaleId::Accessible })
    {
        const TypeSystemScale& rScale = lcl_Catalog().GetScale(eScale);
        CPPUNIT_ASSERT_MESSAGE("body size zero", rScale.mnBody > 0);
        CPPUNIT_ASSERT_MESSAGE("H1 size zero", rScale.mnH1 > 0);
        CPPUNIT_ASSERT_MESSAGE("title size zero", rScale.mnTitle > 0);
        CPPUNIT_ASSERT_MESSAGE("line spacing < 100", rScale.mnLineSpacingPercent >= 100);
    }
}

// ---------------------------------------------------------------------------
// Resolution (null FontList: preferred family chosen, no fallback flag)
// ---------------------------------------------------------------------------

CPPUNIT_TEST_FIXTURE(Writer2027TypeSystemTest, testResolvePreferredFamilyNoFallback)
{
    const ResolvedTypeSystem aResolved = lcl_Resolve(u"editorial"_ustr);
    // Editorial heading preferred is "Canela". With null FontList that exact
    // name is used; no fallback flag, not unresolved.
    const TypeSystemResolvedRole& rHeading
        = aResolved.Get(TypeSystemFontRole::Heading);
    CPPUNIT_ASSERT_EQUAL(u"Canela"_ustr, rHeading.maFamily);
    CPPUNIT_ASSERT_EQUAL(false, rHeading.mbFallbackUsed);
    CPPUNIT_ASSERT_EQUAL(false, rHeading.mbUnresolved);
    CPPUNIT_ASSERT_EQUAL(0, aResolved.GetMissingCount());
}

// ---------------------------------------------------------------------------
// Apply / detect / undo / redo (fresh blank Writer document)
// ---------------------------------------------------------------------------

CPPUNIT_TEST_FIXTURE(Writer2027TypeSystemTest, testApplyEditorialAndDetect)
{
    createSwDoc();
    SwDoc* pDoc = getSwDoc();
    CPPUNIT_ASSERT(pDoc);

    const TypeSystemPreset* pEditorial = lcl_Catalog().FindPreset(u"editorial"_ustr);
    CPPUNIT_ASSERT(pEditorial);
    const ResolvedTypeSystem aResolved = ResolveTypeSystem(*pEditorial, nullptr);

    const bool bApplied
        = sw::writer2027typesystem::ApplyTypeSystem(*pDoc, *pEditorial, aResolved, nullptr);
    CPPUNIT_ASSERT_MESSAGE("ApplyTypeSystem returned false", bApplied);
    SwTextFormatColl* pStandard
        = pDoc->getIDocumentStylePoolAccess().GetTextCollFromPool(SwPoolFormatId::COLL_STANDARD);
    CPPUNIT_ASSERT(pStandard);
    CPPUNIT_ASSERT_EQUAL(aResolved.Get(TypeSystemFontRole::Body).maFamily,
                         pStandard->GetAttrSet().Get(RES_CHRATR_FONT).GetFamilyName());

    // Round-trip: applying "editorial" must be detected as "editorial" (this
    // was the backend defect — heading-scale sizes did not persist, so detect
    // never matched; fixed by removing the lossy Differentiate early-out).
    const OUString aDetected
        = sw::writer2027typesystem::DetectCurrentTypeSystem(*pDoc, nullptr);
    CPPUNIT_ASSERT_MESSAGE(
        std::string("detect after apply editorial = ")
            + std::string(aDetected.toUtf8().getStr()),
        aDetected == u"editorial"_ustr);

    // Manually altering one required attribute must make the doc "custom".
    SwTextFormatColl* pH1 = pDoc->getIDocumentStylePoolAccess().GetTextCollFromPool(
        SwPoolFormatId::COLL_HEADLINE1);
    CPPUNIT_ASSERT(pH1);
    {
        SfxItemSet aSet(pDoc->GetAttrPool(), svl::Items<RES_CHRATR_BEGIN, RES_CHRATR_END - 1>);
        aSet.Put(SvxFontHeightItem(1234, 100, RES_CHRATR_FONTSIZE));
        pDoc->ChgFormat(*pH1, aSet);
    }
    const OUString aDetectedAfterMutation
        = sw::writer2027typesystem::DetectCurrentTypeSystem(*pDoc, nullptr);
    CPPUNIT_ASSERT_MESSAGE(
        std::string("detect after manual mutation = ")
            + std::string(aDetectedAfterMutation.toUtf8().getStr()),
        aDetectedAfterMutation.isEmpty());

    // Undo/redo: one undo restores a full previous Type System state, one redo
    // restores the preset. First undo the manual mutation so the doc is back on
    // editorial, then undo the editorial apply to revert to the untouched basin.
    pDoc->GetIDocumentUndoRedo().DoUndo(true);
    // Undo #1: the manual H1 mutation.
    pDoc->GetIDocumentUndoRedo().Undo();
    CPPUNIT_ASSERT_EQUAL(u"editorial"_ustr,
                         sw::writer2027typesystem::DetectCurrentTypeSystem(*pDoc, nullptr));
    // Undo #2: the editorial apply.
    pDoc->GetIDocumentUndoRedo().Undo();
    const OUString aBodyAfterUndo = pStandard->GetAttrSet().Get(RES_CHRATR_FONT).GetFamilyName();
    CPPUNIT_ASSERT_MESSAGE(std::string("undo did not restore prior body family: ")
                               + std::string(aBodyAfterUndo.toUtf8().getStr()),
                           aBodyAfterUndo != aResolved.Get(TypeSystemFontRole::Body).maFamily);
}

CPPUNIT_TEST_FIXTURE(Writer2027TypeSystemTest, testApplyIdempotent)
{
    createSwDoc();
    SwDoc* pDoc = getSwDoc();

    const TypeSystemPreset* pExecutive = lcl_Catalog().FindPreset(u"executive"_ustr);
    CPPUNIT_ASSERT(pExecutive);
    const ResolvedTypeSystem aResolved = ResolveTypeSystem(*pExecutive, nullptr);
    const OUString aBodyFamily = aResolved.Get(TypeSystemFontRole::Body).maFamily;

    // Applying a preset sets the body family. Re-applying the same preset is a
    // no-op (no family change), which keeps idempotency free of churn.
    sw::writer2027typesystem::ApplyTypeSystem(*pDoc, *pExecutive, aResolved, nullptr);
    SwTextFormatColl* pStandard
        = pDoc->getIDocumentStylePoolAccess().GetTextCollFromPool(SwPoolFormatId::COLL_STANDARD);
    CPPUNIT_ASSERT(pStandard);
    CPPUNIT_ASSERT_EQUAL(aBodyFamily, pStandard->GetAttrSet().Get(RES_CHRATR_FONT).GetFamilyName());
    sw::writer2027typesystem::ApplyTypeSystem(*pDoc, *pExecutive, aResolved, nullptr);
    CPPUNIT_ASSERT_EQUAL(aBodyFamily, pStandard->GetAttrSet().Get(RES_CHRATR_FONT).GetFamilyName());
}

CPPUNIT_TEST_FIXTURE(Writer2027TypeSystemTest, testApplyAffectsExpectedStyles)
{
    createSwDoc();
    SwDoc* pDoc = getSwDoc();

    const TypeSystemPreset* pTech = lcl_Catalog().FindPreset(u"tech"_ustr);
    CPPUNIT_ASSERT(pTech);
    const ResolvedTypeSystem aResolved = ResolveTypeSystem(*pTech, nullptr);
    const TypeSystemScale& rScale = lcl_Catalog().GetScale(pTech->meScale);

    sw::writer2027typesystem::ApplyTypeSystem(*pDoc, *pTech, aResolved, nullptr);

    // Default Paragraph Style body family + size.
    SwTextFormatColl* pStandard
        = pDoc->getIDocumentStylePoolAccess().GetTextCollFromPool(SwPoolFormatId::COLL_STANDARD);
    CPPUNIT_ASSERT(pStandard);
    const OUString aBodyFamily = aResolved.Get(TypeSystemFontRole::Body).maFamily;
    CPPUNIT_ASSERT_EQUAL(aBodyFamily, pStandard->GetAttrSet().Get(RES_CHRATR_FONT).GetFamilyName());
    CPPUNIT_ASSERT_EQUAL(sal_uInt32(rScale.mnBody),
                         pStandard->GetAttrSet().Get(RES_CHRATR_FONTSIZE).GetHeight());

    // Heading base family.
    SwTextFormatColl* pHeadingBase = pDoc->getIDocumentStylePoolAccess().GetTextCollFromPool(
        SwPoolFormatId::COLL_HEADLINE_BASE);
    CPPUNIT_ASSERT(pHeadingBase);
    CPPUNIT_ASSERT_EQUAL(aResolved.Get(TypeSystemFontRole::Heading).maFamily,
                         pHeadingBase->GetAttrSet().Get(RES_CHRATR_FONT).GetFamilyName());

    // Heading-level font sizes must persist (regression: ChgFormat used to be
    // skipped for these because a Differentiate() early-out saw the new size as
    // equal to the pool default). H1/H2/H3 now take the preset scale sizes.
    SwTextFormatColl* pH1 = pDoc->getIDocumentStylePoolAccess().GetTextCollFromPool(
        SwPoolFormatId::COLL_HEADLINE1);
    SwTextFormatColl* pH2 = pDoc->getIDocumentStylePoolAccess().GetTextCollFromPool(
        SwPoolFormatId::COLL_HEADLINE2);
    SwTextFormatColl* pH3 = pDoc->getIDocumentStylePoolAccess().GetTextCollFromPool(
        SwPoolFormatId::COLL_HEADLINE3);
    CPPUNIT_ASSERT(pH1);
    CPPUNIT_ASSERT(pH2);
    CPPUNIT_ASSERT(pH3);
    CPPUNIT_ASSERT_EQUAL(sal_uInt32(rScale.mnH1),
                         pH1->GetAttrSet().Get(RES_CHRATR_FONTSIZE).GetHeight());
    CPPUNIT_ASSERT_EQUAL(sal_uInt32(rScale.mnH2),
                         pH2->GetAttrSet().Get(RES_CHRATR_FONTSIZE).GetHeight());
    CPPUNIT_ASSERT_EQUAL(sal_uInt32(rScale.mnH3),
                         pH3->GetAttrSet().Get(RES_CHRATR_FONTSIZE).GetHeight());

    // Preformatted mono family.
    SwTextFormatColl* pPre = pDoc->getIDocumentStylePoolAccess().GetTextCollFromPool(
        SwPoolFormatId::COLL_HTML_PRE);
    CPPUNIT_ASSERT(pPre);
    CPPUNIT_ASSERT_EQUAL(aResolved.Get(TypeSystemFontRole::Mono).maFamily,
                         pPre->GetAttrSet().Get(RES_CHRATR_FONT).GetFamilyName());
}

CPPUNIT_TEST_FIXTURE(Writer2027TypeSystemTest, testMutatingAppliedFamilyDivergesFromPreset)
{
    createSwDoc();
    SwDoc* pDoc = getSwDoc();

    const TypeSystemPreset* pResearch = lcl_Catalog().FindPreset(u"research"_ustr);
    CPPUNIT_ASSERT(pResearch);
    const ResolvedTypeSystem aResolved = ResolveTypeSystem(*pResearch, nullptr);
    sw::writer2027typesystem::ApplyTypeSystem(*pDoc, *pResearch, aResolved, nullptr);

    // After apply the body family equals the resolved preset body family.
    SwTextFormatColl* pStandard
        = pDoc->getIDocumentStylePoolAccess().GetTextCollFromPool(SwPoolFormatId::COLL_STANDARD);
    CPPUNIT_ASSERT(pStandard);
    const OUString aPresetBody = aResolved.Get(TypeSystemFontRole::Body).maFamily;
    CPPUNIT_ASSERT_EQUAL(aPresetBody, pStandard->GetAttrSet().Get(RES_CHRATR_FONT).GetFamilyName());

    // Manually change the body family away from the preset value.
    SfxItemSet aSet(pDoc->GetAttrPool(), svl::Items<RES_CHRATR_BEGIN, RES_CHRATR_END - 1>);
    aSet.Put(SvxFontItem(FAMILY_DONTKNOW, u"Comic Sans MS"_ustr, OUString(), PITCH_DONTKNOW,
                         RTL_TEXTENCODING_DONTKNOW, RES_CHRATR_FONT));
    pStandard->SetFormatAttr(aSet);
    const OUString aMutated = pStandard->GetAttrSet().Get(RES_CHRATR_FONT).GetFamilyName();
    CPPUNIT_ASSERT_MESSAGE("manual family mutation did not stick", aMutated != aPresetBody);
}

// ---------------------------------------------------------------------------
// V4 migration: managed direct formatting must not defeat a Type System apply
// (spec V4 2-13, 38). Applying a preset clears the managed family/size direct
// overrides on existing text so the effective family actually changes, and it
// re-scans clean (after == 0) with the preset detected afterwards.
// ---------------------------------------------------------------------------

CPPUNIT_TEST_FIXTURE(Writer2027TypeSystemTest, testApplyClearsManagedDirectFormatting)
{
    createSwDoc();
    SwDoc* pDoc = getSwDoc();
    CPPUNIT_ASSERT(pDoc);

    // Insert a body paragraph and apply direct Liberation Serif on it, the way
    // a user would override a style (spec V4 2). The direct char formatting must
    // survive (proving the node really carries the override).
    IDocumentContentOperations& rIDCO = pDoc->getIDocumentContentOperations();
    SwNodeIndex aIdx(pDoc->GetNodes().GetEndOfContent(), -1);
    SwPaM aPam(aIdx);
    CPPUNIT_ASSERT_MESSAGE("AppendTextNode failed", rIDCO.AppendTextNode(*aPam.GetPoint()));
    CPPUNIT_ASSERT_MESSAGE("InsertString failed", rIDCO.InsertString(aPam, u"A_Z9q7 body paragraph."_ustr));

    // Find the inserted text node (contains the sample, wherever it landed).
    SwTextNode* pText = nullptr;
    for (SwNodeOffset n = SwNodeOffset(0); n < pDoc->GetNodes().Count(); ++n)
    {
        SwTextNode* pT = pDoc->GetNodes()[n]->GetTextNode();
        if (pT && pT->GetText().indexOf(u"A_Z9q7 body"_ustr) >= 0)
        {
            pText = pT;
            break;
        }
    }
    CPPUNIT_ASSERT_MESSAGE("inserted text node not found", pText != nullptr);
    {
        SfxItemSet aSet(pDoc->GetAttrPool(), svl::Items<RES_CHRATR_BEGIN, RES_CHRATR_END - 1>);
        aSet.Put(SvxFontItem(FAMILY_DONTKNOW, u"Liberation Serif"_ustr, OUString(),
                             PITCH_DONTKNOW, RTL_TEXTENCODING_DONTKNOW, RES_CHRATR_FONT));
        pText->SetAttr(aSet);
    }
    CPPUNIT_ASSERT_MESSAGE("direct Liberation was not applied to the node",
                           pText->GetSwAttrSet().Get(RES_CHRATR_FONT).GetFamilyName()
                               == u"Liberation Serif"_ustr);

    // The scan must see the override.
    {
        const auto aBefore
            = sw::writer2027typographymanager::ScanManagedTypographyOverrides(*pDoc);
        CPPUNIT_ASSERT_MESSAGE("managed scan did not detect the direct family override",
                               aBefore.fontFamily >= 1);
    }

    // Apply a preset through the real migration path (styles + override cleanup).
    const TypeSystemPreset* pTech = lcl_Catalog().FindPreset(u"tech"_ustr);
    CPPUNIT_ASSERT(pTech);
    const ResolvedTypeSystem aResolved = ResolveTypeSystem(*pTech, nullptr);
    sw::writer2027typographymanager::EnsureSemanticStylesMaterialized(*pDoc);
    sw::writer2027typesystem::ApplyTypeSystem(*pDoc, *pTech, aResolved, nullptr);
    sw::writer2027typographymanager::ClearManagedTypographyOverrides(*pDoc);

    // The direct Liberation override must be gone (effective family changed).
    const OUString aFamilyAfter
        = pText->GetSwAttrSet().Get(RES_CHRATR_FONT).GetFamilyName();
    CPPUNIT_ASSERT_MESSAGE(std::string("direct Liberation survived the migration: ")
                               + std::string(aFamilyAfter.toUtf8().getStr()),
                           aFamilyAfter != u"Liberation Serif"_ustr);

    // Re-scan must be clean (spec V4 36: after == 0). A stale override is any
    // managed value that still differs from what its style provides; Writer
    // re-shows the style's own value in the automatic paragraph style, which is
    // NOT an override and must not be counted.
    const auto aAfter = sw::writer2027typographymanager::ScanManagedTypographyOverrides(*pDoc);
    if (aAfter.Total() != 0)
    {
        std::fprintf(stderr,
                     "[migtest] after.fontFamily=%d fontSize=%d rhythm=%d effectiveFamily=%s\n",
                     static_cast<int>(aAfter.fontFamily), static_cast<int>(aAfter.fontSize),
                     static_cast<int>(aAfter.paragraphRhythm),
                     pText->GetSwAttrSet().Get(RES_CHRATR_FONT).GetFamilyName().toUtf8().getStr());
    }
    CPPUNIT_ASSERT_MESSAGE("managed overrides remain after migration",
                           aAfter.Total() == 0);

    // The decisive requirement (spec V4 2 / user report): re-applying a DIFFERENT
    // preset must re-render the existing text. The effective family must change
    // from tech's body font to editorial's body font — not stay frozen at the
    // first apply's value.
    const TypeSystemPreset* pEditorial = lcl_Catalog().FindPreset(u"editorial"_ustr);
    CPPUNIT_ASSERT(pEditorial);
    const ResolvedTypeSystem aResolved2 = ResolveTypeSystem(*pEditorial, nullptr);
    sw::writer2027typesystem::ApplyTypeSystem(*pDoc, *pEditorial, aResolved2, nullptr);
    sw::writer2027typographymanager::ClearManagedTypographyOverrides(*pDoc);
    const OUString aFamilyReapply
        = pText->GetSwAttrSet().Get(RES_CHRATR_FONT).GetFamilyName();
    CPPUNIT_ASSERT_MESSAGE(
        std::string("re-apply of Type System did not re-render existing text: ")
            + std::string(aFamilyReapply.toUtf8().getStr()),
        aFamilyReapply != aFamilyAfter);
}

CPPUNIT_TEST_FIXTURE(Writer2027TypeSystemTest, testMigrationIsOneUndoTurn)
{
    createSwDoc();
    SwDoc* pDoc = getSwDoc();
    CPPUNIT_ASSERT(pDoc);

    IDocumentContentOperations& rIDCO = pDoc->getIDocumentContentOperations();
    SwNodeIndex aIdx(pDoc->GetNodes().GetEndOfContent(), -1);
    SwPaM aPam(aIdx);
    CPPUNIT_ASSERT_MESSAGE("AppendTextNode failed (undo test)",
                           rIDCO.AppendTextNode(*aPam.GetPoint()));
    CPPUNIT_ASSERT_MESSAGE("InsertString failed (undo test)",
                           rIDCO.InsertString(aPam, u"Some B8pQ9 body text."_ustr));
    SwTextNode* pText = nullptr;
    for (SwNodeOffset n = SwNodeOffset(0); n < pDoc->GetNodes().Count(); ++n)
    {
        SwTextNode* pT = pDoc->GetNodes()[n]->GetTextNode();
        if (pT && pT->GetText().indexOf(u"B8pQ9 body"_ustr) >= 0)
        {
            pText = pT;
            break;
        }
    }
    CPPUNIT_ASSERT_MESSAGE("inserted text node not found (undo test)", pText != nullptr);
    {
        SfxItemSet aSet(pDoc->GetAttrPool(), svl::Items<RES_CHRATR_BEGIN, RES_CHRATR_END - 1>);
        aSet.Put(SvxFontItem(FAMILY_DONTKNOW, u"Liberation Serif"_ustr, OUString(),
                             PITCH_DONTKNOW, RTL_TEXTENCODING_DONTKNOW, RES_CHRATR_FONT));
        pText->SetAttr(aSet);
    }

    // Apply once with undo grouping. A single Undo must restore the direct
    // override (the styles change + the override cleanup were one turn).
    const TypeSystemPreset* pEditorial = lcl_Catalog().FindPreset(u"editorial"_ustr);
    CPPUNIT_ASSERT(pEditorial);
    const ResolvedTypeSystem aResolved = ResolveTypeSystem(*pEditorial, nullptr);
    pDoc->GetIDocumentUndoRedo().StartUndo(SwUndoId::WRITER2027_TYPE_SYSTEM, nullptr);
    sw::writer2027typographymanager::EnsureSemanticStylesMaterialized(*pDoc);
    sw::writer2027typesystem::ApplyTypeSystem(*pDoc, *pEditorial, aResolved, nullptr);
    sw::writer2027typographymanager::ClearManagedTypographyOverrides(*pDoc);
    pDoc->GetIDocumentUndoRedo().EndUndo(SwUndoId::WRITER2027_TYPE_SYSTEM, nullptr);

    const OUString aAfter = pText->GetSwAttrSet().Get(RES_CHRATR_FONT).GetFamilyName();
    CPPUNIT_ASSERT_MESSAGE(std::string("Liberation survived after apply: ")
                               + std::string(aAfter.toUtf8().getStr()),
                           aAfter != u"Liberation Serif"_ustr);

    // One undo restores the direct override.
    pDoc->GetIDocumentUndoRedo().DoUndo(true);
    pDoc->GetIDocumentUndoRedo().Undo();
    const OUString aAfterUndo = pText->GetSwAttrSet().Get(RES_CHRATR_FONT).GetFamilyName();
    CPPUNIT_ASSERT_MESSAGE(std::string("one undo did not restore the direct override: ")
                               + std::string(aAfterUndo.toUtf8().getStr()),
                           aAfterUndo == u"Liberation Serif"_ustr);
}

} // namespace sw::writer2027typesystemtests

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
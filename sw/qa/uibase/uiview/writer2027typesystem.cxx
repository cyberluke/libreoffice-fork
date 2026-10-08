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

#include <IDocumentUndoRedo.hxx>
#include <IDocumentStylePoolAccess.hxx>
#include <docsh.hxx>
#include <swtypes.hxx>
#include <swundo.hxx>
#include <fmtcol.hxx>

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

} // namespace sw::writer2027typesystemtests

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
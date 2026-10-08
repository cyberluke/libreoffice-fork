/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <sal/config.h>
#include <test/screenshot_test.hxx>
#include <vcl/abstdlg.hxx>

#include <svx/writer2027fontpopup.hxx>
#include <svx/writer2027typesystempopup.hxx>
#include <svx/writer2027typography.hxx>
#include <svtools/ctrltool.hxx>
#include <vcl/svapp.hxx>
#include <vcl/vclptr.hxx>
#include <vcl/window.hxx>

#include <memory>

using namespace ::com::sun::star;

/// Test opening a dialog in svx
class SvxDialogsTest : public ScreenshotTest
{
private:
    /// helper method to populate KnownDialogs, called in setUp(). Needs to be
    /// written and has to add entries to KnownDialogs
    virtual void registerKnownDialogsByID(mapType& rKnownDialogs) override;

    /// dialog creation for known dialogs by ID. Has to be implemented for
    /// each registered known dialog
    virtual VclPtr<VclAbstractDialog> createDialogByID(sal_uInt32 nID) override;

public:
    SvxDialogsTest();

    // try to open a dialog
    void openAnyDialog();

    // Writer 2027 premium font picker: load the model and render the popup
    void openWriter2027FontPopup();

    // Writer 2027 Type System picker: open/reopen/close over the real font
    // inventory (same code path as the notebookbar Type System button).
    void openWriter2027TypeSystemPopup();

    CPPUNIT_TEST_SUITE(SvxDialogsTest);
    CPPUNIT_TEST(openAnyDialog);
    CPPUNIT_TEST(openWriter2027FontPopup);
    CPPUNIT_TEST(openWriter2027TypeSystemPopup);
    CPPUNIT_TEST_SUITE_END();
};

SvxDialogsTest::SvxDialogsTest() {}

void SvxDialogsTest::registerKnownDialogsByID(mapType& /*rKnownDialogs*/)
{
    // fill map of known dialogs
}

VclPtr<VclAbstractDialog> SvxDialogsTest::createDialogByID(sal_uInt32 /*nID*/) { return nullptr; }

void SvxDialogsTest::openAnyDialog()
{
    /// process input file containing the UXMLDescriptions of the dialogs to dump
    processDialogBatchFile(u"svx/qa/unit/data/svx-dialogs-test.txt");
}

// Deterministic, no-user reproduction of the Writer 2027 font-picker popup
// open path (font dropdown click). Mirrors the loader used by the font-name
// control when no document holds the font list (FontList from the default
// device), then opens the popup over the real installed-font inventory in
// grouped and search views and forces a redraw. A crash here is the same
// load/render path that faulted in the notebookbar on a font-dropdown click.
void SvxDialogsTest::openWriter2027FontPopup()
{
    // The real installed-font inventory (what the dropdown loads).
    std::unique_ptr<FontList> pFontList(new FontList(Application::GetDefaultDevice()));
    CPPUNIT_ASSERT(pFontList);

    svx::writer2027::FontPickerModel aModel;
    svx::writer2027::Writer2027FontPopup aPopup(aModel);

    // Top-level vcl::Window used only as the popup's geometry anchor.
    VclPtr<vcl::Window> xAnchor = VclPtrInstance<vcl::Window>(nullptr);
    CPPUNIT_ASSERT(xAnchor);

    // Root grouped view (the default dropdown open).
    aPopup.Open(pFontList.get(), OUString(), OUString(), *xAnchor);
    CPPUNIT_ASSERT(aPopup.IsOpen());

    // Search view (typing into the picker).
    aPopup.Open(pFontList.get(), OUString(), u"sans"_ustr, *xAnchor);

    // Reflect a font change then close.
    aPopup.SetCurrentFamily(pFontList.get(), OUString());
    aPopup.Close();
    CPPUNIT_ASSERT(!aPopup.IsOpen());

    // Reopen the root view and twice more to force full Rebuild + repaint of
    // the grouped inventory over the same FontList.
    aPopup.Open(pFontList.get(), OUString(), OUString(), *xAnchor);
    aPopup.Close();
    aPopup.Open(pFontList.get(), OUString(), OUString(), *xAnchor);
    aPopup.Close();

    xAnchor.disposeAndClear();
}

// Deterministic, no-user reproduction of the Writer 2027 Type System picker
// open path (notebookbar "Type System" button click). Loads the real
// installed-font inventory, opens the popup with and without a detected
// preset (the empty case inserts the inert "Custom" row, exercising the
// row-index translation between maRowIds and maResolved), re-opens while
// already open (the re-entrancy guard), and forces repeated Rebuild +
// repaint cycles. A crash here is the same load/render path that faulted on
// a Type System button click in the notebookbar.
void SvxDialogsTest::openWriter2027TypeSystemPopup()
{
    // The real installed-font inventory (what the popup resolves against).
    std::unique_ptr<FontList> pFontList(new FontList(Application::GetDefaultDevice()));
    CPPUNIT_ASSERT(pFontList);

    svx::writer2027::Writer2027TypeSystemPopup aPopup;

    // Top-level vcl::Window used only as the popup's geometry anchor.
    VclPtr<vcl::Window> xAnchor = VclPtrInstance<vcl::Window>(nullptr);
    CPPUNIT_ASSERT(xAnchor);

    // Open with no detected preset: the inert "Custom" row leads the list and
    // every preset card renders (heading/body samples, pairing, missing-font
    // block) over the real font inventory.
    aPopup.Open(pFontList.get(), OUString(), *xAnchor);
    CPPUNIT_ASSERT(aPopup.IsOpen());

    // Re-entrancy: a second open while already open must not wedge the weld
    // layer (the crash/hang the notebookbar button hit on re-clicks).
    aPopup.Open(pFontList.get(), OUString(), *xAnchor);
    CPPUNIT_ASSERT(aPopup.IsOpen());

    aPopup.Close();
    CPPUNIT_ASSERT(!aPopup.IsOpen());

    // Open with a detected preset id (no Custom row; direct pairing).
    aPopup.Open(pFontList.get(), u"editorial"_ustr, *xAnchor);
    CPPUNIT_ASSERT(aPopup.IsOpen());
    aPopup.Close();
    CPPUNIT_ASSERT(!aPopup.IsOpen());

    // Repeat open/close to force full Rebuild + repaint of the card list.
    aPopup.Open(pFontList.get(), OUString(), *xAnchor);
    aPopup.Close();
    aPopup.Open(pFontList.get(), OUString(), *xAnchor);
    aPopup.Close();

    xAnchor.disposeAndClear();
}

CPPUNIT_TEST_SUITE_REGISTRATION(SvxDialogsTest);

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */

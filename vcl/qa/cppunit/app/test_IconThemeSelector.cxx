/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <IconThemeSelector.hxx>

#include <vcl/DesktopType.hxx>
#include <vcl/IconThemeInfo.hxx>

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/plugin/TestPlugIn.h>

class IconThemeSelectorTest : public CppUnit::TestFixture
{
#ifndef _WIN32 //default theme on Windows is Writer 2027 Phosphor independently from any desktop environment
    void BreezeIsReturnedForKde5Desktop();
    void ElementaryIsReturnedForGnomeDesktop();
    void ThemeIsOverriddenByPreferredTheme();
    void ThemeIsOverriddenByHighContrastMode();
    void NotInstalledThemeDoesNotOverride();
    void InstalledThemeIsFound();
    void FirstThemeIsReturnedIfRequestedThemeIsNotFound();
    void FallbackThemeIsReturnedForEmptyInput();
    void DifferentPreferredThemesAreInequal();
    void DifferentHighContrastModesAreInequal();
    static std::vector<vcl::IconThemeInfo> GetFakeInstalledThemes();
#else
    void Writer2027PhosphorIsReturnedByDefault();
    void ExplicitCarbonPreferenceOverridesDefault();
    void MissingDefaultFallsBackSafely();
    void HighContrastStillWins();
    static std::vector<vcl::IconThemeInfo> GetWriter2027InstalledThemes();
#endif

    // Adds code needed to register the test suite
    CPPUNIT_TEST_SUITE(IconThemeSelectorTest);

#ifndef _WIN32
    CPPUNIT_TEST(BreezeIsReturnedForKde5Desktop);
    CPPUNIT_TEST(ElementaryIsReturnedForGnomeDesktop);
    CPPUNIT_TEST(ThemeIsOverriddenByPreferredTheme);
    CPPUNIT_TEST(ThemeIsOverriddenByHighContrastMode);
    CPPUNIT_TEST(NotInstalledThemeDoesNotOverride);
    CPPUNIT_TEST(InstalledThemeIsFound);
    CPPUNIT_TEST(FirstThemeIsReturnedIfRequestedThemeIsNotFound);
    CPPUNIT_TEST(FallbackThemeIsReturnedForEmptyInput);
    CPPUNIT_TEST(DifferentPreferredThemesAreInequal);
    CPPUNIT_TEST(DifferentHighContrastModesAreInequal);
#else
    CPPUNIT_TEST(Writer2027PhosphorIsReturnedByDefault);
    CPPUNIT_TEST(ExplicitCarbonPreferenceOverridesDefault);
    CPPUNIT_TEST(MissingDefaultFallsBackSafely);
    CPPUNIT_TEST(HighContrastStillWins);
#endif

    // End of test suite definition
    CPPUNIT_TEST_SUITE_END();
};

#ifndef _WIN32

/*static*/ std::vector<vcl::IconThemeInfo>
IconThemeSelectorTest::GetFakeInstalledThemes()
{
    std::vector<vcl::IconThemeInfo> r;
    vcl::IconThemeInfo a;
    a.mThemeId = "breeze";
    r.push_back(a);
    a.mThemeId = "elementary";
    r.push_back(a);
    a.mThemeId = "colibre";
    r.push_back(a);
    a.mThemeId = "sifr";
    r.push_back(a);
    return r;
}

void
IconThemeSelectorTest::BreezeIsReturnedForKde5Desktop()
{
    std::vector<vcl::IconThemeInfo> themes = GetFakeInstalledThemes();
    vcl::IconThemeSelector s;
    OUString r = s.SelectIconThemeForDesktopEnvironment(themes, DesktopType::Plasma5);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("'breeze' theme is returned for Plasma 5 desktop", u"breeze"_ustr, r);
}

void
IconThemeSelectorTest::ElementaryIsReturnedForGnomeDesktop()
{
    std::vector<vcl::IconThemeInfo> themes = GetFakeInstalledThemes();
    vcl::IconThemeSelector s;
    OUString r = s.SelectIconThemeForDesktopEnvironment(themes, DesktopType::GNOME);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("'elementary' theme is returned for gnome desktop", u"elementary"_ustr, r);
}

void
IconThemeSelectorTest::ThemeIsOverriddenByPreferredTheme()
{
    vcl::IconThemeSelector s;
    OUString preferred(u"breeze"_ustr);
    s.SetPreferredIconTheme(preferred, false);
    std::vector<vcl::IconThemeInfo> themes = GetFakeInstalledThemes();
    OUString selected = s.SelectIconThemeForDesktopEnvironment(themes, DesktopType::GNOME);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("'elementary' theme is overridden by breeze", preferred, selected);
}

void
IconThemeSelectorTest::ThemeIsOverriddenByHighContrastMode()
{
    vcl::IconThemeSelector s;
    s.SetUseHighContrastTheme(true);
    std::vector<vcl::IconThemeInfo> themes = GetFakeInstalledThemes();
    OUString selected = s.SelectIconTheme(themes, u"breeze"_ustr);
    bool sifr = selected.startsWith("sifr");
    CPPUNIT_ASSERT_EQUAL_MESSAGE("'breeze' theme is overridden by high contrast mode", true, sifr);
    s.SetUseHighContrastTheme(false);
    selected = s.SelectIconTheme(themes, u"breeze"_ustr);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("'breeze' theme is no longer overridden by high contrast mode",
            u"breeze"_ustr, selected);
}

void
IconThemeSelectorTest::NotInstalledThemeDoesNotOverride()
{
    vcl::IconThemeSelector s;
    s.SetPreferredIconTheme(u"breeze_foo"_ustr, false);
    std::vector<vcl::IconThemeInfo> themes = GetFakeInstalledThemes();
    OUString selected = s.SelectIconTheme(themes, u"colibre"_ustr);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("'colibre' theme is not overridden by 'breeze_foo'", u"colibre"_ustr, selected);
}

void
IconThemeSelectorTest::InstalledThemeIsFound()
{
    vcl::IconThemeSelector s;
    std::vector<vcl::IconThemeInfo> themes = GetFakeInstalledThemes();
    OUString selected = s.SelectIconTheme(themes, u"colibre"_ustr);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("'colibre' theme is found", u"colibre"_ustr, selected);
}

void
IconThemeSelectorTest::FirstThemeIsReturnedIfRequestedThemeIsNotFound()
{
    vcl::IconThemeSelector s;
    std::vector<vcl::IconThemeInfo> themes = GetFakeInstalledThemes();
    OUString selected = s.SelectIconTheme(themes, u"breeze_foo"_ustr);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("'breeze' theme is found", themes.front().GetThemeId(), selected);
}

void
IconThemeSelectorTest::FallbackThemeIsReturnedForEmptyInput()
{
    vcl::IconThemeSelector s;
    OUString selected = s.SelectIconTheme(std::vector<vcl::IconThemeInfo>(), u"colibre"_ustr);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("fallback is returned for empty input",
            vcl::IconThemeSelector::FALLBACK_LIGHT_ICON_THEME_ID, selected);
}

void
IconThemeSelectorTest::DifferentHighContrastModesAreInequal()
{
    vcl::IconThemeSelector s1;
    vcl::IconThemeSelector s2;
    s1.SetUseHighContrastTheme(true);
    s2.SetUseHighContrastTheme(false);
    bool equal = (s1 == s2);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Different high contrast modes are detected as inequal", false, equal);
}

void
IconThemeSelectorTest::DifferentPreferredThemesAreInequal()
{
    vcl::IconThemeSelector s1;
    vcl::IconThemeSelector s2;
    s1.SetPreferredIconTheme(u"breeze"_ustr, false);
    s2.SetUseHighContrastTheme(true);
    bool equal = (s1 == s2);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Different preferred themes are detected as inequal", false, equal);
}

#endif

#ifdef _WIN32

/*static*/ std::vector<vcl::IconThemeInfo>
IconThemeSelectorTest::GetWriter2027InstalledThemes()
{
    std::vector<vcl::IconThemeInfo> r;
    vcl::IconThemeInfo a;
    a.mThemeId = "writer2027_phosphor_svg";
    r.push_back(a);
    a.mThemeId = "writer2027_carbon_svg";
    r.push_back(a);
    a.mThemeId = "colibre";
    r.push_back(a);
    return r;
}

void
IconThemeSelectorTest::Writer2027PhosphorIsReturnedByDefault()
{
    std::vector<vcl::IconThemeInfo> themes = GetWriter2027InstalledThemes();
    vcl::IconThemeSelector s;
    OUString r = s.SelectIconThemeForDesktopEnvironment(themes, DesktopType::Windows);
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "'writer2027_phosphor_svg' theme is the Writer 2027 Windows default",
        u"writer2027_phosphor_svg"_ustr, r);
}

void
IconThemeSelectorTest::ExplicitCarbonPreferenceOverridesDefault()
{
    vcl::IconThemeSelector s;
    s.SetPreferredIconTheme(u"writer2027_carbon_svg"_ustr, true);
    std::vector<vcl::IconThemeInfo> themes = GetWriter2027InstalledThemes();
    OUString selected = s.SelectIconThemeForDesktopEnvironment(themes, DesktopType::Windows);
    CPPUNIT_ASSERT_EQUAL_MESSAGE(
        "explicit 'writer2027_carbon_svg' preference wins over the default",
        u"writer2027_carbon_svg"_ustr, selected);
}

void
IconThemeSelectorTest::MissingDefaultFallsBackSafely()
{
    // Default (writer2027_phosphor_svg) not installed: a safe installed
    // theme must be selected instead (first installed = colibre here).
    std::vector<vcl::IconThemeInfo> themes;
    vcl::IconThemeInfo a;
    a.mThemeId = "colibre";
    themes.push_back(a);
    vcl::IconThemeSelector s;
    OUString selected = s.SelectIconThemeForDesktopEnvironment(themes, DesktopType::Windows);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("falls back to an installed theme",
                                 u"colibre"_ustr, selected);
}

void
IconThemeSelectorTest::HighContrastStillWins()
{
    vcl::IconThemeSelector s;
    s.SetUseHighContrastTheme(true);
    std::vector<vcl::IconThemeInfo> themes = GetWriter2027InstalledThemes();
    // High contrast is resolved through SelectIconTheme() and stays intact:
    // when no high-contrast theme is installed, the requested theme is used.
    OUString selected = s.SelectIconTheme(themes, u"writer2027_phosphor_svg"_ustr);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("high contrast mode does not regress theme selection",
                                 u"writer2027_phosphor_svg"_ustr, selected);
}

#endif

// Put the test suite in the registry
CPPUNIT_TEST_SUITE_REGISTRATION(IconThemeSelectorTest);

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <sal/types.h>
#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/plugin/TestPlugIn.h>

#include <officelabs/RewriteProtocol.hxx>

#include <rtl/ustring.hxx>

#include <string>

using officelabs::buildRewriteRequest;
using officelabs::parseRewriteSuggestion;
using officelabs::sanitizeRewriteSuggestion;

namespace
{
class RewriteProtocolTest : public CppUnit::TestFixture
{
public:
    // GIVEN a selection and a style WHEN a rewrite request is built THEN the
    // body carries mode=rewrite, the style key, and the escaped text.
    void testBuildRequest()
    {
        const OString aBody = buildRewriteRequest(u"Say \"hi\"\nnext"_ustr, u"formal"_ustr);
        CPPUNIT_ASSERT(aBody.indexOf("mode\":\"rewrite\"") >= 0);
        CPPUNIT_ASSERT(aBody.indexOf("style\":\"formal\"") >= 0);
        CPPUNIT_ASSERT(aBody.indexOf("Say \\\"hi\\\"\\nnext") >= 0);
        CPPUNIT_ASSERT(aBody.indexOf("max_suggestions\":1") >= 0);
    }

    // GIVEN a whitespace-only suggestion WHEN sanitized THEN it collapses to
    // empty -- the dialog must not offer blank text to replace a selection.
    void testSanitize_whitespaceOnlyCollapses()
    {
        CPPUNIT_ASSERT(sanitizeRewriteSuggestion(u" \t \n "_ustr).isEmpty());
    }

    // GIVEN a multi-line suggestion WHEN sanitized THEN newlines survive --
    // a rewrite replaces a paragraph, unlike single-line inline completion.
    void testSanitize_keepsNewlines()
    {
        const OUString s = u"First line\nSecond line"_ustr;
        CPPUNIT_ASSERT_EQUAL(s, sanitizeRewriteSuggestion(s));
    }

    // GIVEN a well-formed rewrite reply WHEN parsed THEN suggestions[0].text
    // is returned.
    void testParse_happyPath()
    {
        const std::string aBody
            = R"({"suggestions":[{"text":"A clearer sentence."}]})";
        CPPUNIT_ASSERT_EQUAL(u"A clearer sentence."_ustr,
                             parseRewriteSuggestion(aBody));
    }

    // GIVEN a malformed or empty reply WHEN parsed THEN an empty string is
    // returned.
    void testParse_malformedEmpty()
    {
        CPPUNIT_ASSERT(parseRewriteSuggestion("not json").isEmpty());
        CPPUNIT_ASSERT(parseRewriteSuggestion("{}").isEmpty());
        CPPUNIT_ASSERT(parseRewriteSuggestion("{\"suggestions\":[]}").isEmpty());
        CPPUNIT_ASSERT(parseRewriteSuggestion("{\"suggestions\":[{\"text\":\"  \"}]}").isEmpty());
    }

    CPPUNIT_TEST_SUITE(RewriteProtocolTest);
    CPPUNIT_TEST(testBuildRequest);
    CPPUNIT_TEST(testSanitize_whitespaceOnlyCollapses);
    CPPUNIT_TEST(testSanitize_keepsNewlines);
    CPPUNIT_TEST(testParse_happyPath);
    CPPUNIT_TEST(testParse_malformedEmpty);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_REGISTRATION(RewriteProtocolTest);
} // namespace

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <officelabs/RewriteProtocol.hxx>
#include <officelabs/AgentIdentity.hxx>
#include <officelabs/InlineCompletionEligibility.hxx>

#include <rtl/strbuf.hxx>

#include <cstdio>

namespace officelabs {

namespace {

// Rewrites are paragraph-level LLM work; give the agent a comfortable budget
// (inline completion polls with 3 s -- a rewrite must not starve on the same
// deadline).
const sal_Int32 REWRITE_TIMEOUT_SECONDS = 60;

/// Escape an already-UTF-8-encoded byte string for embedding in the one JSON
/// body we send. Non-ASCII bytes (part of a multi-byte UTF-8 sequence) are
/// passed through unescaped, which is valid inside a JSON string.
/// Duplicated from InlineCompletionEligibility.cxx (that copy is in an
/// anonymous namespace); keep the two escapers in lockstep.
OString jsonEscape(const OString& rValue)
{
    OStringBuffer aOut(rValue.getLength() + 8);
    for (sal_Int32 i = 0; i < rValue.getLength(); ++i)
    {
        const char c = rValue[i];
        switch (c)
        {
            case '"':  aOut.append("\\\""); break;
            case '\\': aOut.append("\\\\"); break;
            case '\n': aOut.append("\\n"); break;
            case '\r': aOut.append("\\r"); break;
            case '\t': aOut.append("\\t"); break;
            default:
                if (static_cast<unsigned char>(c) < 0x20)
                {
                    char aHex[5];
                    std::snprintf(aHex, sizeof(aHex), "%04x",
                                  static_cast<unsigned int>(static_cast<unsigned char>(c)));
                    aOut.append("\\u").append(aHex);
                }
                else
                    aOut.append(c);
        }
    }
    return aOut.makeStringAndClear();
}

} // namespace

OString buildRewriteRequest(const OUString& rText, const OUString& rStyle)
{
    OStringBuffer aBuf;
    aBuf.append("{\"text\":\"").append(jsonEscape(rText.toUtf8()));
    aBuf.append("\",\"style\":\"").append(jsonEscape(rStyle.toUtf8()));
    aBuf.append("\",\"mode\":\"rewrite\",\"max_suggestions\":1}");
    return aBuf.makeStringAndClear();
}

OUString sanitizeRewriteSuggestion(const OUString& rSuggestion)
{
    if (rSuggestion.trim().isEmpty())
        return OUString();
    return rSuggestion;
}

OUString parseRewriteSuggestion(const std::string& rBody)
{
    return sanitizeRewriteSuggestion(parseFirstSuggestion(rBody));
}

AgentResponse fetchRewrite(const OUString& rText, const OUString& rStyle)
{
    return httpRequest("POST", "/completions/", buildRewriteRequest(rText, rStyle),
                       REWRITE_TIMEOUT_SECONDS, readSessionToken(), OString(),
                       { { "X-OfficeLabs-Feature"_ostr, "rewrite"_ostr } });
}

} // namespace officelabs

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
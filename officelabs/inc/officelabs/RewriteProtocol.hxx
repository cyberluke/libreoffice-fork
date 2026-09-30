/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_OFFICELABS_REWRITEPROTOCOL_HXX
#define INCLUDED_OFFICELABS_REWRITEPROTOCOL_HXX

#include <officelabs/officelabsdllapi.h>
#include <officelabs/AgentHttp.hxx>

#include <rtl/string.hxx>
#include <rtl/ustring.hxx>

#include <string>

namespace officelabs {

/// Builds the /completions/ request body for a rewrite of |rText| in style
/// |rStyle| (one of "clarity", "formal", "concise", "simpler", "grammar",
/// "improve"). Same envelope as inline completion
/// (buildCompletionRequest), with mode="rewrite"; the agent picks the
/// rewrite prompt from the style key.
OFFICELABS_DLLPUBLIC OString buildRewriteRequest(const OUString& rText, const OUString& rStyle);

/// Collapses a rewrite reply to empty when it is whitespace-only. Unlike
/// sanitizeSuggestion (inline completion) newlines are preserved: a rewrite
/// replaces a paragraph, it does not complete a line.
OFFICELABS_DLLPUBLIC OUString sanitizeRewriteSuggestion(const OUString& rSuggestion);

/// Parses a rewrite reply with the same shape as inline completion
/// ({"suggestions":[{"text":...}]}) and applies sanitizeRewriteSuggestion.
/// Returns an empty string on malformed/empty replies.
OFFICELABS_DLLPUBLIC OUString parseRewriteSuggestion(const std::string& rBody);

/// One full agent round trip for a rewrite. Blocking (60 s budget); call off
/// the VCL thread, e.g. on a detached worker like the inline-completion
/// fetcher does.
OFFICELABS_DLLPUBLIC AgentResponse fetchRewrite(const OUString& rText, const OUString& rStyle);

} // namespace officelabs

#endif // INCLUDED_OFFICELABS_REWRITEPROTOCOL_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
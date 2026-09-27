/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * OfficeLabs agent identity — the two secrets P0b's local auth is built on.
 *
 * Both live in ~/.officelabs, written 0600 by the agent:
 *
 *   session.token   43 base64url chars, regenerated on EVERY agent start.
 *                   Handed to the WebView, so page content holds it. It
 *                   proves only "this caller is the sidebar of this running
 *                   install" — never attestation.
 *
 *   install.secret  64 hex chars (32 bytes), generated once and persisted.
 *                   Read here and NEVER injected into the WebView or served
 *                   over HTTP. That asymmetry is the whole of D9: a signature
 *                   made with it can only have come from native code, which
 *                   is what lets a consent dialog outside the WebView mean
 *                   something the page cannot fake.
 *
 * Read on demand rather than cached: session.token is stale the moment the
 * agent restarts, which is exactly when a caller needs the new one.
 */

#ifndef INCLUDED_OFFICELABS_AGENTIDENTITY_HXX
#define INCLUDED_OFFICELABS_AGENTIDENTITY_HXX

#include <officelabs/officelabsdllapi.h>
#include <rtl/string.hxx>
#include <string_view>
#include <vector>

namespace officelabs {

/// The current session token, or empty if absent/malformed.
OFFICELABS_DLLPUBLIC OString readSessionToken();

/// The install secret as raw bytes, or empty if absent/malformed.
OFFICELABS_DLLPUBLIC std::vector<unsigned char> readInstallSecret();

/// Lowercase hex HMAC-SHA256, matching Python's hmac.new(key, msg).hexdigest().
OFFICELABS_DLLPUBLIC OString hmacSha256Hex(const std::vector<unsigned char>& rKey,
                                           std::string_view aMessage);

} // namespace officelabs

#endif // INCLUDED_OFFICELABS_AGENTIDENTITY_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */

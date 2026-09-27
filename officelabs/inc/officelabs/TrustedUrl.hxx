/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Which pages OfficeLabs is willing to treat as its own UI.
 *
 * This is a SECURITY boundary, not a convenience. Every `cefQuery` handler --
 * getSessionToken above all -- answers whoever asks, and the renderer injects
 * `cefQuery` into every page it loads. Without a check on WHO is asking, any
 * page that ever reaches an OfficeLabs WebView can obtain the session token and
 * with it the whole non-consent agent surface: macro source read and write,
 * version control, VBA import.
 *
 * The P0b design says the token "removes arbitrary web pages from the caller
 * set". That is only true if navigation is confined and the token handler
 * checks its caller. This header is where both of those live.
 */
#pragma once

#include <rtl/ustring.hxx>

namespace officelabs
{
/** True when |rUrl| is a page OfficeLabs treats as its own UI.
 *
 * Exactly two shapes qualify:
 *   - the bundled bundle, a file:// URL under <instdir>/program/officelabs-ui/
 *   - the dev server, when OFFICELABS_UI_DEV_URL is set, matched on ORIGIN
 *     (scheme + authority) so its own routes qualify but a different host
 *     on the same machine does not.
 *
 * Anything else -- remote pages, about:blank, data: and blob: URLs, any path
 * containing "..", every other file:// path on the disk -- is untrusted.
 */
bool isTrustedUiUrl(const OUString& rUrl);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */

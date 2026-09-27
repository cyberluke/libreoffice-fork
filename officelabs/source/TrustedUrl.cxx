/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * See officelabs/inc/officelabs/TrustedUrl.hxx for why this exists.
 */

#include <officelabs/TrustedUrl.hxx>

#include <osl/file.hxx>
#include <rtl/bootstrap.hxx>
#include <sal/log.hxx>

#include <cstdlib>
#include <cstdio>

namespace officelabs
{
namespace
{
/// Collapse "/segment/.." pairs so a prefix can be compared literally.
///
/// $BRAND_BASE_DIR/$BRAND_SHARE_SUBDIR/.. expands to ".../Contents/Resources/.."
/// and getAbsoluteFileURL keeps that ".." verbatim. CEF reports frame URLs
/// NORMALISED, so comparing the two as strings never matches -- which silently
/// makes every page untrusted, including our own. That is exactly what happened
/// on the first run of this check: the shipping sidebar started 401ing.
OUString collapseDotDot(const OUString& rUrl)
{
    OUString sOut = rUrl;
    for (;;)
    {
        const sal_Int32 nDotDot = sOut.indexOf("/..");
        if (nDotDot <= 0)
            break;
        const sal_Int32 nPrevSlash = sOut.lastIndexOf('/', nDotDot);
        if (nPrevSlash < 0)
            break;
        sOut = sOut.copy(0, nPrevSlash) + sOut.copy(nDotDot + 3);
    }
    return sOut;
}

/// file:// prefix of the bundled UI directory, or empty when it cannot be resolved.
OUString bundledUiPrefix()
{
    OUString sInstDir(u"$BRAND_BASE_DIR/$BRAND_SHARE_SUBDIR/.."_ustr);
    rtl::Bootstrap::expandMacros(sInstDir);

    OUString sFileUrl;
    if (osl::FileBase::getAbsoluteFileURL(OUString(), sInstDir, sFileUrl)
        != osl::FileBase::E_None)
        return OUString();

    sFileUrl = collapseDotDot(sFileUrl);

    if (sFileUrl.endsWith("/"))
        sFileUrl = sFileUrl.copy(0, sFileUrl.getLength() - 1);

    // Mirrors WebViewPanel::getUIUrl(). The trailing slash is load-bearing: it
    // is what stops a sibling directory whose name merely STARTS with
    // "officelabs-ui" from matching the prefix.
    return sFileUrl + "/program/officelabs-ui/";
}

/// The scheme+authority of |rUrl|, e.g. "http://localhost:5173", or empty.
OUString originOf(const OUString& rUrl)
{
    const sal_Int32 nScheme = rUrl.indexOf("://");
    if (nScheme < 0)
        return OUString();

    const sal_Int32 nAuthorityStart = nScheme + 3;
    const sal_Int32 nSlash = rUrl.indexOf('/', nAuthorityStart);
    return nSlash < 0 ? rUrl : rUrl.copy(0, nSlash);
}
}

namespace
{
// A refusal must be OBSERVABLE. SAL_WARN compiles out in builds configured with
// ENABLE_SAL_LOG empty -- which is how this fork is built -- so a security gate
// relying on it rejects silently, and a silent gate is indistinguishable from
// one that is misconfigured and refusing everything. Opt in with
// OFFICELABS_TRUST_LOG=<path>.
void logDecision(const char* what, const OUString& a, const OUString& b)
{
    const char* p = std::getenv("OFFICELABS_TRUST_LOG");
    if (!p || !*p) return;
    std::FILE* f = std::fopen(p, "a");
    if (!f) return;
    std::fprintf(f, "%s\n  url=%s\n  ref=%s\n", what,
                 OUStringToOString(a, RTL_TEXTENCODING_UTF8).getStr(),
                 OUStringToOString(b, RTL_TEXTENCODING_UTF8).getStr());
    std::fclose(f);
}
}

bool isTrustedUiUrl(const OUString& rUrl)
{
    if (rUrl.isEmpty())
        return false;

    // Reject traversal before any prefix comparison. A prefix test on a path
    // containing ".." proves nothing: file://<instdir>/program/officelabs-ui/../../
    // matches the prefix and escapes the directory.
    if (rUrl.indexOf("..") >= 0)
    {
        SAL_WARN("officelabs.cef", "Untrusted URL (contains '..')");
        return false;
    }

    // Dev server, when one is configured. Matched on ORIGIN so its routes and
    // query strings qualify, while a different port on the same machine -- any
    // other local server the user happens to be running -- does not.
    const char* pDevUrl = std::getenv("OFFICELABS_UI_DEV_URL");
    if (pDevUrl && *pDevUrl)
    {
        const OUString sDevOrigin = originOf(OUString::fromUtf8(pDevUrl));
        if (!sDevOrigin.isEmpty() && originOf(rUrl) == sDevOrigin)
            return true;
    }

    const OUString sPrefix = bundledUiPrefix();
    if (sPrefix.isEmpty())
    {
        // Fail CLOSED. If the install directory cannot be resolved we cannot
        // say what is ours, and answering "trusted" to be helpful is how a
        // security check becomes decoration.
        SAL_WARN("officelabs.cef", "Cannot resolve the bundled UI path; treating all URLs as untrusted");
        return false;
    }

    const bool bOk = rUrl.startsWith(sPrefix);
    logDecision(bOk ? "TRUST ok" : "TRUST REJECT", rUrl, sPrefix);
    return bOk;
}
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */

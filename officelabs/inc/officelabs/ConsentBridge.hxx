/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * OfficeLabs consent bridge — the native half of D9 (spec §7.4).
 *
 * The defect this closes: any localhost caller can self-assert
 * `edit_mode="accept_all", force=True`. A consent token minted by the same
 * CEF page that sends the request would change nothing, so the approval has
 * to come from outside the WebView and reach the agent without passing
 * through page content.
 *
 * The flow, and why each hop exists:
 *
 *   1. The page mints a challenge (POST /consent/challenge). That is all it
 *      may do — it never learns the payload and cannot approve anything.
 *   2. The page asks us to run the dialog, by challenge id only.
 *   3. We prove we are native (HMAC over the challenge id with
 *      install.secret, which the page has never seen) and GET the challenge,
 *      so the dialog shows the agent-resolved macro rather than whatever the
 *      page claimed. Capabilities come from the page but only from a fixed
 *      allowlist, and the grant is bounded by them -- so the prompt can be
 *      stricter than the run, never laxer. Fields the agent does not verify at
 *      all, notably document_identity, are not shown.
 *   4. The user answers a VCL dialog outside the WebView.
 *   5. We sign the agent's canonical payload and POST the approval
 *      ourselves. The page sees the resulting one-shot grant id, but could
 *      never have produced it.
 *
 * Timing: the challenge expires in 90 s and the bridge read-timeout kills the
 * UNO subprocess at 110 s, so the dialog auto-denies well before either.
 */

#ifndef INCLUDED_OFFICELABS_CONSENTBRIDGE_HXX
#define INCLUDED_OFFICELABS_CONSENTBRIDGE_HXX

#include <officelabs/officelabsdllapi.h>
#include <rtl/string.hxx>
#include <functional>

namespace officelabs {

struct ConsentOutcome
{
    bool bGranted = false;
    OString sConsentId;   ///< one-shot grant id, when granted
    OString sError;       ///< human-readable reason, when not
};

/**
 * Run the whole handshake for one challenge and report the outcome.
 *
 * Safe to call from the CEF browser UI thread: it returns immediately and
 * does its network and dialog work elsewhere. @p fnDone is invoked exactly
 * once, on an unspecified thread.
 */
OFFICELABS_DLLPUBLIC void requestConsentAsync(const OString& rChallengeId,
                                              std::function<void(ConsentOutcome)> fnDone);

/// Seconds the dialog waits before auto-denying. Exposed for tests.
OFFICELABS_DLLPUBLIC int consentDialogTimeoutSeconds();

} // namespace officelabs

#endif // INCLUDED_OFFICELABS_CONSENTBRIDGE_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */

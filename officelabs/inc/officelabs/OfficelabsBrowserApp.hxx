/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * OfficeLabs CEF browser-process CefApp (macOS message-pump integration)
 *
 * On macOS, CEF cannot own its own UI thread the way it does on Windows
 * (multi_threaded_message_loop does not exist on mac). LibreOffice already
 * owns the NSApplication run loop, so CEF is configured with
 * external_message_pump = true and this CefApp / CefBrowserProcessHandler
 * receives OnScheduleMessagePumpWork() callbacks. Each callback schedules a
 * single CefDoMessageLoopWork() on the AppKit main thread (see
 * MessagePumpMac.mm), which is the same thread as LibreOffice's VCL main
 * thread and CEF's UI thread on mac.
 *
 * This app is only passed to CefInitialize() in the browser process; the
 * renderer/helper processes use OfficelabsRendererApp (cef_subprocess_main).
 *
 * Registered on both platforms so the object files exist, but its
 * message-pump behaviour is only meaningful under external_message_pump,
 * i.e. the macOS build. On Windows the OnScheduleMessagePumpWork() override
 * is never invoked because multi_threaded_message_loop = true there.
 */

#ifndef INCLUDED_OFFICELABS_OFFICELABSBROWSERAPP_HXX
#define INCLUDED_OFFICELABS_OFFICELABSBROWSERAPP_HXX

#ifdef HAVE_FEATURE_CEF

#include <include/cef_app.h>
#include <include/cef_browser_process_handler.h>
#include <include/cef_command_line.h>

namespace officelabs {

// Browser-process application handler. Owns the external message-pump
// integration on macOS via OnScheduleMessagePumpWork(). The coalescing state
// is a file-static in OfficelabsBrowserApp.cxx (there is exactly one
// browser-process app instance for the process lifetime, and the mac dispatch
// shim takes a context-free callback).
class OfficelabsBrowserApp final : public CefApp,
                                   public CefBrowserProcessHandler
{
public:
    OfficelabsBrowserApp() = default;

    // CefApp
    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override
    {
        return this;
    }

    // macOS: skip the OS Keychain for Chromium's os_crypt "Safe Storage" key.
    // Every dev rebuild re-signs the app with a fresh ad-hoc identity, so the
    // Keychain ACL never matches and securityd stalls on consent while a
    // second CEF thread holds the legacy-keychain mutex -> CefInitialize
    // deadlocks on the main thread (observed 2026-07-10: main thread in
    // SecKeychainSearchCopyNext under cef_initialize, second thread in
    // ClientSession::decrypt). The sidebar stores no OS-encrypted secrets, so
    // the mock keychain is correct here, not just expedient.
    void OnBeforeCommandLineProcessing(
        const CefString& process_type,
        CefRefPtr<CefCommandLine> command_line) override
    {
#ifdef MACOSX
        if (process_type.empty())
            command_line->AppendSwitch("use-mock-keychain");
#else
        (void)process_type;
        (void)command_line;
#endif
    }

    // CefBrowserProcessHandler
    // Called by CEF (external_message_pump mode) to request that CEF work be
    // pumped after |delay_ms| milliseconds. Implemented in
    // OfficelabsBrowserApp.cxx; on mac it hops to the AppKit main thread via
    // the C shim in MessagePumpMac.mm. No-op on Windows.
    void OnScheduleMessagePumpWork(int64_t delay_ms) override;

private:
    IMPLEMENT_REFCOUNTING(OfficelabsBrowserApp);
};

// Start/stop the 30Hz CefDoMessageLoopWork heartbeat (mac only; no-op
// elsewhere). Call Start after a successful CefInitialize() and Stop before
// CefShutdown(), both on the main thread.
void StartCefPumpHeartbeat();
void StopCefPumpHeartbeat();

} // namespace officelabs

#endif // HAVE_FEATURE_CEF
#endif // INCLUDED_OFFICELABS_OFFICELABSBROWSERAPP_HXX

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */

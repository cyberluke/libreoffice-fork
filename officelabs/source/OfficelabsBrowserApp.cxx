/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * OfficeLabs CEF browser-process CefApp implementation.
 *
 * See OfficelabsBrowserApp.hxx for the design rationale. The mac message-pump
 * hop is done through a tiny Objective-C++ shim (MessagePumpMac.mm) so that
 * this translation unit stays plain C++ and can be compiled on every platform.
 */

#ifdef HAVE_FEATURE_CEF

#include <officelabs/OfficelabsBrowserApp.hxx>

#include <include/cef_app.h>

#ifdef MACOSX
#include <atomic>

// Implemented in MessagePumpMac.mm. Schedules |fn| to run on the AppKit main
// (NSApplication) thread after |delay_ms| milliseconds via CFRunLoop, which is
// serviced even inside VCL's nested run loop (unlike the libdispatch main
// queue). The heartbeat variant re-arms |fn| at a fixed interval.
extern "C" void officelabs_schedule_pump_on_main(int64_t delay_ms,
                                                 void (*fn)());
extern "C" void officelabs_start_pump_heartbeat(int64_t interval_ms,
                                                void (*fn)());
extern "C" void officelabs_stop_pump_heartbeat();

namespace {

// Coalescing guard. Set true while a pump block is already queued so CEF's
// frequent OnScheduleMessagePumpWork() calls do not enqueue thousands of
// redundant CefDoMessageLoopWork() blocks. There is exactly one
// browser-process app instance, so file-static state is correct here.
std::atomic<bool> g_bPumpScheduled{ false };

// Master enable flag. Set false in StopCefPumpHeartbeat() (which the shutdown
// path calls before CefShutdown). Any CFRunLoop block or timer already queued
// when teardown begins then no-ops instead of calling CefDoMessageLoopWork()
// on a half-torn-down CEF. Atomic so it is safe to clear from whatever thread
// drives shutdown.
std::atomic<bool> g_bPumpEnabled{ true };

// Runs one CEF pump iteration on the AppKit main thread, guarded against both
// post-shutdown execution and reentrancy (CefDoMessageLoopWork() is not
// reentrant, and a common-mode timer/block can fire while an earlier pump
// spins a nested run loop). All pump execution is on the main thread, so a
// plain bool suffices for the reentrancy guard.
void runCefPumpOnce()
{
    if (!g_bPumpEnabled.load(std::memory_order_acquire))
        return;
    static bool bInPump = false;
    if (bInPump)
        return;
    bInPump = true;
    CefDoMessageLoopWork();
    bInPump = false;
}

// On-demand pump. Clears the coalescing guard *before* pumping so that any
// OnScheduleMessagePumpWork() calls issued by CEF while CefDoMessageLoopWork()
// runs will schedule the next pump.
void officelabsPumpOnMain()
{
    g_bPumpScheduled.store(false, std::memory_order_release);
    runCefPumpOnce();
}

// 30Hz heartbeat body (see MessagePumpMac.mm for why a heartbeat exists).
void officelabsHeartbeatPump()
{
    runCefPumpOnce();
}

} // anonymous namespace
#endif // MACOSX

namespace officelabs {

void OfficelabsBrowserApp::OnScheduleMessagePumpWork(int64_t delay_ms)
{
#ifdef MACOSX
    // Do not queue new pumps once shutdown has begun.
    if (!g_bPumpEnabled.load(std::memory_order_acquire))
        return;

    // Cap the requested delay so an urgent 0-delay pump (e.g. the renderer's
    // Mojo handshake tasks) is never starved behind a long delay that the
    // coalescing guard would otherwise swallow. Mirrors cefclient's
    // kMaxTimerDelay = 1000/30 (~33ms).
    constexpr int64_t kMaxTimerDelayMs = 1000 / 30;
    if (delay_ms > kMaxTimerDelayMs)
        delay_ms = kMaxTimerDelayMs;

    // Coalesce: if a pump is already queued, do nothing. The queued block runs
    // CefDoMessageLoopWork(), after which CEF re-schedules if work remains.
    bool bExpected = false;
    if (!g_bPumpScheduled.compare_exchange_strong(bExpected, true,
                                                  std::memory_order_acq_rel))
        return;

    officelabs_schedule_pump_on_main(delay_ms, &officelabsPumpOnMain);
#else
    // Windows uses multi_threaded_message_loop = true, so CEF never calls this.
    (void)delay_ms;
#endif
}

void StartCefPumpHeartbeat()
{
#ifdef MACOSX
    g_bPumpEnabled.store(true, std::memory_order_release);
    officelabs_start_pump_heartbeat(33, &officelabsHeartbeatPump);
#endif
}

void StopCefPumpHeartbeat()
{
#ifdef MACOSX
    // Disable pump execution BEFORE stopping the heartbeat and (by the caller)
    // CefShutdown, so any already-queued CFRunLoop block/timer that fires
    // during teardown no-ops instead of touching a torn-down CEF.
    g_bPumpEnabled.store(false, std::memory_order_release);
    officelabs_stop_pump_heartbeat();
#endif
}

} // namespace officelabs

#endif // HAVE_FEATURE_CEF

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */

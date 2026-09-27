/* -*- Mode: ObjC++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * OfficeLabs CEF message-pump shim for macOS.
 *
 * Bridges CefBrowserProcessHandler::OnScheduleMessagePumpWork() (plain C++ in
 * OfficelabsBrowserApp.cxx) to the AppKit main-thread CFRunLoop. Kept in its
 * own Objective-C++ (.mm) translation unit so that the CefApp implementation
 * stays platform-neutral C++.
 *
 * The scheduled callback |fn| is CefDoMessageLoopWork() (wrapped by a small
 * static in OfficelabsBrowserApp.cxx that clears the coalescing flag first).
 * Scheduling via CFRunLoop (not the libdispatch main queue) guarantees it runs
 * even inside VCL's nested run loop, on the same thread that called
 * CefInitialize() — CEF's UI thread and LibreOffice's VCL main thread are both
 * the AppKit main thread on mac. See the block comment below for why GCD fails.
 */

#ifdef HAVE_FEATURE_CEF
#ifdef MACOSX

#include <CoreFoundation/CoreFoundation.h>
#include <cstdint>

/*
 * WHY CFRunLoop AND NOT GCD (dispatch_get_main_queue):
 *
 * LibreOffice's VCL runs the whole application inside a NESTED run loop:
 * [NSApp run] -> sendEvent: -> handleAppDefinedEvent -> Desktop::Main ->
 * Application::Yield -> nextEventMatchingMask (proof: `sample soffice` main
 * thread stack). CoreFoundation only drains the GCD main queue at the
 * OUTERMOST run-loop invocation, so dispatch_async/dispatch_after blocks
 * queued to the main queue NEVER execute in LibreOffice -> the CEF pump
 * never ran -> browser UI-thread tasks never processed -> helper processes
 * timed out after 15s with no Mojo connection -> white sidebar panel.
 *
 * CFRunLoopPerformBlock + CFRunLoopWakeUp and CFRunLoopTimer registered in
 * kCFRunLoopCommonModes ARE serviced by nested run-loop invocations, so the
 * pump fires regardless of VCL's nesting. Both are documented thread-safe
 * to call from any thread (CEF calls OnScheduleMessagePumpWork from
 * arbitrary threads).
 */
namespace {
// Repeating heartbeat timer. CEF's on-demand OnScheduleMessagePumpWork
// contract proved fragile under LibreOffice's nested run loop (observed:
// exactly one schedule request during CefInitialize, then silence while
// browser-creation work was clearly pending -> renderers starved at 15s).
// A 30Hz CefDoMessageLoopWork heartbeat is the standard embedder fallback;
// an idle pump iteration is near-free.
CFRunLoopTimerRef g_pHeartbeatTimer = nullptr;
}

extern "C" void officelabs_start_pump_heartbeat(int64_t interval_ms,
                                                void (*fn)())
{
    if (fn == nullptr || g_pHeartbeatTimer != nullptr)
        return;

    const double interval = static_cast<double>(interval_ms) / 1000.0;
    g_pHeartbeatTimer = CFRunLoopTimerCreateWithHandler(
        kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + interval, interval,
        0, 0,
        ^(CFRunLoopTimerRef) {
            fn();
        });
    if (g_pHeartbeatTimer == nullptr)
        return;
    CFRunLoopAddTimer(CFRunLoopGetMain(), g_pHeartbeatTimer,
                      kCFRunLoopCommonModes);
}

extern "C" void officelabs_stop_pump_heartbeat()
{
    if (g_pHeartbeatTimer == nullptr)
        return;
    CFRunLoopTimerInvalidate(g_pHeartbeatTimer);
    CFRelease(g_pHeartbeatTimer);
    g_pHeartbeatTimer = nullptr;
}

extern "C" void officelabs_schedule_pump_on_main(int64_t delay_ms,
                                                 void (*fn)())
{
    if (fn == nullptr)
        return;

    CFRunLoopRef mainLoop = CFRunLoopGetMain();

    if (delay_ms <= 0)
    {
        // Pump on the next main run-loop pass (nested-loop safe).
        CFRunLoopPerformBlock(mainLoop, kCFRunLoopCommonModes, ^{
            fn();
        });
        CFRunLoopWakeUp(mainLoop);
    }
    else
    {
        // One-shot timer (interval 0 auto-invalidates after firing); the
        // run loop retains it, so release our reference after adding.
        const CFAbsoluteTime fireAt =
            CFAbsoluteTimeGetCurrent() + static_cast<double>(delay_ms) / 1000.0;
        CFRunLoopTimerRef timer = CFRunLoopTimerCreateWithHandler(
            kCFAllocatorDefault, fireAt, 0, 0, 0,
            ^(CFRunLoopTimerRef) {
                fn();
            });
        if (timer == nullptr)
        {
            // Allocation failed; fall back to an immediate perform-block so the
            // pump is not lost entirely.
            CFRunLoopPerformBlock(mainLoop, kCFRunLoopCommonModes, ^{ fn(); });
            CFRunLoopWakeUp(mainLoop);
            return;
        }
        CFRunLoopAddTimer(mainLoop, timer, kCFRunLoopCommonModes);
        CFRelease(timer);
    }
}

#endif // MACOSX
#endif // HAVE_FEATURE_CEF

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */

/* -*- Mode: ObjC++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * macOS support for the Macro Studio window (D25 / ADR-027).
 *
 * Two things, both small, both load-bearing.
 */

#include <officelabs/StudioWindow.hxx>

#if defined(HAVE_FEATURE_CEF)

#import <AppKit/AppKit.h>

/* ---------------------------------------------------------------------------
 * 1. CefAppProtocol.
 *
 * CEF requires the NSApplication to implement -isHandlingSendEvent and
 * -setHandlingSendEvent:. LibreOffice's VCL_NSApplication (vcl/inc/osx/vclnsapp.h)
 * is `NSApplication <NSApplicationDelegate>` and implements neither, while the
 * bundled CEF framework binary does contain the selector -- so a call is an
 * unrecognized-selector CRASH.
 *
 * The D25 spike run of 2026-08-20 established this is not theoretical: the
 * selector WAS called (once, during shutdown). Without these methods the app
 * crashes on quit as soon as a Views window has existed.
 *
 * Supplying them from a category in OUR module is what keeps D25 alive: the
 * ADR's stated reversal trigger is "if the fix requires patching VCL's
 * NSApplication, fall back to a WorkWindow child embed". A category is not a
 * VCL patch, so the trigger does not fire.
 *
 * The flag is a plain static. -setHandlingSendEvent: was never observed being
 * called (sets=0 across every spike run), because that is CefScopedSendingEvent
 * and no CEF-involved nested event loop was ever entered. If one ever is, this
 * records what it was told; what it cannot do is make VCL's own -sendEvent:
 * maintain the flag, which would need swizzling. Documented as a known limit
 * rather than pre-solved: see 05 Specs/2026-08-17-macro-studio-window.md 2.1.4.
 * ------------------------------------------------------------------------- */
static BOOL g_bHandlingSendEvent = NO;

@interface NSApplication (OfficelabsCefAppProtocol)
- (BOOL)isHandlingSendEvent;
- (void)setHandlingSendEvent:(BOOL)handlingSendEvent;
@end

@implementation NSApplication (OfficelabsCefAppProtocol)
- (BOOL)isHandlingSendEvent { return g_bHandlingSendEvent; }
- (void)setHandlingSendEvent:(BOOL)handlingSendEvent
{
    g_bHandlingSendEvent = handlingSendEvent;
}
@end

namespace officelabs
{
/* ---------------------------------------------------------------------------
 * 2. Pumping while waiting for the window to close.
 *
 * On macOS the CEF UI thread IS the AppKit main thread, and closeStudioWindow-
 * AndWait() runs on it. Sleeping there would block the very run loop that has
 * to deliver the close, so the wait has to pump instead.
 * ------------------------------------------------------------------------- */
void studioWindowPumpNativeEvents(double fSeconds)
{
    @autoreleasepool
    {
        [[NSRunLoop currentRunLoop]
            runMode:NSDefaultRunLoopMode
            beforeDate:[NSDate dateWithTimeIntervalSinceNow:fSeconds]];
    }
}
}

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * OfficeLabs INativeCefHost — Windows implementation.
 *
 * This is today's Win32 CEF-host code (custom "OfficeLabsCefHost" window
 * class, owned WS_POPUP window, LO-frame subclass for instant move/resize/
 * activate tracking, fullscreen-slideshow guard, F5/Esc keyboard-forwarding
 * hack), moved out of WebViewPanel.cxx behind INativeCefHost (mac port,
 * Decision 5). Behavior is preserved: same window class, same subclass
 * messages handled the same way, same fullscreen-guard/z-order/minimized
 * logic. The one structural change is where the frame-tracking hook's
 * lifetime lives: it is now installed once in create() and removed once in
 * destroy() (tied to the per-frame-persistent host), instead of being
 * added/removed on every panel construct/destruct — the target panel is
 * rebound via setPanel() instead, mirroring how WebViewMessageHandler and
 * WebViewCefClient are already rebound on reattach. This is a disclosed,
 * behavior-preserving simplification (see WebViewPanel.cxx history / PR
 * description for the pre-refactor single-file version).
 */

#ifdef HAVE_FEATURE_CEF
#ifdef _WIN32

#include <officelabs/INativeCefHost.hxx>
#include <officelabs/WebViewPanel.hxx>

#include <sal/log.hxx>

#include <prewin.h>
#include <windows.h>
#include <postwin.h>
#include <commctrl.h> // SetWindowSubclass / DefSubclassProc / RemoveWindowSubclass

#include <include/cef_browser.h>

namespace officelabs {

namespace {

constexpr UINT_PTR CEFHOST_SUBCLASS_ID = 0x4F4C4345; // "OLCE"

// WndProc for the "OfficeLabsCefHost" window class.
// Unlike "Static", this class has no background brush and suppresses
// WM_ERASEBKGND. This prevents the black flash when Windows invalidates
// the popup during focus transitions between CEF and the document.
LRESULT CALLBACK CefHostWndProc(HWND hWnd, UINT uMsg,
                                WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_ERASEBKGND:
        // Do NOT erase the background. CEF paints the entire client area.
        // Returning 1 tells Windows "I handled it" so it won't paint a
        // default background (which causes the black flash).
        return 1;

    case WM_PAINT:
    {
        // Validate the dirty region without drawing anything.
        // Without BeginPaint/EndPaint, Windows would keep sending WM_PAINT.
        PAINTSTRUCT ps;
        BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_NCHITTEST:
        // The popup should never receive mouse input itself --
        // all input goes to the CEF child HWND inside it.
        return HTTRANSPARENT;

    case WM_ACTIVATE:
        // Suppress activation processing. During OLE in-place activation
        // (charts, equations), Windows sends WM_ACTIVATE to owned popups.
        // If DefWindowProc processes this, CEF interferes with the OLE
        // operation and crashes LibreOffice. Return 0 = "handled".
        return 0;

    case WM_NCACTIVATE:
        // Same rationale: suppress non-client activation visual updates.
        // Return TRUE to accept the state change without visual update.
        return TRUE;

    default:
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
}

} // anonymous namespace

// ============================================================
// WebViewPanelHostWin — INativeCefHost, Windows
// ============================================================
class WebViewPanelHostWin final : public INativeCefHost
{
public:
    explicit WebViewPanelHostWin(WebViewPanel* pPanel)
        : m_pPanel(pPanel)
    {
    }

    ~WebViewPanelHostWin() override
    {
        destroyInternal();
    }

    NativeWindowHandle create(NativeParent frameParent) override
    {
        m_hFrameWnd = static_cast<HWND>(frameParent.handle);
        if (!m_hFrameWnd)
            return nullptr;

        registerCefHostClass();

        // Create an OWNED popup window. Owner = m_hFrameWnd: popup is bound
        // to this specific document frame. When m_hFrameWnd is destroyed,
        // Windows auto-destroys this popup. Initial rect is (0,0,0,0); the
        // caller (WebViewPanel::initCefBrowser) immediately follows up with
        // setBounds().
        m_hPopup = CreateWindowExW(
            WS_EX_TOOLWINDOW,
            L"OfficeLabsCefHost", L"",
            WS_POPUP | WS_CLIPCHILDREN,
            0, 0, 0, 0,
            m_hFrameWnd, nullptr, GetModuleHandle(nullptr), nullptr);

        if (!m_hPopup)
        {
            SAL_WARN("officelabs.cef", "WebViewPanelHostWin::create: CreateWindowExW failed");
            return nullptr;
        }

        // Subclass the frame HWND for instant move/resize/activate
        // notifications. Installed once here and removed once in destroy()
        // -- the host is per-frame persistent, so the subclass lives as
        // long as the frame does, independent of panel destroy/recreate.
        if (SetWindowSubclass(m_hFrameWnd, FrameSubclassProc,
                              CEFHOST_SUBCLASS_ID,
                              reinterpret_cast<DWORD_PTR>(this)))
        {
            SAL_INFO("officelabs.cef", "Frame HWND subclassed for instant tracking");
        }
        else
        {
            SAL_WARN("officelabs.cef",
                     "SetWindowSubclass failed, falling back to timer-only tracking");
        }

        return static_cast<NativeWindowHandle>(m_hPopup);
    }

    void destroy() override
    {
        destroyInternal();
    }

    void setBounds(int x, int y, int w, int h) override
    {
        applyNativeSync(/*bApplyBounds=*/true, x, y, w, h);
    }

    void show() override
    {
        if (m_hPopup)
            ShowWindow(m_hPopup, SW_SHOWNA);
    }

    void hide() override
    {
        if (m_hPopup)
            ShowWindow(m_hPopup, SW_HIDE);
    }

    void raiseAbove() override
    {
        if (m_hPopup)
            SetWindowPos(m_hPopup, HWND_TOP,
                         0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    void keepBelowFullscreen() override
    {
        if (isOtherWindowFullscreen())
            hide();
    }

    void onFrameMoved() override
    {
        // WM_WINDOWPOSCHANGED / WM_EXITSIZEMOVE / WM_EXITMENULOOP: ask the
        // panel to recompute VCL geometry and re-sync (mirrors the
        // pre-refactor "if (pPanel) pPanel->syncCefWindowSize();").
        if (m_pPanel)
            m_pPanel->syncCefWindowSize();
    }

    void onFrameResized() override
    {
        onFrameMoved();
    }

    void onFrameActivated(bool active) override
    {
        // WM_ACTIVATE: re-sync ALL live panels immediately so the
        // transition is instant instead of waiting for each panel's timer.
        WebViewPanel::broadcastSyncToAllPanels();

        if (active)
            raiseAbove();
    }

    void forwardKeyEvent(int vkCode, bool bShift) override
    {
        if (!m_hFrameWnd || !IsWindow(m_hFrameWnd))
            return;

        // Build WPARAM/LPARAM for WM_KEYDOWN as Windows would. Bit 29
        // (context code) = 0 for WM_KEYDOWN. Other extended-key and repeat
        // fields are left at 0 -- the accelerator dispatcher only looks at
        // the VK code.
        WPARAM wp = static_cast<WPARAM>(vkCode);
        LPARAM lp = 0;
        if (bShift)
            lp |= (MapVirtualKey(VK_SHIFT, MAPVK_VK_TO_VSC) << 16);

        // Bring the LO frame to foreground so its message loop receives
        // input, then post the key message. PostMessage is non-blocking and
        // safe to call from the CEF UI thread.
        SetForegroundWindow(m_hFrameWnd);
        PostMessage(m_hFrameWnd, WM_KEYDOWN, wp, lp);
    }

    NativeWindowHandle cefParent() const override
    {
        return static_cast<NativeWindowHandle>(m_hPopup);
    }

    void setPanel(WebViewPanel* pPanel) override
    {
        m_pPanel = pPanel;
    }

    void forceFrameRelayout() override
    {
        // Option C: send synthetic WM_SIZE to the frame to force the
        // sidebar deck to re-layout with the correct dimensions once CEF
        // has attached.
        if (!m_hFrameWnd)
            return;

        RECT rc;
        GetClientRect(m_hFrameWnd, &rc);
        SendMessage(m_hFrameWnd, WM_SIZE, SIZE_RESTORED,
                    MAKELPARAM(rc.right - rc.left, rc.bottom - rc.top));
    }

private:
    void destroyInternal()
    {
        if (m_hFrameWnd && IsWindow(m_hFrameWnd))
            RemoveWindowSubclass(m_hFrameWnd, FrameSubclassProc, CEFHOST_SUBCLASS_ID);

        // The popup is an owned window of m_hFrameWnd, so Windows may have
        // already destroyed it (e.g. if the frame itself was destroyed
        // first, see FrameSubclassProc's WM_NCDESTROY case). IsWindow()
        // guards against double-destroying.
        if (m_hPopup && IsWindow(m_hPopup))
            DestroyWindow(m_hPopup);

        m_hPopup = nullptr;
        m_hFrameWnd = nullptr;
    }

    // Detect popup/floating windows by scanning Z-order above us. If a
    // visible WS_POPUP window exists between our frame and the top of
    // Z-order (that isn't our CEF window), a dropdown is open -- don't
    // boost to HWND_TOP so the dropdown stays above us.
    bool isPopupAboveUs() const
    {
        HWND hAbove = GetWindow(m_hPopup, GW_HWNDPREV);
        for (int i = 0; hAbove && i < 20; ++i, hAbove = GetWindow(hAbove, GW_HWNDPREV))
        {
            if (hAbove == m_hFrameWnd)
                break;
            if (!IsWindowVisible(hAbove))
                continue;
            LONG style = GetWindowLong(hAbove, GWL_STYLE);
            if (style & WS_POPUP)
                return true;
        }
        return false;
    }

    // The CEF sidebar popup is a separate top-level window. The z-order
    // logic keeps it on top of our frame, which would also place it OVER a
    // fullscreen presentation (started via F5 / Slide Show), hiding the
    // show. If a fullscreen window other than ours holds the foreground,
    // this returns true so the caller hides the popup. It is restored when
    // the frame regains foreground (Esc / show end), which re-runs the sync
    // via WM_ACTIVATE / onFrameActivated().
    bool isOtherWindowFullscreen() const
    {
        HWND hFg = GetForegroundWindow();
        if (!hFg || hFg == m_hPopup || hFg == m_hFrameWnd)
            return false;

        RECT rcFg{};
        HMONITOR hMon = MonitorFromWindow(hFg, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{};
        mi.cbSize = sizeof(MONITORINFO);
        // A fullscreen slideshow is a BORDERLESS (no WS_CAPTION) top-level
        // window covering the whole monitor. Requiring "no caption" excludes
        // ordinary MAXIMIZED windows (which keep their caption and cover only
        // the work area), so we don't hide the popup for those. A small edge
        // tolerance absorbs DPI/rounding insets.
        constexpr LONG kEdge = 2;
        return GetWindowRect(hFg, &rcFg) && GetMonitorInfo(hMon, &mi)
            && !(GetWindowLong(hFg, GWL_STYLE) & WS_CAPTION)
            && rcFg.left   <= mi.rcMonitor.left   + kEdge
            && rcFg.top    <= mi.rcMonitor.top    + kEdge
            && rcFg.right  >= mi.rcMonitor.right  - kEdge
            && rcFg.bottom >= mi.rcMonitor.bottom - kEdge;
    }

    void applyNativeSync(bool bApplyBounds, int x, int y, int w, int h)
    {
        if (!m_hPopup)
            return;

        // --- Fullscreen-window guard (e.g. Impress slideshow) ---
        if (isOtherWindowFullscreen())
        {
            hide();
            return;
        }

        HWND hFg = GetForegroundWindow();
        // Check if frame is active: direct match OR foreground is a
        // descendant of our frame (handles OLE in-place activation windows
        // like charts).
        bool bFrameActive = (hFg == m_hFrameWnd || hFg == m_hPopup
                             || (m_hFrameWnd && IsChild(m_hFrameWnd, hFg)));
        bool bFrameMinimized = m_hFrameWnd && IsIconic(m_hFrameWnd);
        bool bShouldShow = !bFrameMinimized;
        bool bIsShown = IsWindowVisible(m_hPopup);

        if (bShouldShow && !bIsShown)
            ShowWindow(m_hPopup, SW_SHOWNA);
        else if (!bShouldShow && bIsShown)
            ShowWindow(m_hPopup, SW_HIDE);

        if (!bShouldShow)
            return;

        // --- Z-order ---
        if (bFrameActive && !isPopupAboveUs())
        {
            SetWindowPos(m_hPopup, HWND_TOP,
                         0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }

        if (!bApplyBounds)
            return;

        // --- Position and size ---
        SetWindowPos(m_hPopup, nullptr,
                     x, y, w, h,
                     SWP_NOZORDER | SWP_NOACTIVATE);

        // Resize ALL CEF child windows to match the parent.
        // CEF creates nested HWNDs: Chrome_WidgetWin_1 -> Chrome_RenderWidgetHostHWND.
        // Both must be resized or the viewport stays at the initial (wrong) size.
        HWND hChild = GetWindow(m_hPopup, GW_CHILD);
        while (hChild)
        {
            HWND hNext = GetWindow(hChild, GW_HWNDNEXT);
            if (IsWindow(hChild))
            {
                MoveWindow(hChild, 0, 0, w, h, FALSE);
                HWND hGrandChild = GetWindow(hChild, GW_CHILD);
                while (hGrandChild)
                {
                    HWND hGrandNext = GetWindow(hGrandChild, GW_HWNDNEXT);
                    if (IsWindow(hGrandChild))
                        MoveWindow(hGrandChild, 0, 0, w, h, FALSE);
                    hGrandChild = hGrandNext;
                }
            }
            hChild = hNext;
        }
    }

    static bool registerCefHostClass()
    {
        static bool bRegistered = false;
        if (bRegistered)
            return true;

        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(WNDCLASSEXW);
        wc.style         = 0;
        wc.lpfnWndProc   = CefHostWndProc;
        wc.cbClsExtra    = 0;
        wc.cbWndExtra    = 0;
        wc.hInstance     = GetModuleHandle(nullptr);
        wc.hIcon         = nullptr;
        wc.hIconSm       = nullptr;
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;   // KEY: no background brush = no flash
        wc.lpszMenuName  = nullptr;
        wc.lpszClassName = L"OfficeLabsCefHost";

        if (!RegisterClassExW(&wc))
        {
            DWORD err = GetLastError();
            if (err != ERROR_CLASS_ALREADY_EXISTS)
            {
                SAL_WARN("officelabs.cef",
                         "RegisterClassExW(OfficeLabsCefHost) failed, error=" << err);
                return false;
            }
        }

        bRegistered = true;
        SAL_INFO("officelabs.cef", "Registered OfficeLabsCefHost window class");
        return true;
    }

    // Subclass proc installed on the frame HWND for instant position/
    // activation tracking. dwRefData is the owning WebViewPanelHostWin*.
    static LRESULT CALLBACK FrameSubclassProc(
        HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
        UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
    {
        auto* pHost = reinterpret_cast<WebViewPanelHostWin*>(dwRefData);

        switch (uMsg)
        {
        case WM_WINDOWPOSCHANGED:
        {
            // Fires on every frame move/resize, including during drag.
            // This is the primary fix for the lag: instant position sync.
            LRESULT result = DefSubclassProc(hWnd, uMsg, wParam, lParam);
            if (pHost)
                pHost->onFrameMoved();
            return result;
        }

        case WM_ACTIVATE:
        {
            LRESULT result = DefSubclassProc(hWnd, uMsg, wParam, lParam);
            WORD wActivate = LOWORD(wParam);
            if (pHost && (wActivate == WA_ACTIVE || wActivate == WA_CLICKACTIVE))
                pHost->onFrameActivated(true);
            else if (pHost && wActivate == WA_INACTIVE)
                pHost->onFrameActivated(false);
            return result;
        }

        case WM_NCACTIVATE:
        {
            // When the CEF popup takes focus, the frame gets WM_NCACTIVATE(FALSE).
            // This dims the title bar. We want the frame to still LOOK active
            // when the user is interacting with the sidebar (it's part of LO).
            if (!wParam && pHost && pHost->m_hPopup)
            {
                HWND hFg = GetForegroundWindow();
                if (hFg == pHost->m_hPopup)
                {
                    // Force visual "active" state on the frame
                    return DefWindowProcW(hWnd, WM_NCACTIVATE, TRUE, lParam);
                }
            }
            return DefSubclassProc(hWnd, uMsg, wParam, lParam);
        }

        case WM_ENTERMENULOOP:
        case WM_INITMENUPOPUP:
        {
            // A menu or popup is opening — temporarily push CEF behind so
            // dropdown menus render on top of the AI sidebar.
            if (pHost && pHost->m_hPopup)
            {
                SetWindowPos(pHost->m_hPopup, HWND_BOTTOM,
                             0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            }
            return DefSubclassProc(hWnd, uMsg, wParam, lParam);
        }

        case WM_EXITMENULOOP:
        case WM_UNINITMENUPOPUP:
        {
            // Menu closed — re-sync instead of a raw HWND_TOP raise. The
            // resulting setBounds() restores the popup to top normally, BUT
            // its fullscreen guard keeps it hidden if a slideshow is up
            // (e.g. Slide Show menu -> Start Presentation, where the show
            // window appears right after the menu closes).
            if (pHost)
                pHost->onFrameMoved();
            return DefSubclassProc(hWnd, uMsg, wParam, lParam);
        }

        case WM_ENTERSIZEMOVE:
        {
            if (pHost)
                pHost->m_bInSizeMove = true;
            return DefSubclassProc(hWnd, uMsg, wParam, lParam);
        }

        case WM_EXITSIZEMOVE:
        {
            if (pHost)
            {
                pHost->m_bInSizeMove = false;
                pHost->onFrameMoved();
            }
            return DefSubclassProc(hWnd, uMsg, wParam, lParam);
        }

        case WM_NCDESTROY:
        {
            RemoveWindowSubclass(hWnd, FrameSubclassProc, uIdSubclass);
            if (pHost && pHost->m_pPanel)
            {
                // The LO frame HWND is being destroyed. Remove this frame's
                // entry from the per-frame state map -- this also destroys
                // `pHost` (owned by that map entry's unique_ptr<host>).
                // Safe: nothing below touches pHost/this again. The popup
                // is an owned window of hWnd, so Windows destroys it
                // automatically; destroyInternal()'s IsWindow() guard
                // handles either ordering.
                WebViewPanel::eraseFrameState(static_cast<NativeWindowHandle>(hWnd));
                SAL_INFO("officelabs.cef", "FrameSubclassProc WM_NCDESTROY: "
                         "removed per-frame CEF state for frame " << hWnd);
            }
            return DefSubclassProc(hWnd, uMsg, wParam, lParam);
        }

        default:
            return DefSubclassProc(hWnd, uMsg, wParam, lParam);
        }
    }

    WebViewPanel* m_pPanel = nullptr;
    HWND m_hFrameWnd = nullptr;
    HWND m_hPopup = nullptr;
    bool m_bInSizeMove = false;
};

std::unique_ptr<INativeCefHost> createNativeCefHost(WebViewPanel* pPanel)
{
    return std::make_unique<WebViewPanelHostWin>(pPanel);
}

} // namespace officelabs

#endif // _WIN32
#endif // HAVE_FEATURE_CEF

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */

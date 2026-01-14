


#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif


struct BorderlessState {
    BOOL   enabled = FALSE;
    BOOL   draggable = TRUE;
    int    resize_px = 8;   // border width for resize hit-test
    WNDPROC orig_proc = nullptr;
};

static const wchar_t* kPropName = L"BL_STATE_PTR";

class WindowCfg {
private:
    static inline bool g_borderless_applied = false;
    static inline LONG g_saved_style = 0;
    static inline LONG g_saved_exstyle = 0;
    static inline RECT g_saved_rect{};

public:
    /* ---------------- Window Geometry ---------------- */
    static void ResizeWindow(int width, int height) {
        HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
        if (!hwnd) return;

        RECT r;
        GetWindowRect(hwnd, &r);
        MoveWindow(hwnd, r.left, r.top, width, height, TRUE);
    }

    static void MoveWindowTo(int x, int y) {
        HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
        if (!hwnd) return;

        RECT r;
        GetWindowRect(hwnd, &r);
        MoveWindow(hwnd, x, y, r.right - r.left, r.bottom - r.top, TRUE);
    }

    static void SetWindowGeometry(int x, int y, int w, int h) {
        HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
        if (!hwnd) return;

        RECT r = { x, y, x + w, y + h };
        AdjustWindowRectEx(&r,
            GetWindowLong(hwnd, GWL_STYLE),
            FALSE,
            GetWindowLong(hwnd, GWL_EXSTYLE));

        MoveWindow(hwnd, x - 8, y, r.right - r.left, h + 8, TRUE);
    }


    /* ---------------- Query ---------------- */

    static std::tuple<int, int, int, int> GetWindowRectFn() {
        HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
        RECT r{};
        if (hwnd && GetWindowRect(hwnd, &r))
            return { r.left, r.top, r.right, r.bottom };
        return { 0,0,0,0 };
    }

    static std::tuple<int, int, int, int> GetClientRectFn() {
        HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
        RECT r{};
        if (hwnd && GetClientRect(hwnd, &r))
            return { r.left, r.top, r.right, r.bottom };
        return { 0,0,0,0 };
    }


    /* ---------------- Focus / Z ---------------- */

    static void SetWindowTitle(const std::wstring& title) {
        HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
        if (hwnd) SetWindowTextW(hwnd, title.c_str());
    }

    static void SetWindowActive() {
        HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
        if (!hwnd) return;
        SetForegroundWindow(hwnd);
        SetFocus(hwnd);
        SetActiveWindow(hwnd);
    }

    static void SetAlwaysOnTop(bool enable) {
        HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
        if (!hwnd) return;
        SetWindowPos(hwnd, enable ? HWND_TOPMOST : HWND_NOTOPMOST,
            0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    static bool IsWindowFocused() {
        return GW::MemoryMgr::GetGWWindowHandle() == GetFocus();
    }

    static bool IsWindowActive() {
        return GW::MemoryMgr::GetGWWindowHandle() == GetActiveWindow();
    }

    static bool IsWindowMinimized() {
        HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
        return hwnd && IsIconic(hwnd);
    }

    static bool IsWindowInBackground() {
        HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
        return hwnd && GetActiveWindow() != hwnd;
    }

    static void ApplyFrameChange(HWND hwnd) {
        // Force non-client to recalc
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }


    /* ---------------- Borderless helpers ---------------- */

    static BorderlessState* GetState(HWND h) {
        return reinterpret_cast<BorderlessState*>(GetPropW(h, kPropName));
    }

    static void SetState(HWND h, BorderlessState* s) {
        if (s) SetPropW(h, kPropName, reinterpret_cast<HANDLE>(s));
        else RemovePropW(h, kPropName);
    }

    // Optional: emulate resize/move on NCHITTEST
    static LRESULT HitTestBorderless(HWND h, BorderlessState* st, LPARAM lParam)
    {
        if (!st || !st->draggable) return HTCLIENT;

        // Screen client coords
        POINT pt_screen{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        POINT pt_client = pt_screen;
        ScreenToClient(h, &pt_client);

        RECT rc_client{};
        GetClientRect(h, &rc_client);
        const int w = rc_client.right - rc_client.left;
        const int hgt = rc_client.bottom - rc_client.top;

        const int x = pt_client.x;
        const int y = pt_client.y;
        const int border = (st->resize_px > 0 ? st->resize_px : 8);

        // If maximized, don't offer resize edges
        if (IsZoomed(h)) {
            return HTCAPTION; // drag anywhere
        }

        const bool left = (x >= 0 && x < border);
        const bool right = (x <= w && x > w - border);
        const bool top = (y >= 0 && y < border);
        const bool bottom = (y <= hgt && y > hgt - border);

        if (top && left)     return HTTOPLEFT;
        if (top && right)    return HTTOPRIGHT;
        if (bottom && left)  return HTBOTTOMLEFT;
        if (bottom && right) return HTBOTTOMRIGHT;
        if (top)             return HTTOP;
        if (bottom)          return HTBOTTOM;
        if (left)            return HTLEFT;
        if (right)           return HTRIGHT;

        return HTCAPTION; // drag everywhere else
    }


    static LRESULT CALLBACK BorderlessProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        BorderlessState* st = GetState(hwnd);

        // If state missing or disabled, pass to original immediately
        if (!st || !st->enabled) {
            WNDPROC orig = st ? st->orig_proc : reinterpret_cast<WNDPROC>(GetWindowLongPtr(hwnd, GWLP_WNDPROC));
            return CallWindowProc(orig, hwnd, msg, wParam, lParam);
        }

        switch (msg) {
        case WM_NCCALCSIZE:
            // Return 0 to tell Windows the whole window is client area (no title/borders).
            if (wParam) return 0;
            return 0;

        case WM_NCPAINT:
            // Suppress default non-client painting (title bar, borders).
            return 0;

        case WM_NCACTIVATE:
            // Prevent Windows from drawing inactive/active titlebar transitions.
            return TRUE;

        case WM_STYLECHANGING:
            // Keep borderless look by stripping frame bits if the app tries to add them.
            if (wParam == GWL_STYLE) {
                STYLESTRUCT* ss = reinterpret_cast<STYLESTRUCT*>(lParam);
                ss->styleNew &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_MAXIMIZE | WS_SYSMENU);
            }
            break;

        case WM_NCHITTEST:
            return HitTestBorderless(hwnd, st, lParam);

        case WM_DESTROY:
        case WM_NCDESTROY:
            // Clean up our state when the window is going away
            if (st && st->orig_proc) {
                SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(st->orig_proc));
            }
            SetState(hwnd, nullptr);
            delete st;
            break;
        }
        return CallWindowProc(st->orig_proc, hwnd, msg, wParam, lParam);
    }

    // Public API: enable/disable true borderless
    // draggable=true -> window can be dragged by client area; resize via edges (resize_px).
    // After enabling/disabling, we force a frame change so Windows recalculates immediately.
    static bool EnableTrueBorderless(HWND hwnd, bool enable, bool draggable, int resize_px) {
        if (!IsWindow(hwnd)) return false;

        BorderlessState* st = GetState(hwnd);

        if (enable) {
            if (!st) {
                st = new BorderlessState();
                st->orig_proc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_WNDPROC);
                SetState(hwnd, st);
                SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)BorderlessProc);
            }
            st->enabled = TRUE;
            st->draggable = draggable;
            st->resize_px = resize_px > 0 ? resize_px : 8;
        }
        else if (st) {
            SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)st->orig_proc);
            SetState(hwnd, nullptr);
            delete st;
        }

        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
            SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
        return true;
    }

    static void SetBorderless(bool enable) {
        HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
        if (hwnd) EnableTrueBorderless(hwnd, enable, false, 8);
    }



    /* ---------------- Attention / Visibility ---------------- */

    static void Flash_Window(int count = 1) {
        HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
        if (!hwnd) return;

        FLASHWINFO fw{ sizeof(fw), hwnd, FLASHW_TRAY, (UINT)count, 0 };
        FlashWindowEx(&fw);
    }

    static void RequestAttention() {
        HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
        if (!hwnd) return;

        FLASHWINFO fw{ sizeof(fw), hwnd, FLASHW_TRAY | FLASHW_TIMERNOFG, UINT_MAX, 0 };
        FlashWindowEx(&fw);
    }

    // Get current Z-order index (lower index = closer to top)
    static int GetZOrder() {
        const HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
        if (!hwnd) return -1;

        int z = 0;
        for (HWND h = hwnd; h != NULL; h = GetWindow(h, GW_HWNDPREV)) {
            z++;
        }
        return z;
    }

    // Set explicit Z-order relative to another window
    static void SetZOrder(int insertAfter) {
        const HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
        if (!hwnd) return;

        SetWindowPos(
            hwnd,
            (HWND)insertAfter, // cast int back to HWND
            0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE
        );
    }


    // Push window to bottom of Z-order stack
    static void SendWindowToBack() {
        SetZOrder((int)HWND_BOTTOM);
    }

    static void BringWindowToFront() {
        SetZOrder((int)HWND_TOP);
    }


    // Make the window transparent to mouse clicks (click-through) and/or normal
    static void TransparentClickThrough(bool enable) {
        const HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
        if (!hwnd) return;

        LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
        if (enable) {
            exStyle |= WS_EX_TRANSPARENT | WS_EX_LAYERED;
        }
        else {
            exStyle &= ~WS_EX_TRANSPARENT;
        }
        SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
        // required to apply
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }

    // Adjust window opacity [0–255]
    static void AdjustWindowOpacity(int alpha) {
        auto ALPHA_BYTE = [](int value) -> BYTE {
            if (value < 0) return 0;
            if (value > 255) return 255;
            return static_cast<BYTE>(value);
            };

        const HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
        if (!hwnd) return;

        LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
        if (!(exStyle & WS_EX_LAYERED)) {
            SetWindowLong(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
        }

        BYTE clamped = ALPHA_BYTE(alpha);
        SetLayeredWindowAttributes(hwnd, 0, clamped, LWA_ALPHA);
    }


    static void HideWindow() {
        HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
        if (hwnd) ShowWindow(hwnd, SW_HIDE);
    }

    static void ShowWindowAgain() {
        HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
        if (hwnd) ShowWindow(hwnd, SW_SHOW);
    }

};

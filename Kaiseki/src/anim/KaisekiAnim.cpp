#include "KaisekiAnimApi.h"
#include "KaisekiVersion.h"
#include <cmath>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

const char* KaisekiAnim_GetVersion(void) {
#if defined(_DEBUG)
    return KAISEKI_VERSION_STRING "-debug-v143-mdd";
#else
    return KAISEKI_VERSION_STRING "-release-v143-md";
#endif
}

BOOL KaisekiAnim_InitWindowFrame(HWND hwnd, BOOL enableDwmTransitions, BOOL enableRoundedCorners) {
    if (!hwnd || !IsWindow(hwnd)) return FALSE;

    // 1. Configure DWM Native Hardware Animations (Windows Default Swoosh & Zoom Transitions)
    if (enableDwmTransitions) {
        BOOL disableTransitions = FALSE;
        DwmSetWindowAttribute(hwnd, DWMWA_TRANSITIONS_FORCEDISABLED, &disableTransitions, sizeof(disableTransitions));
    }

    // 2. Configure Windows 11 Fluent Design Hardware Rounded Corners (DWMWCP_ROUND)
    if (enableRoundedCorners) {
        DWM_WINDOW_CORNER_PREFERENCE preference = DWMWCP_ROUND;
        DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &preference, sizeof(preference));

        COLORREF dwmBorder = RGB(52, 54, 68);
        DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &dwmBorder, sizeof(dwmBorder));
    }

    // 3. Extend DWM frame into client area (1px bottom margin signals DWM composition participation)
    MARGINS margins = { 0, 0, 1, 0 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    // 4. Clear any 1-bit GDI regional mask to allow DWM hardware composited rounding & animations
    SetWindowRgn(hwnd, NULL, TRUE);

    // 5. Notify OS frame change
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    return TRUE;
}

LRESULT KaisekiAnim_HandleNcCalcSize(HWND hwnd, WPARAM wParam, LPARAM lParam, BOOL isMaximized) {
    if (!hwnd || !IsWindow(hwnd)) return DefWindowProcW(hwnd, WM_NCCALCSIZE, wParam, lParam);
    if (wParam == TRUE) {
        NCCALCSIZE_PARAMS* pParams = (NCCALCSIZE_PARAMS*)lParam;
        if (isMaximized || IsZoomed(hwnd)) {
            // When maximized, Windows expands the window frame across the border thickness
            // (-8px on Windows 10/11) to hide borders. We clamp rgrc[0] to the monitor's work area,
            // preventing bleeding into secondary monitors or behind the taskbar!
            HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = { sizeof(mi) };
            if (GetMonitorInfoW(hMonitor, &mi)) {
                pParams->rgrc[0] = mi.rcWork;
            }
        }
        // Returning 0 indicates that the client area covers the entire window rectangle.
        // This completely eliminates the legacy white caption titlebar while keeping
        // the window classified as a frame window with full DWM default animations!
        return 0;
    }
    return DefWindowProcW(hwnd, WM_NCCALCSIZE, wParam, lParam);
}

BOOL KaisekiAnim_PerformTransition(HWND hwnd, KaisekiWindowAction action) {
    if (!hwnd || !IsWindow(hwnd)) return FALSE;

    switch (action) {
    case KAISEKI_ACTION_MINIMIZE:
        return PostMessageW(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);

    case KAISEKI_ACTION_MAXIMIZE:
        return PostMessageW(hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);

    case KAISEKI_ACTION_RESTORE:
        return PostMessageW(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);

    case KAISEKI_ACTION_CLOSE:
        return PostMessageW(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0);
    }
    return FALSE;
}

LRESULT KaisekiAnim_HitTestFrame(
    HWND hwnd,
    POINT ptScreen,
    RECT rcCaption,
    RECT rcMin,
    RECT rcMax,
    RECT rcClose,
    int borderThickness,
    BOOL isMaximized
) {
    if (!hwnd || !IsWindow(hwnd)) return HTNOWHERE;
    POINT ptClient = ptScreen;
    ScreenToClient(hwnd, &ptClient);

    RECT rcClient;
    GetClientRect(hwnd, &rcClient);
    int w = rcClient.right - rcClient.left;
    int h = rcClient.bottom - rcClient.top;

    // 1. Resizing borders (only active when floating / not maximized)
    if (!isMaximized && !IsZoomed(hwnd)) {
        int b = (borderThickness > 0) ? borderThickness : 8;
        if (ptClient.y < b) {
            if (ptClient.x < b) return HTTOPLEFT;
            if (ptClient.x > w - b) return HTTOPRIGHT;
            return HTTOP;
        }
        if (ptClient.y > h - b) {
            if (ptClient.x < b) return HTBOTTOMLEFT;
            if (ptClient.x > w - b) return HTBOTTOMRIGHT;
            return HTBOTTOM;
        }
        if (ptClient.x < b) return HTLEFT;
        if (ptClient.x > w - b) return HTRIGHT;
    }

    // 2. Window control buttons (Hit Testing for Native Windows 11 Snap Flyout & Clicks)
    if (PtInRect(&rcClose, ptClient)) {
        return HTCLOSE;
    }
    if (PtInRect(&rcMax, ptClient)) {
        // Returning HTMAXBUTTON on Windows 11 automatically activates the native Snap Layouts flyout on hover!
        return HTMAXBUTTON;
    }
    if (PtInRect(&rcMin, ptClient)) {
        return HTMINBUTTON;
    }

    // 3. Caption Dragging Area
    if (PtInRect(&rcCaption, ptClient)) {
        return HTCAPTION;
    }

    return HTCLIENT;
}

static inline float Clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

float KaisekiAnim_CalculateEase(KaisekiEaseCurve curve, float t) {
    t = Clamp01(t);

    switch (curve) {
    case KAISEKI_EASE_LINEAR:
        return t;

    case KAISEKI_EASE_OUT_QUAD:
        return 1.0f - (1.0f - t) * (1.0f - t);

    case KAISEKI_EASE_OUT_CUBIC: {
        float f = t - 1.0f;
        return f * f * f + 1.0f;
    }

    case KAISEKI_EASE_IN_OUT_CUBIC:
        return (t < 0.5f) ? 4.0f * t * t * t : 1.0f - (float)pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;

    case KAISEKI_EASE_OUT_BACK: {
        const float c1 = 1.70158f;
        const float c3 = c1 + 1.0f;
        float f = t - 1.0f;
        return 1.0f + c3 * f * f * f + c1 * f * f;
    }

    case KAISEKI_EASE_OUT_BOUNCE: {
        const float n1 = 7.5625f;
        const float d1 = 2.75f;
        if (t < 1.0f / d1) {
            return n1 * t * t;
        } else if (t < 2.0f / d1) {
            t -= 1.5f / d1;
            return n1 * t * t + 0.75f;
        } else if (t < 2.5f / d1) {
            t -= 2.25f / d1;
            return n1 * t * t + 0.9375f;
        } else {
            t -= 2.625f / d1;
            return n1 * t * t + 0.984375f;
        }
    }

    case KAISEKI_EASE_SPRING: {
        // Damped harmonic oscillation curve
        return 1.0f - (float)(exp(-6.0f * t) * cos(12.0f * t));
    }
    }
    return t;
}

void KaisekiAnim_SpringStep(float* current, float* velocity, float target, float tension, float damping, float dt) {
    if (!current || !velocity) return;
    if (dt <= 0.0f) dt = 0.016f; // 60 FPS default

    float displacement = *current - target;
    float springForce = -tension * displacement;
    float dampingForce = -damping * (*velocity);
    float acceleration = springForce + dampingForce;

    *velocity += acceleration * dt;
    *current += (*velocity) * dt;
}

#include "ProgressDialog.h"
#include "KaisekiLang.h"
#include "KaisekiTheme.h"
#include "SettingsManager.h"
#include <windowsx.h>
#include <dwmapi.h>
#include <cmath>

#pragma comment(lib, "dwmapi.lib")

KaisekiProgressDialog::KaisekiProgressDialog() {
}

KaisekiProgressDialog::~KaisekiProgressDialog() {
    Close();
}

LRESULT CALLBACK KaisekiProgressDialog::DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    KaisekiProgressDialog* pThis = (KaisekiProgressDialog*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_NCCREATE: {
        CREATESTRUCT* pCS = (CREATESTRUCT*)lParam;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pCS->lpCreateParams);
        return TRUE;
    }
    case WM_TIMER: {
        if (pThis) {
            pThis->m_animSweep += 8.0f;
            if (pThis->m_animSweep > 620.0f) {
                pThis->m_animSweep = -80.0f;
            }

            InvalidateRect(hwnd, NULL, FALSE);
            UpdateWindow(hwnd);
        }
        return 0;
    }
    case WM_NCHITTEST: {
        // Entire top header region (y < 42) is draggable like a modern titlebar
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hwnd, &pt);
        if (pt.y >= 0 && pt.y < 42) {
            return HTCAPTION;
        }
        return HTCLIENT;
    }
    case WM_ERASEBKGND:
        return 1; // Handled in WM_PAINT with double-buffering

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdcWindow = BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        // Double-buffer DC to eliminate any flicker
        HDC hdc = CreateCompatibleDC(hdcWindow);
        HBITMAP hbm = CreateCompatibleBitmap(hdcWindow, w, h);
        HBITMAP hOldBm = (HBITMAP)SelectObject(hdc, hbm);

        // Load active theme colors from SettingsManager
        auto& sett = SettingsManager::Get().settings;
        D2D1_COLOR_F customAccent = KaisekiTheme::ColorFromRgbU8(sett.accentR, sett.accentG, sett.accentB);
        KaisekiTheme::ThemeColors theme = KaisekiTheme::GetTheme((AppTheme)sett.theme, &customAccent, (BackgroundShade)sett.bgShade);
        if (sett.bgShade == (int)BackgroundShade::Custom) {
            D2D1_COLOR_F customBg = D2D1::ColorF(sett.bgR / 255.0f, sett.bgG / 255.0f, sett.bgB / 255.0f, 1.0f);
            KaisekiTheme::ApplyCustomBackgroundColor(theme, customBg);
        }

        auto toColorRef = [](const D2D1_COLOR_F& c) -> COLORREF {
            return RGB((BYTE)(KaisekiTheme::ClampF(c.r, 0.0f, 1.0f) * 255.0f),
                       (BYTE)(KaisekiTheme::ClampF(c.g, 0.0f, 1.0f) * 255.0f),
                       (BYTE)(KaisekiTheme::ClampF(c.b, 0.0f, 1.0f) * 255.0f));
        };

        COLORREF crCanvas = toColorRef(theme.canvasBg);
        COLORREF crHeader = toColorRef(theme.panelBg);
        COLORREF crBorder = toColorRef(theme.borderSubtle);
        COLORREF crAccent = toColorRef(theme.accent);
        COLORREF crWell   = toColorRef(theme.inputWell);
        COLORREF crTextPri = toColorRef(theme.textPrimary);
        COLORREF crTextSec = toColorRef(theme.textSecondary);
        COLORREF crTextMuted = toColorRef(theme.textMuted);
        COLORREF crSheen = RGB(
            (BYTE)(KaisekiTheme::ClampF(theme.accent.r * 1.25f, 0.0f, 1.0f) * 255.0f),
            (BYTE)(KaisekiTheme::ClampF(theme.accent.g * 1.25f, 0.0f, 1.0f) * 255.0f),
            (BYTE)(KaisekiTheme::ClampF(theme.accent.b * 1.25f, 0.0f, 1.0f) * 255.0f)
        );
        COLORREF crGlow = RGB(
            (BYTE)(KaisekiTheme::ClampF(theme.accent.r * 1.4f + 0.2f, 0.0f, 1.0f) * 255.0f),
            (BYTE)(KaisekiTheme::ClampF(theme.accent.g * 1.4f + 0.2f, 0.0f, 1.0f) * 255.0f),
            (BYTE)(KaisekiTheme::ClampF(theme.accent.b * 1.4f + 0.2f, 0.0f, 1.0f) * 255.0f)
        );

        // 1. Canvas Background
        HBRUSH hBg = CreateSolidBrush(crCanvas);
        FillRect(hdc, &rc, hBg);
        DeleteObject(hBg);

        // 2. Custom Title Bar Header
        RECT rcHeader = { 0, 0, w, 42 };
        HBRUSH hHeader = CreateSolidBrush(crHeader);
        FillRect(hdc, &rcHeader, hHeader);
        DeleteObject(hHeader);

        // Header bottom divider line
        RECT rcDiv = { 0, 41, w, 42 };
        HBRUSH hDiv = CreateSolidBrush(crBorder);
        FillRect(hdc, &rcDiv, hDiv);
        DeleteObject(hDiv);

        // Top Accent Stripe (3px)
        RECT rcTopStripe = { 0, 0, w, 3 };
        HBRUSH hAccent = CreateSolidBrush(crAccent);
        FillRect(hdc, &rcTopStripe, hAccent);
        DeleteObject(hAccent);

        SetBkMode(hdc, TRANSPARENT);

        if (pThis) {
            // Draw Icon or Accent Dot in title bar
            if (pThis->m_hIcon) {
                DrawIconEx(hdc, 16, 11, pThis->m_hIcon, 20, 20, 0, NULL, DI_NORMAL);
            } else {
                HBRUSH hDot = CreateSolidBrush(crAccent);
                RECT rcDot = { 16, 16, 26, 26 };
                FillRect(hdc, &rcDot, hDot);
                DeleteObject(hDot);
            }

            // Draw Window Title
            HFONT hOldFont = NULL;
            if (pThis->m_hFontTitle) hOldFont = (HFONT)SelectObject(hdc, pThis->m_hFontTitle);
            SetTextColor(hdc, crTextPri);
            RECT rcTitleText = { 46, 11, w - 16, 33 };
            DrawTextW(hdc, pThis->m_title.c_str(), -1, &rcTitleText, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

            // Draw Status / Operation detail text
            if (pThis->m_hFontBody) SelectObject(hdc, pThis->m_hFontBody);
            SetTextColor(hdc, crTextSec);
            RECT rcStatusText = { 24, 56, w - 90, 78 };
            DrawTextW(hdc, pThis->m_status.c_str(), -1, &rcStatusText, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

            // Draw Percentage Badge
            if (pThis->m_hFontBold) SelectObject(hdc, pThis->m_hFontBold);
            SetTextColor(hdc, crAccent);
            int dispPct = pThis->m_percent;
            if (dispPct < 0) dispPct = 0; if (dispPct > 100) dispPct = 100;
            std::wstring pctStr = std::to_wstring(dispPct) + L"%";
            RECT rcPctText = { w - 85, 54, w - 24, 78 };
            DrawTextW(hdc, pctStr.c_str(), -1, &rcPctText, DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX);

            // 3. SLEEK ROUNDED CAPSULE PROGRESS BAR
            int barX = 24;
            int barY = 88;
            int barW = w - 48;
            int barH = 16;
            int cornerR = barH;

            // Draw capsule track well
            HPEN hTrackPen = CreatePen(PS_SOLID, 1, crBorder);
            HBRUSH hTrackBrush = CreateSolidBrush(crWell);
            HGDIOBJ oldPen = SelectObject(hdc, hTrackPen);
            HGDIOBJ oldBrush = SelectObject(hdc, hTrackBrush);
            RoundRect(hdc, barX, barY, barX + barW, barY + barH, cornerR, cornerR);
            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBrush);
            DeleteObject(hTrackPen);
            DeleteObject(hTrackBrush);

            // Draw Capsule Progress Fill
            int fillW = (int)((float)barW * (float)dispPct / 100.0f);
            if (fillW > 0) {
                if (fillW > barW) fillW = barW;

                HRGN hTrackClip = CreateRoundRectRgn(barX, barY, barX + barW + 1, barY + barH + 1, cornerR, cornerR);
                SelectClipRgn(hdc, hTrackClip);

                // Base Fill in Theme Accent
                HPEN hFillPen = CreatePen(PS_SOLID, 1, crAccent);
                HBRUSH hFillBrush = CreateSolidBrush(crAccent);
                HGDIOBJ oldFP = SelectObject(hdc, hFillPen);
                HGDIOBJ oldFB = SelectObject(hdc, hFillBrush);
                RoundRect(hdc, barX, barY, barX + fillW + ((fillW < cornerR) ? (cornerR - fillW) : 0), barY + barH, cornerR, cornerR);
                SelectObject(hdc, oldFP);
                SelectObject(hdc, oldFB);
                DeleteObject(hFillPen);
                DeleteObject(hFillBrush);

                // Top highlight sheen on fill
                RECT rcSheen = { barX, barY, barX + fillW, barY + (barH / 2) };
                HBRUSH hSheen = CreateSolidBrush(crSheen);
                FillRect(hdc, &rcSheen, hSheen);
                DeleteObject(hSheen);

                // Animated light scanner beam moving across the fill
                int fillSweep = (int)fmod(pThis->m_animSweep * 1.8f, (float)(fillW + 50)) - 30;
                if (fillSweep + 35 > 0 && fillSweep < fillW) {
                    int pLeft = barX + ((fillSweep < 0) ? 0 : fillSweep);
                    int pRight = barX + ((fillSweep + 35 > fillW) ? fillW : (fillSweep + 35));
                    if (pRight > pLeft) {
                        RECT rcSweepFill = { pLeft, barY, pRight, barY + barH };
                        HBRUSH hSweepFill = CreateSolidBrush(crGlow);
                        FillRect(hdc, &rcSweepFill, hSweepFill);
                        DeleteObject(hSweepFill);
                    }
                }

                SelectClipRgn(hdc, NULL);
                DeleteObject(hTrackClip);
            }

            // 4. Outer Rounded Dialog Border
            HPEN hBorderPen = CreatePen(PS_SOLID, 1, crBorder);
            HBRUSH hNullBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
            HGDIOBJ oldBP = SelectObject(hdc, hBorderPen);
            HGDIOBJ oldBB = SelectObject(hdc, hNullBrush);
            RoundRect(hdc, 0, 0, w, h, 20, 20);
            SelectObject(hdc, oldBP);
            SelectObject(hdc, oldBB);
            DeleteObject(hBorderPen);

            if (hOldFont) SelectObject(hdc, hOldFont);
        }

        BitBlt(hdcWindow, 0, 0, w, h, hdc, 0, 0, SRCCOPY);
        SelectObject(hdc, hOldBm);
        DeleteObject(hbm);
        DeleteDC(hdc);

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CLOSE:
        // Modal operation; prevent dismissal during critical parsing
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void KaisekiProgressDialog::Show(HWND hParent, const wchar_t* title, const wchar_t* initialStatus) {
    if (m_hwnd) {
        Update(0, initialStatus);
        return;
    }

    m_hParent = hParent;
    m_title = (title && wcslen(title) > 0) ? title : L"İşlem Yürütülüyor...";
    m_status = (initialStatus && wcslen(initialStatus) > 0) ? initialStatus : L"Lütfen bekleyin...";
    m_percent = 0;
    m_animPercent = 0.0f;
    m_animSweep = -80.0f;
    m_alpha = 0.0f;

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.style = CS_DROPSHADOW | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DialogProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_WAIT);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"KaisekiProgressDialogFramelessClass";
    RegisterClassExW(&wc);

    int dlgW = 520;
    int dlgH = 126;
    int posX = 100, posY = 100;

    if (hParent && IsWindow(hParent)) {
        RECT rcP;
        GetWindowRect(hParent, &rcP);
        posX = rcP.left + (rcP.right - rcP.left - dlgW) / 2;
        posY = rcP.top + (rcP.bottom - rcP.top - dlgH) / 2;
    } else {
        posX = (GetSystemMetrics(SM_CXSCREEN) - dlgW) / 2;
        posY = (GetSystemMetrics(SM_CYSCREEN) - dlgH) / 2;
    }

    m_hIcon = (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1), IMAGE_ICON, 20, 20, LR_DEFAULTCOLOR);
    if (!m_hIcon) {
        m_hIcon = (HICON)LoadImageW(NULL, L"res\\Kaiseki.ico", IMAGE_ICON, 20, 20, LR_LOADFROMFILE);
    }

    m_hFontTitle = CreateFontW(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    m_hFontBody = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    m_hFontBold = CreateFontW(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    m_hFontSmall = CreateFontW(-10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    // Pure Frameless Custom Dark Popup WITHOUT WS_CAPTION to guarantee ZERO white title bar
    // Notice: WS_EX_TOPMOST is removed so this progress dialog stays bound to Kaiseki window
    // and naturally sinks beneath other applications when the user switches tasks/windows.
    m_hwnd = CreateWindowExW(
        WS_EX_LAYERED,
        L"KaisekiProgressDialogFramelessClass",
        m_title.c_str(),
        WS_POPUP | WS_VISIBLE,
        posX, posY, dlgW, dlgH,
        hParent, NULL, GetModuleHandleW(NULL), this
    );

    if (!m_hwnd) return;

    m_alpha = 1.0f;
    SetLayeredWindowAttributes(m_hwnd, 0, 255, LWA_ALPHA);
    SetTimer(m_hwnd, 1, 16, NULL);

    // Apply DWM hardware rounded corners for modern Windows
    DWORD cornerPref = 2; // DWMWCP_ROUND
    DwmSetWindowAttribute(m_hwnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &cornerPref, sizeof(cornerPref));

    // Apply rounded window region matching Kaiseki card radius
    HRGN hRgn = CreateRoundRectRgn(0, 0, dlgW + 1, dlgH + 1, 20, 20);
    SetWindowRgn(m_hwnd, hRgn, TRUE);

    // True Win32 Modal Dialog Behavior: Disable background parent window to prevent any clicks on it!
    if (m_hParent && IsWindow(m_hParent)) {
        EnableWindow(m_hParent, FALSE);
    }

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    PumpMessages();
}

void KaisekiProgressDialog::Update(int percent, const wchar_t* statusText) {
    if (!m_hwnd || !IsWindow(m_hwnd)) return;
    m_percent = (percent < 0) ? 0 : (percent > 100 ? 100 : percent);
    m_animPercent = (float)m_percent;
    if (statusText && wcslen(statusText) > 0) {
        m_status = statusText;
    }
    InvalidateRect(m_hwnd, NULL, FALSE);
    UpdateWindow(m_hwnd);
    PumpMessages();
}

void KaisekiProgressDialog::Close() {
    if (m_hwnd && IsWindow(m_hwnd)) {
        if (m_percent >= 100) {
            m_animPercent = 100.0f;
            InvalidateRect(m_hwnd, NULL, FALSE);
            UpdateWindow(m_hwnd);
            PumpMessages();
            Sleep(80);
        }
        KillTimer(m_hwnd, 1);
        ShowWindow(m_hwnd, SW_HIDE);
        DestroyWindow(m_hwnd);
        m_hwnd = NULL;
    }

    if (m_hFontTitle) { DeleteObject(m_hFontTitle); m_hFontTitle = NULL; }
    if (m_hFontBody) { DeleteObject(m_hFontBody); m_hFontBody = NULL; }
    if (m_hFontBold) { DeleteObject(m_hFontBold); m_hFontBold = NULL; }
    if (m_hFontSmall) { DeleteObject(m_hFontSmall); m_hFontSmall = NULL; }
    if (m_hIcon) { DestroyIcon(m_hIcon); m_hIcon = NULL; }

    // Re-enable parent window and restore foreground focus cleanly
    if (m_hParent && IsWindow(m_hParent)) {
        EnableWindow(m_hParent, TRUE);
        if (IsIconic(m_hParent)) {
            ShowWindow(m_hParent, SW_RESTORE);
        } else {
            ShowWindow(m_hParent, SW_SHOW);
        }
        SetWindowPos(m_hParent, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        SetForegroundWindow(m_hParent);
        SetActiveWindow(m_hParent);
        SetFocus(m_hParent);
        InvalidateRect(m_hParent, NULL, TRUE);
        UpdateWindow(m_hParent);
    }
}

void KaisekiProgressDialog::PumpMessages() {
    if (!m_hwnd || !IsWindow(m_hwnd)) return;
    MSG msg;
    int count = 0;
    // Pumping exclusively for m_hwnd with a cap guarantees 100% responsiveness without message queue starvation
    while (count++ < 15 && PeekMessageW(&msg, m_hwnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

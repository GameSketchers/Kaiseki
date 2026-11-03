#include "ToolInstallerDialog.h"
#include "SettingsManager.h"
#include <windowsx.h>
#include <dwmapi.h>
#include <wininet.h>
#include <shlobj.h>
#include <wincodec.h>
#include <sstream>
#include <fstream>
#include <cmath>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "windowscodecs.lib")

ToolInstallerDialog::ToolInstallerDialog() {
    m_components = {
        {
            L"Android Platform-Tools (ADB & Fastboot)",
            L"Cihaz bağlantısı, logcat ve APK yükleme araçları (~5 MB)",
            L"https://dl.google.com/android/repository/platform-tools-latest-windows.zip",
            L"",
            L"platform-tools\\adb.exe",
            true, false
        },
        {
            L"Android Command-Line Tools (cmdline-tools)",
            L"SDK Manager ve paket yönetim araçları (~140 MB)",
            L"https://dl.google.com/android/repository/commandlinetools-win-11076708_latest.zip",
            L"cmdline-tools\\latest",
            L"cmdline-tools\\bin\\sdkmanager.bat",
            true, false
        },
        {
            L"Android Build-Tools 34.0.0",
            L"apksigner, zipalign ve aapt2 APK imzalama araçları (~60 MB)",
            L"https://dl.google.com/android/repository/build-tools_r34-windows.zip",
            L"build-tools\\34.0.0",
            L"build-tools\\34.0.0\\apksigner.bat",
            true, false
        }
    };
}

ToolInstallerDialog::~ToolInstallerDialog() {
    Close();
}

void ToolInstallerDialog::UpdateComponentStatus() {
    std::wstring dest;
    {
        std::lock_guard<std::mutex> lock(m_textMutex);
        dest = m_destFolder;
    }
    if (dest.empty()) return;
    for (auto& comp : m_components) {
        std::wstring testPath = dest + L"\\" + comp.testFile;
        comp.installed = (GetFileAttributesW(testPath.c_str()) != INVALID_FILE_ATTRIBUTES);
    }
}

void ToolInstallerDialog::ShowModal(HWND hParent) {
    if (m_hwnd) {
        SetForegroundWindow(m_hwnd);
        return;
    }

    m_hParent = hParent;
    {
        std::lock_guard<std::mutex> lock(m_textMutex);
        m_destFolder = SettingsManager::Get().settings.androidSdkPath;
        if (m_destFolder.empty() || GetFileAttributesW(m_destFolder.c_str()) == INVALID_FILE_ATTRIBUTES) {
            wchar_t localApp[MAX_PATH] = { 0 };
            if (GetEnvironmentVariableW(L"LOCALAPPDATA", localApp, MAX_PATH) > 0) {
                m_destFolder = std::wstring(localApp) + L"\\Android\\Sdk";
            } else {
                m_destFolder = L"C:\\Android\\Sdk";
            }
        }
        m_statusText = L"İndirilecek bileşenleri seçip 'İndirmeyi Başlat' butonuna tıklayın.";
        m_detailText = L"";
    }
    UpdateComponentStatus();
    m_percent = 0;
    m_isInstalling = false;
    m_cancelRequested = false;

    HINSTANCE hInst = GetModuleHandleW(NULL);
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = DialogProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"KaisekiToolInstallerClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassExW(&wc);

    int dlgW = 620;
    int dlgH = 460;
    int posX = (GetSystemMetrics(SM_CXSCREEN) - dlgW) / 2;
    int posY = (GetSystemMetrics(SM_CYSCREEN) - dlgH) / 2;

    if (hParent && IsWindow(hParent)) {
        RECT rcP;
        GetWindowRect(hParent, &rcP);
        posX = rcP.left + ((rcP.right - rcP.left) - dlgW) / 2;
        posY = rcP.top + ((rcP.bottom - rcP.top) - dlgH) / 2;
        EnableWindow(hParent, FALSE);
    }

    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST,
        L"KaisekiToolInstallerClass",
        L"Android Araçları — İndirme ve Kurulum",
        WS_POPUP | WS_VISIBLE,
        posX, posY, dlgW, dlgH,
        hParent, NULL, hInst, this
    );

    COLORREF darkBorder = RGB(51, 54, 64);
    DwmSetWindowAttribute(m_hwnd, DWMWA_BORDER_COLOR, &darkBorder, sizeof(darkBorder));
    COLORREF darkCaption = RGB(24, 25, 30);
    DwmSetWindowAttribute(m_hwnd, DWMWA_CAPTION_COLOR, &darkCaption, sizeof(darkCaption));
    COLORREF textWhite = RGB(240, 240, 242);
    DwmSetWindowAttribute(m_hwnd, DWMWA_TEXT_COLOR, &textWhite, sizeof(textWhite));

    int cornerPref = 2; // DWMWCP_ROUND
    DwmSetWindowAttribute(m_hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));

    m_hFontTitle = CreateFontW(17, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    m_hFontBold = CreateFontW(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    m_hFontBody = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    m_hFontSmall = CreateFontW(11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);

    // Modal loop
    MSG msg;
    while (IsWindow(m_hwnd) && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void ToolInstallerDialog::Close() {
    if (m_isInstalling) {
        CancelInstallation();
    }
    if (m_hwnd) {
        HWND h = m_hwnd;
        m_hwnd = NULL;
        DestroyWindow(h);
    }
    if (m_hParent && IsWindow(m_hParent)) {
        EnableWindow(m_hParent, TRUE);
        SetForegroundWindow(m_hParent);
        InvalidateRect(m_hParent, NULL, TRUE);
    }
    if (m_hFontTitle) { DeleteObject(m_hFontTitle); m_hFontTitle = NULL; }
    if (m_hFontBold) { DeleteObject(m_hFontBold); m_hFontBold = NULL; }
    if (m_hFontBody) { DeleteObject(m_hFontBody); m_hFontBody = NULL; }
    if (m_hFontSmall) { DeleteObject(m_hFontSmall); m_hFontSmall = NULL; }
}

void ToolInstallerDialog::CancelInstallation() {
    if (m_isInstalling) {
        m_cancelRequested = true;
        {
            std::lock_guard<std::mutex> lock(m_textMutex);
            m_statusText = L"İptal ediliyor...";
        }
        if (m_hwnd) InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

void ToolInstallerDialog::StartInstallation() {
    if (m_isInstalling) return;
    m_isInstalling = true;
    m_cancelRequested = false;
    m_percent = 0;
    {
        std::lock_guard<std::mutex> lock(m_textMutex);
        m_statusText = L"İndirme işlemi başlatılıyor...";
        m_detailText = L"";
    }

    std::thread([this]() {
        WorkerThreadFunc();
    }).detach();

    if (m_hwnd) InvalidateRect(m_hwnd, NULL, FALSE);
}

void ToolInstallerDialog::WorkerThreadFunc() {
    std::wstring dest;
    {
        std::lock_guard<std::mutex> lock(m_textMutex);
        dest = m_destFolder;
    }
    CreateDirectoryW(dest.c_str(), NULL);

    wchar_t tempDir[MAX_PATH] = { 0 };
    GetTempPathW(MAX_PATH, tempDir);

    int totalSelected = 0;
    for (const auto& comp : m_components) {
        if (comp.selected) totalSelected++;
    }

    if (totalSelected == 0) {
        {
            std::lock_guard<std::mutex> lock(m_textMutex);
            m_statusText = L"Lütfen en az bir bileşen seçin.";
        }
        m_isInstalling = false;
        if (m_hwnd) InvalidateRect(m_hwnd, NULL, FALSE);
        return;
    }

    bool hasErrors = false;
    std::wstring lastError;
    int currentIdx = 0;

    for (size_t i = 0; i < m_components.size(); ++i) {
        auto& comp = m_components[i];
        if (!comp.selected) continue;
        if (m_cancelRequested) break;

        currentIdx++;
        std::wstring tempZip = std::wstring(tempDir) + L"kaiseki_tool_" + std::to_wstring(i) + L".zip";
        DeleteFileW(tempZip.c_str());

        // Download via WinINet
        {
            std::lock_guard<std::mutex> lock(m_textMutex);
            m_statusText = L"[" + std::to_wstring(currentIdx) + L"/" + std::to_wstring(totalSelected) + L"] İndiriliyor: " + comp.name;
            m_detailText = L"Bağlantı kuruluyor: " + comp.url;
        }
        if (m_hwnd) InvalidateRect(m_hwnd, NULL, FALSE);

        HINTERNET hInternet = InternetOpenW(L"KaisekiSDKDownloader/2.5", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        if (hInternet) {
            HINTERNET hUrl = InternetOpenUrlW(hInternet, comp.url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE, 0);
            if (hUrl) {
                DWORD contentLength = 0;
                DWORD lenSize = sizeof(contentLength);
                HttpQueryInfoW(hUrl, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &contentLength, &lenSize, NULL);

                HANDLE hOut = CreateFileW(tempZip.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                if (hOut != INVALID_HANDLE_VALUE) {
                    char buf[8192];
                    DWORD readBytes = 0;
                    DWORD totalDownloaded = 0;
                    DWORD startTime = GetTickCount();

                    while (InternetReadFile(hUrl, buf, sizeof(buf), &readBytes) && readBytes > 0) {
                        if (m_cancelRequested) break;
                        DWORD written = 0;
                        WriteFile(hOut, buf, readBytes, &written, NULL);
                        totalDownloaded += readBytes;

                        DWORD elapsedMs = GetTickCount() - startTime;
                        float speedMb = (elapsedMs > 0) ? (float)totalDownloaded / (elapsedMs * 1024.0f) : 0.0f;
                        float downloadedMb = (float)totalDownloaded / (1024.0f * 1024.0f);
                        float totalMb = (contentLength > 0) ? (float)contentLength / (1024.0f * 1024.0f) : 0.0f;

                        int compPercent = (contentLength > 0) ? (int)((totalDownloaded * 100) / contentLength) : 0;
                        int overallPercent = ((currentIdx - 1) * 100 + compPercent) / totalSelected;
                        m_percent = overallPercent;

                        wchar_t det[256];
                        if (totalMb > 0.0f) {
                            swprintf_s(det, L"%.1f MB / %.1f MB (%.1f MB/s) — %%%d", downloadedMb, totalMb, speedMb, compPercent);
                        } else {
                            swprintf_s(det, L"%.1f MB indirildi (%.1f MB/s)", downloadedMb, speedMb);
                        }
                        {
                            std::lock_guard<std::mutex> lock(m_textMutex);
                            m_detailText = det;
                        }
                        if (m_hwnd) InvalidateRect(m_hwnd, NULL, FALSE);
                    }
                    CloseHandle(hOut);
                }
                InternetCloseHandle(hUrl);
            }
            InternetCloseHandle(hInternet);
        }

        if (m_cancelRequested) {
            DeleteFileW(tempZip.c_str());
            break;
        }

        // Extract via Windows built-in tar.exe
        {
            std::lock_guard<std::mutex> lock(m_textMutex);
            m_statusText = L"[" + std::to_wstring(currentIdx) + L"/" + std::to_wstring(totalSelected) + L"] Arşiv ayıklanıyor: " + comp.name;
            m_detailText = L"Dosyalar " + dest + L" konumuna açılıyor...";
        }
        if (m_hwnd) InvalidateRect(m_hwnd, NULL, FALSE);

        std::wstring outExtractDir = dest;
        if (!comp.relativeDest.empty()) {
            outExtractDir = dest + L"\\" + comp.relativeDest;
            CreateDirectoryW(outExtractDir.c_str(), NULL);
        }

        std::wstring cmdTar = L"tar.exe -xf \"" + tempZip + L"\" -C \"" + outExtractDir + L"\"";
        STARTUPINFOW si = { sizeof(si) };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi = { 0 };

        DWORD exitCode = 1;
        if (CreateProcessW(NULL, &cmdTar[0], NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 60000);
            GetExitCodeProcess(pi.hProcess, &exitCode);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }

        DeleteFileW(tempZip.c_str());

        // Verify extraction
        std::wstring testPath = dest + L"\\" + comp.testFile;
        if (exitCode != 0 || GetFileAttributesW(testPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
            hasErrors = true;
            lastError = comp.name;
        }
    }

    if (m_cancelRequested) {
        std::lock_guard<std::mutex> lock(m_textMutex);
        m_statusText = L"İşlem kullanıcı tarafından iptal edildi.";
        m_detailText = L"";
    } else if (hasErrors) {
        std::lock_guard<std::mutex> lock(m_textMutex);
        m_statusText = L"Bazı araçların kurulumu tamamlanamadı!";
        m_detailText = L"Ayıklama veya doğrulama başarısız: " + lastError;
        UpdateComponentStatus();
    } else {
        m_percent = 100;
        {
            std::lock_guard<std::mutex> lock(m_textMutex);
            m_statusText = L"Kurulum başarıyla tamamlandı!";
            m_detailText = L"Android araçları kullanıma hazır. Yapılandırma güncellendi.";
        }

        // Update SettingsManager paths
        SettingsManager::Get().settings.androidSdkPath = dest;
        SettingsManager::Get().settings.adbPath = SettingsManager::Get().FindAdbPath();
        SettingsManager::Get().settings.buildToolsPath = SettingsManager::Get().FindBuildToolsPath();
        SettingsManager::Get().settings.firstRunCompleted = true;
        SettingsManager::Get().Save();

        UpdateComponentStatus();
    }

    m_isInstalling = false;
    if (m_hwnd) InvalidateRect(m_hwnd, NULL, FALSE);
}

LRESULT CALLBACK ToolInstallerDialog::DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ToolInstallerDialog* pThis = (ToolInstallerDialog*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_NCCREATE: {
        CREATESTRUCT* pCS = (CREATESTRUCT*)lParam;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pCS->lpCreateParams);
        return TRUE;
    }
    case WM_CLOSE: {
        if (pThis) pThis->Close();
        return 0;
    }
    case WM_NCHITTEST: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hwnd, &pt);
        RECT rc;
        GetClientRect(hwnd, &rc);
        if (pt.y >= 0 && pt.y < 44 && pt.x < (rc.right - 46)) return HTCAPTION;
        return HTCLIENT;
    }
    case WM_MOUSEMOVE: {
        if (!pThis) break;
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        bool hover = (x >= pThis->m_rcHeaderClose.left && x <= pThis->m_rcHeaderClose.right &&
                      y >= pThis->m_rcHeaderClose.top && y <= pThis->m_rcHeaderClose.bottom);
        if (hover != pThis->m_hoverClose) {
            pThis->m_hoverClose = hover;
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        break;
    }
    case WM_MOUSELEAVE: {
        if (pThis && pThis->m_hoverClose) {
            pThis->m_hoverClose = false;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        break;
    }
    case WM_LBUTTONDOWN: {
        if (!pThis) break;
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        // Header Close Button
        if (x >= pThis->m_rcHeaderClose.left && x <= pThis->m_rcHeaderClose.right &&
            y >= pThis->m_rcHeaderClose.top && y <= pThis->m_rcHeaderClose.bottom) {
            pThis->Close();
            return 0;
        }

        // Browse SDK Folder
        if (x >= pThis->m_rcBtnBrowse.left && x <= pThis->m_rcBtnBrowse.right &&
            y >= pThis->m_rcBtnBrowse.top && y <= pThis->m_rcBtnBrowse.bottom) {
            if (!pThis->m_isInstalling) {
                BROWSEINFOW bi = { 0 };
                bi.hwndOwner = hwnd;
                bi.lpszTitle = L"Android SDK ve Araçlarının Kurulacağı Klasörü Seçin:";
                bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
                PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
                if (pidl) {
                    wchar_t szPath[MAX_PATH];
                    if (SHGetPathFromIDListW(pidl, szPath)) {
                        {
                            std::lock_guard<std::mutex> lock(pThis->m_textMutex);
                            pThis->m_destFolder = szPath;
                        }
                        pThis->UpdateComponentStatus();
                        InvalidateRect(hwnd, NULL, FALSE);
                    }
                    CoTaskMemFree(pidl);
                }
            }
            return 0;
        }

        // Checkbox toggles
        for (size_t i = 0; i < pThis->m_components.size(); ++i) {
            const RECT& rc = pThis->m_rcCheckbox[i];
            if (x >= rc.left && x <= rc.right && y >= rc.top && y <= rc.bottom) {
                if (!pThis->m_isInstalling) {
                    pThis->m_components[i].selected = !pThis->m_components[i].selected;
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                return 0;
            }
        }

        // Install / Cancel Button
        if (x >= pThis->m_rcBtnInstall.left && x <= pThis->m_rcBtnInstall.right &&
            y >= pThis->m_rcBtnInstall.top && y <= pThis->m_rcBtnInstall.bottom) {
            if (pThis->m_isInstalling) {
                pThis->CancelInstallation();
            } else {
                pThis->StartInstallation();
            }
            return 0;
        }

        // Close Button
        if (x >= pThis->m_rcBtnClose.left && x <= pThis->m_rcBtnClose.right &&
            y >= pThis->m_rcBtnClose.top && y <= pThis->m_rcBtnClose.bottom) {
            pThis->Close();
            return 0;
        }
        break;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdcWin = BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        HDC hdc = CreateCompatibleDC(hdcWin);
        HBITMAP hbm = CreateCompatibleBitmap(hdcWin, w, h);
        HBITMAP hOldBm = (HBITMAP)SelectObject(hdc, hbm);

        if (pThis) {
            pThis->PaintContent(hdc, w, h);
        }

        BitBlt(hdcWin, 0, 0, w, h, hdc, 0, 0, SRCCOPY);
        SelectObject(hdc, hOldBm);
        DeleteObject(hbm);
        DeleteDC(hdc);

        EndPaint(hwnd, &ps);
        return 0;
    }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void ToolInstallerDialog::PaintContent(HDC hdc, int w, int h) {
    auto DrawRoundedCard = [&](HDC hdcTarget, const RECT& r, COLORREF bg, COLORREF border, int radius) {
        HPEN hPen = CreatePen(PS_SOLID, 1, border);
        HBRUSH hBr = CreateSolidBrush(bg);
        HPEN oPen = (HPEN)SelectObject(hdcTarget, hPen);
        HBRUSH oBr = (HBRUSH)SelectObject(hdcTarget, hBr);
        RoundRect(hdcTarget, r.left, r.top, r.right, r.bottom, radius, radius);
        SelectObject(hdcTarget, oPen);
        SelectObject(hdcTarget, oBr);
        DeleteObject(hPen);
        DeleteObject(hBr);
    };

    RECT rc = { 0, 0, w, h };

    // Window Background (#18191E Kaiseki Dark)
    HBRUSH hBg = CreateSolidBrush(RGB(24, 25, 30));
    FillRect(hdc, &rc, hBg);
    DeleteObject(hBg);

    // Modern Sleek Header Bar (#14151A, Height 44px matching Kaiseki)
    RECT rcHeader = { 0, 0, w, 44 };
    HBRUSH hHdr = CreateSolidBrush(RGB(20, 21, 26));
    FillRect(hdc, &rcHeader, hHdr);
    DeleteObject(hHdr);

    // Subtle header bottom divider line (#2B2D38)
    RECT rcDiv = { 0, 43, w, 44 };
    HBRUSH hDiv = CreateSolidBrush(RGB(43, 45, 56));
    FillRect(hdc, &rcDiv, hDiv);
    DeleteObject(hDiv);

    // Header Close Button (w - 46, 0, w, 44)
    m_rcHeaderClose = { w - 46, 0, w, 44 };
    if (m_hoverClose) {
        HBRUSH hBrRed = CreateSolidBrush(RGB(217, 38, 51));
        FillRect(hdc, &m_rcHeaderClose, hBrRed);
        DeleteObject(hBrRed);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        SelectObject(hdc, m_hFontBold);
        DrawTextW(hdc, L"✕", 1, &m_rcHeaderClose, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(140, 145, 160));
        SelectObject(hdc, m_hFontBold);
        DrawTextW(hdc, L"✕", 1, &m_rcHeaderClose, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    SetBkMode(hdc, TRANSPARENT);

    std::wstring destFolder;
    std::wstring statusText;
    std::wstring detailText;
    {
        std::lock_guard<std::mutex> lock(m_textMutex);
        destFolder = m_destFolder;
        statusText = m_statusText;
        detailText = m_detailText;
    }

    // Title & Subtitle in Header Bar
    SelectObject(hdc, m_hFontBold);
    SetTextColor(hdc, RGB(245, 245, 248));
    TextOutW(hdc, 20, 7, L"Android Araçları — İndirme ve Kurulum", 37);

    SelectObject(hdc, m_hFontSmall);
    SetTextColor(hdc, RGB(140, 145, 160));
    TextOutW(hdc, 20, 25, L"Google resmi SDK geliştirici araçları (ADB, Build-Tools, Cmdline-Tools)", 71);

    // 1. Destination Folder Card
    int curY = 56;
    RECT rcCardDest = { 20, curY, w - 20, curY + 66 };
    DrawRoundedCard(hdc, rcCardDest, RGB(30, 32, 40), RGB(48, 52, 66), 8);

    SelectObject(hdc, m_hFontBold);
    SetTextColor(hdc, RGB(255, 128, 184)); // Sakura Pink
    TextOutW(hdc, 34, curY + 10, L"Hedef Kurulum Klasörü (Android SDK):", 36);

    SelectObject(hdc, m_hFontBody);
    SetTextColor(hdc, RGB(225, 228, 235));
    RECT rcPathText = { 34, curY + 32, w - 150, curY + 54 };
    std::wstring maskedPath = SettingsManager::MaskUserPath(destFolder);
    DrawTextW(hdc, maskedPath.c_str(), -1, &rcPathText, DT_LEFT | DT_PATH_ELLIPSIS | DT_VCENTER | DT_SINGLELINE);

    // Browse Button
    m_rcBtnBrowse = { w - 140, curY + 24, w - 34, curY + 54 };
    DrawRoundedCard(hdc, m_rcBtnBrowse, RGB(42, 45, 58), RGB(66, 72, 92), 6);

    SelectObject(hdc, m_hFontBold);
    SetTextColor(hdc, RGB(240, 242, 248));
    DrawTextW(hdc, L"Klasör Seç", -1, &m_rcBtnBrowse, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // 2. Component Selection List
    curY += 76;
    SelectObject(hdc, m_hFontBold);
    SetTextColor(hdc, RGB(170, 175, 190));
    TextOutW(hdc, 22, curY, L"RESMİ GOOGLE ANDROID GELİŞTİRİCİ PAKETLERİ:", 43);

    curY += 22;
    for (size_t i = 0; i < m_components.size(); ++i) {
        const auto& comp = m_components[i];
        RECT rcItem = { 20, curY, w - 20, curY + 46 };
        COLORREF itemBg = comp.selected ? RGB(32, 34, 44) : RGB(25, 26, 33);
        COLORREF itemBorder = comp.selected ? RGB(255, 128, 184) : RGB(44, 47, 60);
        DrawRoundedCard(hdc, rcItem, itemBg, itemBorder, 8);

        // Modern Checkbox Pill
        m_rcCheckbox[i] = { 32, curY + 13, 52, curY + 33 };
        COLORREF cbBg = comp.selected ? RGB(255, 128, 184) : RGB(36, 38, 48);
        COLORREF cbBorder = comp.selected ? RGB(255, 128, 184) : RGB(65, 70, 88);
        DrawRoundedCard(hdc, m_rcCheckbox[i], cbBg, cbBorder, 5);

        if (comp.selected) {
            SelectObject(hdc, m_hFontBold);
            SetTextColor(hdc, RGB(24, 25, 30));
            RECT rcCheck = m_rcCheckbox[i];
            DrawTextW(hdc, L"✓", 1, &rcCheck, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        // Component Name & Description
        SelectObject(hdc, m_hFontBold);
        SetTextColor(hdc, RGB(245, 245, 250));
        TextOutW(hdc, 62, curY + 7, comp.name.c_str(), (int)comp.name.length());

        SelectObject(hdc, m_hFontSmall);
        SetTextColor(hdc, RGB(155, 160, 175));
        TextOutW(hdc, 62, curY + 26, comp.description.c_str(), (int)comp.description.length());

        // Modern Pill Status Badge
        RECT rcBadge = { w - 120, curY + 12, w - 32, curY + 34 };
        if (comp.installed) {
            DrawRoundedCard(hdc, rcBadge, RGB(18, 38, 26), RGB(39, 174, 96), 11);
            SetTextColor(hdc, RGB(46, 204, 113));
            SelectObject(hdc, m_hFontSmall);
            DrawTextW(hdc, L"● Yüklü", -1, &rcBadge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else {
            DrawRoundedCard(hdc, rcBadge, RGB(42, 30, 18), RGB(230, 126, 34), 11);
            SetTextColor(hdc, RGB(243, 156, 18));
            SelectObject(hdc, m_hFontSmall);
            DrawTextW(hdc, L"○ Eksik", -1, &rcBadge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        curY += 52;
    }

    // 3. Progress Bar & Status Section
    curY += 6;
    RECT rcProgTrack = { 20, curY, w - 20, curY + 8 };
    DrawRoundedCard(hdc, rcProgTrack, RGB(38, 40, 50), RGB(38, 40, 50), 6);

    int pVal = m_percent.load();
    if (pVal > 0) {
        int fillW = (int)(((w - 40) * pVal) / 100);
        if (fillW > 0) {
            RECT rcProgFill = { 20, curY, 20 + fillW, curY + 8 };
            DrawRoundedCard(hdc, rcProgFill, RGB(255, 128, 184), RGB(255, 128, 184), 6);
        }
    }

    curY += 15;
    SelectObject(hdc, m_hFontBold);
    SetTextColor(hdc, RGB(242, 242, 245));
    TextOutW(hdc, 22, curY, statusText.c_str(), (int)statusText.length());

    if (!detailText.empty()) {
        curY += 18;
        SelectObject(hdc, m_hFontSmall);
        SetTextColor(hdc, RGB(170, 174, 185));
        TextOutW(hdc, 22, curY, detailText.c_str(), (int)detailText.length());
    }

    // 4. Action Buttons at the Bottom
    int btnY = h - 50;
    m_rcBtnInstall = { w - 290, btnY, w - 120, btnY + 36 };
    bool isInst = m_isInstalling.load();
    COLORREF btnInstBg = isInst ? RGB(217, 38, 51) : RGB(42, 45, 58);
    COLORREF btnInstBorder = isInst ? RGB(217, 38, 51) : RGB(255, 128, 184);
    DrawRoundedCard(hdc, m_rcBtnInstall, btnInstBg, btnInstBorder, 6);

    SelectObject(hdc, m_hFontBold);
    SetTextColor(hdc, isInst ? RGB(255, 255, 255) : RGB(255, 128, 184));
    DrawTextW(hdc, isInst ? L"✕ İptal Et" : L"\x2193 İndirmeyi Başlat", -1, &m_rcBtnInstall, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    m_rcBtnClose = { w - 110, btnY, w - 20, btnY + 36 };
    DrawRoundedCard(hdc, m_rcBtnClose, RGB(38, 40, 52), RGB(60, 64, 80), 6);

    SetTextColor(hdc, RGB(240, 240, 245));
    DrawTextW(hdc, L"Kapat", -1, &m_rcBtnClose, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Subtle 1px outer window border (#333640)
    HPEN hWinBorder = CreatePen(PS_SOLID, 1, RGB(51, 54, 64));
    HPEN hOldP = (HPEN)SelectObject(hdc, hWinBorder);
    HBRUSH hOldB = (HBRUSH)SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, 0, 0, w, h);
    SelectObject(hdc, hOldP);
    SelectObject(hdc, hOldB);
    DeleteObject(hWinBorder);
}

bool ToolInstallerDialog::SaveScreenshot(const wchar_t* outPngPath) {
    if (!outPngPath) return false;
    int w = 620;
    int h = 460;

    HDC hdcScreen = GetDC(NULL);
    HDC hdc = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hbm = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    if (!hbm) {
        DeleteDC(hdc);
        ReleaseDC(NULL, hdcScreen);
        return false;
    }
    HBITMAP hOldBm = (HBITMAP)SelectObject(hdc, hbm);

    bool createdFonts = false;
    if (!m_hFontTitle) {
        m_hFontTitle = CreateFontW(17, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        m_hFontBold = CreateFontW(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        m_hFontBody = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        m_hFontSmall = CreateFontW(11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        createdFonts = true;
    }

    {
        std::lock_guard<std::mutex> lock(m_textMutex);
        if (m_destFolder.empty()) {
            m_destFolder = SettingsManager::Get().settings.androidSdkPath;
            if (m_destFolder.empty() || GetFileAttributesW(m_destFolder.c_str()) == INVALID_FILE_ATTRIBUTES) {
                wchar_t localApp[MAX_PATH] = { 0 };
                if (GetEnvironmentVariableW(L"LOCALAPPDATA", localApp, MAX_PATH) > 0) {
                    m_destFolder = std::wstring(localApp) + L"\\Android\\Sdk";
                } else {
                    m_destFolder = L"C:\\Android\\Sdk";
                }
            }
        }
    }

    UpdateComponentStatus();
    PaintContent(hdc, w, h);

    GdiFlush();

    // Save via WIC
    IWICImagingFactory* pFactory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFactory));
    bool success = false;
    if (SUCCEEDED(hr) && pFactory) {
        IWICBitmap* pWicBmp = nullptr;
        hr = pFactory->CreateBitmapFromHBITMAP(hbm, NULL, WICBitmapIgnoreAlpha, &pWicBmp);
        if (SUCCEEDED(hr) && pWicBmp) {
            IWICStream* pStream = nullptr;
            IWICBitmapEncoder* pEncoder = nullptr;
            IWICBitmapFrameEncode* pFrame = nullptr;
            if (SUCCEEDED(pFactory->CreateStream(&pStream)) &&
                SUCCEEDED(pStream->InitializeFromFilename(outPngPath, GENERIC_WRITE)) &&
                SUCCEEDED(pFactory->CreateEncoder(GUID_ContainerFormatPng, NULL, &pEncoder)) &&
                SUCCEEDED(pEncoder->Initialize(pStream, WICBitmapEncoderNoCache)) &&
                SUCCEEDED(pEncoder->CreateNewFrame(&pFrame, NULL)) &&
                SUCCEEDED(pFrame->Initialize(NULL))) {

                pFrame->SetSize(w, h);
                WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGR;
                pFrame->SetPixelFormat(&fmt);
                if (SUCCEEDED(pFrame->WriteSource(pWicBmp, NULL))) {
                    pFrame->Commit();
                    pEncoder->Commit();
                    success = true;
                }
                pFrame->Release();
            }
            if (pEncoder) pEncoder->Release();
            if (pStream) pStream->Release();
            pWicBmp->Release();
        }
        pFactory->Release();
    }

    SelectObject(hdc, hOldBm);
    DeleteObject(hbm);
    DeleteDC(hdc);
    ReleaseDC(NULL, hdcScreen);

    if (createdFonts) {
        DeleteObject(m_hFontTitle); m_hFontTitle = NULL;
        DeleteObject(m_hFontBold); m_hFontBold = NULL;
        DeleteObject(m_hFontBody); m_hFontBody = NULL;
        DeleteObject(m_hFontSmall); m_hFontSmall = NULL;
    }

    return success;
}

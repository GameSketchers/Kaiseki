#include "SettingsDialog.h"
#include "ToolInstallerDialog.h"
#include "VectorIcons.h"
#include <windowsx.h>
#include <dwmapi.h>
#include <shlobj.h>
#include <cmath>
#include <sstream>
#include <fstream>

#ifdef DrawText
#undef DrawText
#endif

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib")

SettingsDialog::SettingsDialog() {
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&m_pDWriteFactory);
    CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_pWicFactory));

    if (m_pDWriteFactory) {
        m_fonts.Initialize(m_pDWriteFactory);
    }
}

SettingsDialog::~SettingsDialog() {
    DiscardDeviceResources();
    if (m_pWicFactory) { m_pWicFactory->Release(); m_pWicFactory = nullptr; }
    if (m_pDWriteFactory) { m_pDWriteFactory->Release(); m_pDWriteFactory = nullptr; }
    if (m_pD2DFactory) { m_pD2DFactory->Release(); m_pD2DFactory = nullptr; }
}

void SettingsDialog::ApplyTheme() {
    SettingsManager::Get().Load();
    m_sliderR = SettingsManager::Get().settings.accentR;
    m_sliderG = SettingsManager::Get().settings.accentG;
    m_sliderB = SettingsManager::Get().settings.accentB;
    m_currentTheme = (AppTheme)SettingsManager::Get().settings.theme;
    m_currentBgShade = (BackgroundShade)SettingsManager::Get().settings.bgShade;

    m_customAccentColor = KaisekiTheme::ColorFromRgbU8(m_sliderR, m_sliderG, m_sliderB);
    m_customBgColor = D2D1::ColorF(
        SettingsManager::Get().settings.bgR / 255.0f,
        SettingsManager::Get().settings.bgG / 255.0f,
        SettingsManager::Get().settings.bgB / 255.0f,
        1.0f
    );
    m_colors = KaisekiTheme::GetTheme(m_currentTheme, &m_customAccentColor, m_currentBgShade);
    if (m_currentBgShade == BackgroundShade::Custom) {
        KaisekiTheme::ApplyCustomBackgroundColor(m_colors, m_customBgColor);
    }

    CreateBrushes();
    if (m_hwnd && IsWindow(m_hwnd)) {
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

void SettingsDialog::SetCustomAccentColorU8(uint8_t r, uint8_t g, uint8_t b) {
    m_sliderR = r; m_sliderG = g; m_sliderB = b;
    SettingsManager::Get().settings.accentR = r;
    SettingsManager::Get().settings.accentG = g;
    SettingsManager::Get().settings.accentB = b;
    SettingsManager::Get().settings.theme = (int)AppTheme::Custom;
    SettingsManager::Get().Save();
    ApplyTheme();
    CreateBrushes();
}

void SettingsDialog::SetCustomBackgroundColorU8(uint8_t r, uint8_t g, uint8_t b) {
    SettingsManager::Get().settings.bgR = r;
    SettingsManager::Get().settings.bgG = g;
    SettingsManager::Get().settings.bgB = b;
    SettingsManager::Get().settings.bgShade = (int)BackgroundShade::Custom;
    SettingsManager::Get().Save();
    ApplyTheme();
    CreateBrushes();
}

void SettingsDialog::SetBackgroundShade(BackgroundShade shade) {
    SettingsManager::Get().settings.bgShade = (int)shade;
    SettingsManager::Get().Save();
    ApplyTheme();
    CreateBrushes();
}

HRESULT SettingsDialog::CreateDeviceResources() {
    HRESULT hr = S_OK;
    if (!m_pRenderTarget) {
        if (!m_pD2DFactory || !m_hwnd) return E_FAIL;
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

        hr = m_pD2DFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(m_hwnd, size, D2D1_PRESENT_OPTIONS_IMMEDIATELY),
            &m_pRenderTarget
        );
        if (FAILED(hr)) return hr;
    }

    UINT winDpi = GetDpiForWindow(m_hwnd);
    if (winDpi == 0) winDpi = 96;
    m_dpiX = (float)winDpi;
    m_dpiY = (float)winDpi;
    m_dpiScale = m_dpiX / 96.0f;
    m_pRenderTarget->SetDpi(m_dpiX, m_dpiY);

    CreateBrushes();
    LoadAppLogoBitmap();

    return S_OK;
}

void SettingsDialog::LoadAppLogoBitmap() {
    if (!m_pRenderTarget || !m_pWicFactory) return;
    if (m_pAppLogoBitmap) return;

    IWICBitmapDecoder* pDecoder = nullptr;

    HMODULE hMods[] = { GetModuleHandleW(NULL) };
    for (HMODULE hMod : hMods) {
        if (!hMod) continue;
        HRSRC hResInfo = FindResourceW(hMod, MAKEINTRESOURCEW(1002), RT_RCDATA);
        if (hResInfo) {
            HGLOBAL hResData = LoadResource(hMod, hResInfo);
            DWORD dwSize = SizeofResource(hMod, hResInfo);
            if (hResData && dwSize > 0) {
                void* pData = LockResource(hResData);
                if (pData) {
                    IWICStream* pStream = nullptr;
                    if (SUCCEEDED(m_pWicFactory->CreateStream(&pStream))) {
                        if (SUCCEEDED(pStream->InitializeFromMemory(reinterpret_cast<BYTE*>(pData), dwSize))) {
                            m_pWicFactory->CreateDecoderFromStream(pStream, NULL, WICDecodeMetadataCacheOnLoad, &pDecoder);
                        }
                        pStream->Release();
                    }
                }
            }
        }
        if (pDecoder) break;
    }

    // 2. Fallback: Search candidate file paths on disk
    if (!pDecoder) {
        wchar_t exePath[MAX_PATH] = { 0 };
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        wchar_t* lastSlash = wcsrchr(exePath, L'\\');
        std::wstring exeDir = (lastSlash) ? std::wstring(exePath, lastSlash - exePath) : L"";

        std::vector<std::wstring> candidates = {
            exeDir + L"\\res\\logo.png",
            exeDir + L"\\logo.png",
            L"res\\logo.png",
            L"Kaiseki\\res\\logo.png",
            L"..\\Kaiseki\\res\\logo.png",
            L"..\\res\\logo.png",
            L"..\\logo.png",
            L"logo.png"
        };

        for (const auto& path : candidates) {
            DWORD dwAttrib = GetFileAttributesW(path.c_str());
            if (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
                HRESULT hr = m_pWicFactory->CreateDecoderFromFilename(
                    path.c_str(), NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder
                );
                if (SUCCEEDED(hr) && pDecoder) {
                    break;
                }
            }
        }
    }

    if (!pDecoder) return;

    IWICBitmapFrameDecode* pFrame = nullptr;
    HRESULT hr = pDecoder->GetFrame(0, &pFrame);
    if (SUCCEEDED(hr)) {
        IWICFormatConverter* pConverter = nullptr;
        hr = m_pWicFactory->CreateFormatConverter(&pConverter);
        if (SUCCEEDED(hr)) {
            IWICPalette* pPalette = nullptr;
            if (SUCCEEDED(m_pWicFactory->CreatePalette(&pPalette))) {
                if (FAILED(pFrame->CopyPalette(pPalette))) {
                    pPalette->Release();
                    pPalette = nullptr;
                }
            }
            hr = pConverter->Initialize(
                pFrame, GUID_WICPixelFormat32bppPBGRA,
                WICBitmapDitherTypeNone, pPalette, 0.0f,
                pPalette ? WICBitmapPaletteTypeCustom : WICBitmapPaletteTypeMedianCut
            );
            if (pPalette) {
                pPalette->Release();
                pPalette = nullptr;
            }
            if (SUCCEEDED(hr)) {
                hr = m_pRenderTarget->CreateBitmapFromWicBitmap(pConverter, NULL, &m_pAppLogoBitmap);
            }
            pConverter->Release();
        }
        pFrame->Release();
    }
    if (pDecoder) pDecoder->Release();
}

void SettingsDialog::CreateBrushes() {
    if (!m_pRenderTarget) return;
    if (m_bPanel) { m_bPanel->Release(); m_bPanel = nullptr; }
    if (m_bCard) { m_bCard->Release(); m_bCard = nullptr; }
    if (m_bCardHover) { m_bCardHover->Release(); m_bCardHover = nullptr; }
    if (m_bWell) { m_bWell->Release(); m_bWell = nullptr; }
    if (m_bBorderSubtle) { m_bBorderSubtle->Release(); m_bBorderSubtle = nullptr; }
    if (m_bTextPrimary) { m_bTextPrimary->Release(); m_bTextPrimary = nullptr; }
    if (m_bTextSecondary) { m_bTextSecondary->Release(); m_bTextSecondary = nullptr; }
    if (m_bTextMuted) { m_bTextMuted->Release(); m_bTextMuted = nullptr; }
    if (m_bAccent) { m_bAccent->Release(); m_bAccent = nullptr; }
    if (m_bSuccess) { m_bSuccess->Release(); m_bSuccess = nullptr; }
    if (m_bWarning) { m_bWarning->Release(); m_bWarning = nullptr; }
    if (m_bCloseRed) { m_bCloseRed->Release(); m_bCloseRed = nullptr; }
    if (m_bBtnCompile) { m_bBtnCompile->Release(); m_bBtnCompile = nullptr; }

    m_pRenderTarget->CreateSolidColorBrush(m_colors.panelBg, &m_bPanel);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.cardBg, &m_bCard);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.cardHover, &m_bCardHover);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.inputWell, &m_bWell);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.borderSubtle, &m_bBorderSubtle);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.textPrimary, &m_bTextPrimary);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.textSecondary, &m_bTextSecondary);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.textMuted, &m_bTextMuted);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.accent, &m_bAccent);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.syntaxString, &m_bSuccess);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.warning, &m_bWarning);
    m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.95f, 0.20f, 0.25f, 1.0f), &m_bCloseRed);
    m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.85f, 0.25f, 0.35f, 1.0f), &m_bBtnCompile);
}

void SettingsDialog::DiscardDeviceResources() {
    if (m_bBtnCompile) { m_bBtnCompile->Release(); m_bBtnCompile = nullptr; }
    if (m_bCloseRed) { m_bCloseRed->Release(); m_bCloseRed = nullptr; }
    if (m_bWarning) { m_bWarning->Release(); m_bWarning = nullptr; }
    if (m_bSuccess) { m_bSuccess->Release(); m_bSuccess = nullptr; }
    if (m_bAccent) { m_bAccent->Release(); m_bAccent = nullptr; }
    if (m_bTextMuted) { m_bTextMuted->Release(); m_bTextMuted = nullptr; }
    if (m_bTextSecondary) { m_bTextSecondary->Release(); m_bTextSecondary = nullptr; }
    if (m_bTextPrimary) { m_bTextPrimary->Release(); m_bTextPrimary = nullptr; }
    if (m_bBorderSubtle) { m_bBorderSubtle->Release(); m_bBorderSubtle = nullptr; }
    if (m_bWell) { m_bWell->Release(); m_bWell = nullptr; }
    if (m_bCardHover) { m_bCardHover->Release(); m_bCardHover = nullptr; }
    if (m_bCard) { m_bCard->Release(); m_bCard = nullptr; }
    if (m_bPanel) { m_bPanel->Release(); m_bPanel = nullptr; }
    if (m_pAppLogoBitmap) { m_pAppLogoBitmap->Release(); m_pAppLogoBitmap = nullptr; }
    if (m_pRenderTarget) { m_pRenderTarget->Release(); m_pRenderTarget = nullptr; }
}

void SettingsDialog::ShowModal(HWND hParent, int initialTab) {
    if (m_hwnd) {
        SetForegroundWindow(m_hwnd);
        return;
    }

    m_hParent = hParent;
    m_settingsTab = initialTab;

    // Discard any stale device resources before creating new window
    DiscardDeviceResources();

    ApplyTheme();

    HINSTANCE hInst = GetModuleHandleW(NULL);
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEXW) };
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcex.lpfnWndProc = SettingsDialog::DialogProc;
    wcex.hInstance = hInst;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = NULL;
    wcex.lpszClassName = L"KaisekiSettingsWindowClass";
    RegisterClassExW(&wcex);

    UINT sysDpi = (hParent && IsWindow(hParent)) ? GetDpiForWindow(hParent) : GetDpiForSystem();
    if (sysDpi == 0) sysDpi = 96;
    m_dpiX = (float)sysDpi;
    m_dpiY = (float)sysDpi;
    m_dpiScale = m_dpiX / 96.0f;

    int dlgW = (int)((float)m_width * m_dpiScale);
    int dlgH = (int)((float)m_height * m_dpiScale);
    int posX = (GetSystemMetrics(SM_CXSCREEN) - dlgW) / 2;
    int posY = (GetSystemMetrics(SM_CYSCREEN) - dlgH) / 2;

    if (hParent && IsWindow(hParent)) {
        RECT rcP;
        GetWindowRect(hParent, &rcP);
        posX = rcP.left + ((rcP.right - rcP.left) - dlgW) / 2;
        posY = rcP.top + ((rcP.bottom - rcP.top) - dlgH) / 2;
        // DISABLE PARENT WINDOW SO USER CANNOT CLICK BEHIND!
        EnableWindow(hParent, FALSE);
    }

    // Initialize animation states from settings
    m_animAutoWorkspace = SettingsManager::Get().settings.restoreWorkspaceOnStartup ? 1.0f : 0.0f;
    m_animLineNumbers = SettingsManager::Get().settings.showLineNumbers ? 1.0f : 0.0f;
    m_animMinimap = SettingsManager::Get().settings.showMinimap ? 1.0f : 0.0f;
    m_animSchemeV1 = SettingsManager::Get().settings.signSchemeV1 ? 1.0f : 0.0f;
    m_animSchemeV2 = SettingsManager::Get().settings.signSchemeV2 ? 1.0f : 0.0f;
    m_animSchemeV3 = SettingsManager::Get().settings.signSchemeV3 ? 1.0f : 0.0f;
    m_animZipAlign = SettingsManager::Get().settings.zipalignEnabled ? 1.0f : 0.0f;
    m_animVerboseLog = SettingsManager::Get().settings.verboseLogging ? 1.0f : 0.0f;
    m_animLangDropdown = m_langDropdownOpen ? 1.0f : 0.0f;

    // Frameless popup window with custom anime header (No double OS titlebar!)
    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST,
        L"KaisekiSettingsWindowClass",
        L"Ayarlar ve Tercihler",
        WS_POPUP | WS_VISIBLE,
        posX, posY, dlgW, dlgH,
        hParent, NULL, hInst, this
    );

    COLORREF darkBorder = RGB(51, 54, 64);
    DwmSetWindowAttribute(m_hwnd, DWMWA_BORDER_COLOR, &darkBorder, sizeof(darkBorder));

    int cornerPref = 2; // DWMWCP_ROUND
    DwmSetWindowAttribute(m_hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));
    MARGINS margins = { 0, 0, 1, 0 };
    DwmExtendFrameIntoClientArea(m_hwnd, &margins);

    CreateDeviceResources();
    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);

    // Modal message loop
    MSG msg;
    while (IsWindow(m_hwnd) && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Restore parent window
    if (m_hParent && IsWindow(m_hParent)) {
        EnableWindow(m_hParent, TRUE);
        SetForegroundWindow(m_hParent);
        InvalidateRect(m_hParent, NULL, FALSE);
    }
}

void SettingsDialog::Close() {
    if (m_hwnd) {
        HWND h = m_hwnd;
        m_hwnd = NULL;
        DestroyWindow(h);
    }
    DiscardDeviceResources();
    if (m_hParent && IsWindow(m_hParent)) {
        EnableWindow(m_hParent, TRUE);
        SetForegroundWindow(m_hParent);
        InvalidateRect(m_hParent, NULL, FALSE);
    }
}

LRESULT CALLBACK SettingsDialog::DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SettingsDialog* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* pcs = (CREATESTRUCT*)lParam;
        pThis = (SettingsDialog*)pcs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->m_hwnd = hwnd;
    } else {
        pThis = (SettingsDialog*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    }

    if (pThis) {
        return pThis->HandleMessage(msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT SettingsDialog::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_TIMER: {
        if (wParam == 1) {
            UpdateAnimations(0.012f);
            InvalidateRect(m_hwnd, NULL, FALSE);
            UpdateWindow(m_hwnd);
            return 0;
        }
        break;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(m_hwnd, &ps);
        OnRender();
        EndPaint(m_hwnd, &ps);
        return 0;
    }
    case WM_SIZE: {
        UINT w = LOWORD(lParam);
        UINT h = HIWORD(lParam);
        OnResize(w, h);
        return 0;
    }
    case WM_DPICHANGED: {
        RECT* const prcNewWindow = (RECT*)lParam;
        SetWindowPos(m_hwnd, NULL, prcNewWindow->left, prcNewWindow->top,
            prcNewWindow->right - prcNewWindow->left, prcNewWindow->bottom - prcNewWindow->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        if (m_pRenderTarget) {
            m_dpiX = (float)LOWORD(wParam);
            m_dpiY = (float)HIWORD(wParam);
            m_dpiScale = m_dpiX / 96.0f;
            m_pRenderTarget->SetDpi(m_dpiX, m_dpiY);
        }
        InvalidateRect(m_hwnd, NULL, FALSE);
        return 0;
    }
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN: {
        if (m_editingShortcutIndex >= 0) {
            if (wParam == VK_ESCAPE) {
                m_editingShortcutIndex = -1;
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
            if (wParam == VK_CONTROL || wParam == VK_SHIFT || wParam == VK_MENU ||
                wParam == VK_LCONTROL || wParam == VK_RCONTROL ||
                wParam == VK_LSHIFT || wParam == VK_RSHIFT ||
                wParam == VK_LMENU || wParam == VK_RMENU) {
                return 0;
            }
            std::wstring combo = L"";
            if (GetKeyState(VK_CONTROL) & 0x8000) combo += L"Ctrl + ";
            if (GetKeyState(VK_MENU) & 0x8000) combo += L"Alt + ";
            if (GetKeyState(VK_SHIFT) & 0x8000) combo += L"Shift + ";

            if (wParam >= 'A' && wParam <= 'Z') {
                combo += (wchar_t)wParam;
            } else if (wParam >= '0' && wParam <= '9') {
                combo += (wchar_t)wParam;
            } else if (wParam >= VK_F1 && wParam <= VK_F24) {
                combo += L"F" + std::to_wstring(wParam - VK_F1 + 1);
            } else if (wParam == VK_SPACE) {
                combo += L"Space";
            } else if (wParam == VK_RETURN) {
                combo += L"Enter";
            } else if (wParam == VK_TAB) {
                combo += L"Tab";
            } else if (wParam == VK_OEM_COMMA) {
                combo += L",";
            } else if (wParam == VK_OEM_PERIOD) {
                combo += L".";
            } else if (wParam == VK_OEM_2) {
                combo += L"/";
            } else if (wParam == VK_DELETE) {
                combo += L"Del";
            } else if (wParam == VK_BACK) {
                combo += L"Backspace";
            } else {
                wchar_t kName[32] = { 0 };
                UINT scanCode = MapVirtualKeyW((UINT)wParam, MAPVK_VK_TO_VSC);
                if (GetKeyNameTextW((LONG)(scanCode << 16), kName, 32) > 0) {
                    combo += kName;
                } else {
                    combo += L"Key";
                }
            }

            SettingsManager::Get().settings.customKeys[m_editingShortcutIndex] = combo;
            SettingsManager::Get().settings.hotkeyPreset = 3; // Custom
            m_editingShortcutIndex = -1;
            SettingsManager::Get().Save();
            InvalidateRect(m_hwnd, NULL, FALSE);
            return 0;
        }

        if (wParam == VK_ESCAPE) {
            if (m_colorPickerPopoverOpen) {
                m_colorPickerPopoverOpen = false;
                InvalidateRect(m_hwnd, NULL, FALSE);
            } else if (m_langDropdownOpen) {
                m_langDropdownOpen = false;
                InvalidateRect(m_hwnd, NULL, FALSE);
            } else {
                Close();
            }
            return 0;
        }
        break;
    }
    case WM_LBUTTONDOWN: {
        float x = PixelToDipX((float)GET_X_LPARAM(lParam));
        float y = PixelToDipY((float)GET_Y_LPARAM(lParam));

        // Close button (X)
        if (x >= m_rcSettingsClose.left && x <= m_rcSettingsClose.right &&
            y >= m_rcSettingsClose.top && y <= m_rcSettingsClose.bottom) {
            Close();
            return 0;
        }

        // Custom Header Dragging (y < 44.0f)
        if (y < 44.0f && x < m_rcSettingsClose.left && !m_colorPickerPopoverOpen) {
            ReleaseCapture();
            SendMessage(m_hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return 0;
        }

        // Color Picker Popover Clicks
        if (m_colorPickerPopoverOpen) {
            if (x >= m_rcColorPickerClose.left && x <= m_rcColorPickerClose.right &&
                y >= m_rcColorPickerClose.top && y <= m_rcColorPickerClose.bottom) {
                m_colorPickerPopoverOpen = false;
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
            if (x >= m_rcColorSatValBox.left && x <= m_rcColorSatValBox.right &&
                y >= m_rcColorSatValBox.top && y <= m_rcColorSatValBox.bottom) {
                m_isDraggingSatVal = true;
                SetCapture(m_hwnd);
                float normX = (x - m_rcColorSatValBox.left) / (m_rcColorSatValBox.right - m_rcColorSatValBox.left);
                float normY = (y - m_rcColorSatValBox.top) / (m_rcColorSatValBox.bottom - m_rcColorSatValBox.top);
                m_pickerSat = KaisekiTheme::ClampF(normX, 0.0f, 1.0f);
                m_pickerVal = KaisekiTheme::ClampF(1.0f - normY, 0.0f, 1.0f);
                D2D1_COLOR_F rgb = KaisekiTheme::HsvToRgb(m_pickerHue, m_pickerSat, m_pickerVal);
                if (m_colorPickerTarget == 0) {
                    SetCustomAccentColorU8((uint8_t)(rgb.r * 255.0f), (uint8_t)(rgb.g * 255.0f), (uint8_t)(rgb.b * 255.0f));
                } else {
                    SetCustomBackgroundColorU8((uint8_t)(rgb.r * 255.0f), (uint8_t)(rgb.g * 255.0f), (uint8_t)(rgb.b * 255.0f));
                }
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
            if (x >= m_rcColorHueSlider.left && x <= m_rcColorHueSlider.right &&
                y >= m_rcColorHueSlider.top && y <= m_rcColorHueSlider.bottom) {
                m_isDraggingHue = true;
                SetCapture(m_hwnd);
                float norm = (x - m_rcColorHueSlider.left) / (m_rcColorHueSlider.right - m_rcColorHueSlider.left);
                m_pickerHue = KaisekiTheme::ClampF(norm, 0.0f, 1.0f) * 360.0f;
                D2D1_COLOR_F rgb = KaisekiTheme::HsvToRgb(m_pickerHue, m_pickerSat, m_pickerVal);
                if (m_colorPickerTarget == 0) {
                    SetCustomAccentColorU8((uint8_t)(rgb.r * 255.0f), (uint8_t)(rgb.g * 255.0f), (uint8_t)(rgb.b * 255.0f));
                } else {
                    SetCustomBackgroundColorU8((uint8_t)(rgb.r * 255.0f), (uint8_t)(rgb.g * 255.0f), (uint8_t)(rgb.b * 255.0f));
                }
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
            // Presets dots in color picker
            for (int p = 0; p < 8; ++p) {
                if (x >= m_rcColorPresetDots[p].left && x <= m_rcColorPresetDots[p].right &&
                    y >= m_rcColorPresetDots[p].top && y <= m_rcColorPresetDots[p].bottom) {
                    static const D2D1_COLOR_F popPresets[8] = {
                        D2D1::ColorF(1.0f, 0.50f, 0.72f), // Sakura
                        D2D1::ColorF(0.65f, 0.37f, 0.92f), // Hasu Mor
                        D2D1::ColorF(0.18f, 0.80f, 0.44f), // Emerald
                        D2D1::ColorF(0.00f, 0.75f, 1.00f), // Cyan
                        D2D1::ColorF(1.00f, 0.75f, 0.00f), // Gold
                        D2D1::ColorF(1.00f, 0.28f, 0.34f), // Torii Red
                        D2D1::ColorF(0.09f, 0.10f, 0.12f), // Dark Charcoal
                        D2D1::ColorF(0.04f, 0.04f, 0.05f)  // OLED
                    };
                    float h, s, v;
                    KaisekiTheme::RgbToHsv(popPresets[p].r, popPresets[p].g, popPresets[p].b, h, s, v);
                    m_pickerHue = h; m_pickerSat = s; m_pickerVal = v;
                    if (m_colorPickerTarget == 0) {
                        SetCustomAccentColorU8((uint8_t)(popPresets[p].r * 255.0f), (uint8_t)(popPresets[p].g * 255.0f), (uint8_t)(popPresets[p].b * 255.0f));
                    } else {
                        SetCustomBackgroundColorU8((uint8_t)(popPresets[p].r * 255.0f), (uint8_t)(popPresets[p].g * 255.0f), (uint8_t)(popPresets[p].b * 255.0f));
                    }
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }
            }
            if (x < m_rcColorPickerCard.left || x > m_rcColorPickerCard.right ||
                y < m_rcColorPickerCard.top || y > m_rcColorPickerCard.bottom) {
                m_colorPickerPopoverOpen = false;
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
            return 0;
        }

        // Sub-tabs clicks
        for (int i = 0; i < 7; ++i) {
            if (x >= m_rcSettingsTabs[i].left && x <= m_rcSettingsTabs[i].right &&
                y >= m_rcSettingsTabs[i].top && y <= m_rcSettingsTabs[i].bottom) {
                m_settingsTab = i;
                m_colorPickerPopoverOpen = false;
                m_langDropdownOpen = false;
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
        }

        // TAB 0: GENEL & PROJE
        if (m_settingsTab == 0) {
            if (x >= m_rcBtnSelectWorkspace.left && x <= m_rcBtnSelectWorkspace.right &&
                y >= m_rcBtnSelectWorkspace.top && y <= m_rcBtnSelectWorkspace.bottom) {
                IFileDialog* pfd = nullptr;
                if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd)))) {
                    DWORD dwOptions;
                    if (SUCCEEDED(pfd->GetOptions(&dwOptions))) {
                        pfd->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
                    }
                    pfd->SetTitle(L"Varsayılan Çalışma Alanı Dizinini Seçin");
                    if (SUCCEEDED(pfd->Show(m_hwnd))) {
                        IShellItem* psi = nullptr;
                        if (SUCCEEDED(pfd->GetResult(&psi))) {
                            PWSTR pszPath = nullptr;
                            if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath))) {
                                SettingsManager::Get().settings.workspacePath = pszPath;
                                SettingsManager::Get().Save();
                                CoTaskMemFree(pszPath);
                            }
                            psi->Release();
                        }
                    }
                    pfd->Release();
                }
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
            if (x >= m_rcToggleRestoreWorkspace.left && x <= m_rcToggleRestoreWorkspace.right &&
                y >= m_rcToggleRestoreWorkspace.top && y <= m_rcToggleRestoreWorkspace.bottom) {
                SettingsManager::Get().settings.restoreWorkspaceOnStartup = !SettingsManager::Get().settings.restoreWorkspaceOnStartup;
                SettingsManager::Get().Save();
                SetTimer(m_hwnd, 1, 12, NULL);
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
            if (x >= m_rcBtnAssocKproj.left && x <= m_rcBtnAssocKproj.right &&
                y >= m_rcBtnAssocKproj.top && y <= m_rcBtnAssocKproj.bottom) {
                wchar_t szExe[MAX_PATH];
                GetModuleFileNameW(NULL, szExe, MAX_PATH);
                SettingsManager::Get().RegisterKprojAssociation(szExe);
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
        }

        // TAB 1: GÖRÜNÜM & TEMA
        if (m_settingsTab == 1) {
            if (x >= m_rcAccentPreviewBox.left && x <= m_rcAccentPreviewBox.right &&
                y >= m_rcAccentPreviewBox.top && y <= m_rcAccentPreviewBox.bottom) {
                m_colorPickerTarget = 0;
                float h, s, v;
                KaisekiTheme::RgbToHsv(m_customAccentColor.r, m_customAccentColor.g, m_customAccentColor.b, h, s, v);
                m_pickerHue = h; m_pickerSat = s; m_pickerVal = v;
                m_colorPickerPopoverOpen = true;
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
            for (int sw = 0; sw < 12; ++sw) {
                if (x >= m_rcAccentSwatches[sw].left && x <= m_rcAccentSwatches[sw].right &&
                    y >= m_rcAccentSwatches[sw].top && y <= m_rcAccentSwatches[sw].bottom) {
                    static const D2D1_COLOR_F swatches[12] = {
                        D2D1::ColorF(1.0f, 0.50f, 0.72f),
                        D2D1::ColorF(0.96f, 0.88f, 0.98f),
                        D2D1::ColorF(1.0f, 0.28f, 0.34f),
                        D2D1::ColorF(0.95f, 0.77f, 0.06f),
                        D2D1::ColorF(0.18f, 0.80f, 0.44f),
                        D2D1::ColorF(0.00f, 0.48f, 1.0f),
                        D2D1::ColorF(0.00f, 0.82f, 1.0f),
                        D2D1::ColorF(0.65f, 0.37f, 0.92f),
                        D2D1::ColorF(0.70f, 0.72f, 0.75f),
                        D2D1::ColorF(1.0f, 0.45f, 0.15f),
                        D2D1::ColorF(0.00f, 0.80f, 0.55f),
                        D2D1::ColorF(1.0f, 0.75f, 0.05f)
                    };
                    SetCustomAccentColorU8((uint8_t)(swatches[sw].r * 255.0f), (uint8_t)(swatches[sw].g * 255.0f), (uint8_t)(swatches[sw].b * 255.0f));
                    return 0;
                }
            }
            if (x >= m_rcBgPreviewBox.left && x <= m_rcBgPreviewBox.right &&
                y >= m_rcBgPreviewBox.top && y <= m_rcBgPreviewBox.bottom) {
                m_colorPickerTarget = 1;
                float h, s, v;
                KaisekiTheme::RgbToHsv(m_customBgColor.r, m_customBgColor.g, m_customBgColor.b, h, s, v);
                m_pickerHue = h; m_pickerSat = s; m_pickerVal = v;
                m_colorPickerPopoverOpen = true;
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
            for (int sh = 0; sh < 8; ++sh) {
                if (x >= m_rcBgShades[sh].left && x <= m_rcBgShades[sh].right &&
                    y >= m_rcBgShades[sh].top && y <= m_rcBgShades[sh].bottom) {
                    SetBackgroundShade((BackgroundShade)sh);
                    return 0;
                }
            }
            if (x >= m_rcToggleLineNumbers.left && x <= m_rcToggleLineNumbers.right &&
                y >= m_rcToggleLineNumbers.top && y <= m_rcToggleLineNumbers.bottom) {
                SettingsManager::Get().settings.showLineNumbers = !SettingsManager::Get().settings.showLineNumbers;
                SettingsManager::Get().Save();
                SetTimer(m_hwnd, 1, 12, NULL);
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
            if (x >= m_rcToggleMinimap.left && x <= m_rcToggleMinimap.right &&
                y >= m_rcToggleMinimap.top && y <= m_rcToggleMinimap.bottom) {
                SettingsManager::Get().settings.showMinimap = !SettingsManager::Get().settings.showMinimap;
                SettingsManager::Get().Save();
                SetTimer(m_hwnd, 1, 12, NULL);
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
        }

        // TAB 2: DIL / LANGUAGE
        if (m_settingsTab == 2) {
            if (x >= m_rcLangDropdownBtn.left && x <= m_rcLangDropdownBtn.right &&
                y >= m_rcLangDropdownBtn.top && y <= m_rcLangDropdownBtn.bottom) {
                m_langDropdownOpen = !m_langDropdownOpen;
                SetTimer(m_hwnd, 1, 12, NULL);
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
            if (m_langDropdownOpen) {
                const auto& langs = KaisekiLang::Get().GetAvailableLanguages();
                float popW = m_rcLangDropdownBtn.right - m_rcLangDropdownBtn.left;
                float optH = 34.0f;
                float curOy = m_rcLangDropdownBtn.bottom + 4.0f;
                for (size_t li = 0; li < langs.size(); ++li) {
                    D2D1_RECT_F rcOpt = D2D1::RectF(m_rcLangDropdownBtn.left, curOy, m_rcLangDropdownBtn.right, curOy + optH);
                    if (x >= rcOpt.left && x <= rcOpt.right && y >= rcOpt.top && y <= rcOpt.bottom) {
                        KaisekiLang::Get().SetLanguage(langs[li].code);
                        m_langDropdownOpen = false;
                        SetTimer(m_hwnd, 1, 12, NULL);
                        InvalidateRect(m_hwnd, NULL, FALSE);
                        return 0;
                    }
                    curOy += optH;
                }
            }
        }

        // TAB 3: ANDROID ARAÇLARI
        if (m_settingsTab == 3) {
            auto pickFolder = [&](std::wstring& targetSetting, const wchar_t* title) {
                IFileDialog* pfd = nullptr;
                if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd)))) {
                    DWORD dwOptions;
                    if (SUCCEEDED(pfd->GetOptions(&dwOptions))) {
                        pfd->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
                    }
                    pfd->SetTitle(title);
                    if (SUCCEEDED(pfd->Show(m_hwnd))) {
                        IShellItem* psi = nullptr;
                        if (SUCCEEDED(pfd->GetResult(&psi))) {
                            PWSTR pszPath = nullptr;
                            if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath))) {
                                targetSetting = pszPath;
                                SettingsManager::Get().ValidateSdkPath();
                                SettingsManager::Get().Save();
                                CoTaskMemFree(pszPath);
                            }
                            psi->Release();
                        }
                    }
                    pfd->Release();
                }
                InvalidateRect(m_hwnd, NULL, FALSE);
            };

            if (x >= m_rcBtnSelectSdk.left && x <= m_rcBtnSelectSdk.right &&
                y >= m_rcBtnSelectSdk.top && y <= m_rcBtnSelectSdk.bottom) {
                pickFolder(SettingsManager::Get().settings.androidSdkPath, L"Android SDK Kök Dizinini Seçin");
                return 0;
            }
            if (x >= m_rcBtnSelectAdb.left && x <= m_rcBtnSelectAdb.right &&
                y >= m_rcBtnSelectAdb.top && y <= m_rcBtnSelectAdb.bottom) {
                pickFolder(SettingsManager::Get().settings.adbPath, L"Platform-Tools (ADB) Dizinini Seçin");
                return 0;
            }
            if (x >= m_rcBtnSelectBuildTools.left && x <= m_rcBtnSelectBuildTools.right &&
                y >= m_rcBtnSelectBuildTools.top && y <= m_rcBtnSelectBuildTools.bottom) {
                pickFolder(SettingsManager::Get().settings.buildToolsPath, L"Android Build-Tools Dizinini Seçin");
                return 0;
            }
            if (x >= m_rcBtnSelectJava.left && x <= m_rcBtnSelectJava.right &&
                y >= m_rcBtnSelectJava.top && y <= m_rcBtnSelectJava.bottom) {
                pickFolder(SettingsManager::Get().settings.jdkPath, L"Java JDK Dizinini Seçin");
                return 0;
            }
            if (x >= m_rcBtnDownloadTools.left && x <= m_rcBtnDownloadTools.right &&
                y >= m_rcBtnDownloadTools.top && y <= m_rcBtnDownloadTools.bottom) {
                ToolInstallerDialog::Get().ShowModal(m_hwnd);
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
        }

        // TAB 4: DEKOMPİLASYON & İMZA
        if (m_settingsTab == 4) {
            if (x >= m_rcToggleDefaultView.left && x <= m_rcToggleDefaultView.right &&
                y >= m_rcToggleDefaultView.top && y <= m_rcToggleDefaultView.bottom) {
                SettingsManager::Get().settings.defaultToJava = !SettingsManager::Get().settings.defaultToJava;
                SettingsManager::Get().Save();
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
            if (x >= m_rcToggleMultidexTree.left && x <= m_rcToggleMultidexTree.right &&
                y >= m_rcToggleMultidexTree.top && y <= m_rcToggleMultidexTree.bottom) {
                SettingsManager::Get().settings.multidexMerged = !SettingsManager::Get().settings.multidexMerged;
                SettingsManager::Get().Save();
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
            if (x >= m_rcBtnDecompileThreads.left && x <= m_rcBtnDecompileThreads.right &&
                y >= m_rcBtnDecompileThreads.top && y <= m_rcBtnDecompileThreads.bottom) {
                int& th = SettingsManager::Get().settings.decompileThreads;
                if (th == 0) th = 2;
                else if (th == 2) th = 4;
                else if (th == 4) th = 8;
                else if (th == 8) th = 16;
                else th = 0;
                SettingsManager::Get().Save();
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
            if (x >= m_rcToggleSchemeV1.left && x <= m_rcToggleSchemeV1.right &&
                y >= m_rcToggleSchemeV1.top && y <= m_rcToggleSchemeV1.bottom) {
                SettingsManager::Get().settings.signSchemeV1 = !SettingsManager::Get().settings.signSchemeV1;
                SettingsManager::Get().Save();
                SetTimer(m_hwnd, 1, 12, NULL);
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
            if (x >= m_rcToggleSchemeV2.left && x <= m_rcToggleSchemeV2.right &&
                y >= m_rcToggleSchemeV2.top && y <= m_rcToggleSchemeV2.bottom) {
                SettingsManager::Get().settings.signSchemeV2 = !SettingsManager::Get().settings.signSchemeV2;
                SettingsManager::Get().Save();
                SetTimer(m_hwnd, 1, 12, NULL);
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
            if (x >= m_rcToggleSchemeV3.left && x <= m_rcToggleSchemeV3.right &&
                y >= m_rcToggleSchemeV3.top && y <= m_rcToggleSchemeV3.bottom) {
                SettingsManager::Get().settings.signSchemeV3 = !SettingsManager::Get().settings.signSchemeV3;
                SettingsManager::Get().Save();
                SetTimer(m_hwnd, 1, 12, NULL);
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
            if (x >= m_rcToggleZipAlign.left && x <= m_rcToggleZipAlign.right &&
                y >= m_rcToggleZipAlign.top && y <= m_rcToggleZipAlign.bottom) {
                SettingsManager::Get().settings.zipalignEnabled = !SettingsManager::Get().settings.zipalignEnabled;
                SettingsManager::Get().Save();
                SetTimer(m_hwnd, 1, 12, NULL);
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
        }

        // TAB 5: KISAYOLLAR
        if (m_settingsTab == 5) {
            if (x >= m_rcPresetVSCode.left && x <= m_rcPresetVSCode.right &&
                y >= m_rcPresetVSCode.top && y <= m_rcPresetVSCode.bottom) {
                SettingsManager::Get().settings.hotkeyPreset = 0;
                for (int k = 0; k < 16; ++k) SettingsManager::Get().settings.customKeys[k] = L"";
                m_editingShortcutIndex = -1;
                SettingsManager::Get().Save();
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
            if (x >= m_rcPresetIDEA.left && x <= m_rcPresetIDEA.right &&
                y >= m_rcPresetIDEA.top && y <= m_rcPresetIDEA.bottom) {
                SettingsManager::Get().settings.hotkeyPreset = 1;
                for (int k = 0; k < 16; ++k) SettingsManager::Get().settings.customKeys[k] = L"";
                m_editingShortcutIndex = -1;
                SettingsManager::Get().Save();
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
            if (x >= m_rcPresetVS.left && x <= m_rcPresetVS.right &&
                y >= m_rcPresetVS.top && y <= m_rcPresetVS.bottom) {
                SettingsManager::Get().settings.hotkeyPreset = 2;
                for (int k = 0; k < 16; ++k) SettingsManager::Get().settings.customKeys[k] = L"";
                m_editingShortcutIndex = -1;
                SettingsManager::Get().Save();
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
            if (x >= m_rcPresetCustom.left && x <= m_rcPresetCustom.right &&
                y >= m_rcPresetCustom.top && y <= m_rcPresetCustom.bottom) {
                SettingsManager::Get().settings.hotkeyPreset = 3;
                m_editingShortcutIndex = -1;
                SettingsManager::Get().Save();
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
            if (x >= m_rcBtnResetHotkeys.left && x <= m_rcBtnResetHotkeys.right &&
                y >= m_rcBtnResetHotkeys.top && y <= m_rcBtnResetHotkeys.bottom) {
                for (int k = 0; k < 16; ++k) SettingsManager::Get().settings.customKeys[k] = L"";
                m_editingShortcutIndex = -1;
                SettingsManager::Get().Save();
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }

            // Check click on any of the 16 shortcut badges
            for (int k = 0; k < 16; ++k) {
                const auto& rb = m_rcShortcutBadges[k];
                if (x >= rb.left && x <= rb.right && y >= rb.top && y <= rb.bottom) {
                    m_editingShortcutIndex = k;
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }
            }

            // Clicked elsewhere cancels active key capture
            if (m_editingShortcutIndex >= 0) {
                m_editingShortcutIndex = -1;
                InvalidateRect(m_hwnd, NULL, FALSE);
            }
        }

        // TAB 6: GELİŞMİŞ & TEŞHİS
        if (m_settingsTab == 6) {
            if (x >= m_rcToggleVerboseLog.left && x <= m_rcToggleVerboseLog.right &&
                y >= m_rcToggleVerboseLog.top && y <= m_rcToggleVerboseLog.bottom) {
                SettingsManager::Get().settings.verboseLogging = !SettingsManager::Get().settings.verboseLogging;
                SettingsManager::Get().Save();
                SetTimer(m_hwnd, 1, 12, NULL);
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
            if (x >= m_rcBtnCopyDiagnostics.left && x <= m_rcBtnCopyDiagnostics.right &&
                y >= m_rcBtnCopyDiagnostics.top && y <= m_rcBtnCopyDiagnostics.bottom) {
                std::wstring diag = SettingsManager::Get().GenerateDiagnosticReport();
                if (OpenClipboard(m_hwnd)) {
                    EmptyClipboard();
                    size_t bytes = (diag.length() + 1) * sizeof(wchar_t);
                    HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, bytes);
                    if (hGlob) {
                        memcpy(GlobalLock(hGlob), diag.c_str(), bytes);
                        GlobalUnlock(hGlob);
                        SetClipboardData(CF_UNICODETEXT, hGlob);
                    }
                    CloseClipboard();
                }
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
            if (x >= m_rcBtnResetSettings.left && x <= m_rcBtnResetSettings.right &&
                y >= m_rcBtnResetSettings.top && y <= m_rcBtnResetSettings.bottom) {
                if (MessageBoxW(m_hwnd, L"Tüm ayarlar fabrika ayarlarına sıfırlansın mı?", L"Kaiseki — Ayarları Sıfırla", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                    SettingsManager::Get().ResetToDefaults();
                    ApplyTheme();
    CreateBrushes();
                }
                return 0;
            }
        }

        InvalidateRect(m_hwnd, NULL, FALSE);
        return 0;
    }
    case WM_MOUSEMOVE: {
        float x = PixelToDipX((float)GET_X_LPARAM(lParam));
        float y = PixelToDipY((float)GET_Y_LPARAM(lParam));

        bool hoverClose = (x >= m_rcSettingsClose.left && x <= m_rcSettingsClose.right &&
                           y >= m_rcSettingsClose.top && y <= m_rcSettingsClose.bottom);
        if (hoverClose != m_hoverClose) {
            m_hoverClose = hoverClose;
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, m_hwnd, 0 };
            TrackMouseEvent(&tme);
            InvalidateRect(m_hwnd, NULL, FALSE);
        }

        if (m_isDraggingSatVal && m_colorPickerPopoverOpen) {
            float normX = (x - m_rcColorSatValBox.left) / (m_rcColorSatValBox.right - m_rcColorSatValBox.left);
            float normY = (y - m_rcColorSatValBox.top) / (m_rcColorSatValBox.bottom - m_rcColorSatValBox.top);
            m_pickerSat = KaisekiTheme::ClampF(normX, 0.0f, 1.0f);
            m_pickerVal = KaisekiTheme::ClampF(1.0f - normY, 0.0f, 1.0f);
            D2D1_COLOR_F rgb = KaisekiTheme::HsvToRgb(m_pickerHue, m_pickerSat, m_pickerVal);
            if (m_colorPickerTarget == 0) {
                SetCustomAccentColorU8((uint8_t)(rgb.r * 255.0f), (uint8_t)(rgb.g * 255.0f), (uint8_t)(rgb.b * 255.0f));
            } else {
                SetCustomBackgroundColorU8((uint8_t)(rgb.r * 255.0f), (uint8_t)(rgb.g * 255.0f), (uint8_t)(rgb.b * 255.0f));
            }
            InvalidateRect(m_hwnd, NULL, FALSE);
            return 0;
        }
        if (m_isDraggingHue && m_colorPickerPopoverOpen) {
            float norm = (x - m_rcColorHueSlider.left) / (m_rcColorHueSlider.right - m_rcColorHueSlider.left);
            m_pickerHue = KaisekiTheme::ClampF(norm, 0.0f, 1.0f) * 360.0f;
            D2D1_COLOR_F rgb = KaisekiTheme::HsvToRgb(m_pickerHue, m_pickerSat, m_pickerVal);
            if (m_colorPickerTarget == 0) {
                SetCustomAccentColorU8((uint8_t)(rgb.r * 255.0f), (uint8_t)(rgb.g * 255.0f), (uint8_t)(rgb.b * 255.0f));
            } else {
                SetCustomBackgroundColorU8((uint8_t)(rgb.r * 255.0f), (uint8_t)(rgb.g * 255.0f), (uint8_t)(rgb.b * 255.0f));
            }
            InvalidateRect(m_hwnd, NULL, FALSE);
            return 0;
        }
        break;
    }
    case WM_MOUSELEAVE: {
        if (m_hoverClose) {
            m_hoverClose = false;
            InvalidateRect(m_hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        if (m_isDraggingSatVal || m_isDraggingHue) {
            m_isDraggingSatVal = false;
            m_isDraggingHue = false;
            ReleaseCapture();
        }
        break;
    }
    case WM_DESTROY: {
        DiscardDeviceResources();
        m_hwnd = NULL;
        return 0;
    }
    }

    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

void SettingsDialog::OnResize(UINT width, UINT height) {
    UINT winDpi = GetDpiForWindow(m_hwnd);
    if (winDpi == 0) winDpi = 96;
    m_dpiX = (float)winDpi;
    m_dpiY = (float)winDpi;
    m_dpiScale = m_dpiX / 96.0f;

    if (m_pRenderTarget) {
        m_pRenderTarget->Resize(D2D1::SizeU(width, height));
        m_pRenderTarget->SetDpi(m_dpiX, m_dpiY);
    }
}

void SettingsDialog::UpdateAnimations(float dt) {
    float rate = 0.30f;
    auto stepAnim = [&](float& current, float target) {
        if (fabs(target - current) > 0.005f) {
            current += (target - current) * rate;
            return true;
        } else {
            current = target;
            return false;
        }
    };

    bool any = false;
    any |= stepAnim(m_animAutoWorkspace, SettingsManager::Get().settings.restoreWorkspaceOnStartup ? 1.0f : 0.0f);
    any |= stepAnim(m_animLineNumbers, SettingsManager::Get().settings.showLineNumbers ? 1.0f : 0.0f);
    any |= stepAnim(m_animMinimap, SettingsManager::Get().settings.showMinimap ? 1.0f : 0.0f);
    any |= stepAnim(m_animSchemeV1, SettingsManager::Get().settings.signSchemeV1 ? 1.0f : 0.0f);
    any |= stepAnim(m_animSchemeV2, SettingsManager::Get().settings.signSchemeV2 ? 1.0f : 0.0f);
    any |= stepAnim(m_animSchemeV3, SettingsManager::Get().settings.signSchemeV3 ? 1.0f : 0.0f);
    any |= stepAnim(m_animZipAlign, SettingsManager::Get().settings.zipalignEnabled ? 1.0f : 0.0f);
    any |= stepAnim(m_animVerboseLog, SettingsManager::Get().settings.verboseLogging ? 1.0f : 0.0f);
    any |= stepAnim(m_animLangDropdown, m_langDropdownOpen ? 1.0f : 0.0f);

    m_animating = any;
    if (!any && m_hwnd) {
        KillTimer(m_hwnd, 1);
    }
}

void SettingsDialog::DrawToggleSwitch(const D2D1_RECT_F& rc, bool isOn, float animVal, const wchar_t* label, const wchar_t* subtext) {
    float trackW = 44.0f;
    float trackH = 24.0f;
    float trackX = rc.left;
    float trackY = rc.top + 3.0f;
    D2D1_RECT_F rcTrack = D2D1::RectF(trackX, trackY, trackX + trackW, trackY + trackH);

    float t = KaisekiTheme::ClampF(animVal, 0.0f, 1.0f);
    D2D1_COLOR_F trackCol = KaisekiTheme::LerpColor(m_colors.inputWell, m_colors.accent, t);
    D2D1_COLOR_F borderCol = KaisekiTheme::LerpColor(m_colors.borderSubtle, m_colors.accent, t);

    ID2D1SolidColorBrush* bTrack = nullptr;
    ID2D1SolidColorBrush* bBorder = nullptr;
    m_pRenderTarget->CreateSolidColorBrush(trackCol, &bTrack);
    m_pRenderTarget->CreateSolidColorBrush(borderCol, &bBorder);
    if (bTrack) m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(rcTrack, 12.0f, 12.0f), bTrack);
    if (bBorder) m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(rcTrack, 12.0f, 12.0f), bBorder, 1.2f);
    if (bTrack) bTrack->Release();
    if (bBorder) bBorder->Release();

    float thumbR = 9.0f;
    float minX = rcTrack.left + 12.0f;
    float maxX = rcTrack.right - 12.0f;
    float thumbX = minX + t * (maxX - minX);
    float thumbY = trackY + trackH * 0.5f;

    ID2D1SolidColorBrush* bThumb = nullptr;
    m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &bThumb);
    if (bThumb) {
        m_pRenderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(thumbX, thumbY), thumbR, thumbR), bThumb);
        bThumb->Release();
    }

    float textX = trackX + trackW + 12.0f;
    if (label && m_fonts.body) {
        m_pRenderTarget->DrawText(label, (UINT32)wcslen(label), m_fonts.body,
            D2D1::RectF(textX, rc.top, rc.right, rc.top + 20.0f), m_bTextPrimary);
    }
    if (subtext && m_fonts.smallText) {
        m_pRenderTarget->DrawText(subtext, (UINT32)wcslen(subtext), m_fonts.smallText,
            D2D1::RectF(textX, rc.top + 20.0f, rc.right, rc.bottom), m_bTextMuted);
    }
}

void SettingsDialog::DrawCountryFlag(const std::string& code, const D2D1_RECT_F& rc) {
    if (!m_pRenderTarget) return;
    float w = rc.right - rc.left;
    float h = rc.bottom - rc.top;
    float cx = rc.left + w * 0.5f;
    float cy = rc.top + h * 0.5f;

    m_pRenderTarget->PushAxisAlignedClip(rc, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    if (code == "tr") {
        // Turkey: Red background, white crescent and star
        ID2D1SolidColorBrush* bRed = nullptr;
        ID2D1SolidColorBrush* bWhite = nullptr;
        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.89f, 0.04f, 0.09f), &bRed);
        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f), &bWhite);

        if (bRed) m_pRenderTarget->FillRectangle(rc, bRed);
        if (bWhite && bRed) {
            float moonR = h * 0.32f;
            m_pRenderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx - w * 0.10f, cy), moonR, moonR), bWhite);
            m_pRenderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx - w * 0.04f, cy), moonR * 0.80f, moonR * 0.80f), bRed);
            float starR = moonR * 0.40f;
            float sx = cx + w * 0.16f;
            m_pRenderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(sx, cy), starR, starR), bWhite);
        }
        if (bRed) bRed->Release();
        if (bWhite) bWhite->Release();
    } else if (code == "en" || code == "gb") {
        // UK Union Jack
        ID2D1SolidColorBrush* bBlue = nullptr;
        ID2D1SolidColorBrush* bWhite = nullptr;
        ID2D1SolidColorBrush* bRed = nullptr;
        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.004f, 0.13f, 0.41f), &bBlue);
        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f), &bWhite);
        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.80f, 0.06f, 0.18f), &bRed);

        if (bBlue) m_pRenderTarget->FillRectangle(rc, bBlue);
        if (bWhite) {
            m_pRenderTarget->DrawLine(D2D1::Point2F(rc.left, rc.top), D2D1::Point2F(rc.right, rc.bottom), bWhite, h * 0.28f);
            m_pRenderTarget->DrawLine(D2D1::Point2F(rc.right, rc.top), D2D1::Point2F(rc.left, rc.bottom), bWhite, h * 0.28f);
            m_pRenderTarget->FillRectangle(D2D1::RectF(cx - w * 0.16f, rc.top, cx + w * 0.16f, rc.bottom), bWhite);
            m_pRenderTarget->FillRectangle(D2D1::RectF(rc.left, cy - h * 0.16f, rc.right, cy + h * 0.16f), bWhite);
        }
        if (bRed) {
            m_pRenderTarget->DrawLine(D2D1::Point2F(rc.left, rc.top), D2D1::Point2F(rc.right, rc.bottom), bRed, h * 0.14f);
            m_pRenderTarget->DrawLine(D2D1::Point2F(rc.right, rc.top), D2D1::Point2F(rc.left, rc.bottom), bRed, h * 0.14f);
            m_pRenderTarget->FillRectangle(D2D1::RectF(cx - w * 0.09f, rc.top, cx + w * 0.09f, rc.bottom), bRed);
            m_pRenderTarget->FillRectangle(D2D1::RectF(rc.left, cy - h * 0.09f, rc.right, cy + h * 0.09f), bRed);
        }
        if (bBlue) bBlue->Release();
        if (bWhite) bWhite->Release();
        if (bRed) bRed->Release();
    } else if (code == "de") {
        // Germany: Black, Red, Gold horizontal stripes
        ID2D1SolidColorBrush* bBlack = nullptr;
        ID2D1SolidColorBrush* bRed = nullptr;
        ID2D1SolidColorBrush* bGold = nullptr;
        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f), &bBlack);
        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.86f, 0.00f, 0.00f), &bRed);
        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.00f, 0.80f, 0.00f), &bGold);

        float third = h / 3.0f;
        if (bBlack) m_pRenderTarget->FillRectangle(D2D1::RectF(rc.left, rc.top, rc.right, rc.top + third), bBlack);
        if (bRed) m_pRenderTarget->FillRectangle(D2D1::RectF(rc.left, rc.top + third, rc.right, rc.top + 2.0f * third), bRed);
        if (bGold) m_pRenderTarget->FillRectangle(D2D1::RectF(rc.left, rc.top + 2.0f * third, rc.right, rc.bottom), bGold);

        if (bBlack) bBlack->Release();
        if (bRed) bRed->Release();
        if (bGold) bGold->Release();
    } else if (code == "ja") {
        // Japan: White background, Red sun circle
        ID2D1SolidColorBrush* bWhite = nullptr;
        ID2D1SolidColorBrush* bRed = nullptr;
        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f), &bWhite);
        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.74f, 0.00f, 0.18f), &bRed);

        if (bWhite) m_pRenderTarget->FillRectangle(rc, bWhite);
        if (bRed) {
            float sunR = h * 0.30f;
            m_pRenderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), sunR, sunR), bRed);
        }
        if (bWhite) bWhite->Release();
        if (bRed) bRed->Release();
    }

    m_pRenderTarget->PopAxisAlignedClip();
    m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(rc, 2.0f, 2.0f), m_bBorderSubtle, 1.0f);
}

void SettingsDialog::DrawIdeLogo(int ideIndex, const D2D1_RECT_F& rc) {
    if (!m_pRenderTarget || !m_pD2DFactory) return;
    float w = rc.right - rc.left;
    float h = rc.bottom - rc.top;

    auto FillPoly = [&](const D2D1_POINT_2F* pts, UINT count, ID2D1Brush* brush) {
        if (!brush || count < 3) return;
        ID2D1PathGeometry* geom = nullptr;
        if (SUCCEEDED(m_pD2DFactory->CreatePathGeometry(&geom)) && geom) {
            ID2D1GeometrySink* sink = nullptr;
            if (SUCCEEDED(geom->Open(&sink)) && sink) {
                sink->BeginFigure(pts[0], D2D1_FIGURE_BEGIN_FILLED);
                sink->AddLines(&pts[1], count - 1);
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                sink->Close();
                sink->Release();
                m_pRenderTarget->FillGeometry(geom, brush);
            }
            geom->Release();
        }
    };

    if (ideIndex == 0) {
        // --- 1. VS Code Authentic Folded Origami Ribbon ---
        float x0 = rc.left + 2.0f;
        float y0 = rc.top + 2.0f;
        float rw = w - 4.0f;
        float rh = h - 4.0f;

        // Dark navy back fold
        ID2D1SolidColorBrush* bDarkNavy = nullptr;
        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.00f, 0.31f, 0.53f), &bDarkNavy);
        D2D1_POINT_2F pBack[4] = {
            D2D1::Point2F(x0 + rw * 0.72f, y0 + rh * 0.05f),
            D2D1::Point2F(x0 + rw * 0.94f, y0 + rh * 0.18f),
            D2D1::Point2F(x0 + rw * 0.94f, y0 + rh * 0.82f),
            D2D1::Point2F(x0 + rw * 0.72f, y0 + rh * 0.95f)
        };
        FillPoly(pBack, 4, bDarkNavy);

        // Bottom blue loop
        ID2D1SolidColorBrush* bMidBlue = nullptr;
        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.00f, 0.44f, 0.75f), &bMidBlue);
        D2D1_POINT_2F pBottom[4] = {
            D2D1::Point2F(x0 + rw * 0.22f, y0 + rh * 0.78f),
            D2D1::Point2F(x0 + rw * 0.72f, y0 + rh * 0.95f),
            D2D1::Point2F(x0 + rw * 0.72f, y0 + rh * 0.62f),
            D2D1::Point2F(x0 + rw * 0.46f, y0 + rh * 0.64f)
        };
        FillPoly(pBottom, 4, bMidBlue);

        // Top sky-blue loop
        ID2D1SolidColorBrush* bLightBlue = nullptr;
        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.12f, 0.62f, 0.95f), &bLightBlue);
        D2D1_POINT_2F pTop[4] = {
            D2D1::Point2F(x0 + rw * 0.22f, y0 + rh * 0.22f),
            D2D1::Point2F(x0 + rw * 0.72f, y0 + rh * 0.05f),
            D2D1::Point2F(x0 + rw * 0.72f, y0 + rh * 0.38f),
            D2D1::Point2F(x0 + rw * 0.46f, y0 + rh * 0.36f)
        };
        FillPoly(pTop, 4, bLightBlue);

        // Left chevron front
        ID2D1SolidColorBrush* bFrontAzure = nullptr;
        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.00f, 0.52f, 0.88f), &bFrontAzure);
        D2D1_POINT_2F pFront[6] = {
            D2D1::Point2F(x0 + rw * 0.22f, y0 + rh * 0.22f),
            D2D1::Point2F(x0 + rw * 0.05f, y0 + rh * 0.50f),
            D2D1::Point2F(x0 + rw * 0.22f, y0 + rh * 0.78f),
            D2D1::Point2F(x0 + rw * 0.46f, y0 + rh * 0.64f),
            D2D1::Point2F(x0 + rw * 0.20f, y0 + rh * 0.50f),
            D2D1::Point2F(x0 + rw * 0.46f, y0 + rh * 0.36f)
        };
        FillPoly(pFront, 6, bFrontAzure);

        if (bDarkNavy) bDarkNavy->Release();
        if (bMidBlue) bMidBlue->Release();
        if (bLightBlue) bLightBlue->Release();
        if (bFrontAzure) bFrontAzure->Release();

    } else if (ideIndex == 1) {
        // --- 2. IntelliJ IDEA Official JetBrains Square with Rainbow Bands & Prompt ---
        D2D1_RECT_F rcBox = D2D1::RectF(rc.left + 2.0f, rc.top + 2.0f, rc.right - 2.0f, rc.bottom - 2.0f);

        ID2D1SolidColorBrush* bJbDark = nullptr;
        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.08f, 0.08f, 0.09f), &bJbDark);
        if (bJbDark) {
            m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(rcBox, 4.0f, 4.0f), bJbDark);
        }

        ID2D1LinearGradientBrush* pJbGrad = nullptr;
        ID2D1GradientStopCollection* pStops = nullptr;
        D2D1_GRADIENT_STOP stops[4] = {
            { 0.0f, D2D1::ColorF(0.99f, 0.16f, 0.34f) },
            { 0.35f, D2D1::ColorF(1.00f, 0.52f, 0.10f) },
            { 0.70f, D2D1::ColorF(1.00f, 0.85f, 0.00f) },
            { 1.0f, D2D1::ColorF(0.13f, 0.59f, 0.95f) }
        };
        if (SUCCEEDED(m_pRenderTarget->CreateGradientStopCollection(stops, 4, &pStops)) && pStops) {
            m_pRenderTarget->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(D2D1::Point2F(rcBox.right, rcBox.top), D2D1::Point2F(rcBox.left, rcBox.bottom)),
                pStops, &pJbGrad
            );
            pStops->Release();
        }
        if (pJbGrad) {
            D2D1_POINT_2F pBand[4] = {
                D2D1::Point2F(rcBox.left + (rcBox.right - rcBox.left) * 0.40f, rcBox.top),
                D2D1::Point2F(rcBox.right, rcBox.top),
                D2D1::Point2F(rcBox.right, rcBox.bottom - (rcBox.bottom - rcBox.top) * 0.30f),
                D2D1::Point2F(rcBox.left, rcBox.bottom)
            };
            FillPoly(pBand, 4, pJbGrad);
            pJbGrad->Release();
        }

        D2D1_RECT_F rcInner = D2D1::RectF(rcBox.left + 3.5f, rcBox.top + 3.5f, rcBox.right - 3.5f, rcBox.bottom - 3.5f);
        if (bJbDark) {
            m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(rcInner, 2.0f, 2.0f), bJbDark);
            bJbDark->Release();
        }

        ID2D1SolidColorBrush* bWhite = nullptr;
        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f), &bWhite);
        if (bWhite) {
            if (m_fonts.button) {
                m_pRenderTarget->DrawText(L"IJ", 2, m_fonts.button,
                    D2D1::RectF(rcInner.left, rcInner.top + 1.0f, rcInner.right, rcInner.bottom - 6.0f), bWhite);
            }
            m_pRenderTarget->DrawLine(
                D2D1::Point2F(rcInner.left + 5.0f, rcInner.bottom - 4.0f),
                D2D1::Point2F(rcInner.left + 14.0f, rcInner.bottom - 4.0f),
                bWhite, 1.8f
            );
            bWhite->Release();
        }

    } else if (ideIndex == 2) {
        // --- 3. Visual Studio Official Purple Infinity Ribbon ---
        float x0 = rc.left + 2.0f;
        float y0 = rc.top + 2.0f;
        float rw = w - 4.0f;
        float rh = h - 4.0f;

        // Left lobe (Medium violet)
        ID2D1SolidColorBrush* bLeftLobe = nullptr;
        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.48f, 0.22f, 0.70f), &bLeftLobe);
        D2D1_POINT_2F pLeft[5] = {
            D2D1::Point2F(x0 + rw * 0.38f, y0 + rh * 0.12f),
            D2D1::Point2F(x0 + rw * 0.08f, y0 + rh * 0.32f),
            D2D1::Point2F(x0 + rw * 0.08f, y0 + rh * 0.68f),
            D2D1::Point2F(x0 + rw * 0.38f, y0 + rh * 0.88f),
            D2D1::Point2F(x0 + rw * 0.62f, y0 + rh * 0.50f)
        };
        FillPoly(pLeft, 5, bLeftLobe);

        // Right back strip (deep violet)
        ID2D1SolidColorBrush* bBackStrip = nullptr;
        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.30f, 0.10f, 0.48f), &bBackStrip);
        D2D1_POINT_2F pBackStrip[4] = {
            D2D1::Point2F(x0 + rw * 0.66f, y0 + rh * 0.12f),
            D2D1::Point2F(x0 + rw * 0.92f, y0 + rh * 0.28f),
            D2D1::Point2F(x0 + rw * 0.92f, y0 + rh * 0.72f),
            D2D1::Point2F(x0 + rw * 0.66f, y0 + rh * 0.88f)
        };
        FillPoly(pBackStrip, 4, bBackStrip);

        // Right front crossover ribbon (vibrant glowing purple)
        ID2D1SolidColorBrush* bFrontPurple = nullptr;
        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.66f, 0.38f, 0.94f), &bFrontPurple);
        D2D1_POINT_2F pRightFront[5] = {
            D2D1::Point2F(x0 + rw * 0.66f, y0 + rh * 0.12f),
            D2D1::Point2F(x0 + rw * 0.36f, y0 + rh * 0.50f),
            D2D1::Point2F(x0 + rw * 0.66f, y0 + rh * 0.88f),
            D2D1::Point2F(x0 + rw * 0.92f, y0 + rh * 0.72f),
            D2D1::Point2F(x0 + rw * 0.92f, y0 + rh * 0.28f)
        };
        FillPoly(pRightFront, 5, bFrontPurple);

        if (bLeftLobe) bLeftLobe->Release();
        if (bBackStrip) bBackStrip->Release();
        if (bFrontPurple) bFrontPurple->Release();

    } else {
        // --- 4. Custom (Özel) Shortcut Key Badge ---
        D2D1_RECT_F rcBox = D2D1::RectF(rc.left + 2.0f, rc.top + 2.0f, rc.right - 2.0f, rc.bottom - 2.0f);
        m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(rcBox, 4.0f, 4.0f), m_bCardHover);
        m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(rcBox, 4.0f, 4.0f), m_bAccent, 1.2f);
        if (m_fonts.button) {
            m_pRenderTarget->DrawText(L"⚡", 1, m_fonts.button, rcBox, m_bAccent);
        }
    }
}

void SettingsDialog::RenderColorWheelPopover(float cx, float cy) {
    float cardW = 320.0f;
    float cardH = 340.0f;
    float px = cx;
    float py = cy;
    RECT clientRc;
    GetClientRect(m_hwnd, &clientRc);
    if (px + cardW > (float)clientRc.right - 10.0f) px = (float)clientRc.right - cardW - 10.0f;
    if (py + cardH > (float)clientRc.bottom - 10.0f) py = (float)clientRc.bottom - cardH - 10.0f;

    m_rcColorPickerCard = D2D1::RectF(px, py, px + cardW, py + cardH);

    // Subtle drop shadow + card
    KaisekiTheme::DrawCard(m_pRenderTarget, m_rcColorPickerCard, m_bCardHover, m_bAccent, 8.0f, 1.4f);

    // Header
    const wchar_t* titleStr = (m_colorPickerTarget == 0) ? L"Vurgu Rengi Seçici (Accent)" : L"Arka Plan Yüzey Rengi (Background)";
    if (m_fonts.subhead) {
        m_pRenderTarget->DrawText(titleStr, (UINT32)wcslen(titleStr), m_fonts.subhead,
            D2D1::RectF(px + 14.0f, py + 12.0f, px + cardW - 36.0f, py + 32.0f), m_bTextPrimary);
    }
    m_rcColorPickerClose = D2D1::RectF(px + cardW - 30.0f, py + 10.0f, px + cardW - 10.0f, py + 30.0f);
    m_pRenderTarget->DrawText(L"✕", 1, m_fonts.button, m_rcColorPickerClose, m_bTextMuted);

    // 2D Saturation / Value Gradient Box
    float boxX = px + 14.0f;
    float boxY = py + 38.0f;
    float boxW = cardW - 28.0f;
    float boxH = 150.0f;
    m_rcColorSatValBox = D2D1::RectF(boxX, boxY, boxX + boxW, boxY + boxH);

    D2D1_COLOR_F pureHue = KaisekiTheme::HsvToRgb(m_pickerHue, 1.0f, 1.0f);
    ID2D1LinearGradientBrush* pHoriz = nullptr;
    ID2D1GradientStopCollection* pStopsH = nullptr;
    D2D1_GRADIENT_STOP stopsH[2] = {
        { 0.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f) },
        { 1.0f, pureHue }
    };
    if (SUCCEEDED(m_pRenderTarget->CreateGradientStopCollection(stopsH, 2, &pStopsH)) && pStopsH) {
        m_pRenderTarget->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(D2D1::Point2F(boxX, boxY), D2D1::Point2F(boxX + boxW, boxY)),
            pStopsH, &pHoriz
        );
        pStopsH->Release();
    }
    if (pHoriz) {
        m_pRenderTarget->FillRectangle(&m_rcColorSatValBox, pHoriz);
        pHoriz->Release();
    }

    ID2D1LinearGradientBrush* pVert = nullptr;
    ID2D1GradientStopCollection* pStopsV = nullptr;
    D2D1_GRADIENT_STOP stopsV[2] = {
        { 0.0f, D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f) },
        { 1.0f, D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f) }
    };
    if (SUCCEEDED(m_pRenderTarget->CreateGradientStopCollection(stopsV, 2, &pStopsV)) && pStopsV) {
        m_pRenderTarget->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(D2D1::Point2F(boxX, boxY), D2D1::Point2F(boxX, boxY + boxH)),
            pStopsV, &pVert
        );
        pStopsV->Release();
    }
    if (pVert) {
        m_pRenderTarget->FillRectangle(&m_rcColorSatValBox, pVert);
        pVert->Release();
    }
    m_pRenderTarget->DrawRectangle(&m_rcColorSatValBox, m_bBorderSubtle, 1.0f);

    // Indicator thumb in 2D box
    float indX = boxX + m_pickerSat * boxW;
    float indY = boxY + (1.0f - m_pickerVal) * boxH;
    ID2D1SolidColorBrush* bWhite = nullptr;
    ID2D1SolidColorBrush* bBlack = nullptr;
    m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &bWhite);
    m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f), &bBlack);
    if (bWhite && bBlack) {
        m_pRenderTarget->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(indX, indY), 6.0f, 6.0f), bBlack, 2.0f);
        m_pRenderTarget->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(indX, indY), 5.0f, 5.0f), bWhite, 1.5f);
        bWhite->Release();
        bBlack->Release();
    }

    // 1D Rainbow Hue Slider Bar
    float hueY = boxY + boxH + 12.0f;
    float hueH = 18.0f;
    m_rcColorHueSlider = D2D1::RectF(boxX, hueY, boxX + boxW, hueY + hueH);

    ID2D1LinearGradientBrush* pHue = nullptr;
    ID2D1GradientStopCollection* pStopsHue = nullptr;
    D2D1_GRADIENT_STOP stopsHue[7] = {
        { 0.00f, D2D1::ColorF(1.0f, 0.0f, 0.0f) },
        { 0.17f, D2D1::ColorF(1.0f, 1.0f, 0.0f) },
        { 0.33f, D2D1::ColorF(0.0f, 1.0f, 0.0f) },
        { 0.50f, D2D1::ColorF(0.0f, 1.0f, 1.0f) },
        { 0.67f, D2D1::ColorF(0.0f, 0.0f, 1.0f) },
        { 0.83f, D2D1::ColorF(1.0f, 0.0f, 1.0f) },
        { 1.00f, D2D1::ColorF(1.0f, 0.0f, 0.0f) }
    };
    if (SUCCEEDED(m_pRenderTarget->CreateGradientStopCollection(stopsHue, 7, &pStopsHue)) && pStopsHue) {
        m_pRenderTarget->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(D2D1::Point2F(boxX, hueY), D2D1::Point2F(boxX + boxW, hueY)),
            pStopsHue, &pHue
        );
        pStopsHue->Release();
    }
    if (pHue) {
        m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(m_rcColorHueSlider, 4.0f, 4.0f), pHue);
        m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(m_rcColorHueSlider, 4.0f, 4.0f), m_bBorderSubtle, 1.0f);
        pHue->Release();
    }

    // Thumb for Hue slider
    float hueThumbX = boxX + (m_pickerHue / 360.0f) * boxW;
    D2D1_RECT_F rcThumb = D2D1::RectF(hueThumbX - 3.0f, hueY - 2.0f, hueThumbX + 3.0f, hueY + hueH + 2.0f);
    m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(rcThumb, 2.0f, 2.0f), m_bTextPrimary);
    m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(rcThumb, 2.0f, 2.0f), m_bCard, 1.0f);

    // Preview Swatch + Hex Input Display
    float prevY = hueY + hueH + 12.0f;
    m_rcColorPreviewBox = D2D1::RectF(boxX, prevY, boxX + 44.0f, prevY + 36.0f);
    ID2D1SolidColorBrush* bCur = nullptr;
    D2D1_COLOR_F curCol = (m_colorPickerTarget == 0) ? m_customAccentColor : m_customBgColor;
    m_pRenderTarget->CreateSolidColorBrush(curCol, &bCur);
    if (bCur) {
        m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(m_rcColorPreviewBox, 5.0f, 5.0f), bCur);
        m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(m_rcColorPreviewBox, 5.0f, 5.0f), m_bBorderSubtle, 1.2f);
        bCur->Release();
    }

    wchar_t hexBuf[32];
    uint8_t r8 = (uint8_t)(curCol.r * 255.0f);
    uint8_t g8 = (uint8_t)(curCol.g * 255.0f);
    uint8_t b8 = (uint8_t)(curCol.b * 255.0f);
    swprintf_s(hexBuf, L"#%02X%02X%02X", r8, g8, b8);

    D2D1_RECT_F rcHexBox = D2D1::RectF(boxX + 52.0f, prevY, boxX + 170.0f, prevY + 36.0f);
    m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(rcHexBox, 5.0f, 5.0f), m_bWell);
    m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(rcHexBox, 5.0f, 5.0f), m_bBorderSubtle, 1.0f);
    if (m_fonts.button) {
        m_pRenderTarget->DrawText(hexBuf, (UINT32)wcslen(hexBuf), m_fonts.button,
            D2D1::RectF(rcHexBox.left + 8.0f, prevY + 8.0f, rcHexBox.right, rcHexBox.bottom), m_bTextPrimary);
    }

    wchar_t rgbBuf[64];
    swprintf_s(rgbBuf, L"RGB(%d, %d, %d)", r8, g8, b8);
    if (m_fonts.smallText) {
        m_pRenderTarget->DrawText(rgbBuf, (UINT32)wcslen(rgbBuf), m_fonts.smallText,
            D2D1::RectF(boxX + 176.0f, prevY + 10.0f, boxX + boxW, prevY + 32.0f), m_bTextSecondary);
    }

    // Palette preset swatches dots
    float dotY = prevY + 44.0f;
    if (m_fonts.smallText) {
        m_pRenderTarget->DrawText(L"Hızlı Renkler:", 13, m_fonts.smallText,
            D2D1::RectF(boxX, dotY, boxX + 75.0f, dotY + 22.0f), m_bTextMuted);
    }
    static const D2D1_COLOR_F popPresets[8] = {
        D2D1::ColorF(1.0f, 0.50f, 0.72f), // Sakura
        D2D1::ColorF(0.65f, 0.37f, 0.92f), // Hasu Mor
        D2D1::ColorF(0.18f, 0.80f, 0.44f), // Emerald
        D2D1::ColorF(0.00f, 0.75f, 1.00f), // Cyan
        D2D1::ColorF(1.00f, 0.75f, 0.00f), // Gold
        D2D1::ColorF(1.00f, 0.28f, 0.34f), // Torii Red
        D2D1::ColorF(0.09f, 0.10f, 0.12f), // Dark Charcoal
        D2D1::ColorF(0.04f, 0.04f, 0.05f)  // OLED
    };
    float dotStartX = boxX + 80.0f;
    for (int p = 0; p < 8; ++p) {
        m_rcColorPresetDots[p] = D2D1::RectF(dotStartX + p * 26.0f, dotY, dotStartX + p * 26.0f + 20.0f, dotY + 20.0f);
        ID2D1SolidColorBrush* bDot = nullptr;
        m_pRenderTarget->CreateSolidColorBrush(popPresets[p], &bDot);
        if (bDot) {
            m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(m_rcColorPresetDots[p], 10.0f, 10.0f), bDot);
            m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(m_rcColorPresetDots[p], 10.0f, 10.0f), m_bBorderSubtle, 1.0f);
            bDot->Release();
        }
    }
}

void SettingsDialog::OnRender() {
    if (!m_pRenderTarget) {
        if (FAILED(CreateDeviceResources())) return;
    }

    float w = (float)m_width;
    float h = (float)m_height;
    if (m_hwnd) {
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        if (rc.right > rc.left && rc.bottom > rc.top) {
            w = (float)(rc.right - rc.left) * (96.0f / m_dpiX);
            h = (float)(rc.bottom - rc.top) * (96.0f / m_dpiY);
        }
    }

    m_pRenderTarget->BeginDraw();
    m_pRenderTarget->Clear(m_colors.canvasBg);

    D2D1_RECT_F rcModal = D2D1::RectF(0, 0, w, h);
    KaisekiTheme::DrawCard(m_pRenderTarget, rcModal, m_bCard, m_bAccent, 0.0f, 1.0f);

    // Modal Header (App Icon in Title)
    if (m_pAppLogoBitmap) {
        D2D1_RECT_F rcAppIcon = D2D1::RectF(16.0f, 10.0f, 44.0f, 38.0f);
        m_pRenderTarget->DrawBitmap(m_pAppLogoBitmap, &rcAppIcon);
    } else {
        VectorIcons::DrawKamon(m_pRenderTarget, 30.0f, 24.0f, 12.0f, m_bAccent);
    }

    if (m_fonts.title) {
        const wchar_t* settModalTitle = LocW(StringId::Settings_Title);
        m_pRenderTarget->DrawText(settModalTitle, (UINT32)wcslen(settModalTitle), m_fonts.title,
            D2D1::RectF(52.0f, 14.0f, 500.0f, 38.0f), m_bTextPrimary);
    }

    m_rcSettingsClose = D2D1::RectF(w - 46.0f, 0.0f, w, 44.0f);
    if (m_hoverClose) {
        ID2D1SolidColorBrush* bCloseBg = nullptr;
        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.85f, 0.15f, 0.20f, 1.0f), &bCloseBg);
        if (bCloseBg) {
            m_pRenderTarget->FillRectangle(&m_rcSettingsClose, bCloseBg);
            bCloseBg->Release();
        }
        VectorIcons::DrawWinClose(m_pRenderTarget, m_rcSettingsClose, m_bTextPrimary);
    } else {
        VectorIcons::DrawWinClose(m_pRenderTarget, m_rcSettingsClose, m_bTextSecondary);
    }

    m_pRenderTarget->DrawLine(D2D1::Point2F(0, 44.0f), D2D1::Point2F(w, 44.0f), m_bBorderSubtle, 1.0f);

    // Left Sub-Tabs Rail (Width 230px)
    float railW = 230.0f;
    D2D1_RECT_F rcTabsRail = D2D1::RectF(0, 44.0f, railW, h);
    m_pRenderTarget->FillRectangle(&rcTabsRail, m_bPanel);
    m_pRenderTarget->DrawLine(D2D1::Point2F(railW, 44.0f), D2D1::Point2F(railW, h), m_bBorderSubtle, 1.0f);

    const wchar_t* subTabs[7] = {
        LocW(StringId::Settings_TabGeneral),      // 0: Genel
        LocW(StringId::Settings_TabTheme),        // 1: Görünüm
        LocW(StringId::Settings_TabLanguage),     // 2: Dil
        LocW(StringId::Settings_TabTools),        // 3: Android Araçları
        LocW(StringId::Settings_TabDecompile),    // 4: APK İşlemleri
        LocW(StringId::Settings_TabHotkeys),      // 5: Kısayollar
        LocW(StringId::Settings_TabAdvanced)      // 6: Gelişmiş ve Tanılama
    };
    float tabY = 56.0f;
    float tabH = 38.0f;

    for (int i = 0; i < 7; ++i) {
        m_rcSettingsTabs[i] = D2D1::RectF(8.0f, tabY, railW - 8.0f, tabY + tabH);
        bool isAct = (m_settingsTab == i);

        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcSettingsTabs[i], isAct ? m_bCardHover : m_bPanel, isAct ? m_bAccent : nullptr, 6.0f);

        if (m_fonts.button) {
            m_pRenderTarget->DrawText(subTabs[i], (UINT32)wcslen(subTabs[i]), m_fonts.button,
                D2D1::RectF(18.0f, tabY + 10.0f, railW - 12.0f, tabY + tabH),
                isAct ? m_bTextPrimary : m_bTextSecondary);
        }
        tabY += 44.0f;
    }

    // Right Content Area
    float contentX = railW + 28.0f;
    float contentY = 64.0f;
    float contentW = w - contentX - 28.0f;

    if (m_settingsTab == 0) {
        // TAB 0: GENEL & PROJE
        if (m_fonts.subhead) {
            m_pRenderTarget->DrawText(L"1. Varsayılan Çalışma Alanı (Workspace):", 40, m_fonts.subhead,
                D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 22.0f), m_bTextPrimary);
        }
        contentY += 28.0f;

        std::wstring ws = SettingsManager::MaskUserPath(SettingsManager::Get().settings.workspacePath);
        D2D1_RECT_F rcWsBox = D2D1::RectF(contentX, contentY, contentX + contentW - 130.0f, contentY + 36.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, rcWsBox, m_bWell, m_bBorderSubtle, 5.0f);
        if (m_fonts.body) {
            m_pRenderTarget->DrawText(ws.c_str(), (UINT32)ws.length(), m_fonts.body,
                D2D1::RectF(rcWsBox.left + 10.0f, contentY + 8.0f, rcWsBox.right - 10.0f, rcWsBox.bottom), m_bTextPrimary);
        }

        m_rcBtnSelectWorkspace = D2D1::RectF(contentX + contentW - 120.0f, contentY, contentX + contentW, contentY + 36.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBtnSelectWorkspace, m_bCardHover, m_bAccent, 5.0f);
        if (m_fonts.button) {
            m_pRenderTarget->DrawText(L"📂 Klasör Seç", 13, m_fonts.button,
                D2D1::RectF(m_rcBtnSelectWorkspace.left + 8.0f, contentY + 8.0f, m_rcBtnSelectWorkspace.right, m_rcBtnSelectWorkspace.bottom), m_bTextPrimary);
        }
        contentY += 52.0f;

        if (m_fonts.subhead) {
            m_pRenderTarget->DrawText(L"2. Oturum ve Proje Hatırlama:", 30, m_fonts.subhead,
                D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 22.0f), m_bTextPrimary);
        }
        contentY += 28.0f;
        m_rcToggleRestoreWorkspace = D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 44.0f);
        DrawToggleSwitch(m_rcToggleRestoreWorkspace, SettingsManager::Get().settings.restoreWorkspaceOnStartup, m_animAutoWorkspace,
            L"Önceki Çalışma Alanını Otomatik Yükle", L"Açılışta son açık APK projesini ve sekmeleri geri yükler.");
        contentY += 56.0f;

        if (m_fonts.subhead) {
            m_pRenderTarget->DrawText(L"3. Windows Dosya İlişkilendirmesi:", 33, m_fonts.subhead,
                D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 22.0f), m_bTextPrimary);
        }
        contentY += 28.0f;
        m_rcBtnAssocKproj = D2D1::RectF(contentX, contentY, contentX + 380.0f, contentY + 40.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBtnAssocKproj, m_bCardHover, m_bAccent, 6.0f);
        if (m_fonts.button) {
            const wchar_t* szAssoc = L"🔗  .kproj Dosyalarını Kaiseki ile İlişkilendir";
            m_pRenderTarget->DrawText(szAssoc, (UINT32)wcslen(szAssoc), m_fonts.button,
                D2D1::RectF(m_rcBtnAssocKproj.left + 14.0f, contentY + 10.0f, m_rcBtnAssocKproj.right, contentY + 36.0f), m_bTextPrimary);
        }
        contentY += 52.0f;

        if (m_fonts.subhead) {
            m_pRenderTarget->DrawText(L"4. Yapılandırma Dosyası (%APPDATA%):", 35, m_fonts.subhead,
                D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 22.0f), m_bTextPrimary);
        }
        contentY += 28.0f;
        std::wstring cfgPath = L"%APPDATA%\\Kaiseki\\settings.xml";
        if (m_fonts.code) {
            m_pRenderTarget->DrawText(cfgPath.c_str(), (UINT32)cfgPath.length(), m_fonts.code,
                D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 24.0f), m_bAccent);
        }

    } else if (m_settingsTab == 1) {
        // TAB 1: GÖRÜNÜM & TEMA
        if (m_fonts.subhead) {
            m_pRenderTarget->DrawText(L"1. Vurgu Rengi (Accent Palette):", 31, m_fonts.subhead,
                D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 22.0f), m_bTextPrimary);
        }
        contentY += 26.0f;

        // Custom accent color preview square
        m_rcAccentPreviewBox = D2D1::RectF(contentX, contentY, contentX + 42.0f, contentY + 42.0f);
        m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(m_rcAccentPreviewBox, 6.0f, 6.0f), m_bAccent);
        m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(m_rcAccentPreviewBox, 6.0f, 6.0f), m_bTextPrimary, 1.5f);

        wchar_t hexAcc[128];
        swprintf_s(hexAcc, L"Seçili Renk: #%02X%02X%02X  (Tıklayarak Renk Seçiciyi Açın)",
            (uint8_t)(m_customAccentColor.r * 255.0f), (uint8_t)(m_customAccentColor.g * 255.0f), (uint8_t)(m_customAccentColor.b * 255.0f));
        if (m_fonts.body) {
            m_pRenderTarget->DrawText(hexAcc, (UINT32)wcslen(hexAcc), m_fonts.body,
                D2D1::RectF(contentX + 54.0f, contentY + 11.0f, contentX + contentW, contentY + 35.0f), m_bTextPrimary);
        }
        contentY += 50.0f;

        // 12 Curated Preset Accent Swatches
        static const struct { const wchar_t* name; D2D1_COLOR_F color; } swatches[12] = {
            { L"Sakura Pembe", D2D1::ColorF(1.0f, 0.50f, 0.72f) },
            { L"Pastel Pembe", D2D1::ColorF(0.96f, 0.88f, 0.98f) },
            { L"Torii Kırmızı", D2D1::ColorF(1.0f, 0.28f, 0.34f) },
            { L"Kintsugi Altın", D2D1::ColorF(0.95f, 0.77f, 0.06f) },
            { L"Matcha Yeşil", D2D1::ColorF(0.18f, 0.80f, 0.44f) },
            { L"Kanagawa Mavi", D2D1::ColorF(0.00f, 0.48f, 1.0f) },
            { L"Siyan (Cyan)", D2D1::ColorF(0.00f, 0.82f, 1.0f) },
            { L"Hasu Mor", D2D1::ColorF(0.65f, 0.37f, 0.92f) },
            { L"Gümüş Gri", D2D1::ColorF(0.70f, 0.72f, 0.75f) },
            { L"Asahi Turuncu", D2D1::ColorF(1.0f, 0.45f, 0.15f) },
            { L"Zümrüt Yeşil", D2D1::ColorF(0.00f, 0.80f, 0.55f) },
            { L"Kehribar Sarı", D2D1::ColorF(1.0f, 0.75f, 0.05f) }
        };

        float swW = 104.0f;
        float swH = 40.0f;
        float gapX = 12.0f;
        float gapY = 8.0f;

        for (int i = 0; i < 12; ++i) {
            int row = i / 6;
            int col = i % 6;
            float sx = contentX + col * (swW + gapX);
            float sy = contentY + row * (swH + gapY);
            m_rcAccentSwatches[i] = D2D1::RectF(sx, sy, sx + swW, sy + swH);

            ID2D1SolidColorBrush* bSw = nullptr;
            m_pRenderTarget->CreateSolidColorBrush(swatches[i].color, &bSw);
            if (bSw) {
                m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(sx, sy, sx + swW, sy + 18.0f), 4.0f, 4.0f), bSw);
                bSw->Release();
            }

            m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(sx, sy + 18.0f, sx + swW, sy + swH), 4.0f, 4.0f), m_bCard);
            m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(m_rcAccentSwatches[i], 4.0f, 4.0f), m_bBorderSubtle, 1.0f);

            if (m_fonts.smallText) {
                m_pRenderTarget->DrawText(swatches[i].name, (UINT32)wcslen(swatches[i].name), m_fonts.smallText,
                    D2D1::RectF(sx + 2.0f, sy + 21.0f, sx + swW - 2.0f, sy + swH), m_bTextPrimary);
            }
        }
        contentY += 2 * (swH + gapY) + 18.0f;

        // 2. Background Surface Shades
        if (m_fonts.subhead) {
            m_pRenderTarget->DrawText(L"2. Arka Plan Yüzey Tonları (Background Palette):", 47, m_fonts.subhead,
                D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 22.0f), m_bTextPrimary);
        }
        contentY += 26.0f;

        // Background Preview Box
        m_rcBgPreviewBox = D2D1::RectF(contentX, contentY, contentX + 42.0f, contentY + 42.0f);
        ID2D1SolidColorBrush* bBgCur = nullptr;
        m_pRenderTarget->CreateSolidColorBrush(m_customBgColor, &bBgCur);
        if (bBgCur) {
            m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(m_rcBgPreviewBox, 6.0f, 6.0f), bBgCur);
            m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(m_rcBgPreviewBox, 6.0f, 6.0f), m_bTextPrimary, 1.5f);
            bBgCur->Release();
        }

        wchar_t hexBg[128];
        swprintf_s(hexBg, L"Seçili Renk: #%02X%02X%02X  (Tıklayarak Renk Seçiciyi Açın)",
            (uint8_t)(m_customBgColor.r * 255.0f), (uint8_t)(m_customBgColor.g * 255.0f), (uint8_t)(m_customBgColor.b * 255.0f));
        if (m_fonts.body) {
            m_pRenderTarget->DrawText(hexBg, (UINT32)wcslen(hexBg), m_fonts.body,
                D2D1::RectF(contentX + 54.0f, contentY + 11.0f, contentX + contentW, contentY + 35.0f), m_bTextPrimary);
        }
        contentY += 50.0f;

        // 8 Preset Background Palette Cards (matching accent swatch style)
        static const struct { const wchar_t* name; D2D1_COLOR_F color; } bgPresets[8] = {
            { L"Kömür", D2D1::ColorF(0.094f, 0.098f, 0.118f) },
            { L"OLED Siyah", D2D1::ColorF(0.043f, 0.043f, 0.051f) },
            { L"Gece Mavisi", D2D1::ColorF(0.063f, 0.078f, 0.122f) },
            { L"İmparatorluk", D2D1::ColorF(0.094f, 0.067f, 0.102f) },
            { L"Derin Arduvaz", D2D1::ColorF(0.070f, 0.075f, 0.086f) },
            { L"Koyu Obsidyen", D2D1::ColorF(0.055f, 0.067f, 0.090f) },
            { L"Nordik Kar", D2D1::ColorF(0.941f, 0.949f, 0.968f) },
            { L"Fildişi Gündüz", D2D1::ColorF(0.972f, 0.960f, 0.933f) }
        };

        float bgCardW = 104.0f;
        float bgCardH = 40.0f;
        for (int b = 0; b < 8; ++b) {
            int row = b / 4;
            int col = b % 4;
            float bx = contentX + col * (bgCardW + gapX);
            float by = contentY + row * (bgCardH + gapY);
            m_rcBgShades[b] = D2D1::RectF(bx, by, bx + bgCardW, by + bgCardH);
            bool isCur = (m_currentBgShade == (BackgroundShade)b);

            ID2D1SolidColorBrush* bBgCard = nullptr;
            m_pRenderTarget->CreateSolidColorBrush(bgPresets[b].color, &bBgCard);
            if (bBgCard) {
                m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(bx, by, bx + bgCardW, by + 18.0f), 4.0f, 4.0f), bBgCard);
                bBgCard->Release();
            }

            m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(bx, by + 18.0f, bx + bgCardW, by + bgCardH), 4.0f, 4.0f), m_bCard);
            m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(m_rcBgShades[b], 4.0f, 4.0f), isCur ? m_bAccent : m_bBorderSubtle, isCur ? 1.8f : 1.0f);

            if (m_fonts.smallText) {
                m_pRenderTarget->DrawText(bgPresets[b].name, (UINT32)wcslen(bgPresets[b].name), m_fonts.smallText,
                    D2D1::RectF(bx + 2.0f, by + 21.0f, bx + bgCardW - 2.0f, by + bgCardH),
                    isCur ? m_bAccent : m_bTextPrimary);
            }
        }
        contentY += 2 * (bgCardH + gapY) + 18.0f;

        // 3. Code Editor Preferences
        if (m_fonts.subhead) {
            m_pRenderTarget->DrawText(L"3. Kod Editörü Tercihleri:", 26, m_fonts.subhead,
                D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 22.0f), m_bTextPrimary);
        }
        contentY += 28.0f;

        m_rcToggleLineNumbers = D2D1::RectF(contentX, contentY, contentX + 240.0f, contentY + 36.0f);
        DrawToggleSwitch(m_rcToggleLineNumbers, SettingsManager::Get().settings.showLineNumbers, m_animLineNumbers, L"Satır Numaraları", nullptr);

        m_rcToggleMinimap = D2D1::RectF(contentX + 260.0f, contentY, contentX + 500.0f, contentY + 36.0f);
        DrawToggleSwitch(m_rcToggleMinimap, SettingsManager::Get().settings.showMinimap, m_animMinimap, L"Kod Haritası (Minimap)", nullptr);

    } else if (m_settingsTab == 2) {
        // TAB 2: DIL / LANGUAGE
        if (m_fonts.subhead) {
            m_pRenderTarget->DrawText(L"Uygulama Arayüz Dili (Language):", 32, m_fonts.subhead,
                D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 24.0f), m_bTextPrimary);
        }
        contentY += 28.0f;

        if (m_fonts.body) {
            m_pRenderTarget->DrawText(L"Arayüz metinleri, menüler, sekmeler ve bildirimler anında seçilen dile çevrilir. lang/ klasöründeki XML dosyaları otomatik taranır.", 130, m_fonts.smallText,
                D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 36.0f), m_bTextMuted);
        }
        contentY += 44.0f;

        if (m_fonts.body) {
            m_pRenderTarget->DrawText(L"Uygulama Dili:", 14, m_fonts.body,
                D2D1::RectF(contentX, contentY + 8.0f, contentX + 130.0f, contentY + 34.0f), m_bTextPrimary);
        }

        std::string curCode = KaisekiLang::Get().GetLanguageCode();
        std::wstring curDisplay = (curCode == "tr") ? L"Türkçe (Turkish)" : (curCode == "de") ? L"Deutsch (German)" : (curCode == "ja") ? L"日本語 (Japanese)" : L"English (International)";
        const auto& langs = KaisekiLang::Get().GetAvailableLanguages();
        for (const auto& l : langs) {
            if (l.code == curCode) { curDisplay = l.displayName; break; }
        }

        m_rcLangDropdownBtn = D2D1::RectF(contentX + 140.0f, contentY, contentX + 380.0f, contentY + 38.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcLangDropdownBtn, m_bWell, m_bAccent, 6.0f, 1.2f);
        DrawCountryFlag(curCode, D2D1::RectF(m_rcLangDropdownBtn.left + 12.0f, contentY + 11.0f, m_rcLangDropdownBtn.left + 36.0f, contentY + 27.0f));
        if (m_fonts.body) {
            m_pRenderTarget->DrawText(curDisplay.c_str(), (UINT32)curDisplay.length(), m_fonts.body,
                D2D1::RectF(m_rcLangDropdownBtn.left + 44.0f, contentY + 8.0f, m_rcLangDropdownBtn.right - 30.0f, m_rcLangDropdownBtn.bottom), m_bTextPrimary);
            m_pRenderTarget->DrawText(m_langDropdownOpen ? L"▲" : L"▼", 1, m_fonts.smallText,
                D2D1::RectF(m_rcLangDropdownBtn.right - 26.0f, contentY + 10.0f, m_rcLangDropdownBtn.right - 8.0f, m_rcLangDropdownBtn.bottom), m_bAccent);
        }
        contentY += 56.0f;

        if (m_fonts.smallText) {
            const wchar_t* szLangDesc = L"Kaiseki çok dilli arayüz motoru (Türkçe, English, Deutsch, 日本語) desteklemektedir.";
            m_pRenderTarget->DrawText(szLangDesc, (UINT32)wcslen(szLangDesc), m_fonts.smallText,
                D2D1::RectF(contentX, contentY + 8.0f, contentX + contentW, contentY + 28.0f), m_bTextMuted);
        }

        // Render Animated Dropdown Popover if open
        if (m_animLangDropdown > 0.005f) {
            const auto& langs = KaisekiLang::Get().GetAvailableLanguages();
            float popW = m_rcLangDropdownBtn.right - m_rcLangDropdownBtn.left;
            float optH = 34.0f;
            float fullPopH = (float)langs.size() * optH + 8.0f;
            float animH = fullPopH * KaisekiTheme::EaseOutCubic(m_animLangDropdown);
            D2D1_RECT_F rcPop = D2D1::RectF(m_rcLangDropdownBtn.left, m_rcLangDropdownBtn.bottom + 4.0f, m_rcLangDropdownBtn.right, m_rcLangDropdownBtn.bottom + 4.0f + animH);

            m_pRenderTarget->PushAxisAlignedClip(rcPop, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            KaisekiTheme::DrawCard(m_pRenderTarget, rcPop, m_bCardHover, m_bAccent, 6.0f, 1.5f);

            for (size_t i = 0; i < langs.size(); ++i) {
                float oy = m_rcLangDropdownBtn.bottom + 8.0f + (float)i * optH;
                D2D1_RECT_F rcOpt = D2D1::RectF(rcPop.left + 4.0f, oy, rcPop.right - 4.0f, oy + optH);
                bool isSel = (langs[i].code == curCode);
                if (isSel) {
                    m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(rcOpt, 4.0f, 4.0f), m_bCardHover);
                }
                DrawCountryFlag(langs[i].code, D2D1::RectF(rcOpt.left + 10.0f, oy + 8.0f, rcOpt.left + 34.0f, oy + 24.0f));
                if (m_fonts.body) {
                    m_pRenderTarget->DrawText(langs[i].displayName.c_str(), (UINT32)langs[i].displayName.length(), m_fonts.body,
                        D2D1::RectF(rcOpt.left + 42.0f, oy + 7.0f, rcOpt.right - 10.0f, oy + optH), isSel ? m_bAccent : m_bTextPrimary);
                }
            }
            m_pRenderTarget->PopAxisAlignedClip();
        }

    } else if (m_settingsTab == 3) {
        // TAB 3: ANDROID ARAÇLARI
        std::wstring sdkPath = SettingsManager::Get().settings.androidSdkPath;
        if (sdkPath.empty()) sdkPath = SettingsManager::Get().FindAndroidSdkRoot();
        std::wstring adb = SettingsManager::Get().FindAdbPath();
        std::wstring bt = SettingsManager::Get().FindBuildToolsPath();
        std::wstring java = SettingsManager::Get().FindJavaPath();

        auto drawSdkItem = [&](const wchar_t* title, const std::wstring& path, bool ok, D2D1_RECT_F& btnRect) {
            if (m_fonts.subhead) {
                m_pRenderTarget->DrawText(title, (UINT32)wcslen(title), m_fonts.subhead,
                    D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 20.0f), m_bTextPrimary);
            }
            contentY += 24.0f;

            D2D1_RECT_F rcBox = D2D1::RectF(contentX, contentY, contentX + contentW - 130.0f, contentY + 36.0f);
            KaisekiTheme::DrawCard(m_pRenderTarget, rcBox, m_bWell, ok ? m_bSuccess : m_bBorderSubtle, 5.0f);

            if (m_fonts.body) {
                std::wstring dispPath = path.empty() ? L"(Belirtilmedi / Bulunamadı)" :
                    (ok ? (L"✓ " + SettingsManager::MaskUserPath(path)) : (L"✕ " + SettingsManager::MaskUserPath(path)));
                m_pRenderTarget->DrawText(dispPath.c_str(), (UINT32)dispPath.length(), m_fonts.body,
                    D2D1::RectF(rcBox.left + 10.0f, contentY + 8.0f, rcBox.right - 10.0f, rcBox.bottom),
                    ok ? m_bTextPrimary : m_bTextMuted);
            }

            btnRect = D2D1::RectF(contentX + contentW - 120.0f, contentY, contentX + contentW, contentY + 36.0f);
            KaisekiTheme::DrawCard(m_pRenderTarget, btnRect, m_bCardHover, m_bAccent, 5.0f);
            if (m_fonts.button) {
                m_pRenderTarget->DrawText(L"📂 Klasör Seç", 13, m_fonts.button,
                    D2D1::RectF(btnRect.left + 8.0f, contentY + 8.0f, btnRect.right, btnRect.bottom), m_bTextPrimary);
            }
            contentY += 46.0f;
        };

        drawSdkItem(L"1. Android SDK Kök Dizini (Root):", sdkPath, !sdkPath.empty(), m_rcBtnSelectSdk);
        drawSdkItem(L"2. Platform-Tools (ADB & Fastboot):", adb, !adb.empty(), m_rcBtnSelectAdb);
        drawSdkItem(L"3. Build-Tools (apksigner / zipalign):", bt, !bt.empty(), m_rcBtnSelectBuildTools);
        drawSdkItem(L"4. Java JDK Runtime (java.exe):", java, !java.empty(), m_rcBtnSelectJava);

        contentY += 6.0f;
        m_rcBtnDownloadTools = D2D1::RectF(contentX, contentY, contentX + 440.0f, contentY + 46.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBtnDownloadTools, m_bCardHover, m_bAccent, 6.0f, 1.2f);
        if (m_fonts.button) {
            const wchar_t* szDownTools = L"⬇  Android Araçlarını İndir ve Kur...";
            m_pRenderTarget->DrawText(szDownTools, (UINT32)wcslen(szDownTools), m_fonts.button,
                D2D1::RectF(m_rcBtnDownloadTools.left + 16.0f, contentY + 6.0f, m_rcBtnDownloadTools.right, contentY + 26.0f), m_bAccent);
        }
        if (m_fonts.smallText) {
            const wchar_t* szDownSub = L"Resmi Google ADB, Build-Tools ve Cmdline-Tools paketlerini otomatik kurun";
            m_pRenderTarget->DrawText(szDownSub, (UINT32)wcslen(szDownSub), m_fonts.smallText,
                D2D1::RectF(m_rcBtnDownloadTools.left + 16.0f, contentY + 26.0f, m_rcBtnDownloadTools.right, contentY + 42.0f), m_bTextMuted);
        }

    } else if (m_settingsTab == 4) {
        // TAB 4: DEKOMPİLASYON & İMZA
        if (m_fonts.subhead) {
            m_pRenderTarget->DrawText(L"1. Dalvik Bytecode & Dekompilasyon:", 36, m_fonts.subhead,
                D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 22.0f), m_bTextPrimary);
        }
        contentY += 28.0f;

        m_rcToggleDefaultView = D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 38.0f);
        bool defJava = SettingsManager::Get().settings.defaultToJava;
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcToggleDefaultView, m_bWell, defJava ? m_bAccent : m_bBorderSubtle, 5.0f);
        if (m_fonts.body) {
            std::wstring dv = defJava ? L"✔ Varsayılan Görünüm: [Java Kaynak Kodu] (Tıklayarak Smali yapın)" :
                                        L"✕ Varsayılan Görünüm: [Smali Dalvik Bayt Kodu] (Tıklayarak Java yapın)";
            m_pRenderTarget->DrawText(dv.c_str(), (UINT32)dv.length(), m_fonts.body,
                D2D1::RectF(contentX + 12.0f, contentY + 8.0f, contentX + contentW, contentY + 38.0f), defJava ? m_bAccent : m_bTextPrimary);
        }
        contentY += 46.0f;

        m_rcToggleMultidexTree = D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 38.0f);
        bool multi = SettingsManager::Get().settings.multidexMerged;
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcToggleMultidexTree, m_bWell, multi ? m_bSuccess : m_bBorderSubtle, 5.0f);
        if (m_fonts.body) {
            std::wstring mv = multi ? L"✔ Çoklu DEX (Multidex): classes.dex, classes2.dex tek ağaçta birleştirildi (Aktif)" :
                                      L"✕ Çoklu DEX (Multidex): Her DEX ayrı bir kök dal olarak listelenir";
            m_pRenderTarget->DrawText(mv.c_str(), (UINT32)mv.length(), m_fonts.body,
                D2D1::RectF(contentX + 12.0f, contentY + 8.0f, contentX + contentW, contentY + 38.0f), multi ? m_bSuccess : m_bTextMuted);
        }
        contentY += 46.0f;

        // Hardware Acceleration & Multi-Threading Button
        m_rcBtnDecompileThreads = D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 42.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBtnDecompileThreads, m_bWell, m_bAccent, 5.0f);
        if (m_fonts.body) {
            int curTh = SettingsManager::Get().settings.decompileThreads;
            unsigned int hwCores = std::thread::hardware_concurrency();
            std::wstring thTxt;
            if (curTh == 0) {
                thTxt = L"⚡ Donanım Hızlandırması: [Otomatik • " + std::to_wstring(hwCores) + L" Çekirdek Aktif] — Tıklayarak Değiştirin";
            } else {
                thTxt = L"⚡ Donanım Hızlandırması: [" + std::to_wstring(curTh) + L" İş Parçacığı / Çekirdek Sabit] — Tıklayarak Değiştirin";
            }
            m_pRenderTarget->DrawText(thTxt.c_str(), (UINT32)thTxt.length(), m_fonts.body,
                D2D1::RectF(contentX + 12.0f, contentY + 10.0f, contentX + contentW, contentY + 42.0f), m_bAccent);
        }
        contentY += 54.0f;

        if (m_fonts.subhead) {
            m_pRenderTarget->DrawText(L"2. Dijital İmza Şemaları (Signature Schemes) & Paketleme:", 57, m_fonts.subhead,
                D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 22.0f), m_bTextPrimary);
        }
        contentY += 28.0f;

        m_rcToggleSchemeV1 = D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 36.0f);
        DrawToggleSwitch(m_rcToggleSchemeV1, SettingsManager::Get().settings.signSchemeV1, m_animSchemeV1,
            L"Scheme v1 (JAR İmzalama)", L"META-INF MANIFEST.MF & CERT.RSA (Android 2.3+)");
        contentY += 42.0f;

        m_rcToggleSchemeV2 = D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 36.0f);
        DrawToggleSwitch(m_rcToggleSchemeV2, SettingsManager::Get().settings.signSchemeV2, m_animSchemeV2,
            L"Scheme v2 (APK Signature Scheme v2)", L"APK Signing Block 42 doğrulaması (Android 7.0+)");
        contentY += 42.0f;

        m_rcToggleSchemeV3 = D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 36.0f);
        DrawToggleSwitch(m_rcToggleSchemeV3, SettingsManager::Get().settings.signSchemeV3, m_animSchemeV3,
            L"Scheme v3 (Key Rotation Desteği)", L"Android 9.0+ Güvenlik Standardı");
        contentY += 42.0f;

        m_rcToggleZipAlign = D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 36.0f);
        DrawToggleSwitch(m_rcToggleZipAlign, SettingsManager::Get().settings.zipalignEnabled, m_animZipAlign,
            L"4-Bayt Hizalaması (ZipAlign)", L"Uncompressed .so ve kaynaklar için 4 bayt bellek hizalaması");

    } else if (m_settingsTab == 5) {
        // TAB 5: KISAYOLLAR & ÖZELLEŞTİRİLEBİLİR TUŞ ATAMALARI
        int km = SettingsManager::Get().settings.hotkeyPreset;
        float presW = (contentW - 30.0f) / 4.0f;

        m_rcPresetVSCode = D2D1::RectF(contentX, contentY, contentX + presW, contentY + 48.0f);
        m_rcPresetIDEA = D2D1::RectF(contentX + presW + 10.0f, contentY, contentX + 2.0f * presW + 10.0f, contentY + 48.0f);
        m_rcPresetVS = D2D1::RectF(contentX + 2.0f * presW + 20.0f, contentY, contentX + 3.0f * presW + 20.0f, contentY + 48.0f);
        m_rcPresetCustom = D2D1::RectF(contentX + 3.0f * presW + 30.0f, contentY, contentX + contentW, contentY + 48.0f);

        auto drawPresetCard = [&](int ideIdx, const D2D1_RECT_F& r, const wchar_t* title, const wchar_t* sub, bool active) {
            KaisekiTheme::DrawCard(m_pRenderTarget, r, active ? m_bCardHover : m_bWell, active ? m_bAccent : m_bBorderSubtle, 6.0f, active ? 1.5f : 1.0f);
            DrawIdeLogo(ideIdx, D2D1::RectF(r.left + 8.0f, r.top + 10.0f, r.left + 36.0f, r.top + 38.0f));
            if (m_fonts.button) {
                m_pRenderTarget->DrawText(title, (UINT32)wcslen(title), m_fonts.button,
                    D2D1::RectF(r.left + 42.0f, r.top + 6.0f, r.right - 8.0f, r.top + 24.0f), active ? m_bAccent : m_bTextPrimary);
            }
            if (m_fonts.smallText) {
                m_pRenderTarget->DrawText(sub, (UINT32)wcslen(sub), m_fonts.smallText,
                    D2D1::RectF(r.left + 42.0f, r.top + 25.0f, r.right - 8.0f, r.bottom), m_bTextMuted);
            }
        };

        drawPresetCard(0, m_rcPresetVSCode, L"VS Code Default", L"Standart Modern Düzen", km == 0);
        drawPresetCard(1, m_rcPresetIDEA, L"IntelliJ IDEA", L"Android Geliştirici Düzeni", km == 1);
        drawPresetCard(2, m_rcPresetVS, L"Visual Studio", L"Klasik C++ Düzeni", km == 2);
        drawPresetCard(3, m_rcPresetCustom, L"Özel (Custom)", L"Kullanıcı Tanımlı Tuşlar", km == 3);
        contentY += 56.0f;

        if (m_fonts.smallText) {
            const wchar_t* tipText = L"💡 İpucu: Bir kısayolu özelleştirmek için tuş rozetine tıklayın, ardından klavyenizden yeni tuş kombinasyonuna basın (örn: Ctrl+Alt+K).";
            m_pRenderTarget->DrawText(tipText, (UINT32)wcslen(tipText), m_fonts.smallText,
                D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 20.0f), m_bAccent);
        }
        contentY += 24.0f;

        float colW = (contentW - 14.0f) * 0.5f;
        D2D1_RECT_F rcCardLeft = D2D1::RectF(contentX, contentY, contentX + colW, contentY + 295.0f);
        D2D1_RECT_F rcCardRight = D2D1::RectF(contentX + colW + 14.0f, contentY, contentX + contentW, contentY + 295.0f);

        KaisekiTheme::DrawCard(m_pRenderTarget, rcCardLeft, m_bWell, m_bBorderSubtle, 6.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, rcCardRight, m_bWell, m_bBorderSubtle, 6.0f);

        if (m_fonts.subhead) {
            const wchar_t* titleL = L"📁 Dosya, Proje ve Düzenleyici";
            const wchar_t* titleR = L"⚡ Arama, Derleme ve Araçlar";
            m_pRenderTarget->DrawText(titleL, (UINT32)wcslen(titleL), m_fonts.subhead,
                D2D1::RectF(rcCardLeft.left + 12.0f, rcCardLeft.top + 8.0f, rcCardLeft.right - 12.0f, rcCardLeft.top + 28.0f), m_bAccent);
            m_pRenderTarget->DrawText(titleR, (UINT32)wcslen(titleR), m_fonts.subhead,
                D2D1::RectF(rcCardRight.left + 12.0f, rcCardRight.top + 8.0f, rcCardRight.right - 12.0f, rcCardRight.top + 28.0f), m_bAccent);
        }

        auto renderKeyTable = [&](const D2D1_RECT_F& card, int startIdx, int count) {
            float rowY = card.top + 34.0f;
            float rowH = 31.0f;
            for (int r = 0; r < count; ++r) {
                int itemIdx = startIdx + r;
                D2D1_RECT_F rcBadge = D2D1::RectF(card.left + 10.0f, rowY + 2.0f, card.left + 130.0f, rowY + rowH - 2.0f);
                m_rcShortcutBadges[itemIdx] = rcBadge;

                bool isEditing = (m_editingShortcutIndex == itemIdx);
                KaisekiTheme::DrawCard(m_pRenderTarget, rcBadge, isEditing ? m_bCardHover : m_bCard, isEditing ? m_bAccent : m_bBorderSubtle, 4.0f, isEditing ? 1.6f : 1.0f);

                std::wstring keyStr = isEditing ? L"Tuşa Basın..." : SettingsManager::Get().settings.GetShortcutKey(itemIdx);
                const wchar_t* descStr = KaisekiSettings::GetActionName(itemIdx);

                if (m_fonts.code) {
                    m_pRenderTarget->DrawText(keyStr.c_str(), (UINT32)keyStr.length(), m_fonts.code,
                        D2D1::RectF(rcBadge.left + 4.0f, rcBadge.top + 3.0f, rcBadge.right - 4.0f, rcBadge.bottom), isEditing ? m_bAccent : m_bTextPrimary);
                }
                if (m_fonts.smallText && descStr) {
                    m_pRenderTarget->DrawText(descStr, (UINT32)wcslen(descStr), m_fonts.smallText,
                        D2D1::RectF(card.left + 138.0f, rowY + 5.0f, card.right - 10.0f, rowY + rowH), m_bTextSecondary);
                }
                rowY += rowH + 1.0f;
            }
        };

        renderKeyTable(rcCardLeft, 0, 8);
        renderKeyTable(rcCardRight, 8, 8);

        contentY += 302.0f;
        m_rcBtnResetHotkeys = D2D1::RectF(contentX, contentY, contentX + 280.0f, contentY + 34.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBtnResetHotkeys, m_bCardHover, m_bBorderSubtle, 5.0f);
        if (m_fonts.button) {
            const wchar_t* szResetHk = L"↺  Seçili Düzen Varsayılanlarına Sıfırla";
            m_pRenderTarget->DrawText(szResetHk, (UINT32)wcslen(szResetHk), m_fonts.button,
                D2D1::RectF(m_rcBtnResetHotkeys.left + 12.0f, contentY + 7.0f, m_rcBtnResetHotkeys.right, m_rcBtnResetHotkeys.bottom), m_bTextPrimary);
        }

    } else if (m_settingsTab == 6) {
        // TAB 6: GELİŞMİŞ & TEŞHİS
        if (m_fonts.subhead) {
            m_pRenderTarget->DrawText(L"Gelişmiş Tanılama ve Sistem Araçları:", 38, m_fonts.subhead,
                D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 22.0f), m_bTextPrimary);
        }
        contentY += 28.0f;

        m_rcToggleVerboseLog = D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 36.0f);
        DrawToggleSwitch(m_rcToggleVerboseLog, SettingsManager::Get().settings.verboseLogging, m_animVerboseLog,
            L"Ayrıntılı Hata Ayıklama Günlüğü (Verbose Logging)", L"Tüm sistem işlemlerini ve ayrıştırma detaylarını konsola döker");
        contentY += 54.0f;

        m_rcBtnCopyDiagnostics = D2D1::RectF(contentX, contentY, contentX + 380.0f, contentY + 42.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBtnCopyDiagnostics, m_bCardHover, m_bAccent, 6.0f);
        if (m_fonts.button) {
            const wchar_t* szDiag = L"📋  Sistem Tanılama Raporunu Panoya Kopyala";
            m_pRenderTarget->DrawText(szDiag, (UINT32)wcslen(szDiag), m_fonts.button,
                D2D1::RectF(m_rcBtnCopyDiagnostics.left + 14.0f, contentY + 10.0f, m_rcBtnCopyDiagnostics.right, contentY + 36.0f), m_bTextPrimary);
        }
        contentY += 56.0f;

        m_rcBtnResetSettings = D2D1::RectF(contentX, contentY, contentX + 380.0f, contentY + 42.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBtnResetSettings, m_bCard, m_bCloseRed, 6.0f);
        if (m_fonts.button) {
            const wchar_t* szReset = L"⚠️  Tüm Ayarları Fabrika Ayarlarına Sıfırla";
            m_pRenderTarget->DrawText(szReset, (UINT32)wcslen(szReset), m_fonts.button,
                D2D1::RectF(m_rcBtnResetSettings.left + 14.0f, contentY + 10.0f, m_rcBtnResetSettings.right, contentY + 36.0f), m_bCloseRed);
        }
    }

    // Render Color Picker Popover on top if open
    if (m_colorPickerPopoverOpen) {
        RenderColorWheelPopover(contentX + 50.0f, 120.0f);
    }

    m_pRenderTarget->EndDraw();
}

bool SettingsDialog::SaveScreenshot(const wchar_t* outPngPath) {
    if (!outPngPath) return false;

    if (!m_pWicFactory) {
        CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_pWicFactory));
    }
    if (!m_pD2DFactory) {
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);
    }
    if (!m_pDWriteFactory) {
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&m_pDWriteFactory);
    }
    if (m_pDWriteFactory && !m_fonts.hero) {
        m_fonts.Initialize(m_pDWriteFactory);
    }

    if (!m_pD2DFactory || !m_pWicFactory) return false;

    UINT w = (UINT)m_width;
    UINT h = (UINT)m_height;

    IWICBitmap* pWicBmp = nullptr;
    HRESULT hr = m_pWicFactory->CreateBitmap(w, h, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnDemand, &pWicBmp);
    if (FAILED(hr)) return false;

    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        0.0f, 0.0f,
        D2D1_RENDER_TARGET_USAGE_NONE,
        D2D1_FEATURE_LEVEL_DEFAULT
    );

    ID2D1RenderTarget* pWicRT = nullptr;
    hr = m_pD2DFactory->CreateWicBitmapRenderTarget(pWicBmp, &props, &pWicRT);
    if (FAILED(hr)) {
        pWicBmp->Release();
        return false;
    }

    ID2D1HwndRenderTarget* savedRT = m_pRenderTarget;
    ID2D1Bitmap* savedLogo = m_pAppLogoBitmap;
    m_pRenderTarget = (ID2D1HwndRenderTarget*)pWicRT;
    m_pAppLogoBitmap = nullptr;
    ApplyTheme();
    CreateBrushes();
    LoadAppLogoBitmap();

    OnRender();

    // Save pWicBmp to PNG file via WIC encoder
    IWICStream* pStream = nullptr;
    IWICBitmapEncoder* pEncoder = nullptr;
    IWICBitmapFrameEncode* pFrame = nullptr;
    bool saved = false;

    hr = m_pWicFactory->CreateStream(&pStream);
    if (SUCCEEDED(hr)) {
        hr = pStream->InitializeFromFilename(outPngPath, GENERIC_WRITE);
    }
    if (SUCCEEDED(hr)) {
        hr = m_pWicFactory->CreateEncoder(GUID_ContainerFormatPng, NULL, &pEncoder);
    }
    if (SUCCEEDED(hr)) {
        hr = pEncoder->Initialize(pStream, WICBitmapEncoderNoCache);
    }
    if (SUCCEEDED(hr)) {
        hr = pEncoder->CreateNewFrame(&pFrame, NULL);
    }
    if (SUCCEEDED(hr)) {
        hr = pFrame->Initialize(NULL);
    }
    if (SUCCEEDED(hr)) {
        pFrame->SetSize(w, h);
        WICPixelFormatGUID pFormat = GUID_WICPixelFormat32bppPBGRA;
        pFrame->SetPixelFormat(&pFormat);
        hr = pFrame->WriteSource(pWicBmp, NULL);
        if (SUCCEEDED(hr)) {
            pFrame->Commit();
            pEncoder->Commit();
            saved = true;
        }
        pFrame->Release();
    }
    if (pEncoder) pEncoder->Release();
    if (pStream) pStream->Release();

    DiscardDeviceResources();
    m_pAppLogoBitmap = savedLogo;
    m_pRenderTarget = savedRT;
    if (m_pRenderTarget) {
        CreateBrushes();
    }

    pWicRT->Release();
    pWicBmp->Release();
    return saved;
}

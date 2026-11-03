#include "SplashScreen.h"
#include "KaisekiModuleManager.h"
#include "SettingsManager.h"
#include "KaisekiTheme.h"
#include <cmath>
#include <algorithm>

#ifdef DrawText
#undef DrawText
#endif

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")

SplashScreen::SplashScreen() {}

SplashScreen::~SplashScreen() {
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
    if (m_hwnd && IsWindow(m_hwnd)) {
        ShowWindow(m_hwnd, SW_HIDE);
        DestroyWindow(m_hwnd);
        m_hwnd = NULL;
    }
    CleanupD2D();
}

void SplashScreen::CleanupD2D() {
    if (m_pRoundRectGeom) { m_pRoundRectGeom->Release(); m_pRoundRectGeom = nullptr; }
    if (m_pLayer) { m_pLayer->Release(); m_pLayer = nullptr; }
    if (m_pBannerBitmap) { m_pBannerBitmap->Release(); m_pBannerBitmap = nullptr; }
    if (m_pBrushSuccess) { m_pBrushSuccess->Release(); m_pBrushSuccess = nullptr; }
    if (m_pBrushErrorBg) { m_pBrushErrorBg->Release(); m_pBrushErrorBg = nullptr; }
    if (m_pBrushError) { m_pBrushError->Release(); m_pBrushError = nullptr; }
    if (m_pBrushSakuraLight) { m_pBrushSakuraLight->Release(); m_pBrushSakuraLight = nullptr; }
    if (m_pBrushSakura) { m_pBrushSakura->Release(); m_pBrushSakura = nullptr; }
    if (m_pBrushWhite) { m_pBrushWhite->Release(); m_pBrushWhite = nullptr; }
    if (m_pBrushPillBorder) { m_pBrushPillBorder->Release(); m_pBrushPillBorder = nullptr; }
    if (m_pBrushPillBg) { m_pBrushPillBg->Release(); m_pBrushPillBg = nullptr; }
    if (m_pBrushBorder) { m_pBrushBorder->Release(); m_pBrushBorder = nullptr; }
    if (m_pBrushBg) { m_pBrushBg->Release(); m_pBrushBg = nullptr; }

    if (m_pTextBadge) { m_pTextBadge->Release(); m_pTextBadge = nullptr; }
    if (m_pTextLibStatus) { m_pTextLibStatus->Release(); m_pTextLibStatus = nullptr; }

    if (m_pDCRenderTarget) { m_pDCRenderTarget->Release(); m_pDCRenderTarget = nullptr; }
    if (m_pWicFactory) { m_pWicFactory->Release(); m_pWicFactory = nullptr; }
    if (m_pDWriteFactory) { m_pDWriteFactory->Release(); m_pDWriteFactory = nullptr; }
    if (m_pD2DFactory) { m_pD2DFactory->Release(); m_pD2DFactory = nullptr; }

    if (m_hdcMem) {
        if (m_hOldBitmap) SelectObject(m_hdcMem, m_hOldBitmap);
        DeleteDC(m_hdcMem);
        m_hdcMem = NULL;
    }
    if (m_hBitmap) {
        DeleteObject(m_hBitmap);
        m_hBitmap = NULL;
    }
    m_pBits = nullptr;
}

HRESULT SplashScreen::LoadBannerBitmap() {
    if (!m_pDCRenderTarget || !m_pWicFactory) return E_FAIL;
    if (m_pBannerBitmap) return S_OK;

    IWICBitmapDecoder* pDecoder = nullptr;

    // 1. Check embedded Win32 RCDATA resource (1001) in EXE or DLL
    HMODULE hMods[] = { GetModuleHandleW(NULL), m_hInstance };
    for (HMODULE hMod : hMods) {
        if (!hMod) continue;
        HRSRC hResInfo = FindResourceW(hMod, MAKEINTRESOURCEW(1001), RT_RCDATA);
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
            exeDir + L"\\res\\banner.png",
            exeDir + L"\\banner.png",
            L"res\\banner.png",
            L"Kaiseki\\res\\banner.png",
            L"..\\Kaiseki\\res\\banner.png",
            L"..\\res\\banner.png",
            L"..\\banner.png",
            L"banner.png"
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

    if (!pDecoder) return E_FAIL;

    IWICBitmapFrameDecode* pFrame = nullptr;
    HRESULT hr = pDecoder->GetFrame(0, &pFrame);
    if (SUCCEEDED(hr)) {
        IWICFormatConverter* pConverter = nullptr;
        hr = m_pWicFactory->CreateFormatConverter(&pConverter);
        if (SUCCEEDED(hr)) {
            hr = pConverter->Initialize(
                pFrame, GUID_WICPixelFormat32bppPBGRA,
                WICBitmapDitherTypeNone, NULL, 0.0f, WICBitmapPaletteTypeCustom
            );
            if (SUCCEEDED(hr)) {
                hr = m_pDCRenderTarget->CreateBitmapFromWicBitmap(pConverter, NULL, &m_pBannerBitmap);
            }
            pConverter->Release();
        }
        pFrame->Release();
    }
    pDecoder->Release();

    return hr;
}

HRESULT SplashScreen::CreateD2DResources() {
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);
    if (FAILED(hr)) return hr;

    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&m_pDWriteFactory)
    );
    if (FAILED(hr)) return hr;

    hr = CoCreateInstance(
        CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&m_pWicFactory)
    );
    if (FAILED(hr)) return hr;

    // Create 32-bit ARGB Memory DC and DIB Section for Hardware Per-Pixel Alpha Layered Window
    BITMAPINFO bi = { 0 };
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = m_width;
    bi.bmiHeader.biHeight = -m_height; // Top-down DIB
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    HDC hdcScreen = GetDC(NULL);
    m_hdcMem = CreateCompatibleDC(hdcScreen);
    m_hBitmap = CreateDIBSection(m_hdcMem, &bi, DIB_RGB_COLORS, &m_pBits, NULL, 0);
    m_hOldBitmap = (HBITMAP)SelectObject(m_hdcMem, m_hBitmap);
    ReleaseDC(NULL, hdcScreen);

    if (!m_hBitmap || !m_hdcMem) return E_FAIL;

    // Create GDI-compatible Direct2D DC Render Target with Premultiplied Alpha
    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        0.0f, 0.0f,
        D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE,
        D2D1_FEATURE_LEVEL_DEFAULT
    );

    hr = m_pD2DFactory->CreateDCRenderTarget(&props, &m_pDCRenderTarget);
    if (FAILED(hr)) return hr;

    RECT rcBind = { 0, 0, m_width, m_height };
    hr = m_pDCRenderTarget->BindDC(m_hdcMem, &rcBind);
    if (FAILED(hr)) return hr;

    // Create 22px Rounded Rectangle Geometry for smooth antialiased clipping of banner
    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(
        D2D1::RectF(0.0f, 0.0f, (float)m_width, (float)m_height), 22.0f, 22.0f
    );
    hr = m_pD2DFactory->CreateRoundedRectangleGeometry(&rr, &m_pRoundRectGeom);
    if (FAILED(hr)) return hr;

    hr = m_pDCRenderTarget->CreateLayer(NULL, &m_pLayer);
    if (FAILED(hr)) return hr;

    // Text Formats
    m_pDWriteFactory->CreateTextFormat(
        L"Segoe UI", NULL,
        DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        11.5f, L"tr-TR", &m_pTextLibStatus
    );

    m_pDWriteFactory->CreateTextFormat(
        L"Segoe UI", NULL,
        DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        9.5f, L"tr-TR", &m_pTextBadge
    );

    // Dynamic Theme Color Harmony loaded from SettingsManager
    SettingsManager::Get().Load();
    auto& sett = SettingsManager::Get().settings;
    D2D1_COLOR_F customAccent = KaisekiTheme::ColorFromRgbU8(sett.accentR, sett.accentG, sett.accentB);
    KaisekiTheme::ThemeColors theme = KaisekiTheme::GetTheme((AppTheme)sett.theme, &customAccent, (BackgroundShade)sett.bgShade);
    if (sett.bgShade == (int)BackgroundShade::Custom) {
        D2D1_COLOR_F customBg = D2D1::ColorF(sett.bgR / 255.0f, sett.bgG / 255.0f, sett.bgB / 255.0f, 1.0f);
        KaisekiTheme::ApplyCustomBackgroundColor(theme, customBg);
    }

    // Dynamic Brushes matching the user's active theme
    m_pDCRenderTarget->CreateSolidColorBrush(theme.canvasBg, &m_pBrushBg);
    
    D2D1_COLOR_F borderClr = theme.accent;
    borderClr.a = 0.85f;
    m_pDCRenderTarget->CreateSolidColorBrush(borderClr, &m_pBrushBorder);

    D2D1_COLOR_F pillBg = theme.panelBg;
    pillBg.a = 0.88f;
    m_pDCRenderTarget->CreateSolidColorBrush(pillBg, &m_pBrushPillBg);

    m_pDCRenderTarget->CreateSolidColorBrush(theme.borderSubtle, &m_pBrushPillBorder);
    m_pDCRenderTarget->CreateSolidColorBrush(theme.textPrimary, &m_pBrushWhite);
    m_pDCRenderTarget->CreateSolidColorBrush(theme.accent, &m_pBrushSakura);

    D2D1_COLOR_F accentLight = D2D1::ColorF(
        KaisekiTheme::ClampF(theme.accent.r * 1.25f, 0.0f, 1.0f),
        KaisekiTheme::ClampF(theme.accent.g * 1.25f, 0.0f, 1.0f),
        KaisekiTheme::ClampF(theme.accent.b * 1.25f, 0.0f, 1.0f),
        1.0f
    );
    m_pDCRenderTarget->CreateSolidColorBrush(accentLight, &m_pBrushSakuraLight);

    // Dedicated Error & Success Brushes
    m_pDCRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.96f, 0.28f, 0.28f, 1.0f), &m_pBrushError);
    m_pDCRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.20f, 0.05f, 0.06f, 0.92f), &m_pBrushErrorBg);
    m_pDCRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.25f, 0.85f, 0.45f, 1.0f), &m_pBrushSuccess);

    // Load banner bitmap
    LoadBannerBitmap();

    return S_OK;
}

LRESULT CALLBACK SplashScreen::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    SplashScreen* pThis = (SplashScreen*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (uMsg) {
    case WM_NCCREATE: {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return TRUE;
    }
    case WM_NCHITTEST: {
        // Entire card is draggable by clicking anywhere
        return HTCAPTION;
    }
    case WM_LBUTTONDOWN: {
        if (pThis && pThis->m_isErrorState) {
            // Dismiss error immediately upon user click
            pThis->m_isDone = true;
            return 0;
        }
        ReleaseCapture();
        SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return 0;
    }
    case WM_SPLASH_PROGRESS: {
        if (pThis) {
            std::vector<ModuleProgressEvent> events;
            {
                std::lock_guard<std::mutex> lock(pThis->m_eventMutex);
                events.swap(pThis->m_pendingEvents);
            }
            for (const auto& ev : events) {
                pThis->m_currentLibText = ev.statusText;
                pThis->m_progress = ev.overallProgress;
                if (!ev.success && ev.stage == ModuleProgressStage::ModuleFailed) {
                    pThis->m_isErrorState = true;
                }
            }
            pThis->Render();
        }
        return 0;
    }
    case WM_SPLASH_COMPLETE: {
        if (pThis) {
            pThis->m_loadSuccess = (wParam != 0);
            pThis->m_workerFinished = true;
            if (pThis->m_loadSuccess) {
                pThis->m_currentLibText = L"Ready";
                pThis->m_progress = 1.0f;
            } else {
                pThis->m_isErrorState = true;
            }
            pThis->Render();
        }
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

bool SplashScreen::Create(HINSTANCE hInstance) {
    m_hInstance = hInstance;

    WNDCLASSEXW wcex = { sizeof(WNDCLASSEXW) };
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = SplashScreen::WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = NULL;
    wcex.lpszClassName = L"KaisekiSplashScreenClass";

    RegisterClassExW(&wcex);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenW - m_width) / 2;
    int posY = (screenH - m_height) / 2;

    // WS_EX_LAYERED enables true hardware per-pixel alpha transparency.
    // The window shape and antialiased rounded transparent corners are driven by UpdateLayeredWindow ULW_ALPHA.
    m_hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        L"KaisekiSplashScreenClass",
        L"Kaiseki Initializing",
        WS_POPUP | WS_VISIBLE,
        posX, posY, m_width, m_height,
        NULL, NULL, hInstance, this
    );

    if (!m_hwnd) return false;

    if (FAILED(CreateD2DResources())) {
        return false;
    }

    // Initial render to make it visible instantly
    Render();
    return true;
}

void SplashScreen::Render() {
    if (!m_pDCRenderTarget || !m_hdcMem) return;

    RECT rcBind = { 0, 0, m_width, m_height };
    m_pDCRenderTarget->BindDC(m_hdcMem, &rcBind);

    m_pDCRenderTarget->BeginDraw();
    m_pDCRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

    // 1. Clear memory buffer with 100% transparent black (Alpha = 0).
    // Every pixel outside the rounded 22px card is completely transparent to the desktop!
    m_pDCRenderTarget->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

    // 2. Antialiased Rounded Clipping Layer for the Banner Artwork
    if (m_pRoundRectGeom && m_pLayer) {
        D2D1_LAYER_PARAMETERS layerParams = D2D1::LayerParameters(
            D2D1::InfiniteRect(),
            m_pRoundRectGeom,
            D2D1_ANTIALIAS_MODE_PER_PRIMITIVE
        );
        m_pDCRenderTarget->PushLayer(layerParams, m_pLayer);

        D2D1_RECT_F fullRect = D2D1::RectF(0.0f, 0.0f, (float)m_width, (float)m_height);
        if (m_pBannerBitmap) {
            m_pDCRenderTarget->DrawBitmap(m_pBannerBitmap, fullRect);
        } else {
            m_pDCRenderTarget->FillRectangle(fullRect, m_pBrushBg);
        }

        m_pDCRenderTarget->PopLayer();
    }

    // 3. Smooth Sakura Pastel Pink rounded outer border with anti-aliasing (red on error)
    D2D1_ROUNDED_RECT outerBorder = D2D1::RoundedRect(
        D2D1::RectF(0.75f, 0.75f, (float)m_width - 0.75f, (float)m_height - 0.75f), 22.0f, 22.0f
    );
    m_pDCRenderTarget->DrawRoundedRectangle(outerBorder, m_isErrorState ? m_pBrushError : m_pBrushBorder, 1.5f);

    // 4. Subtle, high-precision loading progress line along the bottom border
    if (m_progress > 0.0f) {
        float clampedProgress = (std::min)(1.0f, (std::max)(0.0f, m_progress));
        float barStartX = 24.0f;
        float barMaxW = (float)m_width - 48.0f;
        float barW = barMaxW * clampedProgress;
        float barY = (float)m_height - 8.0f;

        D2D1_ROUNDED_RECT progressBar = D2D1::RoundedRect(
            D2D1::RectF(barStartX, barY, barStartX + barW, barY + 2.5f), 1.25f, 1.25f
        );
        m_pDCRenderTarget->FillRoundedRectangle(progressBar, m_isErrorState ? m_pBrushError : m_pBrushSakura);
    }

    // 5. Bottom-Left glass-style status pill (Translucent dark glass)
    if (!m_currentLibText.empty()) {
        float padX = 24.0f;
        float pillY = (float)m_height - 46.0f;
        float pillH = 26.0f;
        
        // Dynamic pill width accommodating the real module name and error codes
        float textWidth = (float)m_currentLibText.length() * 7.5f + 40.0f;
        if (textWidth > (float)m_width - 48.0f) textWidth = (float)m_width - 48.0f;
        if (textWidth < 140.0f) textWidth = 140.0f;

        D2D1_ROUNDED_RECT pill = D2D1::RoundedRect(
            D2D1::RectF(padX, pillY, padX + textWidth, pillY + pillH), 13.0f, 13.0f
        );
        m_pDCRenderTarget->FillRoundedRectangle(pill, m_isErrorState ? m_pBrushErrorBg : m_pBrushPillBg);
        m_pDCRenderTarget->DrawRoundedRectangle(pill, m_isErrorState ? m_pBrushError : m_pBrushPillBorder, 1.0f);

        // State indicator dot: Green when Ready, Red on Error, Sakura Pink during normal load
        D2D1_ELLIPSE dot = D2D1::Ellipse(D2D1::Point2F(padX + 14.0f, pillY + pillH * 0.5f), 3.5f, 3.5f);
        ID2D1SolidColorBrush* pDotBrush = m_isErrorState ? m_pBrushError : 
            (m_workerFinished && m_loadSuccess ? m_pBrushSuccess : m_pBrushSakura);
        m_pDCRenderTarget->FillEllipse(dot, pDotBrush);

        // Real Loading / Error Text
        D2D1_RECT_F textPos = D2D1::RectF(padX + 24.0f, pillY + 4.0f, padX + textWidth - 8.0f, pillY + pillH);
        m_pDCRenderTarget->DrawText(
            m_currentLibText.c_str(), (UINT32)m_currentLibText.length(),
            m_pTextLibStatus, textPos, m_pBrushWhite,
            D2D1_DRAW_TEXT_OPTIONS_NONE
        );
    }

    m_pDCRenderTarget->EndDraw();

    // 6. Deliver Premultiplied 32-bit ARGB surface directly to Windows Desktop Window Manager
    RECT rcWnd;
    GetWindowRect(m_hwnd, &rcWnd);
    POINT ptDst = { rcWnd.left, rcWnd.top };
    POINT ptSrc = { 0, 0 };
    SIZE sz = { m_width, m_height };
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    HDC hdcScreen = GetDC(NULL);
    UpdateLayeredWindow(m_hwnd, hdcScreen, &ptDst, &sz, m_hdcMem, &ptSrc, 0, &bf, ULW_ALPHA);
    ReleaseDC(NULL, hdcScreen);
}

bool SplashScreen::RunLoadingLoop() {
    // Launch genuine DLL loading on a dedicated background worker thread
    m_workerThread = std::thread([this]() {
        bool success = ModuleManager::Get().LoadModules([this](const ModuleProgressEvent& ev) {
            {
                std::lock_guard<std::mutex> lock(m_eventMutex);
                m_pendingEvents.push_back(ev);
            }
            if (m_hwnd && IsWindow(m_hwnd)) {
                PostMessageW(m_hwnd, WM_SPLASH_PROGRESS, 0, 0);
            }
            // 25ms frame pacing allows the Direct2D UI thread to render each module event smoothly
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        });

        if (m_hwnd && IsWindow(m_hwnd)) {
            PostMessageW(m_hwnd, WM_SPLASH_COMPLETE, success ? 1 : 0, 0);
        }
    });

    MSG msg;
    while (!m_isDone) {
        // Drain message queue
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                m_isDone = true;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (m_workerFinished) {
            if (m_loadSuccess) {
                // Keep "Ready" visible briefly (150ms) for smooth visual handoff
                if (m_readyTime == 0) {
                    m_readyTime = GetTickCount();
                } else if (GetTickCount() - m_readyTime >= 150) {
                    m_isDone = true;
                }
            } else {
                // On failure: wait 4.5 seconds for user to read real error, or dismiss instantly on click
                if (m_errorDismissTime == 0) {
                    m_errorDismissTime = GetTickCount();
                } else if (GetTickCount() - m_errorDismissTime >= 4500) {
                    m_isDone = true;
                }
            }
        }

        Render();
        MsgWaitForMultipleObjects(0, NULL, FALSE, 16, QS_ALLINPUT);
    }

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    return m_loadSuccess;
}

bool SplashScreen::Show(HINSTANCE hInstance) {
    SplashScreen splash;
    if (!splash.Create(hInstance)) {
        return false;
    }

    return splash.RunLoadingLoop();
}

#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef DrawText
#undef DrawText
#endif
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include "KaisekiModuleManager.h"

// Custom window messages for thread-safe UI communication
#define WM_SPLASH_PROGRESS (WM_USER + 101)
#define WM_SPLASH_COMPLETE (WM_USER + 102)

class SplashScreen {
public:
    SplashScreen();
    ~SplashScreen();

    // Displays the splash screen, drives genuine background DLL loading,
    // and cleanly transitions to main UI. Returns true if all required modules loaded successfully.
    static bool Show(HINSTANCE hInstance);

private:
    bool Create(HINSTANCE hInstance);
    bool RunLoadingLoop();
    void Render();
    void CleanupD2D();
    HRESULT CreateD2DResources();
    HRESULT LoadBannerBitmap();

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
    HWND m_hwnd = NULL;
    HINSTANCE m_hInstance = NULL;

    const int m_width = 672;
    const int m_height = 384;

    // GDI Memory DC & 32-bit ARGB DIB for hardware alpha transparency
    HDC m_hdcMem = NULL;
    HBITMAP m_hBitmap = NULL;
    HBITMAP m_hOldBitmap = NULL;
    void* m_pBits = nullptr;

    // Direct2D DC Render Target, DirectWrite & WIC
    ID2D1Factory* m_pD2DFactory = nullptr;
    IDWriteFactory* m_pDWriteFactory = nullptr;
    IWICImagingFactory* m_pWicFactory = nullptr;
    ID2D1DCRenderTarget* m_pDCRenderTarget = nullptr;
    ID2D1Bitmap* m_pBannerBitmap = nullptr;

    // Geometry Clipping for smooth rounded transparent corners
    ID2D1RoundedRectangleGeometry* m_pRoundRectGeom = nullptr;
    ID2D1Layer* m_pLayer = nullptr;

    // Text Formats
    IDWriteTextFormat* m_pTextLibStatus = nullptr;
    IDWriteTextFormat* m_pTextBadge = nullptr;

    // Brushes
    ID2D1SolidColorBrush* m_pBrushBg = nullptr;
    ID2D1SolidColorBrush* m_pBrushBorder = nullptr;
    ID2D1SolidColorBrush* m_pBrushPillBg = nullptr;
    ID2D1SolidColorBrush* m_pBrushPillBorder = nullptr;
    ID2D1SolidColorBrush* m_pBrushWhite = nullptr;
    ID2D1SolidColorBrush* m_pBrushSakura = nullptr;
    ID2D1SolidColorBrush* m_pBrushSakuraLight = nullptr;
    ID2D1SolidColorBrush* m_pBrushError = nullptr;
    ID2D1SolidColorBrush* m_pBrushErrorBg = nullptr;
    ID2D1SolidColorBrush* m_pBrushSuccess = nullptr;

    // Thread-safe state & worker
    std::thread m_workerThread;
    std::mutex m_eventMutex;
    std::vector<ModuleProgressEvent> m_pendingEvents;

    std::wstring m_currentLibText = L"Initializing Kaiseki...";
    float m_progress = 0.05f;
    std::atomic<bool> m_workerFinished{ false };
    std::atomic<bool> m_loadSuccess{ false };
    bool m_isDone = false;
    bool m_isErrorState = false;
    DWORD m_readyTime = 0;
    DWORD m_errorDismissTime = 0;
};

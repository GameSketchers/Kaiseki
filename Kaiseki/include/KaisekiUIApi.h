#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "KaisekiCommon.h"

#if defined(KAISEKIUI_EXPORTS) || defined(APKUI_EXPORTS)
#define KAISEKI_UI_API __declspec(dllexport)
#else
#define KAISEKI_UI_API __declspec(dllimport)
#endif

#ifndef APK_UI_API
#define APK_UI_API KAISEKI_UI_API
#endif

// ============================================================================
// WNDPROC & GDI+ INITIALIZATION CONTRACT:
// 1) ApkUI_Initialize / KaisekiUI_Initialize initializes COM, Direct2D,
//    DirectWrite and WIC (Windows Imaging Component).
// 2) The main window created by KaisekiUI_CreateMainWindow integrates with
//    KaisekiAnim for hardware DWM caption and frame transitions.
// ============================================================================

extern "C" {
    KAISEKI_UI_API bool KaisekiUI_Initialize();
    KAISEKI_UI_API bool KaisekiUI_ShowSplashScreen(HINSTANCE hInstance);
    KAISEKI_UI_API HWND KaisekiUI_CreateMainWindow(HINSTANCE hInstance, int nCmdShow, const wchar_t* initialApk);
    KAISEKI_UI_API int KaisekiUI_RunMessageLoop();
    KAISEKI_UI_API bool KaisekiUI_SaveScreenshot(const wchar_t* outPngPath);
    KAISEKI_UI_API void KaisekiUI_SetRailMode(int mode);
    KAISEKI_UI_API void KaisekiUI_SetSettingsTab(int tabIndex);
    KAISEKI_UI_API void KaisekiUI_ShowSettingsModal();
    KAISEKI_UI_API void KaisekiUI_SimulateEdit(const wchar_t* textToInsert);
    KAISEKI_UI_API void KaisekiUI_SaveActiveTab();
    KAISEKI_UI_API void KaisekiUI_SetSelection(int startLine, int startCol, int endLine, int endCol);
    KAISEKI_UI_API void KaisekiUI_ClickSecReport();
    KAISEKI_UI_API bool KaisekiUI_CaptureToolInstaller(const wchar_t* outPngPath);
    KAISEKI_UI_API bool KaisekiUI_CaptureSettingsDialog(const wchar_t* outPngPath, int tabIndex);
    KAISEKI_UI_API void KaisekiUI_OpenMenu(int menuIdx);
    KAISEKI_UI_API const char* KaisekiUI_GetVersion();
    KAISEKI_UI_API void KaisekiUI_Shutdown();

    // Genuine Dynamic Module Manager & Splash Loading Integration
    KAISEKI_UI_API bool KaisekiUI_LoadAllModules();
    KAISEKI_UI_API void KaisekiUI_UnloadAllModules();
    KAISEKI_UI_API bool KaisekiUI_IsModuleLoaded(const wchar_t* moduleName);
    KAISEKI_UI_API const wchar_t* KaisekiUI_GetLastErrorDetails();

    // Compatibility aliases with ApkUI_*
    KAISEKI_UI_API bool ApkUI_Initialize();
    KAISEKI_UI_API bool ApkUI_ShowSplashScreen(HINSTANCE hInstance);
    KAISEKI_UI_API HWND ApkUI_CreateMainWindow(HINSTANCE hInstance, int nCmdShow, const wchar_t* initialApk);
    KAISEKI_UI_API int ApkUI_RunMessageLoop();
    KAISEKI_UI_API bool ApkUI_SaveScreenshot(const wchar_t* outPngPath);
    KAISEKI_UI_API void ApkUI_SetRailMode(int mode);
    KAISEKI_UI_API void ApkUI_SetSettingsTab(int tabIndex);
    KAISEKI_UI_API void ApkUI_ShowSettingsModal();
    KAISEKI_UI_API void ApkUI_SimulateEdit(const wchar_t* textToInsert);
    KAISEKI_UI_API void ApkUI_SaveActiveTab();
    KAISEKI_UI_API void ApkUI_SetSelection(int startLine, int startCol, int endLine, int endCol);
    KAISEKI_UI_API const char* ApkUI_GetVersion();
    KAISEKI_UI_API void ApkUI_Shutdown();
}

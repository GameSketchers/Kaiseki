#ifndef APKUI_EXPORTS
#define APKUI_EXPORTS
#endif
#include "ApkUIApi.h"
#include "UIWindow.h"
#include "SplashScreen.h"
#include "KaisekiModuleManager.h"
#include "ToolInstallerDialog.h"
#include "SettingsDialog.h"
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

static UIWindow* g_pWindow = nullptr;

bool ApkUI_Initialize() {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    return true;
}

bool ApkUI_ShowSplashScreen(HINSTANCE hInstance) {
    return SplashScreen::Show(hInstance);
}

HWND ApkUI_CreateMainWindow(HINSTANCE hInstance, int nCmdShow, const wchar_t* initialApk) {
    if (!g_pWindow) {
        g_pWindow = new UIWindow();
    }
    if (!g_pWindow->Create(hInstance, nCmdShow, initialApk)) {
        delete g_pWindow;
        g_pWindow = nullptr;
        return NULL;
    }
    return g_pWindow->GetHwnd();
}

int ApkUI_RunMessageLoop() {
    if (!g_pWindow) return 1;
    return g_pWindow->Run();
}

bool ApkUI_SaveScreenshot(const wchar_t* outPngPath) {
    if (!g_pWindow) return false;
    return g_pWindow->SaveScreenshot(outPngPath);
}

void ApkUI_SetRailMode(int mode) {
    if (g_pWindow) {
        g_pWindow->SetRailMode((RailMode)mode);
    }
}

void ApkUI_SetSettingsTab(int tabIndex) {
    if (g_pWindow) {
        g_pWindow->SetSettingsTab(tabIndex);
    }
}

void ApkUI_ShowSettingsModal() {
    if (g_pWindow) {
        g_pWindow->OpenSettingsModal();
    }
}

void ApkUI_SimulateEdit(const wchar_t* textToInsert) {
    if (g_pWindow) {
        g_pWindow->SimulateEdit(textToInsert);
    }
}

void ApkUI_SaveActiveTab() {
    if (g_pWindow) {
        g_pWindow->SaveActiveTab();
    }
}

void ApkUI_SetSelection(int startLine, int startCol, int endLine, int endCol) {
    if (g_pWindow) {
        g_pWindow->SetTestSelection(startLine, startCol, endLine, endCol);
    }
}

void ApkUI_Shutdown() {
    if (g_pWindow) {
        delete g_pWindow;
        g_pWindow = nullptr;
    }
    ModuleManager::Get().UnloadModules();
}

#include "KaisekiVersion.h"

const char* KaisekiUI_GetVersion() {
#if defined(_DEBUG)
    return KAISEKI_VERSION_STRING "-debug-v143-mdd";
#else
    return KAISEKI_VERSION_STRING "-release-v143-md";
#endif
}

const char* ApkUI_GetVersion() {
    return KaisekiUI_GetVersion();
}

bool KaisekiUI_Initialize() { return ApkUI_Initialize(); }
bool KaisekiUI_ShowSplashScreen(HINSTANCE hInstance) { return ApkUI_ShowSplashScreen(hInstance); }
HWND KaisekiUI_CreateMainWindow(HINSTANCE hInstance, int nCmdShow, const wchar_t* initialApk) { return ApkUI_CreateMainWindow(hInstance, nCmdShow, initialApk); }
int KaisekiUI_RunMessageLoop() { return ApkUI_RunMessageLoop(); }
bool KaisekiUI_SaveScreenshot(const wchar_t* outPngPath) { return ApkUI_SaveScreenshot(outPngPath); }
void KaisekiUI_SetRailMode(int mode) { ApkUI_SetRailMode(mode); }
void KaisekiUI_SetSettingsTab(int tabIndex) { ApkUI_SetSettingsTab(tabIndex); }
void KaisekiUI_ShowSettingsModal() { ApkUI_ShowSettingsModal(); }
void KaisekiUI_SimulateEdit(const wchar_t* text) { ApkUI_SimulateEdit(text); }
void KaisekiUI_SaveActiveTab() { ApkUI_SaveActiveTab(); }
void KaisekiUI_SetSelection(int sL, int sC, int eL, int eC) { ApkUI_SetSelection(sL, sC, eL, eC); }
void KaisekiUI_ClickSecReport() { if (g_pWindow) g_pWindow->SimulateClickSecReport(); }
bool KaisekiUI_CaptureToolInstaller(const wchar_t* outPngPath) { return ToolInstallerDialog::Get().SaveScreenshot(outPngPath); }
bool KaisekiUI_CaptureSettingsDialog(const wchar_t* outPngPath, int tabIndex) {
    KaisekiLang::Get().Initialize("lang");
    SettingsManager::Get().Load();
    if (tabIndex == 20) {
        SettingsDialog::Get().SetTab(2);
        SettingsDialog::Get().SetLangDropdownOpen(true);
    } else if (tabIndex >= 0) {
        SettingsDialog::Get().SetTab(tabIndex);
        SettingsDialog::Get().SetLangDropdownOpen(false);
    }
    return SettingsDialog::Get().SaveScreenshot(outPngPath);
}
void KaisekiUI_OpenMenu(int menuIdx) {
    if (g_pWindow) {
        g_pWindow->SetActiveMenu(menuIdx);
    }
}
void KaisekiUI_Shutdown() { ApkUI_Shutdown(); }

bool KaisekiUI_LoadAllModules() {
    return ModuleManager::Get().LoadModules(nullptr);
}

void KaisekiUI_UnloadAllModules() {
    ModuleManager::Get().UnloadModules();
}

bool KaisekiUI_IsModuleLoaded(const wchar_t* moduleName) {
    if (!moduleName) return false;
    return ModuleManager::Get().IsLoaded(moduleName);
}

static std::wstring s_lastErrorStr;
const wchar_t* KaisekiUI_GetLastErrorDetails() {
    s_lastErrorStr = ModuleManager::Get().GetLastErrorDetails();
    return s_lastErrorStr.c_str();
}

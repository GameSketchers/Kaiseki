#include <windows.h>
#include <timeapi.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <string>
#include <fstream>
#include "KaisekiCoreApi.h"
#include "KaisekiUIApi.h"
#include "KaisekiDexApi.h"
#include "KaisekiAdbApi.h"
#include "KaisekiAnimApi.h"
#include "KaisekiDiffApi.h"
#include "KaisekiHexApi.h"
#include "KaisekiResApi.h"
#include "KaisekiSecApi.h"
#include "KaisekiModuleManager.h"

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "winmm.lib")

static std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int sz = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), NULL, 0, NULL, NULL);
    std::string s(sz, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], sz, NULL, NULL);
    return s;
}

static std::wstring GetExecutableDir() {
    wchar_t path[MAX_PATH];
    if (GetModuleFileNameW(NULL, path, MAX_PATH) > 0) {
        PathRemoveFileSpecW(path);
        return path;
    }
    return L".";
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    timeBeginPeriod(1); // Set 1ms multimedia timer resolution for 60 FPS silky smooth animations

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    bool isHeadless = false;
    for (int i = 1; i < argc; ++i) {
        if (_wcsicmp(argv[i], L"--screenshot") == 0 ||
            _wcsicmp(argv[i], L"--tool-installer-screenshot") == 0 ||
            _wcsicmp(argv[i], L"--settings-screenshot") == 0 ||
            _wcsicmp(argv[i], L"--export-gradle") == 0) {
            isHeadless = true;
            break;
        }
    }

    // 1. Single Instance Protection
    HANDLE hMutex = NULL;
    if (!isHeadless) {
        hMutex = CreateMutexW(NULL, FALSE, L"Kaiseki_SingleInstance_Mutex");
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            HWND hExisting = FindWindowW(L"KaisekiMainWindow", NULL);
            if (!hExisting) {
                hExisting = FindWindowW(NULL, L"Kaiseki");
            }
            if (hExisting) {
                ShowWindow(hExisting, SW_RESTORE);
                SetForegroundWindow(hExisting);
            }
            if (hMutex) CloseHandle(hMutex);
            if (argv) LocalFree(argv);
            return 0;
        }
    }

    // Explicit AppUserModelID ensures Windows 11 Taskbar groups and names the application "Kaiseki" properly
    SetCurrentProcessExplicitAppUserModelID(L"Kaiseki.App");

    // 2. Memory State Check
    MEMORYSTATUSEX memStatus = { sizeof(memStatus) };
    GlobalMemoryStatusEx(&memStatus);

    // 3. Command Line Argument Parsing (Flags: --no-splash, --export-gradle, --debug, --screenshot)
    bool skipSplash = false;
    bool debugMode = false;
    std::wstring initialApk = L"";
    std::wstring screenshotPath = L"";
    int viewMode = -1;

    for (int i = 1; i < argc; ++i) {
        if (_wcsicmp(argv[i], L"--no-splash") == 0 ||
            _wcsicmp(argv[i], L"--settings-screenshot") == 0 ||
            _wcsicmp(argv[i], L"--tool-installer-screenshot") == 0) {
            skipSplash = true;
        }
        if (_wcsicmp(argv[i], L"--debug") == 0) debugMode = true;
        if (_wcsicmp(argv[i], L"--view") == 0 && i + 1 < argc) {
            std::wstring vm = argv[i + 1];
            if (vm == L"info") viewMode = 1;
            else if (vm == L"signer") viewMode = 2;
            else if (vm == L"adb") viewMode = 3;
            else if (vm == L"settings") viewMode = 4;
            else if (vm == L"explorer") viewMode = 0;
        }
        if (_wcsicmp(argv[i], L"--screenshot") == 0 && i + 1 < argc) {
            screenshotPath = argv[i + 1];
            skipSplash = true;
        }
        std::wstring candidate = argv[i];
        size_t dotPos = candidate.find_last_of(L'.');
        if (dotPos != std::wstring::npos) {
            std::wstring ext = candidate.substr(dotPos);
            for (auto& c : ext) c = towlower(c);
            if (ext == L".apk" || ext == L".kproj") {
                initialApk = candidate;
            }
        }
    }

    if (pCmdLine && wcslen(pCmdLine) > 0) {
        if (wcsstr(pCmdLine, L"--no-splash")) skipSplash = true;
        if (wcsstr(pCmdLine, L"--debug")) debugMode = true;
    }

    std::wstring debugLogPath = GetExecutableDir() + L"\\kaiseki_debug.log";

    if (debugMode) {
        std::ofstream dbg(debugLogPath, std::ios::app);
        if (dbg.is_open()) {
            dbg << "[KAISEKI] Initializing with physical RAM: " << (memStatus.ullTotalPhys / (1024 * 1024)) << " MB\n";
            dbg << "[KAISEKI] initialApk: " << WideToUtf8(initialApk) << "\n";
            dbg << "[KAISEKI] screenshotPath: " << WideToUtf8(screenshotPath) << "\n";
            dbg.close();
        }
    }

    // Check for Headless CLI export: Kaiseki.exe --export-gradle <input.apk> <outDir>
    if (argc >= 4 && _wcsicmp(argv[1], L"--export-gradle") == 0) {
        bool ok = KaisekiCore_ExportGradleProject(argv[2], argv[3]);
        if (hMutex) CloseHandle(hMutex);
        if (argv) LocalFree(argv);
        return ok ? 0 : 1;
    }

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) {
        if (debugMode) {
            std::ofstream dbg(debugLogPath, std::ios::app);
            dbg << "[KAISEKI] CoInitializeEx FAILED: 0x" << std::hex << hr << "\n";
        }
        if (hMutex) CloseHandle(hMutex);
        if (argv) LocalFree(argv);
        return 1;
    }

    {
        std::ofstream dbg(debugLogPath, std::ios::app);
        dbg << "[KAISEKI] 1. Calling KaisekiUI_Initialize...\n";
        dbg.flush();
    }

    if (!KaisekiUI_Initialize()) {
        {
            std::ofstream dbg(debugLogPath, std::ios::app);
            dbg << "[KAISEKI] KaisekiUI_Initialize FAILED\n";
            dbg.flush();
        }
        MessageBoxW(NULL, L"UI Alt Sistemi başlatılamadı!", L"Hata", MB_ICONERROR);
        CoUninitialize();
        if (hMutex) CloseHandle(hMutex);
        if (argv) LocalFree(argv);
        return 1;
    }

    {
        std::ofstream dbg(debugLogPath, std::ios::app);
        dbg << "[KAISEKI] 2. KaisekiUI_Initialize OK, skipSplash: " << skipSplash << "\n";
        dbg.flush();
    }

    // Modern Frameless Japanese Splash Screen with live DLL loading and initialization pipeline
    if (!skipSplash) {
        {
            std::ofstream dbg(debugLogPath, std::ios::app);
            dbg << "[KAISEKI] 3. Calling KaisekiUI_ShowSplashScreen...\n";
            dbg.flush();
        }
        if (!KaisekiUI_ShowSplashScreen(hInstance)) {
            {
                std::ofstream dbg(debugLogPath, std::ios::app);
                dbg << "[KAISEKI] KaisekiUI_ShowSplashScreen returned false\n";
                dbg.flush();
            }
            // Required subsystem loading failed. Splash screen already visualized the error.
            KaisekiUI_Shutdown();
            CoUninitialize();
            if (hMutex) CloseHandle(hMutex);
            if (argv) LocalFree(argv);
            return 1;
        }
    } else {
        {
            std::ofstream dbg(debugLogPath, std::ios::app);
            dbg << "[KAISEKI] 3. Calling KaisekiUI_LoadAllModules...\n";
            dbg.flush();
        }
        if (!KaisekiUI_LoadAllModules()) {
            {
                std::ofstream dbg(debugLogPath, std::ios::app);
                dbg << "[KAISEKI] KaisekiUI_LoadAllModules returned false: " << WideToUtf8(KaisekiUI_GetLastErrorDetails()) << "\n";
                dbg.flush();
            }
            MessageBoxW(NULL, KaisekiUI_GetLastErrorDetails(), L"Kaiseki — Modül Yükleme Hatası", MB_ICONERROR);
            KaisekiUI_Shutdown();
            CoUninitialize();
            if (hMutex) CloseHandle(hMutex);
            if (argv) LocalFree(argv);
            return 1;
        }
    }

    {
        std::ofstream dbg(debugLogPath, std::ios::app);
        dbg << "[KAISEKI] 4. Centralized ModuleManager active: "
            << ModuleManager::Get().GetLoadedCount() << "/"
            << ModuleManager::Get().GetTotalCount() << " modules loaded.\n";
        dbg.flush();
    }

    for (int i = 1; i < argc; ++i) {
        if (_wcsicmp(argv[i], L"--tool-installer-screenshot") == 0 && i + 1 < argc) {
            bool ok = KaisekiUI_CaptureToolInstaller(argv[i + 1]);
            KaisekiUI_Shutdown();
            CoUninitialize();
            if (hMutex) CloseHandle(hMutex);
            if (argv) LocalFree(argv);
            return ok ? 0 : 1;
        }
        if (_wcsicmp(argv[i], L"--settings-screenshot") == 0 && i + 1 < argc) {
            int sTab = 0;
            if (i + 2 < argc && argv[i + 2][0] >= L'0' && argv[i + 2][0] <= L'9') {
                sTab = _wtoi(argv[i + 2]);
            }
            bool ok = KaisekiUI_CaptureSettingsDialog(argv[i + 1], sTab);
            KaisekiUI_Shutdown();
            CoUninitialize();
            if (hMutex) CloseHandle(hMutex);
            if (argv) LocalFree(argv);
            return ok ? 0 : 1;
        }
    }

    HWND hwnd = KaisekiUI_CreateMainWindow(hInstance, nCmdShow, initialApk.c_str());
    if (viewMode >= 0) {
        KaisekiUI_SetRailMode(viewMode);
    }

    for (int i = 1; i < argc; ++i) {
        if (_wcsicmp(argv[i], L"--select") == 0 && i + 4 < argc) {
            int sL = _wtoi(argv[i + 1]);
            int sC = _wtoi(argv[i + 2]);
            int eL = _wtoi(argv[i + 3]);
            int eC = _wtoi(argv[i + 4]);
            KaisekiUI_SetSelection(sL, sC, eL, eC);
        }
        if (_wcsicmp(argv[i], L"--open-menu") == 0 && i + 1 < argc) {
            int mIdx = _wtoi(argv[i + 1]);
            KaisekiUI_OpenMenu(mIdx);
        }
        if (_wcsicmp(argv[i], L"--settings-tab") == 0 && i + 1 < argc) {
            int sTab = _wtoi(argv[i + 1]);
            KaisekiUI_SetSettingsTab(sTab);
        }
        if (_wcsicmp(argv[i], L"--settings-modal") == 0) {
            KaisekiUI_ShowSettingsModal();
        }
        if (_wcsicmp(argv[i], L"--simulate-edit") == 0 && i + 1 < argc) {
            KaisekiUI_SimulateEdit(argv[i + 1]);
        }
        if (_wcsicmp(argv[i], L"--save-active-tab") == 0) {
            KaisekiUI_SaveActiveTab();
        }
        if (_wcsicmp(argv[i], L"--click-sec-report") == 0) {
            KaisekiUI_ClickSecReport();
        }
    }

    if (debugMode) {
        std::ofstream dbg(debugLogPath, std::ios::app);
        dbg << "[KAISEKI] KaisekiUI_CreateMainWindow hwnd: " << hwnd << "\n";
    }

    if (!screenshotPath.empty()) {
        bool sOk = KaisekiUI_SaveScreenshot(screenshotPath.c_str());
        if (debugMode) {
            std::ofstream dbg(debugLogPath, std::ios::app);
            dbg << "[KAISEKI] KaisekiUI_SaveScreenshot result: " << (sOk ? "SUCCESS" : "FAILED") << "\n";
        }
        KaisekiUI_Shutdown();
        CoUninitialize();
        if (hMutex) CloseHandle(hMutex);
        if (argv) LocalFree(argv);
        ExitProcess(sOk ? 0 : 1);
        return sOk ? 0 : 1;
    }

    int ret = KaisekiUI_RunMessageLoop();

    KaisekiUI_Shutdown();
    CoUninitialize();

    if (hMutex) {
        CloseHandle(hMutex);
    }
    if (argv) {
        LocalFree(argv);
    }
    timeEndPeriod(1);
    return ret;
}

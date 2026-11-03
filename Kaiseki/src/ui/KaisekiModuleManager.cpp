#include "KaisekiModuleManager.h"
#include "KaisekiVersion.h"
#include <shlwapi.h>
#include <sstream>

#pragma comment(lib, "shlwapi.lib")

static std::wstring Utf8ToWideStr(const std::string& u8) {
    if (u8.empty()) return L"";
    int sz = MultiByteToWideChar(CP_UTF8, 0, u8.data(), (int)u8.size(), NULL, 0);
    std::wstring w(sz, 0);
    MultiByteToWideChar(CP_UTF8, 0, u8.data(), (int)u8.size(), &w[0], sz);
    return w;
}

ModuleManager::ModuleManager() {
    wchar_t exePath[MAX_PATH] = { 0 };
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH) > 0) {
        PathRemoveFileSpecW(exePath);
        m_baseDir = exePath;
    } else {
        m_baseDir = L".";
    }
    InitializeModuleList();
}

ModuleManager::~ModuleManager() {
    UnloadModules();
}

ModuleManager& ModuleManager::Get() {
    static ModuleManager s_instance;
    return s_instance;
}

void ModuleManager::SetBaseDirectory(const std::wstring& dir) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_baseDir = dir;
    // Update existing module paths
    for (auto& mod : m_modules) {
        mod.path = m_baseDir + L"\\" + mod.name;
    }
}

std::wstring ModuleManager::GetBaseDirectory() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_baseDir;
}

void ModuleManager::InitializeModuleList() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_modules.clear();

    // ========================================================================
    // TOPOLOGICAL DEPENDENCY ORDER:
    // Level 0: No internal Kaiseki dependencies
    // Level 1: Core Engine (depends on KaisekiDex)
    // Level 2: Subsystems (depend on KaisekiCore)
    // Level 3: Host UI Library
    // ========================================================================
    
    // 1. Dalvik DEX AST Engine (Required by Core)
    m_modules.push_back({
        L"KaisekiDex.dll", m_baseDir + L"\\KaisekiDex.dll",
        nullptr, true, false, ModuleLoadStatus::Waiting, 0, L"",
        "KaisekiDex_GetVersion", ""
    });

    // 2. Android Binary Resource Table Engine (Required)
    m_modules.push_back({
        L"KaisekiRes.dll", m_baseDir + L"\\KaisekiRes.dll",
        nullptr, true, false, ModuleLoadStatus::Waiting, 0, L"",
        "KaisekiRes_GetVersion", ""
    });

    // 3. Hex Viewer & Binary Analysis Engine (Required)
    m_modules.push_back({
        L"KaisekiHex.dll", m_baseDir + L"\\KaisekiHex.dll",
        nullptr, true, false, ModuleLoadStatus::Waiting, 0, L"",
        "KaisekiHex_GetVersion", ""
    });

    // 4. Native DWM Frame & UI Animation Subsystem (Required)
    m_modules.push_back({
        L"KaisekiAnim.dll", m_baseDir + L"\\KaisekiAnim.dll",
        nullptr, true, false, ModuleLoadStatus::Waiting, 0, L"",
        "KaisekiAnim_GetVersion", ""
    });

    // 5. Kaiseki Core Engine (PKZIP, AXML, Crypto, Dex Bridge - Required)
    m_modules.push_back({
        L"KaisekiCore.dll", m_baseDir + L"\\KaisekiCore.dll",
        nullptr, true, false, ModuleLoadStatus::Waiting, 0, L"",
        "KaisekiCore_GetVersion", ""
    });

    // 6. Security & Vulnerability Analysis Engine (Optional Subsystem)
    m_modules.push_back({
        L"KaisekiSec.dll", m_baseDir + L"\\KaisekiSec.dll",
        nullptr, false, false, ModuleLoadStatus::Waiting, 0, L"",
        "KaisekiSec_GetVersion", ""
    });

    // 7. APK Diffing & Comparison Engine (Optional Subsystem)
    m_modules.push_back({
        L"KaisekiDiff.dll", m_baseDir + L"\\KaisekiDiff.dll",
        nullptr, false, false, ModuleLoadStatus::Waiting, 0, L"",
        "KaisekiDiff_GetVersion", ""
    });

    // 8. Android Debug Bridge Client (Optional Subsystem)
    m_modules.push_back({
        L"KaisekiADB.dll", m_baseDir + L"\\KaisekiADB.dll",
        nullptr, false, false, ModuleLoadStatus::Waiting, 0, L"",
        "KaisekiADB_GetVersion", ""
    });

    // 9. Primary Direct2D UI & Editor Host (Required)
    m_modules.push_back({
        L"KaisekiUI.dll", m_baseDir + L"\\KaisekiUI.dll",
        nullptr, true, false, ModuleLoadStatus::Waiting, 0, L"",
        "KaisekiUI_GetVersion", ""
    });

    m_initialized = true;
}

std::wstring ModuleManager::FormatSystemError(DWORD errorCode) {
    if (errorCode == 0) return L"";
    LPWSTR pBuffer = nullptr;
    DWORD len = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&pBuffer, 0, NULL
    );
    std::wstring msg = L"";
    if (len > 0 && pBuffer) {
        msg = pBuffer;
        LocalFree(pBuffer);
        // Strip trailing \r\n
        while (!msg.empty() && (msg.back() == L'\r' || msg.back() == L'\n' || msg.back() == L' ')) {
            msg.pop_back();
        }
    }
    return msg;
}

bool ModuleManager::LoadModules(ModuleLoadingCallback callback) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (!m_initialized || m_modules.empty()) {
        InitializeModuleList();
    }

    m_lastErrorDetails.clear();

    // 1. Initial State Notification
    if (callback) {
        ModuleProgressEvent ev;
        ev.stage = ModuleProgressStage::Starting;
        ev.moduleName = L"";
        ev.success = true;
        ev.errorCode = 0;
        ev.statusText = L"Initializing Kaiseki...";
        ev.overallProgress = 0.05f;
        callback(ev);
    }

    const char* expectedBuildTag =
#if defined(_DEBUG)
        "debug";
#else
        "release";
#endif

    size_t totalCount = m_modules.size();
    bool allRequiredLoaded = true;

    for (size_t i = 0; i < totalCount; ++i) {
        auto& mod = m_modules[i];
        float baseProgress = 0.10f + (0.75f * ((float)i / (float)totalCount));

        mod.status = ModuleLoadStatus::Loading;
        mod.path = m_baseDir + L"\\" + mod.name;

        if (callback) {
            ModuleProgressEvent ev;
            ev.stage = ModuleProgressStage::ModuleStarting;
            ev.moduleName = mod.name;
            ev.success = true;
            ev.errorCode = 0;
            ev.statusText = L"[LOADING] " + mod.name;
            ev.overallProgress = baseProgress;
            callback(ev);
        }

        // Check if module is already loaded in the process (e.g. KaisekiUI.dll hosting this code)
        HMODULE hExisting = GetModuleHandleW(mod.name.c_str());
        HMODULE hMod = hExisting;

        if (!hMod) {
            // Check file presence on disk before invoking loader
            DWORD dwAttrib = GetFileAttributesW(mod.path.c_str());
            if (dwAttrib == INVALID_FILE_ATTRIBUTES || (dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
                mod.errorCode = ERROR_FILE_NOT_FOUND;
                mod.errorMessage = L"Modül dosyası belirtilen konumda bulunamadı: " + mod.path;
                mod.status = ModuleLoadStatus::Failed;

                if (mod.required) {
                    allRequiredLoaded = false;
                    m_lastErrorDetails = L"Kritik DLL eksik: " + mod.name + L" (Kod: 2 - Dosya Bulunamadı)";
                    if (callback) {
                        ModuleProgressEvent ev;
                        ev.stage = ModuleProgressStage::ModuleFailed;
                        ev.moduleName = mod.name;
                        ev.success = false;
                        ev.errorCode = mod.errorCode;
                        ev.statusText = L"[HATA] " + mod.name + L" (Bulunamadı - Kod 2)";
                        ev.overallProgress = baseProgress;
                        callback(ev);
                    }
                    return false;
                } else {
                    mod.status = ModuleLoadStatus::Skipped;
                    if (callback) {
                        ModuleProgressEvent ev;
                        ev.stage = ModuleProgressStage::ModuleSkipped;
                        ev.moduleName = mod.name;
                        ev.success = false;
                        ev.errorCode = mod.errorCode;
                        ev.statusText = L"[ATLANDI] " + mod.name + L" (Opsiyonel / Eksik)";
                        ev.overallProgress = baseProgress;
                        callback(ev);
                    }
                    continue;
                }
            }

            // Real dynamic load with LOAD_WITH_ALTERED_SEARCH_PATH using absolute path
            hMod = LoadLibraryExW(mod.path.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
        }

        if (!hMod) {
            DWORD err = GetLastError();
            mod.errorCode = err;
            mod.errorMessage = FormatSystemError(err);
            mod.status = ModuleLoadStatus::Failed;

            if (mod.required) {
                allRequiredLoaded = false;
                m_lastErrorDetails = L"Kritik DLL yüklenemedi: " + mod.name + L" (Hata: " + std::to_wstring(err) + L" - " + mod.errorMessage + L")";
                if (callback) {
                    ModuleProgressEvent ev;
                    ev.stage = ModuleProgressStage::ModuleFailed;
                    ev.moduleName = mod.name;
                    ev.success = false;
                    ev.errorCode = err;
                    ev.statusText = L"[HATA] " + mod.name + L" (Hata: " + std::to_wstring(err) + L")";
                    ev.overallProgress = baseProgress;
                    callback(ev);
                }
                return false;
            } else {
                mod.status = ModuleLoadStatus::Skipped;
                if (callback) {
                    ModuleProgressEvent ev;
                    ev.stage = ModuleProgressStage::ModuleSkipped;
                    ev.moduleName = mod.name;
                    ev.success = false;
                    ev.errorCode = err;
                    ev.statusText = L"[ATLANDI] " + mod.name + L" (Opsiyonel - Hata: " + std::to_wstring(err) + L")";
                    ev.overallProgress = baseProgress;
                    callback(ev);
                }
                continue;
            }
        }

        // Verify required exports
        if (!mod.expectedVersionExport.empty()) {
            using GetVersionFn = const char* (*)();
            auto pfnGetVer = (GetVersionFn)GetProcAddress(hMod, mod.expectedVersionExport.c_str());
            if (!pfnGetVer) {
                DWORD err = GetLastError();
                if (err == 0) err = ERROR_PROC_NOT_FOUND;
                mod.errorCode = err;
                mod.errorMessage = L"Zorunlu export '" + Utf8ToWideStr(mod.expectedVersionExport) + L"' bulunamadı.";
                
                // Do not free if it was already loaded prior to this function
                if (!hExisting) FreeLibrary(hMod);
                hMod = nullptr;
                mod.status = ModuleLoadStatus::Failed;

                if (mod.required) {
                    allRequiredLoaded = false;
                    m_lastErrorDetails = mod.name + L": " + mod.errorMessage;
                    if (callback) {
                        ModuleProgressEvent ev;
                        ev.stage = ModuleProgressStage::ModuleFailed;
                        ev.moduleName = mod.name;
                        ev.success = false;
                        ev.errorCode = err;
                        ev.statusText = L"[HATA] " + mod.name + L" (Export Yok)";
                        ev.overallProgress = baseProgress;
                        callback(ev);
                    }
                    return false;
                } else {
                    mod.status = ModuleLoadStatus::Skipped;
                    continue;
                }
            }

            // Verify ABI compatibility
            const char* verStr = pfnGetVer();
            mod.versionString = verStr ? verStr : "";
            if (!verStr || std::string(verStr).find(expectedBuildTag) == std::string::npos) {
                mod.errorCode = ERROR_IMAGE_MACHINE_TYPE_MISMATCH_EXE;
                mod.errorMessage = L"Modül '" + mod.name + L"' farklı derleme yapılandırmasıyla yüklendi.\n" +
                                   L"Beklenen: " + Utf8ToWideStr(expectedBuildTag) + L", Bulunan: " + Utf8ToWideStr(verStr ? verStr : "null");
                
                if (!hExisting) FreeLibrary(hMod);
                hMod = nullptr;
                mod.status = ModuleLoadStatus::Failed;

                if (mod.required) {
                    allRequiredLoaded = false;
                    m_lastErrorDetails = mod.errorMessage;
                    if (callback) {
                        ModuleProgressEvent ev;
                        ev.stage = ModuleProgressStage::ModuleFailed;
                        ev.moduleName = mod.name;
                        ev.success = false;
                        ev.errorCode = mod.errorCode;
                        ev.statusText = L"[HATA] " + mod.name + L" (ABI Uyuşmazlığı)";
                        ev.overallProgress = baseProgress;
                        callback(ev);
                    }
                    return false;
                } else {
                    mod.status = ModuleLoadStatus::Skipped;
                    continue;
                }
            }
        }

        // Successfully loaded and verified
        mod.handle = hMod;
        mod.loaded = true;
        mod.errorCode = 0;
        mod.status = ModuleLoadStatus::Loaded;

        if (callback) {
            ModuleProgressEvent ev;
            ev.stage = ModuleProgressStage::ModuleSuccess;
            ev.moduleName = mod.name;
            ev.success = true;
            ev.errorCode = 0;
            ev.statusText = L"[OK] " + mod.name;
            ev.overallProgress = 0.10f + (0.75f * ((float)(i + 1) / (float)totalCount));
            callback(ev);
        }
    }

    if (!allRequiredLoaded) {
        return false;
    }

    // Subsystem resources initialization stage
    if (callback) {
        ModuleProgressEvent ev;
        ev.stage = ModuleProgressStage::InitializingResources;
        ev.moduleName = L"";
        ev.success = true;
        ev.errorCode = 0;
        ev.statusText = L"Initializing resources...";
        ev.overallProgress = 0.92f;
        callback(ev);
    }

    // Final Ready Stage
    if (callback) {
        ModuleProgressEvent ev;
        ev.stage = ModuleProgressStage::Completed;
        ev.moduleName = L"";
        ev.success = true;
        ev.errorCode = 0;
        ev.statusText = L"Ready";
        ev.overallProgress = 1.0f;
        callback(ev);
    }

    return true;
}

void ModuleManager::UnloadModules() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    // Unload in REVERSE topological dependency order
    for (auto it = m_modules.rbegin(); it != m_modules.rend(); ++it) {
        if (it->handle) {
            HMODULE hSelf = GetModuleHandleW(L"KaisekiUI.dll");
            if (it->handle != hSelf) {
                // FreeLibrary(it->handle);
            }
            it->handle = nullptr;
            it->loaded = false;
            it->status = ModuleLoadStatus::Waiting;
        }
    }
}

bool ModuleManager::IsLoaded(const std::wstring& name) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (const auto& mod : m_modules) {
        if (_wcsicmp(mod.name.c_str(), name.c_str()) == 0) {
            return mod.loaded;
        }
    }
    return false;
}

HMODULE ModuleManager::GetModuleHandle(const std::wstring& name) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (const auto& mod : m_modules) {
        if (_wcsicmp(mod.name.c_str(), name.c_str()) == 0) {
            return mod.handle;
        }
    }
    return nullptr;
}

const ModuleInfo* ModuleManager::GetModuleInfo(const std::wstring& name) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (const auto& mod : m_modules) {
        if (_wcsicmp(mod.name.c_str(), name.c_str()) == 0) {
            return &mod;
        }
    }
    return nullptr;
}

size_t ModuleManager::GetLoadedCount() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    size_t cnt = 0;
    for (const auto& mod : m_modules) {
        if (mod.loaded) cnt++;
    }
    return cnt;
}

size_t ModuleManager::GetTotalCount() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_modules.size();
}

const std::vector<ModuleInfo>& ModuleManager::GetModules() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_modules;
}

const std::wstring& ModuleManager::GetLastErrorDetails() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_lastErrorDetails;
}

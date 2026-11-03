#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <cstdint>

#if defined(KAISEKIUI_EXPORTS) || defined(APKUI_EXPORTS)
#define KAISEKI_MODULE_API __declspec(dllexport)
#else
#define KAISEKI_MODULE_API __declspec(dllimport)
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4251)
#endif

enum class ModuleLoadStatus {
    Waiting,
    Loading,
    Loaded,
    Failed,
    Skipped
};

struct ModuleInfo {
    std::wstring name;                 // e.g. L"KaisekiCore.dll"
    std::wstring path;                 // Full absolute path on disk
    HMODULE handle = nullptr;
    bool required = true;
    bool loaded = false;
    ModuleLoadStatus status = ModuleLoadStatus::Waiting;
    DWORD errorCode = 0;
    std::wstring errorMessage;
    std::string expectedVersionExport; // e.g. "KaisekiCore_GetVersion"
    std::string versionString;
};

enum class ModuleProgressStage {
    Starting,
    ModuleStarting,
    ModuleSuccess,
    ModuleSkipped,
    ModuleFailed,
    InitializingResources,
    Completed,
    FatalError
};

struct ModuleProgressEvent {
    ModuleProgressStage stage;
    std::wstring moduleName;
    bool success = false;
    DWORD errorCode = 0;
    std::wstring statusText;
    float overallProgress = 0.0f; // 0.0f to 1.0f
};

using ModuleLoadingCallback = std::function<void(const ModuleProgressEvent& event)>;

class KAISEKI_MODULE_API ModuleManager {
public:
    static ModuleManager& Get();

    // Sets the base directory containing the DLLs (defaults to the executable's directory)
    void SetBaseDirectory(const std::wstring& dir);
    std::wstring GetBaseDirectory() const;

    // Registers all Kaiseki modules in topological dependency order
    void InitializeModuleList();

    // Performs real DLL loading with verification of exports and ABI compatibility
    bool LoadModules(ModuleLoadingCallback callback = nullptr);

    // Safely unloads all loaded modules in reverse dependency order (RAII)
    void UnloadModules();

    // Query methods
    bool IsLoaded(const std::wstring& name) const;
    HMODULE GetModuleHandle(const std::wstring& name) const;
    const ModuleInfo* GetModuleInfo(const std::wstring& name) const;

    size_t GetLoadedCount() const;
    size_t GetTotalCount() const;
    const std::vector<ModuleInfo>& GetModules() const;
    const std::wstring& GetLastErrorDetails() const;

private:
    ModuleManager();
    ~ModuleManager();
    ModuleManager(const ModuleManager&) = delete;
    ModuleManager& operator=(const ModuleManager&) = delete;

    std::wstring FormatSystemError(DWORD errorCode);

private:
    std::wstring m_baseDir;
    std::vector<ModuleInfo> m_modules;
    mutable std::recursive_mutex m_mutex;
    std::wstring m_lastErrorDetails;
    bool m_initialized = false;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

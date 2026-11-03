#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <vector>
#include <map>

enum class AppLanguage {
    Turkish = 0,
    English = 1,
    German = 2,
    Japanese = 3,
    Custom = 4
};

enum class StringId {
    // Menu Bar
    Menu_File,
    Menu_Inspect,
    Menu_Tools,
    Menu_View,
    Menu_Settings,
    Menu_About,

    // File Menu
    Menu_OpenApk,
    Menu_OpenKproj,
    Menu_SaveKproj,
    Menu_SaveTab,
    Menu_ExportGradle,
    Menu_RepackApk,
    Menu_Exit,

    // Inspect Menu
    Menu_InspectManifest,
    Menu_InspectSignature,
    Menu_InspectSummary,
    Menu_InspectClasses,
    Menu_InspectArsc,
    Menu_InspectSecurity,

    // Tools Menu
    Menu_DiffApk,
    Menu_AdbDevices,
    Menu_ApkSigner,
    Menu_DeobfToggle,
    Menu_LoadMapping,

    // View Menu
    Menu_ViewExplorer,
    Menu_ViewSigner,
    Menu_ViewAdb,
    Menu_ViewSettings,
    Menu_ViewTerminal,

    // Rail Tooltips
    Rail_Explorer,
    Rail_Signer,
    Rail_Adb,
    Rail_Settings,

    // Explorer Tree
    Tree_ProjectExplorer,
    Tree_SourceCode,
    Tree_Resources,
    Tree_Signatures,
    Tree_NoFileOpen,

    // Status Bar
    Status_Ready,
    Status_LoadingApk,
    Status_ClassesCount,
    Status_Standalone,

    // Terminal & Logcat
    Tab_Terminal,
    Tab_Logcat,
    Btn_Refresh,
    Btn_Clear,
    Terminal_Welcome,

    // Settings Navigation (12 Categories)
    Settings_Title,
    Settings_TabGeneral,
    Settings_TabTheme,
    Settings_TabLanguage,
    Settings_TabTools,
    Settings_TabDecompile,
    Settings_TabEditor,
    Settings_TabBuild,
    Settings_TabAdb,
    Settings_TabSecurity,
    Settings_TabPerformance,
    Settings_TabHotkeys,
    Settings_TabAdvanced,

    // Settings Language Tab
    Settings_LangTitle,
    Settings_LangDesc,
    Settings_LangTurkishName,
    Settings_LangTurkishDesc,
    Settings_LangEnglishName,
    Settings_LangEnglishDesc,
    Settings_LangSelected,
    Settings_LangSelectBtn,
    Settings_OpenLangFolder,
    Settings_RefreshLangs,
    Settings_LangFolderHint,

    // Settings Theme Tab
    Settings_ThemePaletteTitle,
    Settings_ThemeSlidersTitle,
    Settings_SliderR,
    Settings_SliderG,
    Settings_SliderB,

    // Settings General Tab
    Settings_TempWorkspaceTitle,
    Settings_ConfigFileTitle,
    Settings_FileAssocTitle,
    Settings_BtnRegisterKproj,

    // Settings Tools Tab
    Settings_EngineTitle,
    Settings_EngineDesc,
    Settings_AdbTitle,
    Settings_AdbDesc,
    Settings_BtnExportGradle,
    Settings_DeobfActive,
    Settings_DeobfInactive,

    // Settings Decompilation Tab
    Settings_DecompTitle,
    Settings_DefaultViewLabel,
    Settings_FontSizeLabel,

    // Progress Dialog
    Progress_Title,
    Progress_Waiting,
    Progress_Completed,
    Progress_ZipScan,
    Progress_DexAxml,
    Progress_Workspace,
    Progress_Footer,

    // Signer & ADB View Titles
    Signer_Title,
    Adb_Title,

    // Toasts
    Toast_ClassDecompiled,
    Toast_NoApkOpen,
    Toast_GradleExportSuccess,
    Toast_GradleExportFail,
    Toast_RepackFail,
    Toast_RepackSignSuccess,
    Toast_SignFail,
    Toast_DiffComplete,
    Toast_DiffFail,
    Toast_KprojSaved,
    Toast_KprojLoaded,
    Toast_KprojReadFail,
    Toast_FileSaved,
    Toast_ApkSuccess,
    Toast_ApkOpenFail,
    Toast_PeComplete,
    Toast_PeFail,
    Toast_OnlyValidExtensions,
    Toast_DeobfEnabled,
    Toast_DeobfDisabled,
    Toast_KprojAssocSuccess,
    Toast_ColorSelected,
    Toast_LogcatRefreshed,
    Toast_LogcatCleared,
    Toast_FridaScriptGen,
    Toast_AdbInstallSuccess,
    Toast_AdbInstallFail,
    Toast_AdbLaunchSuccess,
    Toast_AdbLaunchFail,
    Toast_LangChanged,

    Count_
};

struct LanguagePackInfo {
    std::string code;         // e.g. "tr", "en", "de"
    std::wstring name;        // e.g. "Türkçe", "English"
    std::wstring displayName; // e.g. "🇹🇷  Türkçe", "🇬🇧  English"
    std::wstring author;      // e.g. "Kaiseki Team"
    std::wstring filePath;    // Absolute path to the XML file
    bool isBuiltIn = false;
};

class KaisekiLang {
public:
    static KaisekiLang& Get() {
        static KaisekiLang instance;
        return instance;
    }

    void Initialize(const std::string& initialCode = "tr");
    void ScanLanguages();
    bool SetLanguage(const std::string& code);
    void SetLanguage(AppLanguage lang);

    AppLanguage GetLanguage() const { return m_langEnum; }
    const std::string& GetLanguageCode() const { return m_currentCode; }

    const std::vector<LanguagePackInfo>& GetAvailableLanguages() const {
        return m_languages;
    }

    const wchar_t* GetW(StringId id) const;
    std::string GetA(StringId id) const;

    std::wstring GetLangDirectory() const;

    static const char* GetKeyName(StringId id);
    static bool GetIdByKey(const std::string& key, StringId& outId);
    static size_t GetStringCount() { return (size_t)StringId::Count_; }

private:
    KaisekiLang();
    ~KaisekiLang() = default;

    bool ParseLanguageHeader(const std::wstring& filePath, LanguagePackInfo& outInfo);
    bool LoadLanguageXml(const std::wstring& filePath, std::vector<std::wstring>& outStrings);
    void EnsureDefaultLanguageFiles(const std::wstring& langDir);

    AppLanguage m_langEnum = AppLanguage::Turkish;
    std::string m_currentCode = "tr";

    std::vector<LanguagePackInfo> m_languages;
    std::vector<std::wstring> m_activeStrings;

    static const wchar_t* g_builtInStrings[][2];
    static const char* g_keyNames[];
};

// Shorthand global helpers
inline const wchar_t* LocW(StringId id) {
    return KaisekiLang::Get().GetW(id);
}

inline std::string LocA(StringId id) {
    return KaisekiLang::Get().GetA(id);
}

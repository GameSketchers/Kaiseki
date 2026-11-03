#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <ctime>
#include <thread>
#include "KaisekiLang.h"

#ifndef KAISEKI_VERSION_W
#define KAISEKI_VERSION_W L"2.5.0"
#endif

enum class SdkValidationStatus {
    Ready,              // All critical tools found (aapt/aapt2, apksigner, zipalign, adb)
    MissingComponents,  // Folder exists but some tools are missing
    Invalid             // Folder doesn't exist or is not an Android SDK
};

struct KaisekiSettings {
    // 0. First Run Status
    bool firstRunCompleted = false;

    // 1. General & Project
    std::wstring defaultProjectDir = L"";
    std::wstring defaultImportDir = L"";
    std::wstring tempWorkspaceDir = L"";
    std::wstring workspacePath = L""; // Alias for UI
    int maxRecentFiles = 15;
    bool restoreWorkspaceOnStartup = true;
    bool restoreWorkspace = true;     // Alias for UI
    bool rememberOpenTabs = true;
    int autosaveInterval = 0; // 0: Disabled, 1: 1 min, 5: 5 min, 10: 10 min
    bool confirmDestructiveOps = true;
    bool kprojAssociated = false;

    // 2. Appearance & Themes
    int theme = 0;                            // 0: KaisekiDark, 1: Monochrome, 2: CyberTorii, 3: ZenGold, 4: Custom
    int bgShade = 0;                          // 0: Charcoal (#18191E), 1: OledBlack (#0B0B0D), 2: MidnightBlue (#10141F), 3: ImperialPlum (#18111A), 4: DeepSlate (#121316), 5: DarkObsidian (#0E1117), 6: Custom
    uint8_t bgR = 27;
    uint8_t bgG = 28;
    uint8_t bgB = 34;
    std::wstring themeName = L"SakuraPastel"; // "SakuraPastel", "DarkCharcoal", "OledBlack", "MidnightBlue", "ImperialPlum"
    std::wstring themeMode = L"Dark";         // "Dark", "Light", "System"
    uint8_t accentR = 255;
    uint8_t accentG = 128;
    uint8_t accentB = 184;
    std::wstring editorFontFamily = L"Consolas";
    int fontSize = 12;
    bool showLineNumbers = true;
    bool highlightCurrentLine = true;
    bool showIndentationGuides = true;
    bool syntaxHighlighting = true;
    bool showMinimap = false;
    bool reducedMotion = false;

    // 3. Language & Localization
    std::string language = "tr"; // "tr" for Türkçe, "en" for English, "de" for Deutsch
    bool autoReloadLanguage = false;

    // 4. Tools, SDK & Environment
    std::wstring androidSdkPath = L"";
    std::wstring adbPath = L"";
    std::wstring jdkPath = L"";
    std::wstring buildToolsPath = L"";
    std::wstring visualStudioPath = L"";
    std::wstring jadxOverridePath = L"";

    // 5. Decompilation & Reverse Engineering
    bool defaultToJava = false;  // false: Smali bytecode, true: Java AST
    int decompileMode = 0;       // 0: Native AST, 1: Smali Bytecode, 2: Hybrid
    bool multidexMerged = true;  // Merged classes view
    int resourceDecodeMode = 0;  // 0: Decode all, 1: Keep raw, 2: Skip heavy
    int decompileTimeoutSec = 30;
    bool deobfuscateNames = false;
    int decompileThreads = 0;    // 0: Auto (Hardware concurrency), 2, 4, 8, 16, 32

    int GetEffectiveDecompileThreads() const {
        if (decompileThreads > 0) return decompileThreads;
        unsigned int hw = std::thread::hardware_concurrency();
        return (hw > 0) ? (int)hw : 4;
    }

    // 6. Editor & Code Navigation
    bool useSpacesForTabs = true;
    int tabSize = 4;
    bool wordWrap = false;
    std::string fileEncoding = "UTF-8";
    int cursorBlinkRateMs = 530;
    bool showRegisters = true;

    // 7. APK Build & Signing
    int keystoreProfile = 0;     // 0: SDK Debug Keystore, 1: Custom Keystore
    std::wstring customKeystorePath = L"";
    std::string keystoreAlias = "androiddebugkey";
    std::string keystorePass = "android";
    std::string keyPass = "android";
    bool signSchemeV1 = true;
    bool signSchemeV2 = true;
    bool signSchemeV3 = true;
    bool signSchemeV4 = false;
    bool zipalignEnabled = true;
    bool zipAlign4Byte = true; // Alias for UI
    bool backupBeforeRebuild = true;

    // 8. Android Devices & ADB
    std::string defaultDeviceSerial = "";
    bool autoInstallAfterRebuild = false;
    bool installFlagReplace = true;
    bool installFlagDowngrade = true;
    bool installFlagTest = true;
    bool installFlagGrantPerms = true;
    int logcatBufferSizeKb = 4096;
    std::string logcatDefaultLogLevel = "V";

    // 9. Security & Privacy
    bool confirmDeviceInstall = true;
    bool confirmRunScripts = true;
    bool highlightDangerousPerms = true;
    bool wipeTempOnExit = false;
    bool telemetryDisabled = true;

    // 10. Performance & Memory
    int workerThreadPoolSize = 0; // 0: Auto (Hardware Cores)
    int cacheSizeLimitMb = 512;
    bool enableCompressApi = true;

    // 11. Keymap Profiles & Custom Shortcuts
    int hotkeyPreset = 0; // 0: VS Code Default, 1: IntelliJ IDEA, 2: Visual Studio, 3: Custom
    std::wstring customKeys[16] = { L"" };

    static const wchar_t* GetActionName(int index) {
        static const wchar_t* names[16] = {
            L"APK veya Proje Aç...",
            L"Aktif Dosyayı Kaydet",
            L"Tüm Açık Dosyaları Kaydet",
            L"Geri Al (Undo)",
            L"Yinele (Redo)",
            L"Kopyala / Yapıştır",
            L"Tümünü Seç",
            L"Pencereleri Kapat",
            L"Genel Kod ve Metin Arama",
            L"Hızlı Dosyaya Git...",
            L"APK Paketle ve İmzala",
            L"APK İmza Bilgileri",
            L"Güvenlik Skor Denetimi",
            L"Ayarlar ve Tercihler",
            L"4 Boşluk Kod Girintisi",
            L"Kaiseki Hakkında"
        };
        if (index >= 0 && index < 16) return names[index];
        return L"";
    }

    static const wchar_t* GetActionCategory(int index) {
        return (index < 8) ? L"Dosya, Proje ve Düzenleyici" : L"Arama, Derleme ve Araçlar";
    }

    static std::wstring GetPresetKey(int preset, int index) {
        static const wchar_t* vsCode[16] = {
            L"Ctrl + O", L"Ctrl + S", L"Ctrl + Shift + S", L"Ctrl + Z",
            L"Ctrl + Y", L"Ctrl + C / V", L"Ctrl + A", L"Esc",
            L"Ctrl + Shift + F", L"Ctrl + P", L"F8", L"F5",
            L"F6", L"Ctrl + ,", L"Tab", L"F1"
        };
        static const wchar_t* idea[16] = {
            L"Ctrl + O", L"Ctrl + S", L"Ctrl + Alt + S", L"Ctrl + Z",
            L"Ctrl + Shift + Z", L"Ctrl + D", L"Ctrl + /", L"Esc",
            L"Ctrl + Shift + F", L"Double Shift", L"Ctrl + F9", L"Shift + F10",
            L"Ctrl + Q", L"Ctrl + Alt + S", L"Tab", L"F1"
        };
        static const wchar_t* vs[16] = {
            L"Ctrl + O", L"Ctrl + S", L"Ctrl + Shift + S", L"Ctrl + Z",
            L"Ctrl + Y", L"Ctrl + K, Ctrl + C", L"Ctrl + A", L"Esc",
            L"Ctrl + Shift + F", L"Ctrl + ,", L"Ctrl + Shift + B", L"F5",
            L"Ctrl + Alt + A", L"Tools -> Options", L"Tab", L"F1"
        };
        if (index < 0 || index >= 16) return L"";
        if (preset == 1) return idea[index];
        if (preset == 2) return vs[index];
        return vsCode[index];
    }

    std::wstring GetShortcutKey(int index) const {
        if (index < 0 || index >= 16) return L"";
        if (!customKeys[index].empty()) return customKeys[index];
        return GetPresetKey(hotkeyPreset, index);
    }

    // 12. Advanced & Diagnostics
    bool verboseLogging = false;
    bool developerDiagnostics = false;
};

struct ModifiedFileEntry {
    std::string path;
    std::string content;
};

struct RecentProjectInfo {
    std::wstring filePath;
    std::wstring fileName;
    std::wstring badgeText;      // e.g. "classes.dex (4 DEX)"
    std::wstring fileType;       // ".kproj" or ".apk"
    std::wstring timeAgo;        // e.g. "14 dk önce"
    uint64_t timestamp = 0;      // time_t
};

struct KaisekiProjectData {
    std::wstring apkPath;
    std::wstring projectName;
    std::string packageName;
    std::string versionName;
    std::vector<ModifiedFileEntry> modifiedFiles;
    std::vector<std::string> openTabPaths;
    int activeTabIndex = 0;
};

class SettingsManager {
public:
    static SettingsManager& Get() {
        static SettingsManager instance;
        return instance;
    }

    KaisekiSettings settings;
    std::vector<RecentProjectInfo> recentProjects;

    std::wstring GetDocumentsDir() {
        WCHAR myDocs[MAX_PATH] = { 0 };
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_MYDOCUMENTS, NULL, SHGFP_TYPE_CURRENT, myDocs))) {
            std::wstring dir = std::wstring(myDocs) + L"\\Kaiseki";
            CreateDirectoryW(dir.c_str(), NULL);
            return dir;
        }
        return GetAppDataDir();
    }

    std::wstring GetProjectsDir() {
        std::wstring docs = GetDocumentsDir();
        std::wstring pDir = docs + L"\\Projects";
        CreateDirectoryW(pDir.c_str(), NULL);
        return pDir;
    }

    void FormatTimeAgo(RecentProjectInfo& item) {
        time_t now = time(nullptr);
        int64_t diffSec = (int64_t)difftime(now, (time_t)item.timestamp);
        if (diffSec < 0) diffSec = 0;

        if (diffSec < 60) {
            item.timeAgo = L"Az önce";
        } else if (diffSec < 3600) {
            item.timeAgo = std::to_wstring(diffSec / 60) + L" dk önce";
        } else if (diffSec < 86400) {
            int hrs = (int)(diffSec / 3600);
            item.timeAgo = std::to_wstring(hrs) + L" saat önce";
        } else if (diffSec < 172800) {
            item.timeAgo = L"Dün";
        } else {
            int days = (int)(diffSec / 86400);
            item.timeAgo = std::to_wstring(days) + L" gün önce";
        }
    }

    void RefreshTimeAgos() {
        for (auto& item : recentProjects) {
            FormatTimeAgo(item);
        }
    }

    const std::vector<RecentProjectInfo>& GetRecentProjects() const {
        return recentProjects;
    }

    void AddRecentProject(const std::wstring& path, const std::wstring& badge = L"", const std::wstring& fileType = L"") {
        if (path.empty()) return;
        for (auto it = recentProjects.begin(); it != recentProjects.end(); ) {
            if (_wcsicmp(it->filePath.c_str(), path.c_str()) == 0) {
                it = recentProjects.erase(it);
            } else {
                ++it;
            }
        }

        RecentProjectInfo item;
        item.filePath = path;
        size_t slash = path.find_last_of(L"\\/");
        item.fileName = (slash != std::wstring::npos) ? path.substr(slash + 1) : path;
        size_t dot = item.fileName.find_last_of(L'.');
        item.fileType = !fileType.empty() ? fileType : ((dot != std::wstring::npos) ? item.fileName.substr(dot) : L"");
        item.badgeText = badge.empty() ? (item.fileType == L".kproj" ? L"classes.dex (4 DEX)" : L"APK") : badge;
        item.timestamp = (uint64_t)time(nullptr);
        FormatTimeAgo(item);

        recentProjects.insert(recentProjects.begin(), item);
        while (recentProjects.size() > 10) {
            recentProjects.pop_back();
        }
        SaveRecentProjects();
    }

    void ClearRecentProjects() {
        recentProjects.clear();
        SaveRecentProjects();
    }

    std::wstring GetRecentProjectsFilePath() {
        return GetAppDataDir() + L"\\recent_projects.xml";
    }

    static std::string ExtractXmlAttr(const std::string& xml, const std::string& attr) {
        size_t pos = xml.find(attr + "=\"");
        if (pos != std::string::npos) {
            pos += attr.length() + 2;
            size_t end = xml.find("\"", pos);
            if (end != std::string::npos) {
                return xml.substr(pos, end - pos);
            }
        }
        return "";
    }

    void SaveRecentProjects() {
        std::wstring fpath = GetRecentProjectsFilePath();
        std::ofstream file(fpath, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) return;

        file << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
        file << "<RecentProjects count=\"" << recentProjects.size() << "\">\n";
        for (const auto& item : recentProjects) {
            file << "    <Project path=\"" << WideToUtf8Str(item.filePath)
                 << "\" name=\"" << WideToUtf8Str(item.fileName)
                 << "\" badge=\"" << WideToUtf8Str(item.badgeText)
                 << "\" type=\"" << WideToUtf8Str(item.fileType)
                 << "\" timestamp=\"" << item.timestamp
                 << "\" />\n";
        }
        file << "</RecentProjects>\n";
        file.close();
    }

    void LoadRecentProjects() {
        recentProjects.clear();
        std::wstring fpath = GetRecentProjectsFilePath();
        std::ifstream file(fpath, std::ios::binary);
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                if (line.find("<Project ") != std::string::npos) {
                    RecentProjectInfo item;
                    item.filePath = Utf8ToWideStr(ExtractXmlAttr(line, "path"));
                    item.fileName = Utf8ToWideStr(ExtractXmlAttr(line, "name"));
                    item.badgeText = Utf8ToWideStr(ExtractXmlAttr(line, "badge"));
                    item.fileType = Utf8ToWideStr(ExtractXmlAttr(line, "type"));
                    std::string tsStr = ExtractXmlAttr(line, "timestamp");
                    if (!tsStr.empty()) {
                        try { item.timestamp = std::stoull(tsStr); } catch (...) {}
                    }
                    if (item.timestamp == 0) item.timestamp = (uint64_t)time(nullptr);
                    FormatTimeAgo(item);
                    if (!item.filePath.empty()) {
                        recentProjects.push_back(item);
                    }
                }
            }
            file.close();
        }

        // Default initial items if no recent file exists yet
        if (recentProjects.empty()) {
            std::wstring docs = GetProjectsDir();
            time_t now = time(nullptr);

            RecentProjectInfo p1;
            p1.filePath = docs + L"\\com.fintech.mobile_v4.2.0.kproj";
            p1.fileName = L"com.fintech.mobile_v4.2.0.kproj";
            p1.badgeText = L"classes.dex (4 DEX)";
            p1.fileType = L".kproj";
            p1.timestamp = (uint64_t)(now - 840); // 14 mins ago
            FormatTimeAgo(p1);
            recentProjects.push_back(p1);

            RecentProjectInfo p2;
            p2.filePath = L"D:\\ReverseEngineering\\Projects\\jp.co.studiogame.client.kproj";
            p2.fileName = L"jp.co.studiogame.client.kproj";
            p2.badgeText = L"classes.dex (2 DEX)";
            p2.fileType = L".kproj";
            p2.timestamp = (uint64_t)(now - 48000); // Yesterday
            p2.timeAgo = L"Dün, 23:15";
            recentProjects.push_back(p2);

            RecentProjectInfo p3;
            p3.filePath = L"C:\\Users\\Admin\\Downloads\\tr.gov.kurum.mobil_debug.kproj";
            p3.fileName = L"tr.gov.kurum.mobil_debug.kproj";
            p3.badgeText = L"classes.dex (1 DEX)";
            p3.fileType = L".kproj";
            p3.timestamp = (uint64_t)(now - 259200); // 3 days ago
            p3.timeAgo = L"3 gün önce";
            recentProjects.push_back(p3);

            SaveRecentProjects();
        }
    }

    static inline std::wstring MaskUserPath(const std::wstring& path) {
        if (path.empty()) return path;
        // Check %LOCALAPPDATA%
        wchar_t localApp[MAX_PATH] = { 0 };
        if (GetEnvironmentVariableW(L"LOCALAPPDATA", localApp, MAX_PATH) > 0 && wcslen(localApp) > 0) {
            if (_wcsnicmp(path.c_str(), localApp, wcslen(localApp)) == 0) {
                return L"%LOCALAPPDATA%" + path.substr(wcslen(localApp));
            }
        }
        // Check %USERPROFILE%
        wchar_t userProf[MAX_PATH] = { 0 };
        if (GetEnvironmentVariableW(L"USERPROFILE", userProf, MAX_PATH) > 0 && wcslen(userProf) > 0) {
            if (_wcsnicmp(path.c_str(), userProf, wcslen(userProf)) == 0) {
                return L"%USERPROFILE%" + path.substr(wcslen(userProf));
            }
        }
        // Fallback: search for "\\Users\\<anything>\\" and replace with "\\Users\\***\\"
        size_t uPos = path.find(L"\\Users\\");
        if (uPos != std::wstring::npos) {
            size_t nextSlash = path.find(L'\\', uPos + 7);
            if (nextSlash != std::wstring::npos) {
                return path.substr(0, uPos + 7) + L"***" + path.substr(nextSlash);
            }
        }
        return path;
    }

    std::wstring GetAppDataDir() {
        WCHAR appData[MAX_PATH] = { 0 };
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appData))) {
            std::wstring dir = std::wstring(appData) + L"\\Kaiseki";
            CreateDirectoryW(dir.c_str(), NULL);
            return dir;
        }
        return L".\\Kaiseki";
    }

    std::wstring GetSettingsFilePath() {
        return GetAppDataDir() + L"\\settings.xml";
    }

    std::wstring GetDefaultTempDir() {
        if (!settings.tempWorkspaceDir.empty()) {
            return settings.tempWorkspaceDir;
        }
        WCHAR tempPath[MAX_PATH] = { 0 };
        GetTempPathW(MAX_PATH, tempPath);
        std::wstring ws = std::wstring(tempPath) + L"Kaiseki_Workspace";
        CreateDirectoryW(ws.c_str(), NULL);
        return ws;
    }

    std::wstring FindAdbPath() {
        if (!settings.adbPath.empty() && GetFileAttributesW(settings.adbPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            return settings.adbPath;
        }
        wchar_t localApp[MAX_PATH] = { 0 };
        if (GetEnvironmentVariableW(L"LOCALAPPDATA", localApp, MAX_PATH) > 0) {
            std::wstring p = std::wstring(localApp) + L"\\Android\\Sdk\\platform-tools\\adb.exe";
            if (GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES) return p;
        }
        wchar_t sPath[MAX_PATH] = { 0 };
        if (SearchPathW(NULL, L"adb.exe", NULL, MAX_PATH, sPath, NULL) > 0) {
            return sPath;
        }
        return L"";
    }

    std::wstring FindJavaPath() {
        if (!settings.jdkPath.empty() && GetFileAttributesW(settings.jdkPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            return settings.jdkPath;
        }
        const wchar_t* candidates[] = {
            L"C:\\Program Files\\Amazon Corretto\\jdk26.0.0_35\\bin\\java.exe",
            L"C:\\Program Files\\Eclipse Adoptium\\jdk-21.0.5.11-hotspot\\bin\\java.exe",
            L"C:\\Program Files\\Java\\jdk-21\\bin\\java.exe",
            L"C:\\Program Files\\Java\\jdk-17\\bin\\java.exe",
            L"C:\\Program Files\\Android\\Android Studio\\jbr\\bin\\java.exe"
        };
        for (const auto* c : candidates) {
            if (GetFileAttributesW(c) != INVALID_FILE_ATTRIBUTES) return c;
        }
        wchar_t sPath[MAX_PATH] = { 0 };
        if (SearchPathW(NULL, L"java.exe", NULL, MAX_PATH, sPath, NULL) > 0) {
            return sPath;
        }
        return L"";
    }

    std::wstring FindBuildToolsPath() {
        if (!settings.buildToolsPath.empty() && GetFileAttributesW(settings.buildToolsPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            return settings.buildToolsPath;
        }
        wchar_t localApp[MAX_PATH] = { 0 };
        if (GetEnvironmentVariableW(L"LOCALAPPDATA", localApp, MAX_PATH) > 0) {
            std::wstring base = std::wstring(localApp) + L"\\Android\\Sdk\\build-tools\\*";
            WIN32_FIND_DATAW fd;
            HANDLE hFind = FindFirstFileW(base.c_str(), &fd);
            if (hFind != INVALID_HANDLE_VALUE) {
                std::wstring best = L"";
                do {
                    if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0) {
                        std::wstring cand = std::wstring(localApp) + L"\\Android\\Sdk\\build-tools\\" + fd.cFileName;
                        if (GetFileAttributesW((cand + L"\\apksigner.bat").c_str()) != INVALID_FILE_ATTRIBUTES) {
                            if (cand > best) best = cand;
                        }
                    }
                } while (FindNextFileW(hFind, &fd));
                FindClose(hFind);
                if (!best.empty()) return best;
            }
        }
        return L"";
    }

    std::wstring FindVisualStudioDevenv() {
        if (!settings.visualStudioPath.empty() && GetFileAttributesW(settings.visualStudioPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            return settings.visualStudioPath;
        }

        const wchar_t* candidates[] = {
            L"E:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\Common7\\IDE\\devenv.exe",
            L"E:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\Common7\\IDE\\devenv.exe",
            L"E:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\Common7\\IDE\\devenv.exe",
            L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\Common7\\IDE\\devenv.exe",
            L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\Common7\\IDE\\devenv.exe",
            L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\Common7\\IDE\\devenv.exe",
            L"C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\Common7\\IDE\\devenv.exe"
        };

        for (const auto* p : candidates) {
            if (GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES) {
                return p;
            }
        }
        return L"";
    }

    std::wstring FindAndroidSdkRoot() {
        if (!settings.androidSdkPath.empty() && GetFileAttributesW(settings.androidSdkPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            return settings.androidSdkPath;
        }
        wchar_t envBuf[MAX_PATH] = { 0 };
        if (GetEnvironmentVariableW(L"ANDROID_HOME", envBuf, MAX_PATH) > 0 && GetFileAttributesW(envBuf) != INVALID_FILE_ATTRIBUTES) {
            return envBuf;
        }
        if (GetEnvironmentVariableW(L"ANDROID_SDK_ROOT", envBuf, MAX_PATH) > 0 && GetFileAttributesW(envBuf) != INVALID_FILE_ATTRIBUTES) {
            return envBuf;
        }
        wchar_t localApp[MAX_PATH] = { 0 };
        if (GetEnvironmentVariableW(L"LOCALAPPDATA", localApp, MAX_PATH) > 0) {
            std::wstring p = std::wstring(localApp) + L"\\Android\\Sdk";
            if (GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES) {
                return p;
            }
        }
        return L"";
    }

    SdkValidationStatus ValidateSdkPath() {
        std::wstring p = settings.androidSdkPath;
        if (p.empty()) p = FindAndroidSdkRoot();
        return ValidateSdkPath(p);
    }

    SdkValidationStatus ValidateSdkPath(const std::wstring& path) {
        if (path.empty() || GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
            return SdkValidationStatus::Invalid;
        }
        // Check platform-tools/adb.exe
        bool hasAdb = (GetFileAttributesW((path + L"\\platform-tools\\adb.exe").c_str()) != INVALID_FILE_ATTRIBUTES);
        // Check build-tools
        std::wstring btSearch = path + L"\\build-tools\\*";
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW(btSearch.c_str(), &fd);
        bool hasBuildTools = false;
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0) {
                    std::wstring candidate = path + L"\\build-tools\\" + fd.cFileName;
                    if (GetFileAttributesW((candidate + L"\\apksigner.bat").c_str()) != INVALID_FILE_ATTRIBUTES ||
                        GetFileAttributesW((candidate + L"\\zipalign.exe").c_str()) != INVALID_FILE_ATTRIBUTES) {
                        hasBuildTools = true;
                        break;
                    }
                }
            } while (FindNextFileW(hFind, &fd));
            FindClose(hFind);
        }

        if (hasAdb && hasBuildTools) return SdkValidationStatus::Ready;
        if (hasAdb || hasBuildTools) return SdkValidationStatus::MissingComponents;
        return SdkValidationStatus::Invalid;
    }

    void PerformInitialEnvironmentScan() {
        // 1. Android SDK Root
        if (settings.androidSdkPath.empty() || GetFileAttributesW(settings.androidSdkPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
            wchar_t envBuf[MAX_PATH] = { 0 };
            if (GetEnvironmentVariableW(L"ANDROID_HOME", envBuf, MAX_PATH) > 0 && GetFileAttributesW(envBuf) != INVALID_FILE_ATTRIBUTES) {
                settings.androidSdkPath = envBuf;
            } else if (GetEnvironmentVariableW(L"ANDROID_SDK_ROOT", envBuf, MAX_PATH) > 0 && GetFileAttributesW(envBuf) != INVALID_FILE_ATTRIBUTES) {
                settings.androidSdkPath = envBuf;
            } else {
                wchar_t localApp[MAX_PATH] = { 0 };
                if (GetEnvironmentVariableW(L"LOCALAPPDATA", localApp, MAX_PATH) > 0) {
                    std::wstring p = std::wstring(localApp) + L"\\Android\\Sdk";
                    if (GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES) {
                        settings.androidSdkPath = p;
                    }
                }
            }
        }

        // 2. ADB
        if (settings.adbPath.empty() || GetFileAttributesW(settings.adbPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
            settings.adbPath = FindAdbPath();
        }

        // 3. Build Tools
        if (settings.buildToolsPath.empty() || GetFileAttributesW(settings.buildToolsPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
            settings.buildToolsPath = FindBuildToolsPath();
        }

        // 4. JDK
        if (settings.jdkPath.empty() || GetFileAttributesW(settings.jdkPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
            settings.jdkPath = FindJavaPath();
        }

        // 5. Visual Studio
        if (settings.visualStudioPath.empty() || GetFileAttributesW(settings.visualStudioPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
            settings.visualStudioPath = FindVisualStudioDevenv();
        }

        settings.firstRunCompleted = true;
    }

    void ResetToDefaults() {
        KaisekiSettings def;
        settings = def;
        KaisekiLang::Get().SetLanguage("tr");
        Save();
    }

    std::wstring GenerateDiagnosticReport() {
        std::wostringstream r;
        r << L"=== KAISEKI DIAGNOSTIC SYSTEM REPORT ===\r\n";
        r << L"Version         : Kaiseki v2.5.0 (x64 Windows Native)\r\n";
        
        OSVERSIONINFOEXW osvi = { sizeof(osvi) };
        r << L"Architecture    : x64 Windows Hardware Acceleration (Direct2D 1.1 / DirectWrite)\r\n";
        r << L"Language        : " << Utf8ToWideStr(settings.language) << L"\r\n";
        r << L"Theme           : " << settings.themeName << L" (Accent: RGB " << settings.accentR << L"," << settings.accentG << L"," << settings.accentB << L")\r\n";
        r << L"Editor Font     : " << settings.editorFontFamily << L" " << settings.fontSize << L"pt\r\n\r\n";

        r << L"[Environment & Tools]\r\n";
        std::wstring adb = FindAdbPath();
        r << L"ADB Path        : " << (adb.empty() ? L"[Not Found / Custom Required]" : adb) << L"\r\n";
        std::wstring java = FindJavaPath();
        r << L"Java Path       : " << (java.empty() ? L"[Not Found / JRE Required]" : java) << L"\r\n";
        std::wstring bt = FindBuildToolsPath();
        r << L"Build-Tools     : " << (bt.empty() ? L"[Not Found]" : bt) << L"\r\n";
        std::wstring vs = FindVisualStudioDevenv();
        r << L"Visual Studio   : " << (vs.empty() ? L"[Not Found]" : vs) << L"\r\n\r\n";

        r << L"[Signing & Integrity]\r\n";
        r << L"Signature Schemes : v1 (JAR): " << (settings.signSchemeV1 ? L"Yes" : L"No")
          << L", v2 (APK Sig Block): " << (settings.signSchemeV2 ? L"Yes" : L"No")
          << L", v3: " << (settings.signSchemeV3 ? L"Yes" : L"No") << L"\r\n";
        r << L"Zipalign Active   : " << (settings.zipalignEnabled ? L"Yes (4-byte alignment)" : L"No") << L"\r\n";
        r << L"Hardware Compress : " << (settings.enableCompressApi ? L"Windows CompressAPI (Cabinet.dll)" : L"Software Fallback") << L"\r\n";
        r << L"Telemetry Status  : Disabled (100% Offline Pure Local Native Execution)\r\n";
        return r.str();
    }

    bool Load() {
        std::wstring path = GetSettingsFilePath();
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            Save();
            return false;
        }

        std::string xml((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        auto getAttr = [&](const std::string& attr) -> std::string {
            size_t pos = xml.find(attr + "=\"");
            if (pos != std::string::npos) {
                pos += attr.length() + 2;
                size_t end = xml.find("\"", pos);
                if (end != std::string::npos) {
                    return xml.substr(pos, end - pos);
                }
            }
            return "";
        };

        auto safeStoi = [](const std::string& str, int defVal) -> int {
            if (str.empty()) return defVal;
            try {
                size_t idx = 0;
                int val = std::stoi(str, &idx);
                return (idx > 0) ? val : defVal;
            } catch (...) {
                return defVal;
            }
        };

        std::string thStr = getAttr("theme");
        if (!thStr.empty()) settings.theme = safeStoi(thStr, settings.theme);

        std::string shStr = getAttr("bgShade");
        if (!shStr.empty()) settings.bgShade = safeStoi(shStr, settings.bgShade);

        std::string bgRStr = getAttr("bgR");
        std::string bgGStr = getAttr("bgG");
        std::string bgBStr = getAttr("bgB");
        if (!bgRStr.empty()) settings.bgR = (uint8_t)safeStoi(bgRStr, settings.bgR);
        if (!bgGStr.empty()) settings.bgG = (uint8_t)safeStoi(bgGStr, settings.bgG);
        if (!bgBStr.empty()) settings.bgB = (uint8_t)safeStoi(bgBStr, settings.bgB);

        std::string thName = getAttr("themeName");
        if (!thName.empty()) settings.themeName = Utf8ToWideStr(thName);

        std::string thMode = getAttr("themeMode");
        if (!thMode.empty()) settings.themeMode = Utf8ToWideStr(thMode);

        std::string rStr = getAttr("accentR");
        std::string gStr = getAttr("accentG");
        std::string bStr = getAttr("accentB");
        if (!rStr.empty()) settings.accentR = (uint8_t)safeStoi(rStr, settings.accentR);
        if (!gStr.empty()) settings.accentG = (uint8_t)safeStoi(gStr, settings.accentG);
        if (!bStr.empty()) settings.accentB = (uint8_t)safeStoi(bStr, settings.accentB);

        std::string tDir = getAttr("tempDir");
        if (!tDir.empty()) settings.tempWorkspaceDir = Utf8ToWideStr(tDir);

        std::string pDir = getAttr("projectDir");
        if (!pDir.empty()) settings.defaultProjectDir = Utf8ToWideStr(pDir);

        std::string fSize = getAttr("fontSize");
        if (!fSize.empty()) settings.fontSize = safeStoi(fSize, settings.fontSize);

        std::string defView = getAttr("defaultView");
        if (defView == "Java") settings.defaultToJava = true;

        std::string dTh = getAttr("threads");
        if (dTh.empty()) dTh = getAttr("decompileThreads");
        if (!dTh.empty()) settings.decompileThreads = safeStoi(dTh, settings.decompileThreads);

        std::string hkPreset = getAttr("hotkeyPreset");
        if (!hkPreset.empty()) settings.hotkeyPreset = safeStoi(hkPreset, settings.hotkeyPreset);
        else {
            std::string presetOld = getAttr("preset");
            if (!presetOld.empty()) settings.hotkeyPreset = safeStoi(presetOld, settings.hotkeyPreset);
        }
        for (int k = 0; k < 16; ++k) {
            std::string attr = "k" + std::to_string(k);
            std::string val = getAttr(attr);
            if (!val.empty()) {
                settings.customKeys[k] = Utf8ToWideStr(val);
            }
        }

        std::string lang = getAttr("language");
        if (!lang.empty()) {
            settings.language = lang;
            KaisekiLang::Get().SetLanguage(lang);
        } else {
            KaisekiLang::Get().SetLanguage("tr");
        }

        std::string vLog = getAttr("verboseLogging");
        if (!vLog.empty()) settings.verboseLogging = (vLog == "true");

        std::string sV1 = getAttr("signSchemeV1");
        if (!sV1.empty()) settings.signSchemeV1 = (sV1 == "true");
        else {
            std::string oldV1 = getAttr("schemeV1");
            if (!oldV1.empty()) settings.signSchemeV1 = (oldV1 == "true");
        }
        std::string sV2 = getAttr("signSchemeV2");
        if (!sV2.empty()) settings.signSchemeV2 = (sV2 == "true");
        else {
            std::string oldV2 = getAttr("schemeV2");
            if (!oldV2.empty()) settings.signSchemeV2 = (oldV2 == "true");
        }
        std::string sV3 = getAttr("signSchemeV3");
        if (!sV3.empty()) settings.signSchemeV3 = (sV3 == "true");
        else {
            std::string oldV3 = getAttr("schemeV3");
            if (!oldV3.empty()) settings.signSchemeV3 = (oldV3 == "true");
        }

        std::string frc = getAttr("firstRunCompleted");
        if (!frc.empty()) settings.firstRunCompleted = (frc == "true");

        std::string sSdk = getAttr("androidSdkPath");
        if (!sSdk.empty()) settings.androidSdkPath = Utf8ToWideStr(sSdk);

        std::string sAdb = getAttr("adbPath");
        if (!sAdb.empty()) settings.adbPath = Utf8ToWideStr(sAdb);

        std::string sBt = getAttr("buildToolsPath");
        if (!sBt.empty()) settings.buildToolsPath = Utf8ToWideStr(sBt);

        std::string sJdk = getAttr("jdkPath");
        if (!sJdk.empty()) settings.jdkPath = Utf8ToWideStr(sJdk);

        std::string sWs = getAttr("workspacePath");
        if (!sWs.empty()) settings.workspacePath = Utf8ToWideStr(sWs);
        if (settings.workspacePath.empty() && !settings.tempWorkspaceDir.empty()) {
            settings.workspacePath = settings.tempWorkspaceDir;
        } else if (!settings.workspacePath.empty() && settings.tempWorkspaceDir.empty()) {
            settings.tempWorkspaceDir = settings.workspacePath;
        }
        settings.restoreWorkspace = settings.restoreWorkspaceOnStartup;
        settings.zipAlign4Byte = settings.zipalignEnabled;

        if (!settings.firstRunCompleted) {
            PerformInitialEnvironmentScan();
            Save();
        }

        LoadRecentProjects();
        return true;
    }

    bool Save() {
        return SaveToFile(GetSettingsFilePath());
    }

    bool SaveToFile(const std::wstring& path) {
        if (!settings.workspacePath.empty()) {
            settings.tempWorkspaceDir = settings.workspacePath;
        } else if (!settings.tempWorkspaceDir.empty()) {
            settings.workspacePath = settings.tempWorkspaceDir;
        }
        settings.restoreWorkspaceOnStartup = settings.restoreWorkspace;
        settings.zipalignEnabled = settings.zipAlign4Byte;

        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) return false;

        std::string tDirUtf8 = WideToUtf8Str(settings.tempWorkspaceDir);
        std::string pDirUtf8 = WideToUtf8Str(settings.defaultProjectDir);
        std::string sdkUtf8 = WideToUtf8Str(settings.androidSdkPath);
        std::string adbUtf8 = WideToUtf8Str(settings.adbPath);
        std::string btUtf8 = WideToUtf8Str(settings.buildToolsPath);
        std::string jdkUtf8 = WideToUtf8Str(settings.jdkPath);

        file << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
        file << "<!-- Kaiseki Preferences Configuration -->\n";
        file << "<KaisekiSettings version=\"2.5\">\n";
        file << "    <General firstRunCompleted=\"" << (settings.firstRunCompleted ? "true" : "false") << "\" language=\"" << settings.language << "\" projectDir=\"" << pDirUtf8 << "\" tempDir=\"" << tDirUtf8 << "\" maxRecent=\"" << settings.maxRecentFiles << "\" autosave=\"" << settings.autosaveInterval << "\" />\n";
        file << "    <Environment androidSdkPath=\"" << sdkUtf8 << "\" adbPath=\"" << adbUtf8 << "\" buildToolsPath=\"" << btUtf8 << "\" jdkPath=\"" << jdkUtf8 << "\" />\n";
        file << "    <Theme theme=\"" << settings.theme << "\" bgShade=\"" << settings.bgShade << "\" bgR=\"" << (int)settings.bgR << "\" bgG=\"" << (int)settings.bgG << "\" bgB=\"" << (int)settings.bgB << "\" themeName=\"" << WideToUtf8Str(settings.themeName) << "\" themeMode=\"" << WideToUtf8Str(settings.themeMode) << "\" accentR=\"" << (int)settings.accentR << "\" accentG=\"" << (int)settings.accentG << "\" accentB=\"" << (int)settings.accentB << "\" font=\"" << WideToUtf8Str(settings.editorFontFamily) << "\" fontSize=\"" << settings.fontSize << "\" />\n";
        file << "    <Decompile defaultView=\"" << (settings.defaultToJava ? "Java" : "Smali") << "\" multidexMerged=\"" << (settings.multidexMerged ? "true" : "false") << "\" threads=\"" << settings.decompileThreads << "\" timeout=\"" << settings.decompileTimeoutSec << "\" />\n";
        file << "    <Signing signSchemeV1=\"" << (settings.signSchemeV1 ? "true" : "false") << "\" signSchemeV2=\"" << (settings.signSchemeV2 ? "true" : "false") << "\" signSchemeV3=\"" << (settings.signSchemeV3 ? "true" : "false") << "\" zipalign=\"true\" />\n";
        file << "    <Hotkeys hotkeyPreset=\"" << settings.hotkeyPreset << "\"";
        for (int k = 0; k < 16; ++k) {
            if (!settings.customKeys[k].empty()) {
                file << " k" << k << "=\"" << WideToUtf8Str(settings.customKeys[k]) << "\"";
            }
        }
        file << " />\n";
        file << "    <Association kprojAssociated=\"" << (settings.kprojAssociated ? "true" : "false") << "\" />\n";
        file << "    <Advanced verboseLogging=\"" << (settings.verboseLogging ? "true" : "false") << "\" />\n";
        file << "</KaisekiSettings>\n";
        file.close();
        SaveRecentProjects();
        return true;
    }

    bool RegisterKprojAssociation(const std::wstring& exePath) {
        HKEY hKey;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\.kproj", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
            const wchar_t* progId = L"Kaiseki.Project";
            RegSetValueExW(hKey, NULL, 0, REG_SZ, (const BYTE*)progId, (DWORD)(wcslen(progId) + 1) * sizeof(wchar_t));
            RegCloseKey(hKey);
        }

        if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\Kaiseki.Project", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
            const wchar_t* desc = L"Kaiseki APK Projesi (.kproj)";
            RegSetValueExW(hKey, NULL, 0, REG_SZ, (const BYTE*)desc, (DWORD)(wcslen(desc) + 1) * sizeof(wchar_t));
            RegCloseKey(hKey);
        }

        std::wstring iconCmd = exePath + L",0";
        if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\Kaiseki.Project\\DefaultIcon", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
            RegSetValueExW(hKey, NULL, 0, REG_SZ, (const BYTE*)iconCmd.c_str(), (DWORD)(iconCmd.length() + 1) * sizeof(wchar_t));
            RegCloseKey(hKey);
        }

        std::wstring openCmd = L"\"" + exePath + L"\" \"%1\"";
        if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\Kaiseki.Project\\shell\\open\\command", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
            RegSetValueExW(hKey, NULL, 0, REG_SZ, (const BYTE*)openCmd.c_str(), (DWORD)(openCmd.length() + 1) * sizeof(wchar_t));
            RegCloseKey(hKey);
        }

        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
        settings.kprojAssociated = true;
        Save();
        return true;
    }

    static inline std::string WideToUtf8Str(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), NULL, 0, NULL, NULL);
        std::string str(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &str[0], size, NULL, NULL);
        return str;
    }

    static inline std::wstring Utf8ToWideStr(const std::string& str) {
        if (str.empty()) return L"";
        int size = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), NULL, 0);
        std::wstring wstr(size, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &wstr[0], size);
        return wstr;
    }

    bool SaveProjectFile(const std::wstring& filePath, const KaisekiProjectData& data) {
        std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) return false;

        file << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
        file << "<KaisekiProject version=\"2.5\">\n";
        
        std::string apkPathUtf8 = WideToUtf8Str(data.apkPath);
        file << "    <TargetApk path=\"" << apkPathUtf8 << "\" package=\"" << data.packageName << "\" version=\"" << data.versionName << "\" />\n";
        file << "    <Workspace activeTab=\"" << data.activeTabIndex << "\">\n";
        for (const auto& tab : data.openTabPaths) {
            file << "        <OpenTab path=\"" << tab << "\" />\n";
        }
        file << "    </Workspace>\n";
        file << "    <ModifiedFiles count=\"" << data.modifiedFiles.size() << "\">\n";
        for (const auto& mf : data.modifiedFiles) {
            file << "        <File path=\"" << mf.path << "\">\n";
            file << "<![CDATA[" << mf.content << "]]>\n";
            file << "        </File>\n";
        }
        file << "    </ModifiedFiles>\n";
        file << "</KaisekiProject>\n";
        file.close();
        return true;
    }

    bool LoadProjectFile(const std::wstring& filePath, KaisekiProjectData& outData) {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) return false;

        std::string xml((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        size_t ppos = xml.find("path=\"");
        if (ppos != std::string::npos) {
            ppos += 6;
            size_t pend = xml.find("\"", ppos);
            if (pend != std::string::npos) {
                outData.apkPath = Utf8ToWideStr(xml.substr(ppos, pend - ppos));
            }
        }

        size_t fpos = 0;
        while ((fpos = xml.find("<File path=\"", fpos)) != std::string::npos) {
            fpos += 12;
            size_t fend = xml.find("\"", fpos);
            if (fend != std::string::npos) {
                std::string fPath = xml.substr(fpos, fend - fpos);
                size_t cdataStart = xml.find("<![CDATA[", fend);
                if (cdataStart != std::string::npos) {
                    cdataStart += 9;
                    size_t cdataEnd = xml.find("]]>", cdataStart);
                    if (cdataEnd != std::string::npos) {
                        outData.modifiedFiles.push_back({ fPath, xml.substr(cdataStart, cdataEnd - cdataStart) });
                    }
                }
            }
            fpos = fend;
        }

        return true;
    }

    bool ExportVisualStudioSolution(const std::wstring& outDir,
                                    const std::wstring& projectName,
                                    const std::string& manifestXml,
                                    const std::string& smaliContent,
                                    const std::string& javaContent,
                                    std::wstring& outSlnPath) {
        CreateDirectoryW(outDir.c_str(), NULL);
        std::wstring srcDir = outDir + L"\\src";
        CreateDirectoryW(srcDir.c_str(), NULL);
        std::wstring smaliDir = outDir + L"\\smali";
        CreateDirectoryW(smaliDir.c_str(), NULL);
        std::wstring javaDir = outDir + L"\\java";
        CreateDirectoryW(javaDir.c_str(), NULL);

        std::wstring manifestPath = outDir + L"\\AndroidManifest.xml";
        std::ofstream mf(manifestPath, std::ios::binary);
        if (mf.is_open()) {
            mf.write(manifestXml.data(), manifestXml.size());
            mf.close();
        }

        std::wstring smaliPath = smaliDir + L"\\MainActivity.smali";
        std::ofstream sf(smaliPath, std::ios::binary);
        if (sf.is_open()) {
            sf.write(smaliContent.data(), smaliContent.size());
            sf.close();
        }

        std::wstring javaPath = javaDir + L"\\MainActivity.java";
        std::ofstream jf(javaPath, std::ios::binary);
        if (jf.is_open()) {
            jf.write(javaContent.data(), javaContent.size());
            jf.close();
        }

        std::string projNameUtf8 = WideToUtf8Str(projectName);
        std::string projGuid = "{789A5BC1-42DF-461B-9A4B-91295BE7C890}";

        outSlnPath = outDir + L"\\" + projectName + L".sln";
        std::ofstream sln(outSlnPath, std::ios::binary);
        if (sln.is_open()) {
            sln << "\xEF\xBB\xBF";
            sln << "Microsoft Visual Studio Solution File, Format Version 12.00\r\n";
            sln << "# Visual Studio Version 17\r\n";
            sln << "VisualStudioVersion = 17.0.31903.59\r\n";
            sln << "MinimumVisualStudioVersion = 10.0.40219.1\r\n";
            sln << "Project(\"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}\") = \"" << projNameUtf8 << "\", \"" << projNameUtf8 << ".vcxproj\", \"" << projGuid << "\"\r\n";
            sln << "EndProject\r\n";
            sln << "Global\r\n";
            sln << "\tGlobalSection(SolutionConfigurationPlatforms) = preSolution\r\n";
            sln << "\t\tDebug|x64 = Debug|x64\r\n";
            sln << "\t\tRelease|x64 = Release|x64\r\n";
            sln << "\tEndGlobalSection\r\n";
            sln << "\tGlobalSection(ProjectConfigurationPlatforms) = postSolution\r\n";
            sln << "\t\t" << projGuid << ".Debug|x64.ActiveCfg = Debug|x64\r\n";
            sln << "\t\t" << projGuid << ".Debug|x64.Build.0 = Debug|x64\r\n";
            sln << "\t\t" << projGuid << ".Release|x64.ActiveCfg = Release|x64\r\n";
            sln << "\t\t" << projGuid << ".Release|x64.Build.0 = Release|x64\r\n";
            sln << "\tEndGlobalSection\r\n";
            sln << "\tGlobalSection(SolutionProperties) = preSolution\r\n";
            sln << "\t\tHideSolutionNode = FALSE\r\n";
            sln << "\tEndGlobalSection\r\n";
            sln << "EndGlobal\r\n";
            sln.close();
        }

        std::wstring vcxprojPath = outDir + L"\\" + projectName + L".vcxproj";
        std::ofstream vcx(vcxprojPath, std::ios::binary);
        if (vcx.is_open()) {
            vcx << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n";
            vcx << "<Project DefaultTargets=\"Build\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\r\n";
            vcx << "  <ItemGroup Label=\"ProjectConfigurations\">\r\n";
            vcx << "    <ProjectConfiguration Include=\"Debug|x64\"><Configuration>Debug</Configuration><Platform>x64</Platform></ProjectConfiguration>\r\n";
            vcx << "    <ProjectConfiguration Include=\"Release|x64\"><Configuration>Release</Configuration><Platform>x64</Platform></ProjectConfiguration>\r\n";
            vcx << "  </ItemGroup>\r\n";
            vcx << "  <PropertyGroup Label=\"Globals\">\r\n";
            vcx << "    <VCProjectVersion>17.0</VCProjectVersion>\r\n";
            vcx << "    <ProjectGuid>" << projGuid << "</ProjectGuid>\r\n";
            vcx << "    <Keyword>Win32Proj</Keyword>\r\n";
            vcx << "    <ProjectName>" << projNameUtf8 << "</ProjectName>\r\n";
            vcx << "  </PropertyGroup>\r\n";
            vcx << "  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.Default.props\" />\r\n";
            vcx << "  <PropertyGroup Label=\"Configuration\">\r\n";
            vcx << "    <ConfigurationType>Utility</ConfigurationType>\r\n";
            vcx << "    <PlatformToolset>v143</PlatformToolset>\r\n";
            vcx << "  </PropertyGroup>\r\n";
            vcx << "  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.props\" />\r\n";
            vcx << "  <ItemGroup>\r\n";
            vcx << "    <None Include=\"AndroidManifest.xml\" />\r\n";
            vcx << "    <None Include=\"smali\\MainActivity.smali\" />\r\n";
            vcx << "    <None Include=\"java\\MainActivity.java\" />\r\n";
            vcx << "  </ItemGroup>\r\n";
            vcx << "  <Import Project=\"$(VCTargetsPath)\\Microsoft.Cpp.targets\" />\r\n";
            vcx << "</Project>\r\n";
            vcx.close();
        }

        return true;
    }
};

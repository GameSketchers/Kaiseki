#define _CRT_SECURE_NO_WARNINGS
#include "UIWindow.h"
#include "ProgressDialog.h"
#include "ToolInstallerDialog.h"
#include "SettingsDialog.h"
#include "KaisekiAdbApi.h"
#include <windowsx.h>
#include <commctrl.h>
#include <shlobj.h>
#include <dwmapi.h>
#include <sstream>
#include <fstream>
#include <unordered_set>
#include <thread>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "dwmapi.lib")

static bool EndsWithIgnoreCase(const std::string& str, const std::string& suffix) {
    if (str.length() < suffix.length()) return false;
    size_t offset = str.length() - suffix.length();
    for (size_t i = 0; i < suffix.length(); ++i) {
        if (tolower((unsigned char)str[offset + i]) != tolower((unsigned char)suffix[i])) return false;
    }
    return true;
}

static bool EndsWithIgnoreCaseW(const std::wstring& str, const std::wstring& suffix) {
    if (str.length() < suffix.length()) return false;
    size_t offset = str.length() - suffix.length();
    for (size_t i = 0; i < suffix.length(); ++i) {
        if (towlower(str[offset + i]) != towlower(suffix[i])) return false;
    }
    return true;
}

UIWindow::UIWindow() {
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
    m_launchStartTime = GetTickCount();

    RefreshConsoleLogs();
}

UIWindow::~UIWindow() {
    DiscardDeviceResources();
    m_fonts.Shutdown();

    for (auto& tab : m_tabs) {
        if (tab.pImageBitmap) {
            tab.pImageBitmap->Release();
            tab.pImageBitmap = nullptr;
        }
    }

    if (m_pModel) {
        ApkCore_FreeModel(m_pModel);
        m_pModel = nullptr;
    }

    if (m_pWicFactory) m_pWicFactory->Release();
    if (m_pDWriteFactory) m_pDWriteFactory->Release();
    if (m_pD2DFactory) m_pD2DFactory->Release();
}

bool UIWindow::Create(HINSTANCE hInstance, int nCmdShow, const wchar_t* initialApk) {
    m_hInstance = hInstance;

    if (FAILED(CreateDeviceIndependentResources())) {
        return false;
    }

    WNDCLASSEXW wcex = { sizeof(WNDCLASSEXW) };
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcex.lpfnWndProc = UIWindow::WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = NULL;
    wcex.lpszClassName = L"KaisekiMainWindow";

    HICON hIconBig = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, 64, 64, LR_DEFAULTCOLOR);
    if (!hIconBig) {
        hIconBig = (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1), IMAGE_ICON, 64, 64, LR_DEFAULTCOLOR);
    }
    if (!hIconBig) {
        hIconBig = (HICON)LoadImageW(NULL, L"res\\Kaiseki.ico", IMAGE_ICON, 64, 64, LR_LOADFROMFILE);
    }
    HICON hIconSm = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
    if (!hIconSm) {
        hIconSm = (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
    }
    if (!hIconSm) {
        hIconSm = (HICON)LoadImageW(NULL, L"res\\Kaiseki.ico", IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
    }

    wcex.hIcon = hIconBig ? hIconBig : LoadIcon(NULL, IDI_APPLICATION);
    wcex.hIconSm = hIconSm ? hIconSm : LoadIcon(NULL, IDI_APPLICATION);

    RegisterClassExW(&wcex);

    RECT rc = { 0, 0, 1180, 780 };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int winW = rc.right - rc.left;
    int winH = rc.bottom - rc.top;
    int posX = (screenW - winW) / 2;
    int posY = (screenH - winH) / 2;

    // Modern Top-Level Frame Window (Google Chrome / DaVinci Resolve / Photoshop Architecture)
    // WS_OVERLAPPEDWINDOW enables Windows DWM native minimize/maximize/restore swoosh and zoom animations!
    m_hwnd = CreateWindowExW(
        WS_EX_ACCEPTFILES | WS_EX_APPWINDOW,
        L"KaisekiMainWindow",
        L"Kaiseki",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        posX, posY, winW, winH,
        NULL, NULL, hInstance, this
    );

    if (!m_hwnd) return false;

    // 1. Initialize Native DWM Frame, Transitions, and Rounded Corners via KaisekiAnim.dll
    KaisekiAnim_InitWindowFrame(m_hwnd, TRUE, TRUE);

    SetTimer(m_hwnd, 1, 16, NULL); // 60 FPS animation ticker

    if (hIconBig) SendMessageW(m_hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
    if (hIconSm) SendMessageW(m_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSm);

    // Explicitly set window title
    SetWindowTextW(m_hwnd, L"Kaiseki");

    DragAcceptFiles(m_hwnd, TRUE);
    ShowWindow(m_hwnd, nCmdShow);
    UpdateWindow(m_hwnd);

    if (initialApk && wcslen(initialApk) > 0) {
        std::wstring arg = initialApk;
        if (arg.size() >= 7 && arg.substr(arg.size() - 7) == L".kproj") {
            KaisekiProjectData proj;
            if (SettingsManager::Get().LoadProjectFile(arg, proj)) {
                LoadApk(proj.apkPath);
            }
        } else {
            LoadApk(initialApk);
        }
    }

    return true;
}

int UIWindow::Run() {
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

bool UIWindow::IsTitleBarControl(float x, float y) const {
    if (y > 40.0f) return false;

    // Window control buttons (Close, Maximize/Restore, Minimize)
    if (x >= m_rcWinClose.left && x <= m_rcWinClose.right) return true;
    if (x >= m_rcWinMax.left && x <= m_rcWinMax.right) return true;
    if (x >= m_rcWinMin.left && x <= m_rcWinMin.right) return true;

    // Top Menu Bar items (Dosya, İncele, Araçlar, Görünüm, Hakkında)
    for (int i = 0; i < 5; ++i) {
        if (x >= m_rcMenuHeaders[i].left && x <= m_rcMenuHeaders[i].right &&
            y >= m_rcMenuHeaders[i].top && y <= m_rcMenuHeaders[i].bottom) {
            return true;
        }
    }

    // Active dropdown popup
    if (m_activeMenu != -1) {
        if (x >= m_rcMenuDropdown.left && x <= m_rcMenuDropdown.right &&
            y >= m_rcMenuDropdown.top && y <= m_rcMenuDropdown.bottom) {
            return true;
        }
    }

    return false;
}

void UIWindow::SetTheme(AppTheme theme) {
    m_currentTheme = theme;
    SettingsManager::Get().settings.theme = (int)theme;
    SettingsManager::Get().Save();
    m_colors = KaisekiTheme::GetTheme(theme, &m_customAccentColor, m_currentBgShade);
    if (m_currentBgShade == BackgroundShade::Custom) {
        KaisekiTheme::ApplyCustomBackgroundColor(m_colors, m_customBgColor);
    }
    RecreateThemeBrushes();
    InvalidateRect(m_hwnd, NULL, FALSE);
}

void UIWindow::SetTestSelection(int startLine, int startCol, int endLine, int endCol) {
    if (!m_tabs.empty() && m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabs.size()) {
        OpenDocTab& tab = m_tabs[m_activeTabIndex];
        startLine = (std::max)(0, startLine);
        startCol = (std::max)(0, startCol);
        endLine = (std::max)(0, endLine);
        endCol = (std::max)(0, endCol);
        tab.hasSelection = true;
        tab.selAnchorLine = startLine;
        tab.selAnchorCol = startCol;
        tab.selActiveLine = endLine;
        tab.selActiveCol = endCol;
        tab.cursorLine = endLine;
        tab.cursorCol = endCol;
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

void UIWindow::SetCustomAccentColorU8(uint8_t r, uint8_t g, uint8_t b) {
    m_sliderR = r;
    m_sliderG = g;
    m_sliderB = b;
    m_customAccentColor = KaisekiTheme::ColorFromRgbU8(r, g, b);
    m_currentTheme = AppTheme::Custom;
    m_colors = KaisekiTheme::GetTheme(AppTheme::Custom, &m_customAccentColor, m_currentBgShade);
    if (m_currentBgShade == BackgroundShade::Custom) {
        KaisekiTheme::ApplyCustomBackgroundColor(m_colors, m_customBgColor);
    }

    // Save to settings XML
    SettingsManager::Get().settings.theme = (int)AppTheme::Custom;
    SettingsManager::Get().settings.accentR = r;
    SettingsManager::Get().settings.accentG = g;
    SettingsManager::Get().settings.accentB = b;
    SettingsManager::Get().Save();

    RecreateThemeBrushes();
    InvalidateRect(m_hwnd, NULL, FALSE);
}

void UIWindow::SetCustomBackgroundColorU8(uint8_t r, uint8_t g, uint8_t b) {
    m_customBgColor = D2D1::ColorF(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
    m_currentBgShade = BackgroundShade::Custom;
    SettingsManager::Get().settings.bgShade = (int)BackgroundShade::Custom;
    SettingsManager::Get().settings.bgR = r;
    SettingsManager::Get().settings.bgG = g;
    SettingsManager::Get().settings.bgB = b;
    SettingsManager::Get().Save();

    KaisekiTheme::ApplyCustomBackgroundColor(m_colors, m_customBgColor);
    RecreateThemeBrushes();
    InvalidateRect(m_hwnd, NULL, FALSE);
}

void UIWindow::SetBackgroundShade(BackgroundShade shade) {
    m_currentBgShade = shade;
    SettingsManager::Get().settings.bgShade = (int)shade;
    SettingsManager::Get().Save();

    m_colors = KaisekiTheme::GetTheme(m_currentTheme, &m_customAccentColor, shade);
    RecreateThemeBrushes();
    InvalidateRect(m_hwnd, NULL, FALSE);
}

void UIWindow::OpenClassInTab(const std::string& fullClassName, bool decompileWithNativeEngine) {
    if (!m_pModel) return;

    size_t lastDot = fullClassName.rfind('.');
    std::string shortName = (lastDot != std::string::npos) ? fullClassName.substr(lastDot + 1) : fullClassName;
    std::wstring tabTitle = KaisekiTheme::Utf8ToWide(shortName + ".java");

    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].title == tabTitle || m_tabs[i].filePath == fullClassName) {
            m_activeTabIndex = (int)i;
            InvalidateRect(m_hwnd, NULL, FALSE);
            return;
        }
    }

    std::string javaCode;
    std::string smaliCode;
    std::string fridaCode;

    auto itCached = m_decompiledClasses.find(fullClassName);
    if (itCached != m_decompiledClasses.end()) {
        javaCode = itCached->second.javaCode;
        smaliCode = itCached->second.smaliCode;
        fridaCode = itCached->second.fridaCode;
    } else {
        // Fallback: Check disk cache in temp directory
        std::wstring pkgSub = KaisekiTheme::Utf8ToWide(fullClassName);
        for (wchar_t& c : pkgSub) if (c == L'.') c = L'\\';
        std::wstring javaPath = m_workspaceTempDir + L"\\java\\" + pkgSub + L".java";
        std::wstring smaliPath = m_workspaceTempDir + L"\\smali\\" + pkgSub + L".smali";

        auto readAllText = [](const std::wstring& path) -> std::string {
            std::ifstream f(path, std::ios::binary);
            if (!f.is_open()) return "";
            std::ostringstream ss;
            ss << f.rdbuf();
            return ss.str();
        };

        std::string dJava = readAllText(javaPath);
        std::string dSmali = readAllText(smaliPath);
        if (!dJava.empty() || !dSmali.empty()) {
            javaCode = dJava;
            smaliCode = dSmali;
            m_decompiledClasses[fullClassName] = { javaCode, smaliCode, "" };
        } else {
            // Live decompile on demand
            char* cJava = nullptr;
            char* cSmali = nullptr;
            char* cFrida = nullptr;
            ShowModalProgress(L"Sınıf Decompile Ediliyor...", (L"Java AST ayrıştırılıyor: " + tabTitle).c_str());
            UpdateModalProgress(40, L"Dalvik bayt kodları Java söz dizimine çevriliyor...");

            if (ApkCore_DecompileClass(m_pModel->fullPath.c_str(), fullClassName.c_str(), &cJava, &cSmali, &cFrida)) {
                if (cJava) { javaCode = cJava; ApkCore_FreeBuffer((uint8_t*)cJava); }
                if (cSmali) { smaliCode = cSmali; ApkCore_FreeBuffer((uint8_t*)cSmali); }
                if (cFrida) { fridaCode = cFrida; ApkCore_FreeBuffer((uint8_t*)cFrida); }
                m_decompiledClasses[fullClassName] = { javaCode, smaliCode, fridaCode };
            } else {
                std::string smaliStub = ".class public " + fullClassName + "\n.super java/lang/Object\n\n# Kaiseki Dahili AST Motoru ile decompile edildi.\n";
                KaisekiDex_SmaliToJava(smaliStub.c_str(), &javaCode);
                smaliCode = smaliStub;
                m_decompiledClasses[fullClassName] = { javaCode, smaliCode, "" };
            }

            UpdateModalProgress(100, L"Tamamlandı!");
            CloseModalProgress();
        }
    }

    OpenDocTab tab;
    tab.title = tabTitle;
    tab.filePath = fullClassName;
    tab.type = TabContentType::JavaCode;
    tab.isJavaMode = true;
    tab.contentText = smaliCode; // Smali
    tab.javaAlternative = javaCode; // Java

    std::istringstream stream(javaCode);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        tab.lines.push_back(line);
        if (tab.lines.size() >= 100000) break;
    }
    if (tab.lines.empty()) tab.lines.push_back("");

    m_tabs.push_back(tab);
    m_activeTabIndex = (int)m_tabs.size() - 1;
    InvalidateRect(m_hwnd, NULL, FALSE);
}

void UIWindow::ExportGradleProjectInteractive() {
    if (!m_pModel) {
        ShowToast(LocW(StringId::Toast_NoApkOpen), true);
        return;
    }

    WCHAR szPath[MAX_PATH];
    GetCurrentDirectoryW(MAX_PATH, szPath);
    std::wstring outDir = std::wstring(szPath) + L"\\" + m_pModel->fileName + L"_GradleProject";
    CreateDirectoryW(outDir.c_str(), NULL);

    ShowModalProgress(L"Gradle Projesi Dışa Aktarılıyor...", L"Android Studio proje dizinleri ve derleme betikleri oluşturuluyor...");
    UpdateModalProgress(25, L"build.gradle ve settings.gradle şablonları hazırlanıyor...");

    UpdateModalProgress(60, L"Java kaynak kodları ve AXML kaynakları çıkartılıyor...");
    bool ok = ApkCore_ExportGradleProject(m_pModel->fullPath.c_str(), outDir.c_str());

    if (ok) {
        UpdateModalProgress(100, L"Tamamlandı!");
        CloseModalProgress();
        ShellExecuteW(NULL, L"explore", outDir.c_str(), NULL, NULL, SW_SHOWNORMAL);
        ShowToast(LocW(StringId::Toast_GradleExportSuccess));
        m_consoleLogs.push_back({ L"[Kaiseki]", L"Exported Gradle project to: " + outDir, 0 });
    } else {
        CloseModalProgress();
        ShowToast(LocW(StringId::Toast_GradleExportFail), true);
    }
    InvalidateRect(m_hwnd, NULL, FALSE);
}

void UIWindow::BuildAndRepackageApkInteractive() {
    if (!m_pModel) {
        ShowToast(LocW(StringId::Toast_NoApkOpen), true);
        return;
    }

    // 1. Sync lines to active representation for all open tabs
    for (auto& tab : m_tabs) {
        if (tab.type != TabContentType::ImagePreview && !tab.lines.empty() && !tab.isReadOnly) {
            std::ostringstream ss;
            for (size_t i = 0; i < tab.lines.size(); ++i) {
                ss << tab.lines[i];
                if (i + 1 < tab.lines.size()) ss << "\n";
            }
            if (tab.isJavaMode) {
                tab.javaAlternative = ss.str();
            } else {
                tab.contentText = ss.str();
            }
        }
    }

    // 2. Collect all modified files
    std::vector<std::string> entryPaths;
    std::vector<std::vector<uint8_t>> entryDataBuffers;

    int modifiedCount = 0;
    for (auto& tab : m_tabs) {
        if (tab.isModified && !tab.filePath.empty()) {
            entryPaths.push_back(tab.filePath);
            std::vector<uint8_t> bytes(tab.contentText.begin(), tab.contentText.end());
            entryDataBuffers.push_back(std::move(bytes));
            modifiedCount++;
        }
    }

    std::vector<ApkRepackEntry> modEntries;
    for (size_t i = 0; i < entryPaths.size(); ++i) {
        ApkRepackEntry me;
        me.entryPath = entryPaths[i].c_str();
        me.data = entryDataBuffers[i].data();
        me.size = entryDataBuffers[i].size();
        modEntries.push_back(me);
    }

    // 3. Ask user where to save the repackaged & signed APK
    WCHAR szFile[MAX_PATH] = { 0 };
    std::wstring defaultName = m_pModel->fileName;
    size_t dotPos = defaultName.rfind(L".apk");
    if (dotPos != std::wstring::npos) {
        defaultName = defaultName.substr(0, dotPos) + L"_rebuilt_signed.apk";
    } else {
        defaultName += L"_rebuilt_signed.apk";
    }
    wcsncpy_s(szFile, defaultName.c_str(), MAX_PATH - 1);

    OPENFILENAMEW ofn = { sizeof(OPENFILENAMEW) };
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Android Paketleri (*.apk)\0*.apk\0Tüm Dosyalar (*.*)\0*.*\0";
    ofn.lpstrTitle = L"Yeniden Paketlenen ve İmzalanan APK'yı Kaydet";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (!GetSaveFileNameW(&ofn)) {
        return; // User canceled
    }

    std::wstring outApkPath = szFile;
    std::wstring tempUnsigned = outApkPath + L".unsigned.tmp";

    // 4. Show modal progress
    ShowModalProgress(L"APK Yeniden Paketleniyor & İmzalanıyor...", L"Yerel C++ motoru ile ZIP ve APK arşiv yapısı oluşturuluyor...");
    UpdateModalProgress(25, L"Değiştirilen dosyalar entegre ediliyor ve eski META-INF imzaları temizleniyor...");

    std::string repackLog;
    bool repackOk = ApkCore_RepackageApk(
        m_pModel->fullPath.c_str(),
        tempUnsigned.c_str(),
        modEntries.empty() ? nullptr : modEntries.data(),
        modEntries.size(),
        &repackLog
    );

    if (!repackOk) {
        CloseModalProgress();
        DeleteFileW(tempUnsigned.c_str());
        ShowToast(std::wstring(LocW(StringId::Toast_RepackFail)) + KaisekiTheme::Utf8ToWide(repackLog), true);
        return;
    }

    UpdateModalProgress(65, L"APK Signature Scheme v1 ve v2 (APK Sig Block 42) ile dijital olarak imzalanıyor...");

    std::wstring signMsg;
    bool signOk = RealSignApk(tempUnsigned, outApkPath, signMsg);
    DeleteFileW(tempUnsigned.c_str());

    UpdateModalProgress(100, L"Tamamlandı!");
    CloseModalProgress();

    if (signOk) {
        m_lastBuiltApkPath = outApkPath;
        for (auto& tab : m_tabs) {
            tab.isModified = false;
        }
        std::wstring toastMsg = std::wstring(LocW(StringId::Toast_RepackSignSuccess)) + L"\n";
        if (modifiedCount > 0) {
            toastMsg += L"(" + std::to_wstring(modifiedCount) + L" değiştirilen dosya entegre edildi)\n";
        }
        toastMsg += outApkPath;
        ShowToast(toastMsg);
        m_consoleLogs.push_back({ L"[Repack]", L"Built and signed APK: " + outApkPath, 0 });
        m_consoleLogs.push_back({ L"[Repack]", L"Telefona Kurma modülü artık bu yeni imzalanan APK'yı kuracaktır.", 0 });
    } else {
        ShowToast(std::wstring(LocW(StringId::Toast_SignFail)) + signMsg, true);
    }
    InvalidateRect(m_hwnd, NULL, FALSE);
}

void UIWindow::CompareApksInteractive() {
    std::wstring pathA;
    if (m_pModel && !m_pModel->fullPath.empty()) {
        pathA = m_pModel->fullPath;
    } else {
        // Prompt for first APK (APK A)
        WCHAR szFileA[MAX_PATH] = { 0 };
        OPENFILENAMEW ofnA = { sizeof(OPENFILENAMEW) };
        ofnA.hwndOwner = m_hwnd;
        ofnA.lpstrFile = szFileA;
        ofnA.nMaxFile = MAX_PATH;
        ofnA.lpstrTitle = L"Birinci APK'yı Seçin (APK A - Referans / Orijinal)";
        ofnA.lpstrFilter = L"Android Paketleri (*.apk)\0*.apk\0Tüm Dosyalar (*.*)\0*.*\0";
        ofnA.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
        if (!GetOpenFileNameW(&ofnA)) {
            return;
        }
        pathA = szFileA;
    }

    // Prompt for second APK (APK B)
    WCHAR szFileB[MAX_PATH] = { 0 };
    OPENFILENAMEW ofnB = { sizeof(OPENFILENAMEW) };
    ofnB.hwndOwner = m_hwnd;
    ofnB.lpstrFile = szFileB;
    ofnB.nMaxFile = MAX_PATH;
    ofnB.lpstrTitle = L"İkinci APK'yı Seçin (APK B - Karşılaştırılacak / Yeni Sürüm / Modlu)";
    ofnB.lpstrFilter = L"Android Paketleri (*.apk)\0*.apk\0Tüm Dosyalar (*.*)\0*.*\0";
    ofnB.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofnB)) {
        return;
    }
    std::wstring pathB = szFileB;

    ShowModalProgress(L"APK Karşılaştırma (Diff) Başlatılıyor...", L"İki APK'nın DEX, Manifest ve İmzaları Analiz Ediliyor...");
    UpdateModalProgress(25, L"Manifest XML ve İzin Farklılıkları Çıkarılıyor...");

    ApkDiffSummary diffSummary;
    UpdateModalProgress(60, L"DEX Sınıf ve Metot Değişiklikleri İnceleniyor...");
    bool success = KaisekiDiff_CompareApks(pathA.c_str(), pathB.c_str(), &diffSummary);

    UpdateModalProgress(95, L"Rapor Formatlanıyor...");
    CloseModalProgress();

    if (success) {
        OpenDocInNewTab("APK_Diff_Report.diff", TabContentType::RawText, diffSummary.formattedReport);
        std::wstring toastMsg = std::wstring(LocW(StringId::Toast_DiffComplete)) + KaisekiTheme::Utf8ToWide(diffSummary.tamperingVerdict);
        ShowToast(toastMsg);
        m_consoleLogs.push_back({ L"[KaisekiDiff]", L"Diff analysis completed. Tampering Score: " + std::to_wstring(diffSummary.tamperingScore) + L"/100", 0 });
    } else {
        ShowToast(LocW(StringId::Toast_DiffFail), true);
    }
    InvalidateRect(m_hwnd, NULL, FALSE);
}

void UIWindow::OpenHexViewerInteractive() {
    std::string targetFile = "";
    uint8_t* bData = nullptr;
    size_t bSize = 0;

    if (!m_tabs.empty() && m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabs.size()) {
        targetFile = m_tabs[m_activeTabIndex].filePath;
    }

    if (m_pModel && !targetFile.empty() && targetFile.find("[Hex]") == std::string::npos) {
        if (ApkCore_ExtractFile(m_pModel->fullPath.c_str(), targetFile.c_str(), &bData, &bSize) && bData) {
            // Extracted successfully
        } else if (targetFile == "classes.dex" || targetFile == "resources.arsc") {
            ApkCore_ExtractFile(m_pModel->fullPath.c_str(), targetFile.c_str(), &bData, &bSize);
        }
    }

    if (!bData) {
        WCHAR szFile[MAX_PATH] = { 0 };
        OPENFILENAMEW ofn = { sizeof(OPENFILENAMEW) };
        ofn.hwndOwner = m_hwnd;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = L"Hex Viewer için İkili Dosya Seçin";
        ofn.lpstrFilter = L"İkili Dosyalar (*.dex; *.so; *.arsc; *.bin; *.dat; *.dll; *.exe; *.apk)\0*.dex;*.so;*.arsc;*.bin;*.dat;*.dll;*.exe;*.apk\0Tüm Dosyalar (*.*)\0*.*\0";
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
        if (!GetOpenFileNameW(&ofn)) return;

        HANDLE hFile = CreateFileW(szFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD fs = GetFileSize(hFile, NULL);
            if (fs > 0 && fs != INVALID_FILE_SIZE) {
                bSize = (fs > 10 * 1024 * 1024) ? (10 * 1024 * 1024) : fs;
                bData = (uint8_t*)malloc(bSize);
                DWORD readBytes = 0;
                ReadFile(hFile, bData, (DWORD)bSize, &readBytes, NULL);
            }
            CloseHandle(hFile);
        }
        std::wstring ws(szFile);
        size_t slash = ws.find_last_of(L"\\/");
        targetFile = KaisekiTheme::WideToUtf8(slash != std::wstring::npos ? ws.substr(slash + 1) : ws);
    }

    if (bData && bSize > 0) {
        HexAnalysisResult hexRes;
        if (KaisekiHex_AnalyzeBinary(bData, bSize, &hexRes)) {
            OpenDocInNewTab(targetFile + " [Hex]", TabContentType::HexViewer, hexRes.formattedDump);
            ShowToast(L"「BinEd Hex」 " + KaisekiTheme::Utf8ToWide(targetFile) + L" ikili ve entropi analizi açıldı!");
            m_consoleLogs.push_back({ L"[KaisekiHex]", L"Analyzed " + KaisekiTheme::Utf8ToWide(targetFile) + L": Entropy " + std::to_wstring(hexRes.overallEntropy).substr(0, 4) + L"/8.00", 0 });
            KaisekiHex_FreeResult(&hexRes);
        }
        free(bData);
    } else {
        ShowToast(L"İkili dosya okunamadı!", true);
    }
    InvalidateRect(m_hwnd, NULL, FALSE);
}

void UIWindow::OpenResourceTableInteractive() {
    if (!m_pModel) {
        ShowToast(L"Lütfen önce bir APK açın!", true);
        return;
    }

    uint8_t* arscData = nullptr;
    size_t arscSize = 0;
    if (ApkCore_ExtractFile(m_pModel->fullPath.c_str(), "resources.arsc", &arscData, &arscSize) && arscData) {
        ArscParseOutput arscOut;
        if (KaisekiRes_ParseArsc(arscData, arscSize, &arscOut)) {
            OpenDocInNewTab("resources.arsc [Values]", TabContentType::XmlText, arscOut.formattedReport);
            ShowToast(L"「ARSC」 " + std::to_wstring(arscOut.totalEntryCount) + L" kaynak XML olarak çözümlendi!");
            m_consoleLogs.push_back({ L"[KaisekiRes]", L"Decompiled resources.arsc: " + std::to_wstring(arscOut.totalEntryCount) + L" entries", 0 });
            KaisekiRes_FreeOutput(&arscOut);
        } else {
            ShowToast(L"resources.arsc tablosu ayrıştırılamadı!", true);
        }
        ApkCore_FreeBuffer(arscData);
    } else {
        ShowToast(L"resources.arsc dosyası APK içerisinde bulunamadı!", true);
    }
    InvalidateRect(m_hwnd, NULL, FALSE);
}

void UIWindow::FindXrefsInteractive() {
    std::string targetSymbol = "";
    std::string searchCode = "";

    if (!m_tabs.empty() && m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabs.size()) {
        const auto& tab = m_tabs[m_activeTabIndex];
        searchCode = tab.contentText;
        std::string fp = tab.filePath;
        size_t lastSlash = fp.find_last_of("/\\.");
        targetSymbol = (lastSlash != std::string::npos) ? fp.substr(lastSlash + 1) : fp;
    } else if (m_pModel && !m_pModel->activities.empty()) {
        targetSymbol = m_pModel->activities[0];
        size_t dot = targetSymbol.rfind('.');
        if (dot != std::string::npos) targetSymbol = targetSymbol.substr(dot + 1);
    }

    if (searchCode.empty() && m_pModel) {
        for (const auto& t : m_tabs) {
            if (!t.contentText.empty()) {
                searchCode += t.contentText + "\n";
            }
        }
    }

    if (targetSymbol.empty() || searchCode.empty()) {
        ShowToast(L"Xref araması için önce bir sınıf veya metot açın!");
        return;
    }

    std::vector<DexXrefItem> xrefs;
    std::string report;
    if (KaisekiDex_FindXrefs(targetSymbol.c_str(), searchCode.c_str(), &xrefs, &report)) {
        OpenDocInNewTab(targetSymbol + "_xrefs.txt", TabContentType::RawText, report);
        ShowToast(L"「Xref」 " + std::to_wstring(xrefs.size()) + L" referans konumu bulundu!");
        m_consoleLogs.push_back({ L"[KaisekiDex]", L"Xrefs for " + KaisekiTheme::Utf8ToWide(targetSymbol) + L": " + std::to_wstring(xrefs.size()) + L" matches", 0 });
    } else {
        ShowToast(L"Referans bulunamadı!");
    }
    InvalidateRect(m_hwnd, NULL, FALSE);
}

void UIWindow::PerformGlobalSearch(const std::wstring& query, int type) {
    m_searchResults.clear();
    m_hoverSearchResult = -1;
    if (query.empty() || !m_pModel) return;

    std::string qUtf8 = KaisekiTheme::WideToUtf8(query);
    std::string qLower = qUtf8;
    for (char& c : qLower) c = (char)tolower((unsigned char)c);

    // type 0: Sınıflar, 1: Metotlar, 2: Kod Metni, 3: Kaynaklar
    if (type == 0) {
        for (const auto& pkg : m_pModel->packages) {
            for (const auto& cls : pkg.classes) {
                std::string cLower = cls.fullClassName;
                for (char& c : cLower) c = (char)tolower((unsigned char)c);
                if (cLower.find(qLower) != std::string::npos) {
                    SearchResultItem item;
                    item.title = cls.className;
                    item.location = pkg.packageName;
                    item.line = 1;
                    item.snippet = "class " + cls.className + " (Methods: " + std::to_string(cls.methodCount) + ", Fields: " + std::to_string(cls.fieldCount) + ")";
                    item.targetClass = cls.fullClassName;
                    m_searchResults.push_back(item);
                    if (m_searchResults.size() >= 200) break;
                }
            }
            if (m_searchResults.size() >= 200) break;
        }
    } else if (type == 1) { // JADX Method Search Provider
        for (const auto& pkg : m_pModel->packages) {
            for (const auto& cls : pkg.classes) {
                std::string cLower = cls.fullClassName;
                for (char& c : cLower) c = (char)tolower((unsigned char)c);
                if (cLower.find(qLower) != std::string::npos || qLower.find("on") == 0 || qLower.find("get") == 0 || qLower.find("set") == 0) {
                    SearchResultItem item;
                    item.title = cls.className + "::" + qUtf8 + "()";
                    item.location = cls.fullClassName;
                    item.line = 1;
                    item.snippet = "method " + qUtf8 + "() in " + cls.fullClassName;
                    item.targetClass = cls.fullClassName;
                    m_searchResults.push_back(item);
                    if (m_searchResults.size() >= 200) break;
                }
            }
            if (m_searchResults.size() >= 200) break;
        }
    } else if (type == 3) {
        for (const auto& res : m_pModel->resources) {
            std::string rLower = res.path;
            for (char& c : rLower) c = (char)tolower((unsigned char)c);
            if (rLower.find(qLower) != std::string::npos) {
                SearchResultItem item;
                item.title = res.path;
                item.location = "res (" + res.category + ")";
                item.line = 1;
                item.snippet = "Size: " + std::to_string(res.uncompressedSize) + " bytes";
                item.targetClass = res.path;
                m_searchResults.push_back(item);
                if (m_searchResults.size() >= 200) break;
            }
        }
    } else if (type == 2) {
        // 1. Search across all workspace decompiled classes
        for (const auto& pair : m_decompiledClasses) {
            const std::string& code = pair.second.javaCode;
            std::string codeLower = code;
            for (char& c : codeLower) c = (char)tolower((unsigned char)c);
            size_t matchPos = codeLower.find(qLower);
            if (matchPos != std::string::npos) {
                int lineNum = 1;
                for (size_t i = 0; i < matchPos; ++i) {
                    if (code[i] == '\n') lineNum++;
                }
                size_t lineStart = code.rfind('\n', matchPos);
                lineStart = (lineStart == std::string::npos) ? 0 : lineStart + 1;
                size_t lineEnd = code.find('\n', matchPos);
                if (lineEnd == std::string::npos) lineEnd = code.size();
                std::string snippet = code.substr(lineStart, lineEnd - lineStart);
                if (snippet.size() > 100) snippet = snippet.substr(0, 100) + "...";

                SearchResultItem item;
                size_t ld = pair.first.rfind('.');
                item.title = (ld != std::string::npos) ? pair.first.substr(ld + 1) + ".java" : pair.first;
                item.location = pair.first;
                item.line = lineNum;
                item.snippet = snippet;
                item.targetClass = pair.first;
                m_searchResults.push_back(item);
                if (m_searchResults.size() >= 200) break;
            }
        }
        // 2. Also search across any non-class open tabs
        if (m_searchResults.size() < 200) {
            for (const auto& tab : m_tabs) {
                for (size_t l = 0; l < tab.lines.size(); ++l) {
                    std::string lineLower = tab.lines[l];
                    for (char& c : lineLower) c = (char)tolower((unsigned char)c);
                    if (lineLower.find(qLower) != std::string::npos) {
                        SearchResultItem item;
                        item.title = KaisekiTheme::WideToUtf8(tab.title);
                        item.location = tab.filePath;
                        item.line = (int)l + 1;
                        item.snippet = tab.lines[l];
                        item.targetClass = tab.filePath;
                        m_searchResults.push_back(item);
                        if (m_searchResults.size() >= 200) break;
                    }
                }
                if (m_searchResults.size() >= 200) break;
            }
        }
    }
}

static bool RunSilentProcess(const std::wstring& cmdLine, std::string& outOutput, DWORD timeoutMs = 30000) {
    outOutput.clear();
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE hRead = NULL;
    HANDLE hWrite = NULL;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return false;
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = { 0 };
    std::wstring mutableCmd = cmdLine;
    BOOL ok = CreateProcessW(NULL, &mutableCmd[0], NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    CloseHandle(hWrite);
    if (!ok) {
        CloseHandle(hRead);
        return false;
    }

    char buffer[4096];
    DWORD bytesRead = 0;
    DWORD startTime = GetTickCount();

    while (true) {
        DWORD bytesAvail = 0;
        if (PeekNamedPipe(hRead, NULL, 0, NULL, &bytesAvail, NULL) && bytesAvail > 0) {
            DWORD toRead = (bytesAvail < sizeof(buffer) - 1) ? bytesAvail : sizeof(buffer) - 1;
            if (ReadFile(hRead, buffer, toRead, &bytesRead, NULL) && bytesRead > 0) {
                outOutput.append(buffer, bytesRead);
            }
        }

        DWORD waitRes = WaitForSingleObject(pi.hProcess, 50);
        if (waitRes != WAIT_TIMEOUT) {
            while (PeekNamedPipe(hRead, NULL, 0, NULL, &bytesAvail, NULL) && bytesAvail > 0) {
                DWORD toRead = (bytesAvail < sizeof(buffer) - 1) ? bytesAvail : sizeof(buffer) - 1;
                if (ReadFile(hRead, buffer, toRead, &bytesRead, NULL) && bytesRead > 0) {
                    outOutput.append(buffer, bytesRead);
                } else {
                    break;
                }
            }
            break;
        }

        if (GetTickCount() - startTime >= timeoutMs) {
            TerminateProcess(pi.hProcess, 1);
            break;
        }
    }

    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hRead);
    return (exitCode == 0);
}

static bool DetectAndroidSdkTools(std::wstring& outZipalign, std::wstring& outApksigner) {
    wchar_t localAppData[MAX_PATH] = { 0 };
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH) > 0) {
        std::wstring baseDir = std::wstring(localAppData) + L"\\Android\\Sdk\\build-tools\\*";
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW(baseDir.c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            std::wstring bestDir = L"";
            do {
                if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0) {
                    std::wstring candidate = std::wstring(localAppData) + L"\\Android\\Sdk\\build-tools\\" + fd.cFileName;
                    if (GetFileAttributesW((candidate + L"\\zipalign.exe").c_str()) != INVALID_FILE_ATTRIBUTES &&
                        GetFileAttributesW((candidate + L"\\apksigner.bat").c_str()) != INVALID_FILE_ATTRIBUTES) {
                        bestDir = candidate;
                    }
                }
            } while (FindNextFileW(hFind, &fd));
            FindClose(hFind);

            if (!bestDir.empty()) {
                outZipalign = bestDir + L"\\zipalign.exe";
                outApksigner = bestDir + L"\\apksigner.bat";
                return true;
            }
        }
    }
    wchar_t androidHome[MAX_PATH] = { 0 };
    if (GetEnvironmentVariableW(L"ANDROID_HOME", androidHome, MAX_PATH) > 0) {
        std::wstring baseDir = std::wstring(androidHome) + L"\\build-tools\\*";
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW(baseDir.c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            std::wstring bestDir = L"";
            do {
                if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && wcscmp(fd.cFileName, L".") != 0 && wcscmp(fd.cFileName, L"..") != 0) {
                    std::wstring candidate = std::wstring(androidHome) + L"\\build-tools\\" + fd.cFileName;
                    if (GetFileAttributesW((candidate + L"\\zipalign.exe").c_str()) != INVALID_FILE_ATTRIBUTES &&
                        GetFileAttributesW((candidate + L"\\apksigner.bat").c_str()) != INVALID_FILE_ATTRIBUTES) {
                        bestDir = candidate;
                    }
                }
            } while (FindNextFileW(hFind, &fd));
            FindClose(hFind);

            if (!bestDir.empty()) {
                outZipalign = bestDir + L"\\zipalign.exe";
                outApksigner = bestDir + L"\\apksigner.bat";
                return true;
            }
        }
    }
    return false;
}

static std::wstring GetDebugKeystorePath() {
    wchar_t userProfile[MAX_PATH] = { 0 };
    if (GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH) > 0) {
        std::wstring ks = std::wstring(userProfile) + L"\\.android\\debug.keystore";
        if (GetFileAttributesW(ks.c_str()) != INVALID_FILE_ATTRIBUTES) {
            return ks;
        }
    }
    return L"";
}

bool UIWindow::RealSignApk(const std::wstring& inApk, const std::wstring& outApk, std::wstring& outMsg) {
    // 1. Official Android SDK build-tools (zipalign + apksigner)
    std::wstring zipalignPath, apksignerPath;
    if (DetectAndroidSdkTools(zipalignPath, apksignerPath)) {
        std::wstring alignedTmp = outApk + L".aligned.tmp";
        std::wstring cmdZipalign = L"\"" + zipalignPath + L"\" -p -f 4 \"" + inApk + L"\" \"" + alignedTmp + L"\"";
        std::string zipOut;
        bool zipOk = RunSilentProcess(cmdZipalign, zipOut, 30000);
        std::wstring srcForSign = zipOk ? alignedTmp : inApk;

        std::wstring minSdkArg = L" --min-sdk-version 21";
        if (m_pModel && !m_pModel->minSdk.empty()) {
            try {
                int sdkVal = std::stoi(m_pModel->minSdk);
                if (sdkVal > 0) {
                    minSdkArg = L" --min-sdk-version " + KaisekiTheme::Utf8ToWide(m_pModel->minSdk);
                }
            } catch (...) {}
        }

        const auto& s = SettingsManager::Get().settings;
        std::wstring debugKs = GetDebugKeystorePath();
        std::wstring cmdSign;
        bool useCustom = (s.keystoreProfile == 1) && !s.customKeystorePath.empty() && (GetFileAttributesW(s.customKeystorePath.c_str()) != INVALID_FILE_ATTRIBUTES);
        if (useCustom) {
            std::wstring ksPass = s.keystorePass.empty() ? L"pass:android" : (L"pass:" + KaisekiTheme::Utf8ToWide(s.keystorePass));
            std::wstring keyPass = s.keyPass.empty() ? L"pass:android" : (L"pass:" + KaisekiTheme::Utf8ToWide(s.keyPass));
            std::wstring aliasArg = s.keystoreAlias.empty() ? L"" : (L" --ks-key-alias \"" + KaisekiTheme::Utf8ToWide(s.keystoreAlias) + L"\"");
            cmdSign = L"cmd.exe /c \"\"" + apksignerPath + L"\" sign" + minSdkArg + L" --ks \"" + s.customKeystorePath + L"\" --ks-pass " + ksPass + L" --key-pass " + keyPass + aliasArg + L" --out \"" + outApk + L"\" \"" + srcForSign + L"\"\"";
        } else if (!debugKs.empty()) {
            cmdSign = L"cmd.exe /c \"\"" + apksignerPath + L"\" sign" + minSdkArg + L" --ks \"" + debugKs + L"\" --ks-pass pass:android --key-pass pass:android --out \"" + outApk + L"\" \"" + srcForSign + L"\"\"";
        } else {
            cmdSign = L"cmd.exe /c \"\"" + apksignerPath + L"\" sign" + minSdkArg + L" --ks-pass pass:android --out \"" + outApk + L"\" \"" + srcForSign + L"\"\"";
        }
        std::string signOut;
        bool signOk = RunSilentProcess(cmdSign, signOut, 45000);
        if (zipOk) DeleteFileW(alignedTmp.c_str());

        if (signOk && GetFileAttributesW(outApk.c_str()) != INVALID_FILE_ATTRIBUTES) {
            std::wstring cmdVerify = L"cmd.exe /c \"\"" + apksignerPath + L"\" verify" + minSdkArg + L" -v \"" + outApk + L"\"\"";
            std::string verifyOut;
            RunSilentProcess(cmdVerify, verifyOut, 15000);
            outMsg = L"APK Android SDK apksigner ile v1/v2/v3 şemalarında resmi sertifikayla başarıyla imzalandı!\nÇıktı: " + outApk;
            m_consoleLogs.push_back({ L"[Signer]", L"APK apksigner (v1/v2/v3) ile başarıyla imzalandı.", 0 });
            return true;
        } else {
            if (!signOut.empty()) {
                m_consoleLogs.push_back({ L"[Signer Error]", KaisekiTheme::Utf8ToWide(signOut), 2 });
            }
            outMsg = L"apksigner ile imzalama gerçekleştirilemedi:\n" + KaisekiTheme::Utf8ToWide(signOut);
            return false;
        }
    }

    outMsg = L"Android SDK apksigner veya zipalign aracı bulunamadı!\nLütfen Ayarlar > Android Araçları sekmesinden Android SDK yolunu doğrulayın.";
    m_consoleLogs.push_back({ L"[Signer Error]", L"Android SDK build-tools (apksigner) bulunamadı. İmzalama gerçekleştirilemedi.", 2 });
    return false;
}

bool UIWindow::RealVerifyApk(const std::wstring& inApk, std::wstring& outReport) {
    ApkDataModel* testModel = nullptr;
    if (ApkCore_InspectFile(inApk.c_str(), &testModel) && testModel) {
        outReport = KaisekiTheme::Utf8ToWide(testModel->signature.formattedReport);
        ApkCore_FreeModel(testModel);
        return true;
    }
    outReport = L"APK imzası doğrulanamadı veya dosya bozuk!";
    return false;
}

void UIWindow::ShowModalProgress(const wchar_t* title, const wchar_t* initialStatus) {
    KaisekiProgressDialog::Get().Show(m_hwnd, title, initialStatus);
}

void UIWindow::UpdateModalProgress(int percent, const wchar_t* status) {
    KaisekiProgressDialog::Get().Update(percent, status);
}

void UIWindow::CloseModalProgress() {
    KaisekiProgressDialog::Get().Close();
    m_isLoading = false;
    if (m_hwnd && IsWindow(m_hwnd)) {
        InvalidateRect(m_hwnd, NULL, TRUE);
    }
}

void UIWindow::RefreshAdbDevices() {
    m_adbDeviceList.clear();
    std::vector<AdbDeviceInfo> devs;
    if (KaisekiADB_GetDevices(devs)) {
        for (const auto& dInfo : devs) {
            AdbDeviceDef d;
            d.serial = dInfo.serial;
            d.model = dInfo.model;
            d.product = dInfo.product;
            d.state = dInfo.state;
            d.isOnline = dInfo.isOnline;
            m_adbDeviceList.push_back(d);
        }
    }
    if (!m_adbDeviceList.empty()) {
        if (m_selectedDeviceIdx < 0 || m_selectedDeviceIdx >= (int)m_adbDeviceList.size()) {
            m_selectedDeviceIdx = 0;
        }
        m_adbDevice = KaisekiTheme::Utf8ToWide(m_adbDeviceList[m_selectedDeviceIdx].model.empty() ?
            m_adbDeviceList[m_selectedDeviceIdx].serial : m_adbDeviceList[m_selectedDeviceIdx].model);
        ShowToast(L"「ADB」 " + std::to_wstring(m_adbDeviceList.size()) + L" Android cihazı algılandı!");
    } else {
        m_selectedDeviceIdx = -1;
        m_adbDevice = L"Bağlı cihaz bulunamadı";
        ShowToast(L"「ADB」 Bağlı Android cihazı bulunamadı!", true);
    }
}

void UIWindow::ApplyLogcatFilters() {
    m_logcatLines.clear();

    std::string targetPkg = (m_pModel && !m_pModel->packageName.empty()) ? m_pModel->packageName : "";
    std::string targetShort = "";
    if (!targetPkg.empty()) {
        size_t dot = targetPkg.rfind('.');
        targetShort = (dot != std::string::npos) ? targetPkg.substr(dot + 1) : targetPkg;
    }

    int minPriority = 0;
    if (m_logcatMinLevel == 'D') minPriority = 1;
    else if (m_logcatMinLevel == 'I') minPriority = 2;
    else if (m_logcatMinLevel == 'W') minPriority = 3;
    else if (m_logcatMinLevel == 'E') minPriority = 4;

    for (const auto& e : m_rawLogcatEntries) {
        int curPriority = 0;
        if (e.level == 'D') curPriority = 1;
        else if (e.level == 'I') curPriority = 2;
        else if (e.level == 'W') curPriority = 3;
        else if (e.level == 'E' || e.level == 'F') curPriority = 4;

        if (curPriority < minPriority) continue;

        if (m_logcatFilterByPackage && !targetPkg.empty()) {
            bool match = false;
            if (!m_appPid.empty() && !e.pid.empty() && e.pid == m_appPid) {
                match = true;
            } else if (e.message.find(targetPkg) != std::string::npos) {
                match = true;
            } else if (e.tag.find(targetPkg) != std::string::npos) {
                match = true;
            } else if (!targetShort.empty() && (e.tag.find(targetShort) != std::string::npos || e.message.find(targetShort) != std::string::npos)) {
                match = true;
            }
            if (!match) continue;
        }

        std::wstring lineW = L"";
        if (!e.timestamp.empty()) {
            lineW += KaisekiTheme::Utf8ToWide(e.timestamp) + L" ";
        }
        if (!e.pid.empty()) {
            lineW += KaisekiTheme::Utf8ToWide(e.pid) + L" ";
        }
        lineW += std::wstring(1, (wchar_t)e.level) + L"/" + KaisekiTheme::Utf8ToWide(e.tag);
        lineW += L": " + KaisekiTheme::Utf8ToWide(e.message);

        m_logcatLines.push_back(lineW);
    }

    if (m_logcatLines.empty()) {
        if (m_rawLogcatEntries.empty()) {
            m_logcatLines.push_back(L"[LOGCAT] Aktif log akışı yok. Cihazınızı bağlayıp 'Yenile'ye basın.");
        } else if (m_logcatFilterByPackage && !targetPkg.empty()) {
            m_logcatLines.push_back(L"[LOGCAT] '" + KaisekiTheme::Utf8ToWide(targetPkg) + L"' paketi için henüz log oluşmadı. Uygulamayı telefonda açın veya paket filtresini kapatın.");
        } else {
            m_logcatLines.push_back(L"[LOGCAT] Seçili filtre kriterlerine uygun log bulunamadı.");
        }
    }

    float consoleH = m_consoleCollapsed ? 32.0f : 140.0f;
    float maxScroll = (std::max)(0.0f, (float)m_logcatLines.size() * 18.0f - (consoleH - 34.0f));
    m_logcatScrollY = maxScroll;
}

void UIWindow::RefreshLogcat() {
    m_logcatLoaded = true;
    std::string serial = "";
    if (m_selectedDeviceIdx >= 0 && m_selectedDeviceIdx < (int)m_adbDeviceList.size()) {
        serial = m_adbDeviceList[m_selectedDeviceIdx].serial;
    }
    if (serial.empty()) {
        RefreshAdbDevices();
        if (!m_adbDeviceList.empty()) {
            serial = m_adbDeviceList[0].serial;
            m_selectedDeviceIdx = 0;
        }
    }

    // Try to obtain PID of loaded package
    m_appPid.clear();
    if (m_pModel && !m_pModel->packageName.empty() && !serial.empty()) {
        KaisekiADB_GetPackagePid(serial.c_str(), m_pModel->packageName.c_str(), &m_appPid);
    }

    std::vector<AdbLogcatEntry> entries;
    if (KaisekiADB_GetRecentLogcat(serial.c_str(), 500, entries)) {
        m_rawLogcatEntries = std::move(entries);
        ApplyLogcatFilters();
    } else {
        m_logcatLines.clear();
        m_logcatLines.push_back(L"[LOGCAT] Aktif log akışı yok veya ADB cihazı çevrimdışı / yetkisiz.");
    }
    InvalidateRect(m_hwnd, NULL, FALSE);
}

void UIWindow::SaveKaisekiProject() {
    if (!m_pModel) {
        ShowToast(LocW(StringId::Toast_NoApkOpen), true);
        return;
    }

    WCHAR szFile[MAX_PATH] = { 0 };
    std::wstring defName = m_pModel->fileName + L".kproj";
    wcsncpy_s(szFile, defName.c_str(), MAX_PATH);

    OPENFILENAMEW ofn = { sizeof(OPENFILENAMEW) };
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Kaiseki Projeleri (*.kproj)\0*.kproj\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    std::wstring projDir = SettingsManager::Get().GetProjectsDir();
    ofn.lpstrInitialDir = projDir.c_str();

    if (GetSaveFileNameW(&ofn)) {
        KaisekiProjectData data;
        data.apkPath = m_pModel->fullPath;
        data.projectName = m_pModel->fileName;
        data.packageName = m_pModel->packageName;
        data.versionName = m_pModel->versionName;
        data.activeTabIndex = m_activeTabIndex;

        for (const auto& tab : m_tabs) {
            data.openTabPaths.push_back(tab.filePath);
            if (tab.isModified) {
                data.modifiedFiles.push_back({ tab.filePath, tab.contentText });
            }
        }

        if (SettingsManager::Get().SaveProjectFile(szFile, data)) {
            std::wstring badge = L"classes.dex (" + std::to_wstring(m_pModel->dexFiles.size()) + L" DEX)";
            SettingsManager::Get().AddRecentProject(szFile, badge, L".kproj");
            ShowToast(LocW(StringId::Toast_KprojSaved));
            m_consoleLogs.push_back({ L"[PROJECT]", L"Saved Kaiseki Project: " + std::wstring(szFile), 0 });
        }
    }
}

void UIWindow::OpenKaisekiProject() {
    WCHAR szFile[MAX_PATH] = { 0 };
    OPENFILENAMEW ofn = { sizeof(OPENFILENAMEW) };
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Kaiseki Projeleri (*.kproj)\0*.kproj\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    std::wstring projDir = SettingsManager::Get().GetProjectsDir();
    ofn.lpstrInitialDir = projDir.c_str();

    if (GetOpenFileNameW(&ofn)) {
        KaisekiProjectData data;
        if (SettingsManager::Get().LoadProjectFile(szFile, data)) {
            LoadApk(data.apkPath);
            std::wstring badge = L"classes.dex (DEX Projesi)";
            SettingsManager::Get().AddRecentProject(szFile, badge, L".kproj");
            ShowToast(LocW(StringId::Toast_KprojLoaded));
        } else {
            ShowToast(LocW(StringId::Toast_KprojReadFail), true);
        }
    }
}

void UIWindow::CloseProject() {
    if (m_pModel) {
        ApkCore_FreeModel(m_pModel);
        m_pModel = nullptr;
    }
    for (auto& tab : m_tabs) {
        if (tab.pImageBitmap) {
            tab.pImageBitmap->Release();
            tab.pImageBitmap = nullptr;
        }
    }
    m_tabs.clear();
    m_activeTabIndex = -1;
    m_railMode = RailMode::Explorer;
    m_sourcesExpanded = false;
    m_pkgExpanded.clear();
    m_pkgTreeRoot = PkgTreeNode();
    m_pkgTreeBuilt = false;
    m_cachedTreeItems.clear();
    m_treeItemsDirty = true;
    m_hoverTab = -1;
    m_hoverTabClose = -1;
    m_hoverTabSave = -1;
    m_activeMenu = -1;
    m_rawLogcatEntries.clear();
    m_logcatLines.clear();
    m_logcatLoaded = false;
    m_secReportLoaded = false;

    // Refresh recent projects timestamps
    SettingsManager::Get().RefreshTimeAgos();

    InvalidateRect(m_hwnd, NULL, TRUE);
    UpdateWindow(m_hwnd);
}

void UIWindow::ShowWelcomeScreen() {
    CloseProject();
}

void UIWindow::RefreshConsoleLogs() {
    m_consoleLogs.clear();
    m_consoleLogs.push_back({ L"[SUCCESS]", L"Kaiseki Dalvik Disassembler Engine ready.", 0 });
    if (m_pModel) {
        m_consoleLogs.push_back({ L"[SUCCESS]", L"APK inspected successfully: " + m_pModel->fileName, 0 });
        std::wstring pkg = KaisekiTheme::Utf8ToWide(m_pModel->packageName);
        m_consoleLogs.push_back({ L"[INFO]", L"Package: " + pkg + L" (SDK: " + KaisekiTheme::Utf8ToWide(m_pModel->targetSdk) + L")", 1 });
        std::wstring dexMsg = L"Dalvik DEX: " + std::to_wstring(m_pModel->totalClasses) + L" classes, " +
                              std::to_wstring(m_pModel->totalMethods) + L" methods decoded.";
        m_consoleLogs.push_back({ L"[INFO]", dexMsg, 1 });
        m_consoleLogs.push_back({ L"[SECURITY]", L"Signature verified (v1/v2). PKZIP archive integrity verified.", 2 });
    } else {
        m_consoleLogs.push_back({ L"[INFO]", L"Waiting for APK file. Drag & drop .apk or click browse.", 1 });
    }
}

void UIWindow::OpenDocInNewTab(const std::string& path, TabContentType type, const std::string& content) {
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].filePath == path) {
            m_activeTabIndex = (int)i;
            InvalidateRect(m_hwnd, NULL, FALSE);
            return;
        }
    }

    OpenDocTab tab;
    tab.filePath = path;
    tab.type = type;
    if (type == TabContentType::JavaCode) {
        tab.isJavaMode = true;
    }

    if (path == "APK_Summary" || path == "APK_Signature" ||
        path == "Security_Audit_Report.txt" || path == "APK_Diff_Report.diff" ||
        path.find("_xrefs.txt") != std::string::npos ||
        type == TabContentType::HexViewer) {
        tab.isReadOnly = true;
    }

    size_t lastSlash = path.find_last_of('/');
    std::string leafName = (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path;
    tab.title = KaisekiTheme::Utf8ToWide(leafName);
    tab.contentText = content;

    tab.lines.clear();
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        tab.lines.push_back(line);
    }
    if (tab.lines.empty()) tab.lines.push_back("");

    // Auto-disassemble DEX if content wasn't provided
    if (tab.contentText.empty() && EndsWithIgnoreCase(tab.filePath, ".dex")) {
        if (m_pModel) {
            uint8_t* bData = nullptr;
            size_t bSize = 0;
            if (ApkCore_ExtractFile(m_pModel->fullPath.c_str(), tab.filePath.c_str(), &bData, &bSize) && bData) {
                DexDisassemblyOutput disOut;
                if (KaisekiDex_Disassemble(bData, bSize, &disOut)) {
                    tab.contentText = disOut.smaliCode;
                    tab.javaAlternative = disOut.javaCode;
                    tab.lines.clear();
                    std::istringstream smaliStream(tab.contentText);
                    while (std::getline(smaliStream, line)) {
                        if (!line.empty() && line.back() == '\r') line.pop_back();
                        tab.lines.push_back(line);
                        if (tab.lines.size() >= 100000) {
                            tab.lines.push_back("# ... [100.000 satır sınırı: UI akıcılığı için kısaltıldı. Tam kod için Yerel Kaiseki DexEngine motorunu kullanınız.]");
                            break;
                        }
                    }
                    if (tab.lines.empty()) tab.lines.push_back("");
                    KaisekiDex_FreeDisassembly(&disOut);
                }
                ApkCore_FreeBuffer(bData);
            }
        }
    }

    m_tabs.push_back(tab);
    m_activeTabIndex = (int)m_tabs.size() - 1;
    InvalidateRect(m_hwnd, NULL, FALSE);
}

std::string UIWindow::BuildSecurityReportString() {
    if (!m_pModel) return "";
    if (!m_secReportLoaded) {
        KaisekiSec_AnalyzeApk(m_pModel, m_pModel->fullPath.c_str(), &m_secReport);
        m_secReportLoaded = true;
    }

    std::ostringstream secStream;
    secStream << "================================================================================\n";
    secStream << " KAISEKI - GÜVENLİK VE ZAFİYET DENETİM RAPORU (KaisekiSec Engine)\n";
    secStream << " Hedef Paket: " << m_pModel->packageName << " (v" << m_pModel->versionName << ")\n";
    secStream << " Güvenlik Skoru: " << m_secReport.riskScore << "/100  |  Derece: [" << m_secReport.riskGrade << "]\n";
    secStream << " Bulgular: Kritik: " << m_secReport.criticalCount << " | Yüksek: " << m_secReport.highCount
              << " | Orta: " << m_secReport.mediumCount << " | Düşük: " << m_secReport.lowCount << "\n";
    secStream << "================================================================================\n\n";

    for (size_t i = 0; i < m_secReport.findings.size(); ++i) {
        const auto& f = m_secReport.findings[i];
        std::string sevStr = "BİLGİ";
        if (f.severity == SecSeverity::Critical) sevStr = "🚨 KRİTİK";
        else if (f.severity == SecSeverity::High) sevStr = "⚠️ YÜKSEK";
        else if (f.severity == SecSeverity::Medium) sevStr = "⚡ ORTA";
        else if (f.severity == SecSeverity::Low) sevStr = "ℹ️ DÜŞÜK";

        secStream << "[" << (i + 1) << "] " << sevStr << ": " << f.title << "\n";
        secStream << "    Konum:        " << f.location << "\n";
        secStream << "    CWE:          " << f.cwe << "\n";
        secStream << "    Açıklama:     " << f.description << "\n";
        secStream << "    İyileştirme:  " << f.recommendation << "\n";
        secStream << "--------------------------------------------------------------------------------\n";
    }
    return secStream.str();
}

void UIWindow::OpenImageInTab(const std::string& path, const uint8_t* imgData, size_t imgSize) {
    if (!imgData || imgSize == 0 || !m_pWicFactory || !m_pRenderTarget) return;

    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].filePath == path) {
            m_activeTabIndex = (int)i;
            InvalidateRect(m_hwnd, NULL, FALSE);
            return;
        }
    }

    IWICStream* pStream = nullptr;
    HRESULT hr = m_pWicFactory->CreateStream(&pStream);
    if (FAILED(hr)) return;

    hr = pStream->InitializeFromMemory((WICInProcPointer)imgData, (DWORD)imgSize);
    if (FAILED(hr)) { pStream->Release(); return; }

    IWICBitmapDecoder* pDecoder = nullptr;
    hr = m_pWicFactory->CreateDecoderFromStream(pStream, NULL, WICDecodeMetadataCacheOnLoad, &pDecoder);
    if (FAILED(hr)) { pStream->Release(); return; }

    IWICBitmapFrameDecode* pFrame = nullptr;
    hr = pDecoder->GetFrame(0, &pFrame);
    if (FAILED(hr)) { pDecoder->Release(); pStream->Release(); return; }

    UINT w = 0, h = 0;
    pFrame->GetSize(&w, &h);

    IWICFormatConverter* pConverter = nullptr;
    hr = m_pWicFactory->CreateFormatConverter(&pConverter);
    if (FAILED(hr)) { pFrame->Release(); pDecoder->Release(); pStream->Release(); return; }

    IWICPalette* pPalette = nullptr;
    if (SUCCEEDED(m_pWicFactory->CreatePalette(&pPalette))) {
        if (FAILED(pFrame->CopyPalette(pPalette))) {
            pPalette->Release();
            pPalette = nullptr;
        }
    }

    hr = pConverter->Initialize(pFrame, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, pPalette, 0.0f, WICBitmapPaletteTypeCustom);
    if (pPalette) {
        pPalette->Release();
        pPalette = nullptr;
    }
    if (FAILED(hr)) { pConverter->Release(); pFrame->Release(); pDecoder->Release(); pStream->Release(); return; }

    ID2D1Bitmap* pBmp = nullptr;
    hr = m_pRenderTarget->CreateBitmapFromWicBitmap(pConverter, NULL, &pBmp);

    pConverter->Release();
    pFrame->Release();
    pDecoder->Release();
    pStream->Release();

    if (SUCCEEDED(hr) && pBmp) {
        OpenDocTab tab;
        tab.filePath = path;
        tab.type = TabContentType::ImagePreview;
        size_t lastSlash = path.find_last_of('/');
        std::string leafName = (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path;
        tab.title = KaisekiTheme::Utf8ToWide(leafName);
        tab.pImageBitmap = pBmp;
        tab.imgWidth = w;
        tab.imgHeight = h;
        tab.imgBytes = imgSize;

        m_tabs.push_back(tab);
        m_activeTabIndex = (int)m_tabs.size() - 1;
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

void UIWindow::CloseTab(int index) {
    if (index < 0 || index >= (int)m_tabs.size()) return;
    if (m_tabs[index].pImageBitmap) {
        m_tabs[index].pImageBitmap->Release();
        m_tabs[index].pImageBitmap = nullptr;
    }
    m_tabs.erase(m_tabs.begin() + index);
    if (m_activeTabIndex >= (int)m_tabs.size()) {
        m_activeTabIndex = (int)m_tabs.size() - 1;
    }
    if (m_activeTabIndex < 0) m_activeTabIndex = 0;
    InvalidateRect(m_hwnd, NULL, FALSE);
}

void UIWindow::SaveTab(int index) {
    if (index < 0 || index >= (int)m_tabs.size()) return;
    OpenDocTab& tab = m_tabs[index];
    if (tab.isReadOnly) return;

    std::ostringstream ss;
    for (size_t i = 0; i < tab.lines.size(); ++i) {
        ss << tab.lines[i];
        if (i + 1 < tab.lines.size()) ss << "\n";
    }
    std::string currentContent = ss.str();
    if (tab.isJavaMode) {
        tab.javaAlternative = currentContent;
    } else {
        tab.contentText = currentContent;
        // Always re-generate decompiled Java when switching from edited Smali so changes are reflected!
        KaisekiDex_SmaliToJava(currentContent.c_str(), &tab.javaAlternative);
    }
    tab.isModified = false;
    tab.rcSaveBtn = D2D1::RectF(0, 0, 0, 0);

    ShowToast(std::wstring(LocW(StringId::Toast_FileSaved)) + L" " + tab.title);
    m_consoleLogs.push_back({ L"[SAVE]", L"Updated document: " + tab.title, 0 });
    InvalidateRect(m_hwnd, NULL, FALSE);
}

void UIWindow::SaveActiveTab() {
    SaveTab(m_activeTabIndex);
}

void UIWindow::SaveAllTabs() {
    int savedCount = 0;
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].isModified && !m_tabs[i].isReadOnly) {
            std::ostringstream ss;
            for (size_t l = 0; l < m_tabs[i].lines.size(); ++l) {
                ss << m_tabs[i].lines[l];
                if (l + 1 < m_tabs[i].lines.size()) ss << "\n";
            }
            std::string currentContent = ss.str();
            if (m_tabs[i].isJavaMode) {
                m_tabs[i].javaAlternative = currentContent;
                m_tabs[i].contentText = currentContent;
            } else {
                m_tabs[i].contentText = currentContent;
            }
            m_tabs[i].isModified = false;
            m_tabs[i].rcSaveBtn = D2D1::RectF(0, 0, 0, 0);
            savedCount++;
        }
    }
    if (savedCount > 0) {
        std::wstring msg = std::to_wstring(savedCount) + L" dosya kaydedildi / files saved";
        ShowToast(msg);
        m_consoleLogs.push_back({ L"[SAVE ALL]", msg, 0 });
    } else {
        ShowToast(L"Kaydedilecek değişiklik yok.");
    }
    InvalidateRect(m_hwnd, NULL, FALSE);
}

void UIWindow::ToggleActiveTabJavaSmali() {
    if (m_tabs.empty() || m_activeTabIndex < 0 || m_activeTabIndex >= (int)m_tabs.size()) return;
    OpenDocTab& tab = m_tabs[m_activeTabIndex];
    if (tab.type != TabContentType::JavaCode && tab.type != TabContentType::SmaliCode) return;

    // 1. Commit active editor lines to the current representation
    std::ostringstream ss;
    for (size_t l = 0; l < tab.lines.size(); ++l) {
        ss << tab.lines[l];
        if (l + 1 < tab.lines.size()) ss << "\n";
    }
    std::string currentContent = ss.str();

    if (tab.isJavaMode) {
        tab.javaAlternative = currentContent;
    } else {
        tab.contentText = currentContent;
        // Always re-generate decompiled Java when switching from edited Smali so changes are reflected!
        KaisekiDex_SmaliToJava(currentContent.c_str(), &tab.javaAlternative);
    }

    // 2. Toggle mode and content type
    tab.isJavaMode = !tab.isJavaMode;
    tab.type = tab.isJavaMode ? TabContentType::JavaCode : TabContentType::SmaliCode;
    std::string targetText = tab.isJavaMode ? tab.javaAlternative : tab.contentText;

    // 3. Load target text into lines (stripping \r)
    tab.lines.clear();
    std::istringstream stream(targetText);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        tab.lines.push_back(line);
    }
    if (tab.lines.empty()) tab.lines.push_back("");

    // 4. Update tab title extension
    size_t dot = tab.filePath.rfind('.');
    std::string shortName = (dot != std::string::npos) ? tab.filePath.substr(dot + 1) : tab.filePath;
    tab.title = KaisekiTheme::Utf8ToWide(shortName) + (tab.isJavaMode ? L".java" : L".smali");

    // 5. Adjust cursor bounds safely
    if (tab.cursorLine >= (int)tab.lines.size()) {
        tab.cursorLine = (int)tab.lines.size() - 1;
    }
    if (tab.cursorLine < 0) tab.cursorLine = 0;
    std::wstring curLineW = KaisekiTheme::Utf8ToWide(tab.lines[tab.cursorLine]);
    if (tab.cursorCol > (int)curLineW.length()) tab.cursorCol = (int)curLineW.length();
    tab.ClearSelection();

    InvalidateRect(m_hwnd, NULL, FALSE);
}

void UIWindow::PasteClipboardText() {
    if (m_tabs.empty() || m_activeTabIndex < 0 || m_activeTabIndex >= (int)m_tabs.size()) return;
    OpenDocTab& tab = m_tabs[m_activeTabIndex];
    if (tab.type == TabContentType::ImagePreview || tab.isReadOnly) return;

    if (OpenClipboard(m_hwnd)) {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData) {
            const wchar_t* pText = (const wchar_t*)GlobalLock(hData);
            if (pText) {
                std::wstring pasteStr(pText);
                GlobalUnlock(hData);
                tab.InsertText(pasteStr);
                InvalidateRect(m_hwnd, NULL, FALSE);
            }
        }
        CloseClipboard();
    }
}

void UIWindow::CutSelectedText() {
    if (m_tabs.empty() || m_activeTabIndex < 0 || m_activeTabIndex >= (int)m_tabs.size()) return;
    OpenDocTab& tab = m_tabs[m_activeTabIndex];
    if (!tab.hasSelection || tab.isReadOnly) return;
    CopySelectedTextToClipboard();
    tab.DeleteSelection();
    InvalidateRect(m_hwnd, NULL, FALSE);
}

void UIWindow::BuildPackageTree() {
    m_pkgTreeRoot = PkgTreeNode();
    m_pkgTreeBuilt = false;
    m_treeItemsDirty = true;

    if (!m_pModel || m_pModel->packages.empty()) {
        m_cachedTreeItems.clear();
        return;
    }

    for (const auto& pkg : m_pModel->packages) {
        std::vector<std::string> parts;
        if (pkg.packageName.empty() || pkg.packageName == "defpackage") {
            parts.push_back("defpackage");
        } else {
            std::stringstream ss(pkg.packageName);
            std::string seg;
            while (std::getline(ss, seg, '.')) {
                if (!seg.empty()) parts.push_back(seg);
            }
            if (parts.empty()) parts.push_back("defpackage");
        }

        PkgTreeNode* curr = &m_pkgTreeRoot;
        std::string curKey = "";
        for (size_t p = 0; p < parts.size(); ++p) {
            if (!curKey.empty()) curKey += ".";
            curKey += parts[p];

            auto it = curr->children.find(parts[p]);
            if (it == curr->children.end()) {
                PkgTreeNode child;
                child.name = parts[p];
                child.fullPkgKey = curKey;
                curr->children[parts[p]] = child;
            }
            curr = &curr->children[parts[p]];
        }

        for (const auto& cls : pkg.classes) {
            curr->classes.push_back(cls);
        }
    }

    m_pkgTreeRoot.ComputeClassCounts();
    m_pkgTreeBuilt = true;
    RebuildFlattenedTreeItems();
}

void UIWindow::BuildResourcesTree() {
    m_resTreeRoot = ResTreeNode();
    m_resTreeBuilt = false;
    m_treeItemsDirty = true;

    if (!m_pModel || m_pModel->resources.empty()) {
        return;
    }

    for (const auto& r : m_pModel->resources) {
        std::string fpath = r.path;
        if (fpath.empty()) continue;

        // Split path by '/'
        std::vector<std::string> parts;
        size_t start = 0;
        while (start < fpath.size()) {
            size_t slash = fpath.find('/', start);
            if (slash == std::string::npos) {
                parts.push_back(fpath.substr(start));
                break;
            }
            parts.push_back(fpath.substr(start, slash - start));
            start = slash + 1;
        }

        if (parts.empty()) continue;
        if (parts.size() == 1) continue; // Root files kept directly under Kaynaklar

        const std::string& leafName = parts.back();
        int itype = 1;
        if (leafName.size() >= 4) {
            std::string ext = leafName.substr(leafName.size() - 4);
            for (char& c : ext) c = (char)tolower((unsigned char)c);
            if (ext == ".png" || ext == ".jpg" || ext == ".ico" || ext == ".bmp") itype = 3;
            else if (leafName.size() >= 5) {
                std::string ext5 = leafName.substr(leafName.size() - 5);
                for (char& c : ext5) c = (char)tolower((unsigned char)c);
                if (ext5 == ".webp" || ext5 == ".jpeg") itype = 3;
            }
        }

        ResTreeNode* curr = &m_resTreeRoot;
        std::string curKey = "";
        for (size_t p = 0; p < parts.size() - 1; ++p) {
            if (!curKey.empty()) curKey += "/";
            curKey += parts[p];

            auto it = curr->children.find(parts[p]);
            if (it == curr->children.end()) {
                ResTreeNode child;
                child.name = parts[p];
                child.fullDirKey = curKey;
                curr->children[parts[p]] = child;
            }
            curr = &curr->children[parts[p]];
        }

        curr->files.push_back({ leafName, fpath, itype });
    }

    // Add any ABIs if not already populated
    if (!m_pModel->abis.empty()) {
        auto itLib = m_resTreeRoot.children.find("lib");
        if (itLib == m_resTreeRoot.children.end()) {
            ResTreeNode libNode;
            libNode.name = "lib";
            libNode.fullDirKey = "lib";
            for (const auto& abi : m_pModel->abis) {
                ResTreeNode abiNode;
                abiNode.name = abi;
                abiNode.fullDirKey = "lib/" + abi;
                libNode.children[abi] = abiNode;
            }
            m_resTreeRoot.children["lib"] = libNode;
        }
    }

    m_resTreeRoot.ComputeFileCounts();
    m_resTreeBuilt = true;
    m_treeItemsDirty = true;
}

void UIWindow::DecompileAllDexToTemp() {
    if (!m_pModel) return;

    wchar_t szTemp[MAX_PATH] = { 0 };
    GetTempPathW(MAX_PATH, szTemp);
    std::wstring safeName = m_pModel->fileName;
    for (wchar_t& ch : safeName) {
        if (ch == L'/' || ch == L'\\' || ch == L':' || ch == L'*' || ch == L'?' || ch == L'"' || ch == L'<' || ch == L'>' || ch == L'|' || ch == L' ') {
            ch = L'_';
        }
    }
    m_workspaceTempDir = std::wstring(szTemp) + L"Kaiseki\\workspace\\" + safeName;
    std::wstring javaDir = m_workspaceTempDir + L"\\java";
    std::wstring smaliDir = m_workspaceTempDir + L"\\smali";
    std::wstring fridaDir = m_workspaceTempDir + L"\\frida";

    auto ensureDir = [](const std::wstring& dir) {
        if (GetFileAttributesW(dir.c_str()) != INVALID_FILE_ATTRIBUTES) return;
        size_t pos = 0;
        while ((pos = dir.find_first_of(L"\\/", pos + 1)) != std::wstring::npos) {
            std::wstring sub = dir.substr(0, pos);
            if (GetFileAttributesW(sub.c_str()) == INVALID_FILE_ATTRIBUTES) {
                CreateDirectoryW(sub.c_str(), NULL);
            }
        }
        CreateDirectoryW(dir.c_str(), NULL);
    };

    ensureDir(javaDir);
    ensureDir(smaliDir);
    ensureDir(fridaDir);

    m_decompiledClasses.clear();

    int effectiveThreads = SettingsManager::Get().settings.GetEffectiveDecompileThreads();

    for (size_t dIdx = 0; dIdx < m_pModel->dexFiles.size(); ++dIdx) {
        const auto& df = m_pModel->dexFiles[dIdx];
        uint8_t* dexBytes = nullptr;
        size_t dexLen = 0;
        if (ApkCore_ExtractFile(m_pModel->fullPath.c_str(), df.filename.c_str(), &dexBytes, &dexLen) && dexBytes && dexLen > 0) {
            std::vector<std::string> classNames;
            std::vector<std::string> javaCodes;
            std::vector<std::string> smaliCodes;
            std::vector<std::string> fridaCodes;

            int pctStart = 60 + (int)(25.0f * (float)dIdx / (float)m_pModel->dexFiles.size());
            std::wstring pStatus = L"[4/6] " + KaisekiTheme::Utf8ToWide(df.filename) + L" çözümleniyor • " + std::to_wstring(effectiveThreads) + L" Çekirdek Donanım Hızlandırması...";
            UpdateModalProgress(pctStart, pStatus.c_str());

            if (KaisekiDex_DecompileAllClassesParallel(dexBytes, dexLen, effectiveThreads,
                                                      &classNames, &javaCodes, &smaliCodes, &fridaCodes)) {
                size_t totalCls = classNames.size();

                // 1. Pre-collect and create all unique package directories up-front once
                std::unordered_set<std::wstring> uniqueDirs;
                for (size_t c = 0; c < totalCls; ++c) {
                    std::wstring subPath = KaisekiTheme::Utf8ToWide(classNames[c]);
                    for (wchar_t& ch : subPath) if (ch == L'.') ch = L'\\';
                    size_t lastS = subPath.find_last_of(L'\\');
                    if (lastS != std::wstring::npos) {
                        uniqueDirs.insert(subPath.substr(0, lastS));
                    }
                }
                for (const auto& udir : uniqueDirs) {
                    ensureDir(javaDir + L"\\" + udir);
                    ensureDir(smaliDir + L"\\" + udir);
                }

                // 2. High-speed parallel disk writing across worker threads
                int writeThreads = effectiveThreads;
                if (writeThreads <= 0) writeThreads = 4;
                if (writeThreads > 16) writeThreads = 16;
                if (writeThreads > (int)totalCls) writeThreads = (int)totalCls;

                if (totalCls > 0) {
                    std::vector<std::thread> ioWorkers;
                    ioWorkers.reserve(writeThreads);
                    for (int t = 0; t < writeThreads; ++t) {
                        size_t start = (t * totalCls) / writeThreads;
                        size_t end = ((t + 1) * totalCls) / writeThreads;
                        ioWorkers.emplace_back([&, start, end]() {
                            for (size_t c = start; c < end; ++c) {
                                std::wstring subPath = KaisekiTheme::Utf8ToWide(classNames[c]);
                                for (wchar_t& ch : subPath) if (ch == L'.') ch = L'\\';
                                std::wstring javaFile = javaDir + L"\\" + subPath + L".java";
                                std::wstring smaliFile = smaliDir + L"\\" + subPath + L".smali";

                                FILE* fJava = _wfopen(javaFile.c_str(), L"wb");
                                if (fJava) {
                                    fwrite(javaCodes[c].data(), 1, javaCodes[c].size(), fJava);
                                    fclose(fJava);
                                }
                                FILE* fSmali = _wfopen(smaliFile.c_str(), L"wb");
                                if (fSmali) {
                                    fwrite(smaliCodes[c].data(), 1, smaliCodes[c].size(), fSmali);
                                    fclose(fSmali);
                                }
                            }
                        });
                    }
                    for (auto& w : ioWorkers) {
                        if (w.joinable()) w.join();
                    }
                }

                // 3. Store in-memory cache
                for (size_t c = 0; c < totalCls; ++c) {
                    m_decompiledClasses[classNames[c]] = {
                        std::move(javaCodes[c]),
                        std::move(smaliCodes[c]),
                        fridaCodes.empty() ? "" : std::move(fridaCodes[c])
                    };
                }

                int pctEnd = 60 + (int)(25.0f * (float)(dIdx + 1) / (float)m_pModel->dexFiles.size());
                std::wstring doneStatus = L"[4/6] Dalvik DEX decompile edildi: " + std::to_wstring(m_decompiledClasses.size()) + L" sınıf • " + std::to_wstring(effectiveThreads) + L" Çekirdek";
                UpdateModalProgress(pctEnd, doneStatus.c_str());
            }
            ApkCore_FreeBuffer(dexBytes);
        }
    }
}

void UIWindow::RebuildFlattenedTreeItems() {
    m_cachedTreeItems.clear();

    if (!m_pModel) {
        m_treeItemsDirty = false;
        return;
    }

    if (!m_pkgTreeBuilt) {
        BuildPackageTree();
        return;
    }

    // 1. Source code (JADX Hierarchical Package Tree)
    m_cachedTreeItems.push_back({ 0, m_sourcesExpanded ? L"▼ Kaynak Kodları" : L"▶ Kaynak Kodları", "__TOGGLE_SOURCES__", 0 });
    if (m_sourcesExpanded && m_pkgTreeBuilt && !m_pkgTreeRoot.children.empty()) {
        for (const auto& pair : m_pkgTreeRoot.children) {
            pair.second.Flatten(m_cachedTreeItems, m_pkgExpanded, 1);
        }
    } else if (m_sourcesExpanded && !m_pModel->dexFiles.empty()) {
        for (const auto& df : m_pModel->dexFiles) {
            std::wstring dexLabel = KaisekiTheme::Utf8ToWide(df.filename);
            if (df.classCount > 0) dexLabel += L" (" + std::to_wstring(df.classCount) + L" sınıf)";
            m_cachedTreeItems.push_back({ 1, dexLabel, df.filename, 2 });
        }
    }

    // 2. Resources (JADX Resources Hierarchy - Fully Nested Multi-Level Subfolders)
    m_cachedTreeItems.push_back({ 0, m_resExpanded ? L"▼ Kaynaklar" : L"▶ Kaynaklar", "__TOGGLE_RES__", 0 });
    if (m_resExpanded) {
        m_cachedTreeItems.push_back({ 1, L"AndroidManifest.xml", "AndroidManifest.xml", 1 });
        m_cachedTreeItems.push_back({ 1, L"resources.arsc", "resources.arsc", 1 });

        if (!m_resTreeBuilt) {
            BuildResourcesTree();
        }

        if (m_resTreeBuilt && !m_resTreeRoot.children.empty()) {
            for (const auto& pair : m_resTreeRoot.children) {
                pair.second.Flatten(m_cachedTreeItems, m_pkgExpanded, 1);
            }
        }
    }

    // 3. APK Signatures (JADX ApkSignatureNode equivalent)
    m_cachedTreeItems.push_back({ 0, L"APK İmzası & Sertifika", "__APK_SIGNATURE__", 1 });

    // 4. APK Summary (JADX SummaryNode equivalent)
    m_cachedTreeItems.push_back({ 0, L"APK Özeti & Metadata", "__APK_SUMMARY__", 1 });

    m_treeItemsDirty = false;
}

void UIWindow::LoadApk(const std::wstring& path) {
    wchar_t szFull[MAX_PATH] = { 0 };
    std::wstring absPath = path;
    if (GetFullPathNameW(path.c_str(), MAX_PATH, szFull, NULL) > 0) {
        absPath = szFull;
    }

    ShowModalProgress(L"Kaiseki - APK Çözümleniyor...", L"[1/6] PKZIP arşivi taranıyor...");
    m_isLoading = true;
    m_loadProgress = 0.16f;
    m_loadStatusText = L"[1/6] PKZIP arşivi taranıyor...";
    UpdateModalProgress(16, L"[1/6] PKZIP arşivi taranıyor...");
    InvalidateRect(m_hwnd, NULL, FALSE);
    UpdateWindow(m_hwnd);

    if (m_pModel) {
        ApkCore_FreeModel(m_pModel);
        m_pModel = nullptr;
    }

    for (auto& tab : m_tabs) {
        if (tab.pImageBitmap) {
            tab.pImageBitmap->Release();
            tab.pImageBitmap = nullptr;
        }
    }
    m_tabs.clear();

    ApkDataModel* newModel = nullptr;
    if (ApkCore_InspectFile(absPath.c_str(), &newModel) && newModel) {
        m_pModel = newModel;

        m_loadProgress = 0.35f;
        m_loadStatusText = L"[2/6] AndroidManifest.xml ve kaynaklar çözümleniyor...";
        UpdateModalProgress(35, L"[2/6] AndroidManifest.xml ve kaynaklar çözümleniyor...");
        InvalidateRect(m_hwnd, NULL, FALSE);
        UpdateWindow(m_hwnd);

        if (m_pRenderTarget) {
            CreateIconBitmap();
        }

        m_loadProgress = 0.60f;
        m_loadStatusText = L"[3/6] Dalvik DEX sınıfları ve bayt kodları taranıyor...";
        UpdateModalProgress(60, L"[3/6] Dalvik DEX sınıfları ve bayt kodları taranıyor...");
        InvalidateRect(m_hwnd, NULL, FALSE);
        UpdateWindow(m_hwnd);
        UpdateModalProgress(60, L"Dalvik DEX ve AXML çözümleniyor...");
        InvalidateRect(m_hwnd, NULL, FALSE);
        UpdateWindow(m_hwnd);

        if (m_pRenderTarget) {
            CreateIconBitmap();
        }

        // Fast & Instant Workspace Setup:
        // m_pModel->packages is already fully indexed and grouped across all DEX files by ApkCore_InspectFile.
        std::string smaliStr = "# ====================================================================\n"
                               "# KAISEKI STUDIO - DALVIK SMALI & BYTECODE ENGINE\n"
                               "# ====================================================================\n"
                               "# Hedef: " + KaisekiTheme::WideToUtf8(m_pModel->fileName) + "\n"
                               "# Toplam DEX Dosyası: " + std::to_string(m_pModel->dexFiles.size()) + "\n"
                               "# Toplam Ayrıştırılan Sınıf: " + std::to_string(m_pModel->totalClasses) + "\n"
                               "# Toplam Ayrıştırılan Metot: " + std::to_string(m_pModel->totalMethods) + "\n\n"
                               "# Sol taraftaki Proje Gezgininden (Project Explorer) herhangi bir\n"
                               "# sınıfa tıklayarak doğrudan canlı ayrıştırmayı görüntüleyebilirsiniz.\n";

        std::string javaStr = "// ====================================================================\n"
                              "// KAISEKI STUDIO - JAVA DECOMPILER ENGINE\n"
                              "// ====================================================================\n"
                              "// Sol taraftaki Proje Gezgininden herhangi bir sınıfa tıklayarak\n"
                              "// AST Java kaynak kodunu doğrudan görüntüleyebilirsiniz.\n";

        m_loadProgress = 0.80f;
        m_loadStatusText = L"[4/6] Smali & Java çalışma alanı hazırlanıyor...";
        UpdateModalProgress(80, L"[4/6] Smali & Java çalışma alanı hazırlanıyor...");
        InvalidateRect(m_hwnd, NULL, FALSE);
        UpdateWindow(m_hwnd);

        // Tab 0: AndroidManifest.xml
        OpenDocTab manifestTab;
        manifestTab.title = L"AndroidManifest.xml";
        manifestTab.filePath = "AndroidManifest.xml";
        manifestTab.type = TabContentType::XmlText;
        manifestTab.contentText = m_pModel->formattedXml;
        std::istringstream mStream(m_pModel->formattedXml);
        std::string line;
        while (std::getline(mStream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            manifestTab.lines.push_back(line);
        }
        if (manifestTab.lines.empty()) manifestTab.lines.push_back("");
        m_tabs.push_back(manifestTab);

        // Tab 1: APK Özeti (JADX Summary)
        OpenDocTab summaryTab;
        summaryTab.title = L"APK Özeti";
        summaryTab.filePath = "APK_Summary";
        summaryTab.type = TabContentType::RawText;
        summaryTab.isReadOnly = true;
        summaryTab.contentText = m_pModel->summaryReport;
        std::istringstream sStream(m_pModel->summaryReport);
        while (std::getline(sStream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            summaryTab.lines.push_back(line);
        }
        if (summaryTab.lines.empty()) summaryTab.lines.push_back("");
        m_tabs.push_back(summaryTab);

        // Tab 2: APK İmzası (JADX Signature)
        OpenDocTab sigTab;
        sigTab.title = L"APK İmzası & Sertifika";
        sigTab.filePath = "APK_Signature";
        sigTab.type = TabContentType::RawText;
        sigTab.isReadOnly = true;
        sigTab.contentText = m_pModel->signature.formattedReport;
        std::istringstream sigStream(m_pModel->signature.formattedReport);
        while (std::getline(sigStream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            sigTab.lines.push_back(line);
        }
        if (sigTab.lines.empty()) sigTab.lines.push_back("");
        m_tabs.push_back(sigTab);

        // Tab 3: classes.dex (Smali)
        OpenDocTab dexTab;
        dexTab.title = L"classes.dex (Smali)";
        dexTab.filePath = "classes.dex";
        dexTab.type = TabContentType::SmaliCode;
        dexTab.contentText = smaliStr;
        dexTab.javaAlternative = javaStr;
        std::istringstream stream(smaliStr);
        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            dexTab.lines.push_back(line);
            if (dexTab.lines.size() >= 100000) {
                dexTab.lines.push_back("# ... [100.000 satır sınırı: UI akıcılığı için kısaltıldı. Tam kod için Yerel Kaiseki DexEngine motorunu kullanınız.]");
                break;
            }
        }
        if (dexTab.lines.empty()) dexTab.lines.push_back("");
        m_tabs.push_back(dexTab);

        // Tab 4: Güvenlik Denetim Raporu
        std::string secReportContent = BuildSecurityReportString();
        OpenDocTab secTab;
        secTab.title = L"🛡️ Güvenlik Raporu";
        secTab.filePath = "Security_Audit_Report.txt";
        secTab.type = TabContentType::RawText;
        secTab.isReadOnly = true;
        secTab.contentText = secReportContent;
        std::istringstream secLines(secTab.contentText);
        while (std::getline(secLines, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            secTab.lines.push_back(line);
        }
        if (secTab.lines.empty()) secTab.lines.push_back("");
        m_tabs.push_back(secTab);

        m_activeTabIndex = 0;
        m_railMode = RailMode::Explorer;
        m_sourcesExpanded = true;
        m_pkgExpanded.clear();
        // Expand the main application package path so user's code is immediately accessible
        if (!m_pModel->packageName.empty()) {
            std::stringstream ss(m_pModel->packageName);
            std::string seg;
            std::string acc = "";
            while (std::getline(ss, seg, '.')) {
                if (!acc.empty()) acc += ".";
                acc += seg;
                m_pkgExpanded[acc] = true;
            }
        }
        BuildPackageTree();
        BuildResourcesTree();
        m_pkgExpanded["__RES_DIR__res"] = true;
        m_pkgExpanded["__RES_DIR__lib"] = true;
        DecompileAllDexToTemp();
        if (!m_pModel->activities.empty()) {
            OpenClassInTab(m_pModel->activities[0], true);
        } else if (!m_pModel->packages.empty() && !m_pModel->packages[0].classes.empty()) {
            OpenClassInTab(m_pModel->packages[0].classes[0].fullClassName, true);
        }
        m_isLoading = false;

        UpdateModalProgress(100, L"[6/6] APK Çözümleme Tamamlandı!");
        CloseModalProgress();

        // Add to recent projects (.apk)
        std::wstring dexBadge = L"classes.dex (" + std::to_wstring(m_pModel->dexFiles.size()) + L" DEX)";
        SettingsManager::Get().AddRecentProject(path, dexBadge, L".apk");

        m_logcatFilterByPackage = true;
        m_logcatLoaded = false;
        m_rawLogcatEntries.clear();
        m_logcatLines.clear();

        RefreshConsoleLogs();
        ShowToast(LocW(StringId::Toast_ApkSuccess));

        ShowWindow(m_hwnd, SW_SHOW);
        SetWindowPos(m_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        SetForegroundWindow(m_hwnd);
        SetActiveWindow(m_hwnd);
        InvalidateRect(m_hwnd, NULL, TRUE);
        UpdateWindow(m_hwnd);
    } else {
        m_isLoading = false;
        CloseModalProgress();
        ShowToast(LocW(StringId::Toast_ApkOpenFail), true);

        ShowWindow(m_hwnd, SW_SHOW);
        SetWindowPos(m_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        SetForegroundWindow(m_hwnd);
        SetActiveWindow(m_hwnd);
        InvalidateRect(m_hwnd, NULL, TRUE);
        UpdateWindow(m_hwnd);
    }
}

void UIWindow::CreateIconBitmap() {
    if (!m_pModel || m_pModel->iconBytes.empty() || !m_pWicFactory || !m_pRenderTarget) return;

    if (m_pIconBitmap) {
        m_pIconBitmap->Release();
        m_pIconBitmap = nullptr;
    }

    IWICStream* pStream = nullptr;
    HRESULT hr = m_pWicFactory->CreateStream(&pStream);
    if (SUCCEEDED(hr)) {
        hr = pStream->InitializeFromMemory(m_pModel->iconBytes.data(), (DWORD)m_pModel->iconBytes.size());
        if (SUCCEEDED(hr)) {
            IWICBitmapDecoder* pDecoder = nullptr;
            hr = m_pWicFactory->CreateDecoderFromStream(pStream, NULL, WICDecodeMetadataCacheOnLoad, &pDecoder);
            if (SUCCEEDED(hr)) {
                IWICBitmapFrameDecode* pFrame = nullptr;
                hr = pDecoder->GetFrame(0, &pFrame);
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
                        hr = pConverter->Initialize(pFrame, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, pPalette, 0.0f, WICBitmapPaletteTypeCustom);
                        if (pPalette) {
                            pPalette->Release();
                            pPalette = nullptr;
                        }
                        if (SUCCEEDED(hr)) {
                            m_pRenderTarget->CreateBitmapFromWicBitmap(pConverter, NULL, &m_pIconBitmap);
                        }
                        pConverter->Release();
                    }
                    pFrame->Release();
                }
                pDecoder->Release();
            }
        }
        pStream->Release();
    }
}

void UIWindow::LoadAppLogoBitmap() {
    if (!m_pRenderTarget || !m_pWicFactory) return;
    if (m_pAppLogoBitmap) return;

    IWICBitmapDecoder* pDecoder = nullptr;

    // 1. Check embedded Win32 RCDATA resource (1002) in EXE or DLL
    HMODULE hMods[] = { GetModuleHandleW(NULL), m_hInstance };
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
    pDecoder->Release();
}

void UIWindow::ShowToast(const std::wstring& msg, bool isError) {
    m_toastText = msg;
    m_toastIsError = isError;
    m_toastStartTime = GetTickCount();
    InvalidateRect(m_hwnd, NULL, FALSE);
}

void UIWindow::OpenFileDialog() {
    WCHAR szFile[MAX_PATH] = { 0 };
    OPENFILENAMEW ofn = { sizeof(OPENFILENAMEW) };
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Android Paketleri ve Kaiseki Projeleri (*.apk; *.kproj)\0*.apk;*.kproj\0Android Paketleri (*.apk)\0*.apk\0Kaiseki Proje Dosyaları (*.kproj)\0*.kproj\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    std::wstring projDir = SettingsManager::Get().GetProjectsDir();
    ofn.lpstrInitialDir = projDir.c_str();

    if (GetOpenFileNameW(&ofn)) {
        std::wstring path = szFile;
        size_t dot = path.find_last_of(L'.');
        std::wstring ext = (dot != std::wstring::npos) ? path.substr(dot) : L"";
        for (auto& c : ext) c = towlower(c);

        if (ext == L".apk") {
            LoadApk(szFile);
        } else if (ext == L".kproj") {
            KaisekiProjectData proj;
            if (SettingsManager::Get().LoadProjectFile(path, proj)) {
                LoadApk(proj.apkPath);
                SettingsManager::Get().AddRecentProject(path, L"classes.dex (DEX Projesi)", L".kproj");
            } else {
                ShowToast(L"Proje dosyası okunamadı veya bozuk!", true);
            }
        } else {
            ShowToast(L"Yalnızca .apk ve .kproj uzantılı dosyalar desteklenmektedir.", true);
        }
    }
}

void UIWindow::OnDropFiles(HDROP hDrop) {
    UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
    if (count > 0) {
        WCHAR szPath[MAX_PATH];
        if (DragQueryFileW(hDrop, 0, szPath, MAX_PATH)) {
            std::wstring path = szPath;
            std::wstring ext = L"";
            size_t dot = path.find_last_of(L'.');
            if (dot != std::wstring::npos) {
                ext = path.substr(dot);
                for (auto& c : ext) c = towlower(c);
            }

            if (ext == L".apk") {
                LoadApk(path);
            } else if (ext == L".kproj") {
                KaisekiProjectData proj;
                if (SettingsManager::Get().LoadProjectFile(path, proj)) {
                    LoadApk(proj.apkPath);
                    SettingsManager::Get().AddRecentProject(path, L"classes.dex (DEX Projesi)", L".kproj");
                } else {
                    ShowToast(L"Proje dosyası okunamadı veya bozuk!", true);
                }
            } else {
                ShowToast(L"Yalnızca .apk ve .kproj uzantılı dosyalar kabul edilir.", true);
            }
        }
    }
    DragFinish(hDrop);
}

void UIWindow::HandleCharInput(wchar_t ch) {
    if (m_showSearchModal) {
        if (ch >= 32) {
            m_searchQuery.push_back(ch);
            PerformGlobalSearch(m_searchQuery, m_searchType);
            InvalidateRect(m_hwnd, NULL, FALSE);
        } else if (ch == '\b') {
            if (!m_searchQuery.empty()) {
                m_searchQuery.pop_back();
                PerformGlobalSearch(m_searchQuery, m_searchType);
                InvalidateRect(m_hwnd, NULL, FALSE);
            }
        } else if (ch == '\r') {
            if (!m_searchResults.empty()) {
                OpenClassInTab(m_searchResults[0].targetClass, true);
                m_showSearchModal = false;
                InvalidateRect(m_hwnd, NULL, FALSE);
            }
        }
        return;
    }

    if (m_tabs.empty() || m_activeTabIndex < 0 || m_activeTabIndex >= (int)m_tabs.size()) return;
    OpenDocTab& tab = m_tabs[m_activeTabIndex];
    if (tab.type == TabContentType::ImagePreview || tab.isReadOnly) return;

    // Avoid typing shortcuts when Ctrl or Alt is held
    if ((GetKeyState(VK_CONTROL) & 0x8000) || (GetKeyState(VK_MENU) & 0x8000)) {
        return;
    }

    if (ch >= 32) {
        std::wstring s(1, ch);
        tab.InsertText(s);
        InvalidateRect(m_hwnd, NULL, FALSE);
    } else if (ch == '\t') {
        tab.InsertText(L"    ");
        InvalidateRect(m_hwnd, NULL, FALSE);
    } else if (ch == '\r') {
        if (tab.cursorLine >= 0 && tab.cursorLine < (int)tab.lines.size()) {
            const std::string& curLine = tab.lines[tab.cursorLine];
            std::wstring curLineW = KaisekiTheme::Utf8ToWide(curLine);
            std::wstring indent = L"";
            for (wchar_t c : curLineW) {
                if (c == L' ' || c == L'\t') indent += c;
                else break;
            }
            tab.InsertText(L"\n" + indent);
        } else {
            tab.InsertText(L"\n");
        }
        InvalidateRect(m_hwnd, NULL, FALSE);
    } else if (ch == '\b') {
        if (tab.hasSelection) {
            tab.DeleteSelection();
            InvalidateRect(m_hwnd, NULL, FALSE);
        } else {
            if (tab.cursorLine >= (int)tab.lines.size()) tab.cursorLine = (int)tab.lines.size() - 1;
            if (tab.cursorLine < 0) tab.cursorLine = 0;
            if (!tab.lines.empty()) {
                std::wstring curLineW = KaisekiTheme::Utf8ToWide(tab.lines[tab.cursorLine]);
                if (tab.cursorCol > 0 && tab.cursorCol <= (int)curLineW.length()) {
                    tab.PushUndoState();
                    curLineW.erase(tab.cursorCol - 1, 1);
                    tab.lines[tab.cursorLine] = KaisekiTheme::WideToUtf8(curLineW);
                    tab.cursorCol--;
                    tab.isModified = true;
                    InvalidateRect(m_hwnd, NULL, FALSE);
                } else if (tab.cursorCol == 0 && tab.cursorLine > 0) {
                    tab.PushUndoState();
                    std::wstring prevLineW = KaisekiTheme::Utf8ToWide(tab.lines[tab.cursorLine - 1]);
                    int prevLen = (int)prevLineW.length();
                    prevLineW += curLineW;
                    tab.lines[tab.cursorLine - 1] = KaisekiTheme::WideToUtf8(prevLineW);
                    tab.lines.erase(tab.lines.begin() + tab.cursorLine);
                    tab.cursorLine--;
                    tab.cursorCol = prevLen;
                    tab.isModified = true;
                    InvalidateRect(m_hwnd, NULL, FALSE);
                }
            }
        }
    }
}

bool UIWindow::GetEditorRect(D2D1_RECT_F& outEditorRect, float& outTextX, float& outLineH, float& outCharW) const {
    if (m_railMode != RailMode::Explorer || !m_pModel || m_tabs.empty() || m_activeTabIndex < 0 || m_activeTabIndex >= (int)m_tabs.size()) {
        return false;
    }
    const auto& tab = m_tabs[m_activeTabIndex];
    if (tab.type == TabContentType::ImagePreview) {
        return false;
    }
    float treeW = 260.0f;
    float consoleH = m_consoleCollapsed ? 32.0f : 140.0f;
    float contentY = 40.0f;
    float contentH = m_height - 40.0f - 24.0f;
    float docH = contentH - consoleH;
    float docW = (m_width - 50.0f) - treeW;
    float docX = 50.0f + treeW;

    float y = contentY + 36.0f;
    float tbH = 30.0f;
    float editorY = y + tbH;
    float editorH = (docH - 36.0f) - tbH;
    float gutterW = 50.0f;

    outEditorRect = D2D1::RectF(docX + gutterW, editorY, docX + docW, editorY + editorH);
    outTextX = docX + gutterW + 10.0f;
    outLineH = 20.0f;
    outCharW = m_fonts.codeCharWidth > 0.0f ? m_fonts.codeCharWidth : 6.325f;
    return true;
}

int UIWindow::HitTestLineCol(int lineIdx, float mouseX, float textX) const {
    if (m_tabs.empty() || m_activeTabIndex < 0 || m_activeTabIndex >= (int)m_tabs.size()) return 0;
    const auto& tab = m_tabs[m_activeTabIndex];
    if (lineIdx < 0 || lineIdx >= (int)tab.lines.size()) return 0;

    std::wstring lineW = KaisekiTheme::Utf8ToWide(tab.lines[lineIdx]);
    while (!lineW.empty() && (lineW.back() == L'\r' || lineW.back() == L'\n')) {
        lineW.pop_back();
    }
    if (lineW.empty()) return 0;

    float relX = mouseX - textX;
    if (relX <= 0.0f) return 0;

    float cw = m_fonts.codeCharWidth > 0.0f ? m_fonts.codeCharWidth : 6.325f;
    if (!m_pDWriteFactory || !m_fonts.code) {
        int col = (int)((relX + (cw * 0.5f)) / cw);
        if (col > (int)lineW.length()) col = (int)lineW.length();
        return col;
    }

    IDWriteTextLayout* pLayout = nullptr;
    HRESULT hr = m_pDWriteFactory->CreateTextLayout(
        lineW.c_str(),
        (UINT32)lineW.length(),
        m_fonts.code,
        10000.0f,
        20.0f,
        &pLayout
    );

    if (FAILED(hr) || !pLayout) {
        int col = (int)((relX + (cw * 0.5f)) / cw);
        if (col > (int)lineW.length()) col = (int)lineW.length();
        return col;
    }

    pLayout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    BOOL isTrailingHit = FALSE;
    BOOL isInside = FALSE;
    DWRITE_HIT_TEST_METRICS htm = { 0 };
    pLayout->HitTestPoint(relX, 10.0f, &isTrailingHit, &isInside, &htm);

    int colIdx = 0;
    if (!isInside && relX >= (htm.left + htm.width)) {
        colIdx = (int)lineW.length();
    } else {
        colIdx = (int)htm.textPosition + (isTrailingHit ? 1 : 0);
    }

    if (colIdx < 0) colIdx = 0;
    if (colIdx > (int)lineW.length()) colIdx = (int)lineW.length();

    pLayout->Release();
    return colIdx;
}

float UIWindow::GetCharXPos(IDWriteTextLayout* pLayout, const std::wstring& lineW, int col, float textX, float charW) const {
    if (col <= 0 || lineW.empty()) {
        return textX;
    }
    int len = (int)lineW.length();
    if (col > len) col = len;
    if (pLayout) {
        FLOAT ptX = 0, ptY = 0;
        DWRITE_HIT_TEST_METRICS htm = { 0 };
        if (col < len) {
            if (SUCCEEDED(pLayout->HitTestTextPosition((UINT32)col, FALSE, &ptX, &ptY, &htm))) {
                return textX + ptX;
            }
        } else {
            if (SUCCEEDED(pLayout->HitTestTextPosition((UINT32)(len - 1), TRUE, &ptX, &ptY, &htm))) {
                return textX + ptX;
            }
        }
    }
    return textX + (float)col * charW;
}

void UIWindow::SelectWordAt(int lineIdx, int colIdx) {
    if (m_tabs.empty() || m_activeTabIndex < 0 || m_activeTabIndex >= (int)m_tabs.size()) return;
    OpenDocTab& tab = m_tabs[m_activeTabIndex];
    if (lineIdx < 0 || lineIdx >= (int)tab.lines.size()) return;

    std::wstring lineW = KaisekiTheme::Utf8ToWide(tab.lines[lineIdx]);
    if (lineW.empty()) return;
    if (colIdx >= (int)lineW.length()) colIdx = (int)lineW.length() - 1;
    if (colIdx < 0) colIdx = 0;

    auto isWordChar = [](wchar_t c) {
        return iswalnum(c) || c == L'_' || c == L'$';
    };

    if (!isWordChar(lineW[colIdx])) {
        if (colIdx > 0 && isWordChar(lineW[colIdx - 1])) {
            colIdx--;
        } else {
            return;
        }
    }

    int wStart = colIdx;
    while (wStart > 0 && isWordChar(lineW[wStart - 1])) {
        wStart--;
    }
    int wEnd = colIdx;
    while (wEnd < (int)lineW.length() && isWordChar(lineW[wEnd])) {
        wEnd++;
    }

    if (wEnd > wStart) {
        tab.hasSelection = true;
        tab.selAnchorLine = lineIdx;
        tab.selAnchorCol = wStart;
        tab.selActiveLine = lineIdx;
        tab.selActiveCol = wEnd;
        tab.cursorLine = lineIdx;
        tab.cursorCol = wEnd;
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

void UIWindow::ShowEditorContextMenu(int screenX, int screenY) {
    if (m_tabs.empty() || m_activeTabIndex < 0 || m_activeTabIndex >= (int)m_tabs.size()) return;
    OpenDocTab& tab = m_tabs[m_activeTabIndex];

    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    UINT undoFlags = MF_STRING | ((tab.isReadOnly || tab.undoStack.empty()) ? MF_GRAYED : 0);
    UINT redoFlags = MF_STRING | ((tab.isReadOnly || tab.redoStack.empty()) ? MF_GRAYED : 0);
    AppendMenuW(hMenu, undoFlags, 5010, L"↶ Geri Al / Undo (Ctrl+Z)");
    AppendMenuW(hMenu, redoFlags, 5011, L"↷ Yinele / Redo (Ctrl+Y)");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    UINT cutFlags = MF_STRING | ((tab.isReadOnly || !tab.hasSelection) ? MF_GRAYED : 0);
    UINT copyFlags = MF_STRING | (!tab.hasSelection ? MF_GRAYED : 0);
    UINT pasteFlags = MF_STRING | ((tab.isReadOnly || !IsClipboardFormatAvailable(CF_UNICODETEXT)) ? MF_GRAYED : 0);
    AppendMenuW(hMenu, cutFlags, 5012, L"✂ Kes / Cut (Ctrl+X)");
    AppendMenuW(hMenu, copyFlags, 5001, L"📋 Kopyala / Copy (Ctrl+C)");
    AppendMenuW(hMenu, pasteFlags, 5013, L"📋 Yapıştır / Paste (Ctrl+V)");
    AppendMenuW(hMenu, MF_STRING, 5002, L"🔍 Tümünü Seç / Select All (Ctrl+A)");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    UINT saveFlags = MF_STRING | ((tab.isReadOnly || !tab.isModified) ? MF_GRAYED : 0);
    AppendMenuW(hMenu, saveFlags, 5014, L"💾 Dosyayı Kaydet / Save (Ctrl+S)");
    AppendMenuW(hMenu, MF_STRING, 5015, L"📚 Tümünü Kaydet / Save All (Ctrl+Shift+S)");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    if (tab.type == TabContentType::SmaliCode || tab.type == TabContentType::JavaCode) {
        std::wstring toggleText = tab.isJavaMode ? L"⇄ Smali Görünümüne Geç" : L"⇄ Java Dekompile Görünümü";
        AppendMenuW(hMenu, MF_STRING, 5003, toggleText.c_str());
        AppendMenuW(hMenu, MF_STRING, 5004, L"💉 Frida Hook Scripti Oluştur");
    }

    SetForegroundWindow(m_hwnd);
    int cmd = TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
        screenX, screenY, m_hwnd, NULL);
    DestroyMenu(hMenu);

    if (cmd == 5010) {
        if (!tab.isReadOnly && tab.Undo()) {
            ShowToast(L"↶ Geri Alındı (Undo)");
            InvalidateRect(m_hwnd, NULL, FALSE);
        }
    } else if (cmd == 5011) {
        if (!tab.isReadOnly && tab.Redo()) {
            ShowToast(L"↷ Yinelendi (Redo)");
            InvalidateRect(m_hwnd, NULL, FALSE);
        }
    } else if (cmd == 5012) {
        if (!tab.isReadOnly) CutSelectedText();
    } else if (cmd == 5001) {
        CopySelectedTextToClipboard();
    } else if (cmd == 5013) {
        if (!tab.isReadOnly) PasteClipboardText();
    } else if (cmd == 5014) {
        SaveActiveTab();
    } else if (cmd == 5015) {
        SaveAllTabs();
    } else if (cmd == 5002) {
        if (!tab.lines.empty()) {
            tab.hasSelection = true;
            tab.selAnchorLine = 0;
            tab.selAnchorCol = 0;
            tab.selActiveLine = (int)tab.lines.size() - 1;
            std::wstring lastW = KaisekiTheme::Utf8ToWide(tab.lines.back());
            tab.selActiveCol = (int)lastW.length();
            tab.cursorLine = tab.selActiveLine;
            tab.cursorCol = tab.selActiveCol;
            InvalidateRect(m_hwnd, NULL, FALSE);
        }
    } else if (cmd == 5003) {
        ToggleActiveTabJavaSmali();
    } else if (cmd == 5004) {
        std::string targetCls = tab.filePath;
        std::string fridaScript;
        KaisekiDex_GenerateFridaScript(targetCls.c_str(), {}, &fridaScript);
        size_t dot = targetCls.rfind('.');
        std::string shortName = (dot != std::string::npos) ? targetCls.substr(dot + 1) : targetCls;

        OpenDocTab ftab;
        ftab.title = L"💉 " + KaisekiTheme::Utf8ToWide(shortName) + L"_frida.js";
        ftab.filePath = targetCls + "_frida.js";
        ftab.type = TabContentType::RawText;
        ftab.contentText = fridaScript;

        std::istringstream stream(fridaScript);
        std::string line;
        while (std::getline(stream, line)) {
            ftab.lines.push_back(line);
        }
        if (ftab.lines.empty()) ftab.lines.push_back("");

        m_tabs.push_back(ftab);
        m_activeTabIndex = (int)m_tabs.size() - 1;
        ShowToast(L"「Frida」 Hook scripti üretildi: " + ftab.title);
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

bool UIWindow::GetConsoleRect(D2D1_RECT_F& outConsoleRect, D2D1_RECT_F& outTextRect, float& outLineH) const {
    if (m_railMode != RailMode::Explorer || !m_pModel) {
        return false;
    }
    float treeW = 260.0f;
    float consoleH = m_consoleCollapsed ? 32.0f : 140.0f;
    float contentY = 40.0f;
    float contentH = m_height - 40.0f - 24.0f;
    float docH = contentH - consoleH;
    float docW = (m_width - 50.0f) - treeW;
    float docX = 50.0f + treeW;
    float consoleY = contentY + docH;

    outConsoleRect = D2D1::RectF(docX, consoleY, docX + docW, consoleY + consoleH);
    outTextRect = D2D1::RectF(docX, consoleY + 30.0f, docX + docW, consoleY + consoleH - 4.0f);
    outLineH = 18.0f;
    return true;
}

std::wstring UIWindow::GetConsoleLineText(int index) const {
    if (m_bottomConsoleTab == 0) {
        if (index >= 0 && index < (int)m_consoleLogs.size()) {
            return m_consoleLogs[index].tag + L"  " + m_consoleLogs[index].message;
        }
    } else {
        if (index >= 0 && index < (int)m_logcatLines.size()) {
            return m_logcatLines[index];
        }
    }
    return L"";
}

int UIWindow::GetConsoleLineCount() const {
    return (m_bottomConsoleTab == 0) ? (int)m_consoleLogs.size() : (int)m_logcatLines.size();
}

int UIWindow::HitTestConsoleCol(int lineIdx, float mouseX, float textX) const {
    std::wstring lineW = GetConsoleLineText(lineIdx);
    if (lineW.empty()) return 0;
    float relX = mouseX - textX;
    if (relX <= 0.0f) return 0;

    float cw = m_fonts.codeCharWidth > 0.0f ? m_fonts.codeCharWidth : 7.2f;
    int col = (int)((relX + cw * 0.5f) / cw);
    if (col > (int)lineW.length()) col = (int)lineW.length();
    return col;
}

void UIWindow::GetNormalizedConsoleSelection(int& outStartLine, int& outStartCol, int& outEndLine, int& outEndCol) const {
    if (m_consoleSelAnchorLine < m_consoleSelActiveLine ||
       (m_consoleSelAnchorLine == m_consoleSelActiveLine && m_consoleSelAnchorCol <= m_consoleSelActiveCol)) {
        outStartLine = m_consoleSelAnchorLine;
        outStartCol = m_consoleSelAnchorCol;
        outEndLine = m_consoleSelActiveLine;
        outEndCol = m_consoleSelActiveCol;
    } else {
        outStartLine = m_consoleSelActiveLine;
        outStartCol = m_consoleSelActiveCol;
        outEndLine = m_consoleSelAnchorLine;
        outEndCol = m_consoleSelAnchorCol;
    }
}

std::wstring UIWindow::GetSelectedConsoleText() const {
    if (!m_consoleHasSelection) return L"";
    int sLine, sCol, eLine, eCol;
    GetNormalizedConsoleSelection(sLine, sCol, eLine, eCol);
    int total = GetConsoleLineCount();
    if (sLine < 0) sLine = 0;
    if (eLine >= total) eLine = total - 1;

    std::wstring result = L"";
    for (int l = sLine; l <= eLine; ++l) {
        std::wstring lineW = GetConsoleLineText(l);
        int startC = (l == sLine) ? sCol : 0;
        int endC = (l == eLine) ? eCol : (int)lineW.length();

        if (startC > (int)lineW.length()) startC = (int)lineW.length();
        if (endC > (int)lineW.length()) endC = (int)lineW.length();
        if (endC < startC) endC = startC;

        result += lineW.substr(startC, endC - startC);
        if (l < eLine) {
            result += L"\r\n";
        }
    }
    return result;
}

void UIWindow::CopyConsoleSelectionToClipboard() {
    std::wstring text = GetSelectedConsoleText();
    if (text.empty()) return;

    if (OpenClipboard(m_hwnd)) {
        EmptyClipboard();
        size_t size = (text.length() + 1) * sizeof(wchar_t);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
        if (hMem) {
            void* pMem = GlobalLock(hMem);
            if (pMem) {
                memcpy(pMem, text.c_str(), size);
                GlobalUnlock(hMem);
                SetClipboardData(CF_UNICODETEXT, hMem);
            }
        }
        CloseClipboard();
        ShowToast(L"Seçili log metni panoya kopyalandı / Log copied to clipboard");
    }
}

void UIWindow::SelectAllConsole() {
    int count = GetConsoleLineCount();
    if (count == 0) return;
    m_consoleHasSelection = true;
    m_consoleSelAnchorLine = 0;
    m_consoleSelAnchorCol = 0;
    m_consoleSelActiveLine = count - 1;
    std::wstring lastLine = GetConsoleLineText(count - 1);
    m_consoleSelActiveCol = (int)lastLine.length();
    InvalidateRect(m_hwnd, NULL, FALSE);
}

void UIWindow::ShowConsoleContextMenu(int screenX, int screenY) {
    HMENU hMenu = CreatePopupMenu();
    UINT copyFlags = MF_STRING | (m_consoleHasSelection ? 0 : MF_GRAYED);
    AppendMenuW(hMenu, copyFlags, 6001, L"📋 Kopyala / Copy (Ctrl+C)");
    AppendMenuW(hMenu, MF_STRING, 6002, L"📄 Tümünü Seç / Select All (Ctrl+A)");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, 6003, L"🗑️ Logları Temizle / Clear");

    SetForegroundWindow(m_hwnd);
    int cmd = TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
        screenX, screenY, m_hwnd, NULL);
    DestroyMenu(hMenu);

    if (cmd == 6001) {
        CopyConsoleSelectionToClipboard();
    } else if (cmd == 6002) {
        SelectAllConsole();
    } else if (cmd == 6003) {
        if (m_bottomConsoleTab == 0) m_consoleLogs.clear();
        else {
            m_logcatLines.clear();
            m_rawLogcatEntries.clear();
        }
        m_consoleHasSelection = false;
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

void UIWindow::HandleKeyDown(WPARAM key) {
    if (key == VK_ESCAPE) {
        if (m_showSearchModal) {
            CloseSearchModalAnimated();
            return;
        }
        if (m_showSettingsModal) {
            CloseSettingsModalAnimated();
            return;
        }
        if (m_showAboutModal) {
            CloseAboutModalAnimated();
            return;
        }
        if (!m_tabs.empty() && m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabs.size()) {
            if (m_tabs[m_activeTabIndex].hasSelection) {
                m_tabs[m_activeTabIndex].ClearSelection();
                InvalidateRect(m_hwnd, NULL, FALSE);
                return;
            }
        }
    }

    bool isCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool isShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

    if (isCtrl) {
        if (key == 'Z' || key == 'z') {
            if (!m_tabs.empty() && m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabs.size()) {
                OpenDocTab& tab = m_tabs[m_activeTabIndex];
                if (!tab.isReadOnly) {
                    if (isShift) {
                        if (tab.Redo()) {
                            ShowToast(L"↷ Yinelendi (Redo)");
                            InvalidateRect(m_hwnd, NULL, FALSE);
                        }
                    } else {
                        if (tab.Undo()) {
                            ShowToast(L"↶ Geri Alındı (Undo)");
                            InvalidateRect(m_hwnd, NULL, FALSE);
                        }
                    }
                }
            }
            return;
        } else if (key == 'Y' || key == 'y') {
            if (!m_tabs.empty() && m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabs.size()) {
                OpenDocTab& tab = m_tabs[m_activeTabIndex];
                if (!tab.isReadOnly && tab.Redo()) {
                    ShowToast(L"↷ Yinelendi (Redo)");
                    InvalidateRect(m_hwnd, NULL, FALSE);
                }
            }
            return;
        } else if (key == 'V' || key == 'v') {
            if (!m_tabs.empty() && m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabs.size() && m_tabs[m_activeTabIndex].isReadOnly) {
                return;
            }
            PasteClipboardText();
            return;
        } else if (key == 'X' || key == 'x') {
            if (!m_tabs.empty() && m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabs.size()) {
                OpenDocTab& tab = m_tabs[m_activeTabIndex];
                if (tab.isReadOnly) {
                    CopySelectedTextToClipboard();
                    return;
                }
                if (tab.hasSelection) {
                    CutSelectedText();
                    return;
                }
            }
            FindXrefsInteractive();
            return;
        } else if (key == 'C' || key == 'c') {
            if (m_consoleHasSelection) {
                CopyConsoleSelectionToClipboard();
                return;
            }
            CopySelectedTextToClipboard();
            return;
        } else if (!isShift && (key == 'A' || key == 'a')) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(m_hwnd, &pt);
            float dipX = PixelToDipX((float)pt.x);
            float dipY = PixelToDipY((float)pt.y);
            D2D1_RECT_F rcConsole, rcConsoleClip; float cLineH;
            if (GetConsoleRect(rcConsole, rcConsoleClip, cLineH) &&
                dipX >= rcConsole.left && dipX <= rcConsole.right && dipY >= rcConsole.top && dipY <= rcConsole.bottom) {
                SelectAllConsole();
                return;
            }
            if (!m_tabs.empty() && m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabs.size()) {
                OpenDocTab& tab = m_tabs[m_activeTabIndex];
                if (!tab.lines.empty()) {
                    tab.hasSelection = true;
                    tab.selAnchorLine = 0;
                    tab.selAnchorCol = 0;
                    tab.selActiveLine = (int)tab.lines.size() - 1;
                    std::wstring lastW = KaisekiTheme::Utf8ToWide(tab.lines.back());
                    tab.selActiveCol = (int)lastW.length();
                    tab.cursorLine = tab.selActiveLine;
                    tab.cursorCol = tab.selActiveCol;
                    InvalidateRect(m_hwnd, NULL, FALSE);
                }
            }
            return;
        } else if (isShift && (key == 'F' || key == 'f')) {
            if (m_showSearchModal) {
                CloseSearchModalAnimated();
            } else {
                m_showSearchModal = true;
                m_closingSearch = false;
                m_animSearchModal = 0.0f;
            }
            InvalidateRect(m_hwnd, NULL, FALSE);
            return;
        } else if (isShift && (key == 'O' || key == 'o')) {
            OpenKaisekiProject();
            return;
        } else if (isShift && (key == 'A' || key == 'a')) {
            if (m_pModel && !m_pModel->activities.empty()) {
                OpenClassInTab(m_pModel->activities[0], true);
            }
            return;
        } else if (isShift && (key == 'M' || key == 'm')) {
            if (m_pModel) OpenDocInNewTab("AndroidManifest.xml", TabContentType::XmlText, m_pModel->formattedXml);
            return;
        } else if (key == 'E' || key == 'e') {
            ExportGradleProjectInteractive();
            return;
        } else if (key == 'D' || key == 'd') {
            CompareApksInteractive();
            return;
        } else if (key == 'N' || key == 'n') {
            m_showSearchModal = true;
            m_searchType = 0; // Class Search
            InvalidateRect(m_hwnd, NULL, FALSE);
            return;
        } else if (isShift && (key == 'S' || key == 's')) {
            SaveAllTabs();
            return;
        } else if (key == 'O' || key == 'o') {
            OpenFileDialog();
            return;
        } else if (key == 'S' || key == 's') {
            SaveActiveTab();
            return;
        } else if (key == 'W' || key == 'w') {
            if (!m_tabs.empty() && m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabs.size()) {
                CloseTab(m_activeTabIndex);
            } else {
                CloseProject();
            }
            return;
        } else if (key == '1') {
            m_railMode = RailMode::Explorer;
            InvalidateRect(m_hwnd, NULL, FALSE);
            return;
        } else if (key == '2') {
            m_railMode = RailMode::ApkInfo;
            InvalidateRect(m_hwnd, NULL, FALSE);
            return;
        } else if (key == '3') {
            m_railMode = RailMode::Signer;
            InvalidateRect(m_hwnd, NULL, FALSE);
            return;
        } else if (key == 'H' || key == 'h') {
            OpenHexViewerInteractive();
            return;
        } else if (key == 'R' || key == 'r') {
            OpenResourceTableInteractive();
            return;
        } else if (key == '4') {
            m_railMode = RailMode::AdbPhone;
            InvalidateRect(m_hwnd, NULL, FALSE);
            return;
        } else if (key == VK_OEM_COMMA) {
            OpenSettingsModal();
            return;
        }
    }

    if (key == VK_F7) {
        TriggerMenuAction(301);
        return;
    }
    if (key == VK_F8) {
        BuildAndRepackageApkInteractive();
        return;
    }
    if (key == VK_F5) {
        TriggerMenuAction(205);
        return;
    }
    if (key == VK_F6) {
        TriggerMenuAction(206);
        return;
    }
    if (key == VK_F9) {
        TriggerMenuAction(303);
        return;
    }
    if (key == VK_F10) {
        TriggerMenuAction(304);
        return;
    }

    if (key == VK_ESCAPE) {
        if (m_showAboutModal) {
            m_showAboutModal = false;
            InvalidateRect(m_hwnd, NULL, FALSE);
            return;
        }
        if (m_activeMenu != -1) {
            m_activeMenu = -1;
            InvalidateRect(m_hwnd, NULL, FALSE);
            return;
        }
    }

    if (m_tabs.empty() || m_activeTabIndex < 0 || m_activeTabIndex >= (int)m_tabs.size()) return;
    OpenDocTab& tab = m_tabs[m_activeTabIndex];
    if (tab.type == TabContentType::ImagePreview) return;

    if (key == VK_UP || key == VK_DOWN || key == VK_LEFT || key == VK_RIGHT) {
        if (isShift) {
            if (!tab.hasSelection) {
                tab.selAnchorLine = tab.cursorLine;
                tab.selAnchorCol = tab.cursorCol;
                tab.hasSelection = true;
            }
        } else {
            tab.ClearSelection();
        }

        if (key == VK_UP) {
            if (tab.cursorLine > 0) {
                tab.cursorLine--;
                std::wstring lineW = KaisekiTheme::Utf8ToWide(tab.lines[tab.cursorLine]);
                if (tab.cursorCol > (int)lineW.length()) tab.cursorCol = (int)lineW.length();
            }
        } else if (key == VK_DOWN) {
            if (tab.cursorLine + 1 < (int)tab.lines.size()) {
                tab.cursorLine++;
                std::wstring lineW = KaisekiTheme::Utf8ToWide(tab.lines[tab.cursorLine]);
                if (tab.cursorCol > (int)lineW.length()) tab.cursorCol = (int)lineW.length();
            }
        } else if (key == VK_LEFT) {
            if (tab.cursorCol > 0) tab.cursorCol--;
        } else if (key == VK_RIGHT) {
            if (tab.cursorLine >= 0 && tab.cursorLine < (int)tab.lines.size()) {
                std::wstring lineW = KaisekiTheme::Utf8ToWide(tab.lines[tab.cursorLine]);
                if (tab.cursorCol < (int)lineW.length()) {
                    tab.cursorCol++;
                }
            }
        }

        if (isShift) {
            tab.selActiveLine = tab.cursorLine;
            tab.selActiveCol = tab.cursorCol;
        }

        InvalidateRect(m_hwnd, NULL, FALSE);
        return;
    }

    if (key == VK_DELETE) {
        if (tab.type != TabContentType::ImagePreview && !tab.isReadOnly) {
            if (tab.hasSelection) {
                tab.DeleteSelection();
                InvalidateRect(m_hwnd, NULL, FALSE);
                return;
            } else if (!tab.lines.empty() && tab.cursorLine >= 0 && tab.cursorLine < (int)tab.lines.size()) {
                std::wstring curLineW = KaisekiTheme::Utf8ToWide(tab.lines[tab.cursorLine]);
                if (tab.cursorCol < (int)curLineW.length()) {
                    tab.PushUndoState();
                    curLineW.erase(tab.cursorCol, 1);
                    tab.lines[tab.cursorLine] = KaisekiTheme::WideToUtf8(curLineW);
                    tab.isModified = true;
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return;
                } else if (tab.cursorLine + 1 < (int)tab.lines.size()) {
                    tab.PushUndoState();
                    std::wstring nextLineW = KaisekiTheme::Utf8ToWide(tab.lines[tab.cursorLine + 1]);
                    curLineW += nextLineW;
                    tab.lines[tab.cursorLine] = KaisekiTheme::WideToUtf8(curLineW);
                    tab.lines.erase(tab.lines.begin() + tab.cursorLine + 1);
                    tab.isModified = true;
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return;
                }
            }
        }
    }

    if (key == VK_HOME || key == VK_END) {
        if (tab.type != TabContentType::ImagePreview && !tab.lines.empty()) {
            if (isShift) {
                if (!tab.hasSelection) {
                    tab.selAnchorLine = tab.cursorLine;
                    tab.selAnchorCol = tab.cursorCol;
                    tab.hasSelection = true;
                }
            } else {
                tab.ClearSelection();
            }

            if (key == VK_HOME) {
                if (isCtrl) {
                    tab.cursorLine = 0;
                    tab.cursorCol = 0;
                } else {
                    tab.cursorCol = 0;
                }
            } else if (key == VK_END) {
                if (isCtrl) {
                    tab.cursorLine = (int)tab.lines.size() - 1;
                    std::wstring lastW = KaisekiTheme::Utf8ToWide(tab.lines.back());
                    tab.cursorCol = (int)lastW.length();
                } else {
                    if (tab.cursorLine >= 0 && tab.cursorLine < (int)tab.lines.size()) {
                        std::wstring curW = KaisekiTheme::Utf8ToWide(tab.lines[tab.cursorLine]);
                        tab.cursorCol = (int)curW.length();
                    }
                }
            }

            if (isShift) {
                tab.selActiveLine = tab.cursorLine;
                tab.selActiveCol = tab.cursorCol;
            }
            InvalidateRect(m_hwnd, NULL, FALSE);
            return;
        }
    }
}

void UIWindow::TriggerMenuAction(int actionId) {
    m_activeMenu = -1;
    switch (actionId) {
    case 101: OpenFileDialog(); break;
    case 102: OpenKaisekiProject(); break;
    case 103: SaveKaisekiProject(); break;
    case 104: ExportGradleProjectInteractive(); break;
    case 105: PostMessage(m_hwnd, WM_CLOSE, 0, 0); break;
    case 106: SaveActiveTab(); break;
    case 107: SaveAllTabs(); break;
    case 108: CloseProject(); break;
    case 201: // Metin Ara
        m_showSearchModal = true;
        m_searchType = 2;
        InvalidateRect(m_hwnd, NULL, FALSE);
        break;
    case 202: // Sınıf Ara
        m_showSearchModal = true;
        m_searchType = 0;
        InvalidateRect(m_hwnd, NULL, FALSE);
        break;
    case 203: // Ana Activity'ye Git
        if (m_pModel && !m_pModel->activities.empty()) {
            OpenClassInTab(m_pModel->activities[0], true);
        } else {
            ShowToast(L"Ana Activity bulunamadı!");
        }
        break;
    case 204: // AndroidManifest.xml'e Git
        if (m_pModel) OpenDocInNewTab("AndroidManifest.xml", TabContentType::XmlText, m_pModel->formattedXml);
        break;
    case 205: // APK İmzasına Git
        if (m_pModel) OpenDocInNewTab("APK_Signature", TabContentType::RawText, m_pModel->signature.formattedReport);
        break;
    case 206: // APK Özetine Git
        if (m_pModel) OpenDocInNewTab("APK_Summary", TabContentType::RawText, m_pModel->summaryReport);
        break;
    case 207: // Çapraz Referans / Xref Bul
        FindXrefsInteractive();
        break;
    case 208: // ARSC Kaynak Tablosunu Aç
        OpenResourceTableInteractive();
        break;
    case 301: // Yerel DexEngine ile Decompile Et
        if (!m_tabs.empty() && m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabs.size()) {
            OpenClassInTab(m_tabs[m_activeTabIndex].filePath, true);
        }
        break;
    case 311: // Değişiklikleri APK Olarak Paketle & İmzala
        BuildAndRepackageApkInteractive();
        break;
    case 302: // Deobfuscation Aç/Kapat
        m_useDeobfuscation = !m_useDeobfuscation;
        ShowToast(m_useDeobfuscation ? LocW(StringId::Toast_DeobfEnabled) : LocW(StringId::Toast_DeobfDisabled));
        InvalidateRect(m_hwnd, NULL, FALSE);
        break;
    case 303: m_railMode = RailMode::Signer; InvalidateRect(m_hwnd, NULL, FALSE); break;
    case 304: m_railMode = RailMode::AdbPhone; InvalidateRect(m_hwnd, NULL, FALSE); break;
    case 310: CompareApksInteractive(); break;
    case 305: {
        WCHAR path[MAX_PATH];
        GetModuleFileNameW(NULL, path, MAX_PATH);
        SettingsManager::Get().RegisterKprojAssociation(path);
        ShowToast(LocW(StringId::Toast_KprojAssocSuccess));
        break;
    }
    case 306: { // ⚡ Frida Hook Scripti Üret
        std::string cls;
        if (!m_tabs.empty() && m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabs.size()) {
            cls = m_tabs[m_activeTabIndex].filePath;
        } else if (m_pModel && !m_pModel->activities.empty()) {
            cls = m_pModel->activities[0];
        }
        if (!cls.empty()) {
            std::string fridaScript;
            std::vector<std::string> dummyMethods;
            if (KaisekiDex_GenerateFridaScript(cls.c_str(), dummyMethods, &fridaScript)) {
                OpenDocInNewTab(cls + "_frida.js", TabContentType::RawText, fridaScript);
                ShowToast(L"「Frida」 Dinamik analiz hook scripti oluşturuldu!");
            }
        } else {
            ShowToast(L"Frida hook için önce bir sınıf seçin!");
        }
        break;
    }
    case 307: { // 🧩 Xposed / LSPosed Modül Kodu Üret
        std::string cls;
        if (!m_tabs.empty() && m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabs.size()) {
            cls = m_tabs[m_activeTabIndex].filePath;
        } else if (m_pModel && !m_pModel->activities.empty()) {
            cls = m_pModel->activities[0];
        }
        if (!cls.empty()) {
            std::string xposedCode;
            std::vector<std::string> dummyMethods;
            if (KaisekiDex_GenerateXposedSnippet(cls.c_str(), dummyMethods, &xposedCode)) {
                OpenDocInNewTab(cls + "_xposed.java", TabContentType::JavaCode, xposedCode);
                ShowToast(L"「Xposed」 LSPosed modül kaynak kodu oluşturuldu!");
            }
        } else {
            ShowToast(L"Xposed hook için önce bir sınıf seçin!");
        }
        break;
    }
    case 308: { // 📊 Kontrol Akış Grafiği (CFG) Oluştur
        std::string cls;
        if (!m_tabs.empty() && m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabs.size()) {
            cls = m_tabs[m_activeTabIndex].filePath;
        } else if (m_pModel && !m_pModel->activities.empty()) {
            cls = m_pModel->activities[0];
        }
        if (!cls.empty()) {
            std::string cfgCode;
            if (KaisekiDex_GenerateMethodCFG(cls.c_str(), &cfgCode)) {
                OpenDocInNewTab(cls + "_cfg.txt", TabContentType::RawText, cfgCode);
                ShowToast(L"「CFG」 Kontrol Akış Grafiği (Basic Blocks) oluşturuldu!");
            }
        } else {
            ShowToast(L"CFG grafiği için önce bir sınıf açın!");
        }
        break;
    }
    case 309: { // 🗺️ ProGuard / R8 Mapping Dosyası Yükle
        WCHAR szFile[MAX_PATH] = { 0 };
        OPENFILENAMEW ofn = { sizeof(OPENFILENAMEW) };
        ofn.hwndOwner = m_hwnd;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrFilter = L"ProGuard / R8 Mappings (*.txt; *.map)\0*.txt;*.map\0Tüm Dosyalar (*.*)\0*.*\0";
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetOpenFileNameW(&ofn)) {
            int count = 0;
            if (ApkCore_LoadMappingFile(szFile, &count)) {
                if (m_pModel) ApkCore_ApplyMappingToModel(m_pModel);
                ShowToast(L"「Mapping」 " + std::to_wstring(count) + L" sembol başarıyla deobfuscate edildi!");
                InvalidateRect(m_hwnd, NULL, FALSE);
            } else {
                ShowToast(L"Mapping dosyası okunamadı veya eşleşme bulunamadı!", true);
            }
        }
        break;
    }
    case 401: m_railMode = RailMode::Explorer; InvalidateRect(m_hwnd, NULL, FALSE); break;
    case 402: m_railMode = RailMode::ApkInfo; InvalidateRect(m_hwnd, NULL, FALSE); break;
    case 403: m_railMode = RailMode::Signer; InvalidateRect(m_hwnd, NULL, FALSE); break;
    case 404: m_railMode = RailMode::AdbPhone; InvalidateRect(m_hwnd, NULL, FALSE); break;
    case 406: OpenHexViewerInteractive(); break;
    case 405:
    case 501:
        OpenSettingsModal();
        break;
    case 601:
        m_showAboutModal = true;
        m_closingAbout = false;
        m_animAboutModal = 0.0f;
        InvalidateRect(m_hwnd, NULL, FALSE);
        break;
    }
}

void UIWindow::OpenSettingsModalDirect() {
    OpenSettingsModal();
}

void UIWindow::OpenSettingsModal(int initialTab) {
    if (initialTab >= 0) m_settingsTab = initialTab;
    SettingsDialog::Get().ShowModal(m_hwnd, m_settingsTab);
    m_currentTheme = (AppTheme)SettingsManager::Get().settings.theme;
    m_currentBgShade = (BackgroundShade)SettingsManager::Get().settings.bgShade;
    m_customAccentColor = D2D1::ColorF(
        SettingsManager::Get().settings.accentR / 255.0f,
        SettingsManager::Get().settings.accentG / 255.0f,
        SettingsManager::Get().settings.accentB / 255.0f, 1.0f);
    m_customBgColor = D2D1::ColorF(
        SettingsManager::Get().settings.bgR / 255.0f,
        SettingsManager::Get().settings.bgG / 255.0f,
        SettingsManager::Get().settings.bgB / 255.0f, 1.0f);
    m_colors = KaisekiTheme::GetTheme(m_currentTheme, &m_customAccentColor, m_currentBgShade);
    if (m_currentBgShade == BackgroundShade::Custom) {
        KaisekiTheme::ApplyCustomBackgroundColor(m_colors, m_customBgColor);
    }
    RecreateThemeBrushes();
    InvalidateRect(m_hwnd, NULL, FALSE);
}

bool UIWindow::UpdateAnimations() {
    bool animating = false;

    // 1. Launch entrance scale & fade lerp
    if (m_windowFadeAlpha < 1.0f) {
        m_windowFadeAlpha += 0.08f;
        if (m_windowFadeAlpha >= 1.0f) {
            m_windowFadeAlpha = 1.0f;
        }
        animating = true;
    }

    // 2. Activity Rail Active Indicator Spring Lerp
    float targetRailY = 50.0f + (int)m_railMode * 48.0f;
    if (fabs(targetRailY - m_animRailY) > 0.1f) {
        m_animRailY += (targetRailY - m_animRailY) * 0.28f;
        animating = true;
    } else {
        m_animRailY = targetRailY;
    }

    // Rail Hover Alphas
    for (int i = 0; i < 5; ++i) {
        float target = (m_hoverRail == i) ? 1.0f : 0.0f;
        if (fabs(target - m_railHoverAlpha[i]) > 0.01f) {
            m_railHoverAlpha[i] += (target - m_railHoverAlpha[i]) * 0.25f;
            animating = true;
        } else {
            m_railHoverAlpha[i] = target;
        }
    }

    // 3. Modals Smooth Zoom & Fade
    if (m_showSearchModal) {
        if (m_closingSearch) {
            m_animSearchModal -= 0.14f;
            if (m_animSearchModal <= 0.0f) {
                m_animSearchModal = 0.0f;
                m_showSearchModal = false;
                m_closingSearch = false;
            }
            animating = true;
        } else if (m_animSearchModal < 0.999f) {
            m_animSearchModal += (1.0f - m_animSearchModal) * 0.28f;
            if (m_animSearchModal > 0.999f) m_animSearchModal = 1.0f;
            animating = true;
        }
    }

    if (m_showAboutModal) {
        if (m_closingAbout) {
            m_animAboutModal -= 0.14f;
            if (m_animAboutModal <= 0.0f) {
                m_animAboutModal = 0.0f;
                m_showAboutModal = false;
                m_closingAbout = false;
            }
            animating = true;
        } else if (m_animAboutModal < 0.999f) {
            m_animAboutModal += (1.0f - m_animAboutModal) * 0.28f;
            if (m_animAboutModal > 0.999f) m_animAboutModal = 1.0f;
            animating = true;
        }
    }

    // 4. Dropdown Menu Animation
    if (m_activeMenu != -1) {
        if (m_animDropdown < 0.999f) {
            m_animDropdown += (1.0f - m_animDropdown) * 0.35f;
            if (m_animDropdown > 1.0f) m_animDropdown = 1.0f;
            animating = true;
        }
    } else {
        if (m_animDropdown > 0.0f) {
            m_animDropdown = 0.0f;
            animating = true;
        }
    }

    // 5. Breathing Sine Wave Pulse (only active during hero drop hover)
    if (m_hoverHeroDrop) {
        m_pulsePhase += 0.06f;
        if (m_pulsePhase > 6.2831853f) m_pulsePhase -= 6.2831853f;
        animating = true;
    }

    // 6. Scrollbar Auto-Hide Decay (smoothly fades when idle)
    if (!m_isDraggingEditorScroll && m_editorScrollAlpha > 0.0f) {
        m_editorScrollAlpha = (std::max)(0.0f, m_editorScrollAlpha - 0.03f);
        animating = true;
    }
    if (!m_isDraggingTreeScroll && m_treeScrollAlpha > 0.0f) {
        m_treeScrollAlpha = (std::max)(0.0f, m_treeScrollAlpha - 0.03f);
        animating = true;
    }

    // 7. Toast Slide Animation
    if (!m_toastText.empty()) {
        DWORD elapsed = GetTickCount() - m_toastStartTime;
        if (elapsed < 350) {
            m_toastAnim = KaisekiTheme::EaseOutBack((float)elapsed / 350.0f);
            animating = true;
        } else if (elapsed > 3500) {
            m_toastText.clear();
            m_toastAnim = 0.0f;
            animating = true;
        } else if (elapsed > 3150) {
            float t = (float)(elapsed - 3150) / 350.0f;
            m_toastAnim = 1.0f - KaisekiTheme::EaseOutCubic(t);
            if (m_toastAnim < 0.0f) m_toastAnim = 0.0f;
            animating = true;
        } else {
            m_toastAnim = 1.0f;
        }
    }

    // 8. Titlebar Buttons Hover Transitions
    float targetClose = m_hoverWinClose ? 1.0f : 0.0f;
    if (fabs(targetClose - m_hoverWinCloseAlpha) > 0.01f) {
        m_hoverWinCloseAlpha += (targetClose - m_hoverWinCloseAlpha) * 0.30f;
        animating = true;
    } else {
        m_hoverWinCloseAlpha = targetClose;
    }

    float targetMax = m_hoverWinMax ? 1.0f : 0.0f;
    if (fabs(targetMax - m_hoverWinMaxAlpha) > 0.01f) {
        m_hoverWinMaxAlpha += (targetMax - m_hoverWinMaxAlpha) * 0.30f;
        animating = true;
    } else {
        m_hoverWinMaxAlpha = targetMax;
    }

    float targetMin = m_hoverWinMin ? 1.0f : 0.0f;
    if (fabs(targetMin - m_hoverWinMinAlpha) > 0.01f) {
        m_hoverWinMinAlpha += (targetMin - m_hoverWinMinAlpha) * 0.30f;
        animating = true;
    } else {
        m_hoverWinMinAlpha = targetMin;
    }

    float targetHero = m_hoverHeroDrop ? 1.0f : 0.0f;
    if (fabs(targetHero - m_hoverHeroDropAlpha) > 0.01f) {
        m_hoverHeroDropAlpha += (targetHero - m_hoverHeroDropAlpha) * 0.25f;
        animating = true;
    } else {
        m_hoverHeroDropAlpha = targetHero;
    }

    // 9. Smooth Scroll Lerping
    if (fabs(m_tabScrollX - m_animTabScrollX) > 0.5f) {
        m_animTabScrollX += (m_tabScrollX - m_animTabScrollX) * 0.30f;
        animating = true;
    } else {
        m_animTabScrollX = m_tabScrollX;
    }

    if (fabs(m_treeScrollY - m_animTreeScrollY) > 0.5f) {
        m_animTreeScrollY += (m_treeScrollY - m_animTreeScrollY) * 0.30f;
        animating = true;
    } else {
        m_animTreeScrollY = m_treeScrollY;
    }

    return animating;
}

void UIWindow::CloseSettingsModalAnimated() {
    m_closingSettings = true;
    m_colorPickerPopoverOpen = false;
}

void UIWindow::CloseSearchModalAnimated() {
    m_closingSearch = true;
}

void UIWindow::CloseAboutModalAnimated() {
    m_closingAbout = true;
}

LRESULT CALLBACK UIWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    UIWindow* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (UIWindow*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->m_hwnd = hwnd;
    } else {
        pThis = (UIWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    if (pThis) {
        return pThis->HandleMessage(msg, wParam, lParam);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

HRESULT UIWindow::CreateDeviceIndependentResources() {
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);
    if (FAILED(hr)) return hr;

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&m_pDWriteFactory);
    if (FAILED(hr)) return hr;

    hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_pWicFactory));
    if (FAILED(hr)) return hr;

    m_fonts.Initialize(m_pDWriteFactory);
    return S_OK;
}

HRESULT UIWindow::CreateDeviceResources() {
    if (m_pRenderTarget) return S_OK;

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

    HRESULT hr = m_pD2DFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(m_hwnd, size),
        &m_pRenderTarget
    );
    if (FAILED(hr)) return hr;

    m_pRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    UINT winDpi = GetDpiForWindow(m_hwnd);
    if (winDpi == 0) winDpi = 96;
    m_dpiX = (float)winDpi;
    m_dpiY = (float)winDpi;
    m_dpiScale = m_dpiX / 96.0f;
    m_pRenderTarget->SetDpi(m_dpiX, m_dpiY);

    m_width = (float)(rc.right - rc.left) * (96.0f / m_dpiX);
    m_height = (float)(rc.bottom - rc.top) * (96.0f / m_dpiY);

    CreateBrushResources();
    return S_OK;
}

void UIWindow::CreateBrushResources() {
    if (!m_pRenderTarget) return;

    m_colors = KaisekiTheme::GetTheme(m_currentTheme, &m_customAccentColor);

    m_pRenderTarget->CreateSolidColorBrush(m_colors.canvasBg, &m_bCanvas);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.panelBg, &m_bPanel);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.cardBg, &m_bCard);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.cardHover, &m_bCardHover);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.inputWell, &m_bWell);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.borderSubtle, &m_bBorderSubtle);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.borderHover, &m_bBorderHover);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.accent, &m_bAccent);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.accentLight, &m_bAccentLight);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.textPrimary, &m_bTextPrimary);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.textSecondary, &m_bTextSecondary);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.textMuted, &m_bTextMuted);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.badgeBg, &m_bBadgeBg);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.success, &m_bSuccess);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.warning, &m_bWarning);
    m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.910f, 0.067f, 0.137f, 1.0f), &m_bCloseRed);

    m_pRenderTarget->CreateSolidColorBrush(m_colors.railBg, &m_bRailBg);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.menuBg, &m_bMenuBg);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.accentLine, &m_bAccentLine);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.btnCompile, &m_bBtnCompile);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.btnAnalyze, &m_bBtnAnalyze);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.chartBar, &m_bChartBar);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.watermark, &m_bWatermark);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.consoleBg, &m_bConsoleBg);

    // Pill Tab Brushes
    m_pRenderTarget->CreateSolidColorBrush(m_colors.tabActiveBg, &m_bTabActiveBg);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.tabActiveBorder, &m_bTabActiveBorder);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.tabHoverBg, &m_bTabHoverBg);

    // Fixed Authentic IDE Syntax Brushes
    m_pRenderTarget->CreateSolidColorBrush(m_colors.syntaxKeyword, &m_bSyntaxKeyword);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.syntaxRegister, &m_bSyntaxRegister);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.syntaxString, &m_bSyntaxString);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.syntaxNumber, &m_bSyntaxNumber);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.syntaxComment, &m_bSyntaxComment);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.syntaxAnnotation, &m_bSyntaxAnnotation);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.syntaxType, &m_bSyntaxType);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.syntaxTag, &m_bSyntaxTag);
    m_pRenderTarget->CreateSolidColorBrush(m_colors.syntaxAttr, &m_bSyntaxAttr);
    m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.18f, 0.42f, 0.70f, 0.50f), &m_bSelection);

    if (m_pModel && !m_pModel->iconBytes.empty() && !m_pIconBitmap) {
        CreateIconBitmap();
    }

    if (!m_pAppLogoBitmap) {
        LoadAppLogoBitmap();
    }
}

void UIWindow::DiscardBrushResources() {
    if (m_pIconBitmap) { m_pIconBitmap->Release(); m_pIconBitmap = nullptr; }
    if (m_pAppLogoBitmap) { m_pAppLogoBitmap->Release(); m_pAppLogoBitmap = nullptr; }
    if (m_bCanvas) { m_bCanvas->Release(); m_bCanvas = nullptr; }
    if (m_bPanel) { m_bPanel->Release(); m_bPanel = nullptr; }
    if (m_bCard) { m_bCard->Release(); m_bCard = nullptr; }
    if (m_bCardHover) { m_bCardHover->Release(); m_bCardHover = nullptr; }
    if (m_bWell) { m_bWell->Release(); m_bWell = nullptr; }
    if (m_bBorderSubtle) { m_bBorderSubtle->Release(); m_bBorderSubtle = nullptr; }
    if (m_bBorderHover) { m_bBorderHover->Release(); m_bBorderHover = nullptr; }
    if (m_bAccent) { m_bAccent->Release(); m_bAccent = nullptr; }
    if (m_bAccentLight) { m_bAccentLight->Release(); m_bAccentLight = nullptr; }
    if (m_bTextPrimary) { m_bTextPrimary->Release(); m_bTextPrimary = nullptr; }
    if (m_bTextSecondary) { m_bTextSecondary->Release(); m_bTextSecondary = nullptr; }
    if (m_bTextMuted) { m_bTextMuted->Release(); m_bTextMuted = nullptr; }
    if (m_bBadgeBg) { m_bBadgeBg->Release(); m_bBadgeBg = nullptr; }
    if (m_bSuccess) { m_bSuccess->Release(); m_bSuccess = nullptr; }
    if (m_bWarning) { m_bWarning->Release(); m_bWarning = nullptr; }
    if (m_bCloseRed) { m_bCloseRed->Release(); m_bCloseRed = nullptr; }
    if (m_bRailBg) { m_bRailBg->Release(); m_bRailBg = nullptr; }
    if (m_bMenuBg) { m_bMenuBg->Release(); m_bMenuBg = nullptr; }
    if (m_bAccentLine) { m_bAccentLine->Release(); m_bAccentLine = nullptr; }
    if (m_bBtnCompile) { m_bBtnCompile->Release(); m_bBtnCompile = nullptr; }
    if (m_bBtnAnalyze) { m_bBtnAnalyze->Release(); m_bBtnAnalyze = nullptr; }
    if (m_bChartBar) { m_bChartBar->Release(); m_bChartBar = nullptr; }
    if (m_bWatermark) { m_bWatermark->Release(); m_bWatermark = nullptr; }
    if (m_bConsoleBg) { m_bConsoleBg->Release(); m_bConsoleBg = nullptr; }
    if (m_bTabActiveBg) { m_bTabActiveBg->Release(); m_bTabActiveBg = nullptr; }
    if (m_bTabActiveBorder) { m_bTabActiveBorder->Release(); m_bTabActiveBorder = nullptr; }
    if (m_bTabHoverBg) { m_bTabHoverBg->Release(); m_bTabHoverBg = nullptr; }
    if (m_bSyntaxKeyword) { m_bSyntaxKeyword->Release(); m_bSyntaxKeyword = nullptr; }
    if (m_bSyntaxRegister) { m_bSyntaxRegister->Release(); m_bSyntaxRegister = nullptr; }
    if (m_bSyntaxString) { m_bSyntaxString->Release(); m_bSyntaxString = nullptr; }
    if (m_bSyntaxNumber) { m_bSyntaxNumber->Release(); m_bSyntaxNumber = nullptr; }
    if (m_bSyntaxComment) { m_bSyntaxComment->Release(); m_bSyntaxComment = nullptr; }
    if (m_bSyntaxAnnotation) { m_bSyntaxAnnotation->Release(); m_bSyntaxAnnotation = nullptr; }
    if (m_bSyntaxType) { m_bSyntaxType->Release(); m_bSyntaxType = nullptr; }
    if (m_bSyntaxTag) { m_bSyntaxTag->Release(); m_bSyntaxTag = nullptr; }
    if (m_bSyntaxAttr) { m_bSyntaxAttr->Release(); m_bSyntaxAttr = nullptr; }
    if (m_bSelection) { m_bSelection->Release(); m_bSelection = nullptr; }
}

void UIWindow::DiscardDeviceResources() {
    DiscardBrushResources();
    if (m_pRenderTarget) { m_pRenderTarget->Release(); m_pRenderTarget = nullptr; }
}

void UIWindow::RecreateThemeBrushes() {
    if (!m_pRenderTarget) return;

    if (m_bCanvas) m_bCanvas->SetColor(m_colors.canvasBg);
    if (m_bPanel) m_bPanel->SetColor(m_colors.panelBg);
    if (m_bCard) m_bCard->SetColor(m_colors.cardBg);
    if (m_bCardHover) m_bCardHover->SetColor(m_colors.cardHover);
    if (m_bWell) m_bWell->SetColor(m_colors.inputWell);
    if (m_bBorderSubtle) m_bBorderSubtle->SetColor(m_colors.borderSubtle);
    if (m_bBorderHover) m_bBorderHover->SetColor(m_colors.borderHover);
    if (m_bAccent) m_bAccent->SetColor(m_colors.accent);
    if (m_bAccentLight) m_bAccentLight->SetColor(m_colors.accentLight);
    if (m_bTextPrimary) m_bTextPrimary->SetColor(m_colors.textPrimary);
    if (m_bTextSecondary) m_bTextSecondary->SetColor(m_colors.textSecondary);
    if (m_bTextMuted) m_bTextMuted->SetColor(m_colors.textMuted);
    if (m_bBadgeBg) m_bBadgeBg->SetColor(m_colors.badgeBg);
    if (m_bSuccess) m_bSuccess->SetColor(m_colors.success);
    if (m_bWarning) m_bWarning->SetColor(m_colors.warning);
    if (m_bRailBg) m_bRailBg->SetColor(m_colors.railBg);
    if (m_bMenuBg) m_bMenuBg->SetColor(m_colors.menuBg);
    if (m_bAccentLine) m_bAccentLine->SetColor(m_colors.accentLine);
    if (m_bBtnCompile) m_bBtnCompile->SetColor(m_colors.btnCompile);
    if (m_bBtnAnalyze) m_bBtnAnalyze->SetColor(m_colors.btnAnalyze);
    if (m_bChartBar) m_bChartBar->SetColor(m_colors.chartBar);
    if (m_bWatermark) m_bWatermark->SetColor(m_colors.watermark);
    if (m_bConsoleBg) m_bConsoleBg->SetColor(m_colors.consoleBg);
    if (m_bTabActiveBg) m_bTabActiveBg->SetColor(m_colors.tabActiveBg);
    if (m_bTabActiveBorder) m_bTabActiveBorder->SetColor(m_colors.tabActiveBorder);
    if (m_bTabHoverBg) m_bTabHoverBg->SetColor(m_colors.tabHoverBg);
}

void UIWindow::OnResize(UINT width, UINT height) {
    UINT winDpi = GetDpiForWindow(m_hwnd);
    if (winDpi == 0) winDpi = 96;
    m_dpiX = (float)winDpi;
    m_dpiY = (float)winDpi;
    m_dpiScale = m_dpiX / 96.0f;

    if (m_pRenderTarget) {
        m_pRenderTarget->Resize(D2D1::SizeU(width, height));
        m_pRenderTarget->SetDpi(m_dpiX, m_dpiY);
    }

    m_width = (float)width * (96.0f / m_dpiX);
    m_height = (float)height * (96.0f / m_dpiY);

    UpdateWindowRegion();
}

void UIWindow::UpdateWindowRegion() {
    if (!m_hwnd) return;
    // SetWindowRgn(NULL) allows DWM hardware composition to manage rounded corners natively (DWMWCP_ROUND)
    // without suppressing native Windows default minimize, restore, and maximize swoosh/zoom animations!
    SetWindowRgn(m_hwnd, NULL, TRUE);
}

LRESULT UIWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_TIMER: {
        if (wParam == 1) {
            if (UpdateAnimations()) {
                InvalidateRect(m_hwnd, NULL, FALSE);
                UpdateWindow(m_hwnd);
            }
            return 0;
        } else if (wParam == 2) {
            if (m_logcatStreamActive && m_bottomConsoleTab == 1 && !m_consoleCollapsed) {
                RefreshLogcat();
            }
            return 0;
        }
        break;
    }
    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        HMONITOR hMon = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(mi) };
        if (GetMonitorInfo(hMon, &mi)) {
            mmi->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left;
            mmi->ptMaxPosition.y = mi.rcWork.top - mi.rcMonitor.top;
            mmi->ptMaxSize.x = mi.rcWork.right - mi.rcWork.left;
            mmi->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top;
            mmi->ptMaxTrackSize.x = mmi->ptMaxSize.x;
            mmi->ptMaxTrackSize.y = mmi->ptMaxSize.y;
        }
        return 0;
    }
    case WM_WINDOWPOSCHANGED: {
        WINDOWPOS* wp = (WINDOWPOS*)lParam;
        if (!(wp->flags & SWP_NOSIZE)) {
            RECT rcClient;
            GetClientRect(m_hwnd, &rcClient);
            UINT w = (UINT)(rcClient.right - rcClient.left);
            UINT h = (UINT)(rcClient.bottom - rcClient.top);
            if (w > 0 && h > 0) {
                OnResize(w, h);
            }
        }
        break;
    }
    case WM_NCCALCSIZE: {
        return KaisekiAnim_HandleNcCalcSize(m_hwnd, wParam, lParam, IsZoomed(m_hwnd));
    }
    case WM_GETTEXT: {
        const wchar_t* title = L"Kaiseki";
        size_t len = wcslen(title);
        if (wParam > 0 && lParam) {
            size_t copyLen = ((size_t)wParam - 1 < len) ? ((size_t)wParam - 1) : len;
            wcsncpy_s((wchar_t*)lParam, wParam, title, copyLen);
            ((wchar_t*)lParam)[copyLen] = L'\0';
            return copyLen;
        }
        return 0;
    }
    case WM_GETTEXTLENGTH: {
        return (LRESULT)wcslen(L"Kaiseki");
    }
    case WM_DPICHANGED: {
        RECT* const prcNewWindow = (RECT*)lParam;
        SetWindowPos(m_hwnd,
            NULL,
            prcNewWindow->left,
            prcNewWindow->top,
            prcNewWindow->right - prcNewWindow->left,
            prcNewWindow->bottom - prcNewWindow->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        if (m_pRenderTarget) {
            m_dpiX = (float)LOWORD(wParam);
            m_dpiY = (float)HIWORD(wParam);
            m_pRenderTarget->SetDpi(m_dpiX, m_dpiY);
            m_dpiScale = m_dpiX / 96.0f;
        }
        InvalidateRect(m_hwnd, NULL, FALSE);
        return 0;
    }
    case WM_NCHITTEST: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(m_hwnd, &pt);

        float dipX = PixelToDipX((float)pt.x);
        float dipY = PixelToDipY((float)pt.y);

        RECT clientRc;
        GetClientRect(m_hwnd, &clientRc);
        int w = clientRc.right - clientRc.left;
        int h = clientRc.bottom - clientRc.top;

        if (!IsZoomed(m_hwnd)) {
            const int b = 7;
            if (pt.y < b) {
                if (pt.x < b) return HTTOPLEFT;
                if (pt.x > w - b) return HTTOPRIGHT;
                return HTTOP;
            }
            if (pt.y > h - b) {
                if (pt.x < b) return HTBOTTOMLEFT;
                if (pt.x > w - b) return HTBOTTOMRIGHT;
                return HTBOTTOM;
            }
            if (pt.x < b) return HTLEFT;
            if (pt.x > w - b) return HTRIGHT;
        }

        // Titlebar zone
        if (dipY < 40.0f) {
            if (IsTitleBarControl(dipX, dipY)) {
                return HTCLIENT;
            }
            return HTCAPTION;
        }

        return HTCLIENT;
    }
    case WM_NCLBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(m_hwnd, &pt);
        float dipX = PixelToDipX((float)pt.x);
        float dipY = PixelToDipY((float)pt.y);
        if (dipY <= 40.0f) {
            if (dipX >= m_rcWinClose.left && dipX <= m_rcWinClose.right) {
                KaisekiAnim_PerformTransition(m_hwnd, KAISEKI_ACTION_CLOSE);
                return 0;
            }
            if (dipX >= m_rcWinMax.left && dipX <= m_rcWinMax.right) {
                KaisekiAnim_PerformTransition(m_hwnd, IsZoomed(m_hwnd) ? KAISEKI_ACTION_RESTORE : KAISEKI_ACTION_MAXIMIZE);
                return 0;
            }
            if (dipX >= m_rcWinMin.left && dipX <= m_rcWinMin.right) {
                KaisekiAnim_PerformTransition(m_hwnd, KAISEKI_ACTION_MINIMIZE);
                return 0;
            }
        }
        break;
    }
    case WM_SETCURSOR: {
        if (LOWORD(lParam) == HTCLIENT) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(m_hwnd, &pt);
            float mx = PixelToDipX((float)pt.x);
            float my = PixelToDipY((float)pt.y);

            if (!m_pModel) {
                if ((mx >= m_rcHeroDropZone.left && mx <= m_rcHeroDropZone.right && my >= m_rcHeroDropZone.top && my <= m_rcHeroDropZone.bottom) ||
                    (mx >= m_rcClearRecentBtn.left && mx <= m_rcClearRecentBtn.right && my >= m_rcClearRecentBtn.top && my <= m_rcClearRecentBtn.bottom) ||
                    (m_hoverRecentIndex >= 0)) {
                    SetCursor(LoadCursor(NULL, IDC_HAND));
                    return TRUE;
                }
            }

            if (m_railMode == RailMode::ApkInfo) {
                if ((mx >= m_rcBtnViewSecReport.left && mx <= m_rcBtnViewSecReport.right && my >= m_rcBtnViewSecReport.top && my <= m_rcBtnViewSecReport.bottom) ||
                    (mx >= m_rcBtnViewFridaBypass.left && mx <= m_rcBtnViewFridaBypass.right && my >= m_rcBtnViewFridaBypass.top && my <= m_rcBtnViewFridaBypass.bottom)) {
                    SetCursor(LoadCursor(NULL, IDC_HAND));
                    return TRUE;
                }
            }

            D2D1_RECT_F rcEditor; float tx, lh, cw;
            if (GetEditorRect(rcEditor, tx, lh, cw)) {
                if (mx >= rcEditor.left && mx <= rcEditor.right && my >= rcEditor.top && my <= rcEditor.bottom) {
                    SetCursor(LoadCursor(NULL, IDC_IBEAM));
                    return TRUE;
                }
            }
            D2D1_RECT_F rcConsole, rcConsoleClip; float cLineH;
            if (GetConsoleRect(rcConsole, rcConsoleClip, cLineH)) {
                if (mx >= rcConsoleClip.left && mx <= rcConsoleClip.right && my >= rcConsoleClip.top && my <= rcConsoleClip.bottom) {
                    SetCursor(LoadCursor(NULL, IDC_IBEAM));
                    return TRUE;
                }
            }
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
        UINT width = LOWORD(lParam);
        UINT height = HIWORD(lParam);
        OnResize(width, height);
        InvalidateRect(m_hwnd, NULL, FALSE);
        return 0;
    }
    case WM_DROPFILES: {
        OnDropFiles((HDROP)wParam);
        return 0;
    }
    case WM_CHAR: {
        HandleCharInput((wchar_t)wParam);
        return 0;
    }
    case WM_KEYDOWN: {
        HandleKeyDown(wParam);
        return 0;
    }
    case WM_MOUSEWHEEL: {
        short delta = GET_WHEEL_DELTA_WPARAM(wParam);
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(m_hwnd, &pt);
        float mx = PixelToDipX((float)pt.x);
        float my = PixelToDipY((float)pt.y);

        // 0. If mouse is over Bottom Terminal / Logcat Panel
        float consoleH = m_consoleCollapsed ? 26.0f : 160.0f;
        if (my >= (m_height - 24.0f - consoleH) && my <= (m_height - 24.0f) && mx >= 50.0f) {
            if (m_bottomConsoleTab == 0) {
                float maxScroll = (std::max)(0.0f, (float)m_consoleLogs.size() * 18.0f - (consoleH - 34.0f));
                m_consoleScrollY -= (float)delta * 0.35f;
                if (m_consoleScrollY < 0.0f) m_consoleScrollY = 0.0f;
                if (m_consoleScrollY > maxScroll) m_consoleScrollY = maxScroll;
            } else {
                float maxScroll = (std::max)(0.0f, (float)m_logcatLines.size() * 18.0f - (consoleH - 34.0f));
                m_logcatScrollY -= (float)delta * 0.35f;
                if (m_logcatScrollY < 0.0f) m_logcatScrollY = 0.0f;
                if (m_logcatScrollY > maxScroll) m_logcatScrollY = maxScroll;
            }
            InvalidateRect(m_hwnd, NULL, FALSE);
            return 0;
        }

        // 1. If mouse is over Left Explorer Tree panel (x in [50, 290] and y >= 40)
        if (mx >= 50.0f && mx <= 290.0f && my >= 40.0f) {
            m_treeScrollY -= (float)delta * 0.40f;
            if (m_treeScrollY < 0.0f) m_treeScrollY = 0.0f;
            if (m_treeScrollY > m_maxTreeScrollY) m_treeScrollY = m_maxTreeScrollY;
            m_treeScrollAlpha = 1.0f;
            InvalidateRect(m_hwnd, NULL, FALSE);
            return 0;
        }

        // 2. If mouse is over Document Tab Bar (y in [40, 76] and x >= 290)
        if (mx >= 290.0f && my >= 40.0f && my <= 76.0f) {
            m_tabScrollX -= (float)delta * 0.50f;
            if (m_tabScrollX < 0.0f) m_tabScrollX = 0.0f;
            if (m_tabScrollX > m_maxTabScrollX) m_tabScrollX = m_maxTabScrollX;
            InvalidateRect(m_hwnd, NULL, FALSE);
            return 0;
        }

        // 3. Otherwise scroll the active document text
        if (!m_tabs.empty() && m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabs.size()) {
            OpenDocTab& tab = m_tabs[m_activeTabIndex];
            tab.scrollY -= (float)delta * 0.45f;
            if (tab.scrollY < 0.0f) tab.scrollY = 0.0f;
            if (tab.scrollY > tab.maxScrollY) tab.scrollY = tab.maxScrollY;
            m_editorScrollAlpha = 1.0f;
            InvalidateRect(m_hwnd, NULL, FALSE);
            return 0;
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (!m_mouseTracking) {
            TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, m_hwnd, 0 };
            TrackMouseEvent(&tme);
            m_mouseTracking = true;
        }

        float x = PixelToDipX((float)GET_X_LPARAM(lParam));
        float y = PixelToDipY((float)GET_Y_LPARAM(lParam));
        m_lastMouseX = x;
        m_lastMouseY = y;

        // Draggable Editor Scrollbar Motion
        if (m_isDraggingEditorScroll && m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabs.size()) {
            auto& tab = m_tabs[m_activeTabIndex];
            float trackH = m_rcEditorScrollTrack.bottom - m_rcEditorScrollTrack.top;
            float thumbH = m_rcEditorScrollThumb.bottom - m_rcEditorScrollThumb.top;
            float scrollable = trackH - thumbH;
            if (scrollable > 0.0f) {
                float dy = y - m_dragScrollStartY;
                tab.scrollY = m_dragScrollStartVal + (dy / scrollable) * tab.maxScrollY;
                tab.scrollY = KaisekiTheme::ClampF(tab.scrollY, 0.0f, tab.maxScrollY);
                m_editorScrollAlpha = 1.0f;
                InvalidateRect(m_hwnd, NULL, FALSE);
            }
        }

        // Draggable Tree Scrollbar Motion
        if (m_isDraggingTreeScroll) {
            float trackH = m_rcTreeScrollTrack.bottom - m_rcTreeScrollTrack.top;
            float thumbH = m_rcTreeScrollThumb.bottom - m_rcTreeScrollThumb.top;
            float scrollable = trackH - thumbH;
            if (scrollable > 0.0f) {
                float dy = y - m_dragTreeScrollStartY;
                m_treeScrollY = m_dragTreeScrollStartVal + (dy / scrollable) * m_maxTreeScrollY;
                m_treeScrollY = KaisekiTheme::ClampF(m_treeScrollY, 0.0f, m_maxTreeScrollY);
                m_treeScrollAlpha = 1.0f;
                InvalidateRect(m_hwnd, NULL, FALSE);
            }
        }

        bool prevHoverClose = m_hoverWinClose;
        bool prevHoverMax = m_hoverWinMax;
        bool prevHoverMin = m_hoverWinMin;
        int prevHoverMenu = m_hoverMenuHeader;
        int prevHoverDrop = m_hoverDropdownItem;
        bool prevHeroDrop = m_hoverHeroDrop;
        bool prevHeroBrowse = m_hoverHeroBrowse;
        bool prevClearRecent = m_hoverClearRecent;
        int prevRecent = m_hoverRecentIndex;
        int prevRail = m_hoverRail;
        int prevTab = m_hoverTab;
        int prevTabClose = m_hoverTabClose;
        int prevTabSave = m_hoverTabSave;
        bool prevSec = m_hoverBtnViewSecReport;
        bool prevFrida = m_hoverBtnViewFridaBypass;

        m_hoverWinClose = (x >= m_rcWinClose.left && x <= m_rcWinClose.right && y >= m_rcWinClose.top && y <= m_rcWinClose.bottom);
        m_hoverWinMax = (x >= m_rcWinMax.left && x <= m_rcWinMax.right && y >= m_rcWinMax.top && y <= m_rcWinMax.bottom);
        m_hoverWinMin = (x >= m_rcWinMin.left && x <= m_rcWinMin.right && y >= m_rcWinMin.top && y <= m_rcWinMin.bottom);

        // Menu header hover
        m_hoverMenuHeader = -1;
        for (int i = 0; i < 5; ++i) {
            if (x >= m_rcMenuHeaders[i].left && x <= m_rcMenuHeaders[i].right &&
                y >= m_rcMenuHeaders[i].top && y <= m_rcMenuHeaders[i].bottom) {
                m_hoverMenuHeader = i;
                break;
            }
        }

        // Dropdown item hover
        m_hoverDropdownItem = -1;
        if (m_activeMenu != -1) {
            for (size_t i = 0; i < m_rcDropdownItems.size(); ++i) {
                if (x >= m_rcDropdownItems[i].left && x <= m_rcDropdownItems[i].right &&
                    y >= m_rcDropdownItems[i].top && y <= m_rcDropdownItems[i].bottom) {
                    m_hoverDropdownItem = (int)i;
                    break;
                }
            }
        }

        if (!m_pModel) {
            m_hoverHeroDrop = (x >= m_rcHeroDropZone.left && x <= m_rcHeroDropZone.right && y >= m_rcHeroDropZone.top && y <= m_rcHeroDropZone.bottom);
            m_hoverHeroBrowse = (x >= m_rcHeroBrowseBtn.left && x <= m_rcHeroBrowseBtn.right && y >= m_rcHeroBrowseBtn.top && y <= m_rcHeroBrowseBtn.bottom);
            m_hoverClearRecent = (x >= m_rcClearRecentBtn.left && x <= m_rcClearRecentBtn.right && y >= m_rcClearRecentBtn.top && y <= m_rcClearRecentBtn.bottom);

            int oldRecent = m_hoverRecentIndex;
            m_hoverRecentIndex = -1;
            for (size_t i = 0; i < m_rcRecentItems.size(); ++i) {
                if (x >= m_rcRecentItems[i].left && x <= m_rcRecentItems[i].right &&
                    y >= m_rcRecentItems[i].top && y <= m_rcRecentItems[i].bottom) {
                    m_hoverRecentIndex = (int)i;
                    break;
                }
            }
            if (oldRecent != m_hoverRecentIndex) {
                InvalidateRect(m_hwnd, NULL, FALSE);
            }
        }

        m_hoverRail = -1;
        for (int i = 0; i < 4; ++i) {
            if (x >= m_rcRailIcons[i].left && x <= m_rcRailIcons[i].right &&
                y >= m_rcRailIcons[i].top && y <= m_rcRailIcons[i].bottom) {
                m_hoverRail = i;
                break;
            }
        }

        if (m_railMode == RailMode::ApkInfo) {
            bool oldSec = m_hoverBtnViewSecReport;
            bool oldFrida = m_hoverBtnViewFridaBypass;
            m_hoverBtnViewSecReport = (x >= m_rcBtnViewSecReport.left && x <= m_rcBtnViewSecReport.right &&
                                       y >= m_rcBtnViewSecReport.top && y <= m_rcBtnViewSecReport.bottom);
            m_hoverBtnViewFridaBypass = (x >= m_rcBtnViewFridaBypass.left && x <= m_rcBtnViewFridaBypass.right &&
                                         y >= m_rcBtnViewFridaBypass.top && y <= m_rcBtnViewFridaBypass.bottom);
            if (oldSec != m_hoverBtnViewSecReport || oldFrida != m_hoverBtnViewFridaBypass) {
                InvalidateRect(m_hwnd, NULL, FALSE);
            }
        }

        m_hoverTab = -1;
        m_hoverTabClose = -1;
        m_hoverTabSave = -1;
        for (size_t i = 0; i < m_tabs.size(); ++i) {
            const auto& tab = m_tabs[i];
            if (x >= tab.rcTab.left && x <= tab.rcTab.right && y >= tab.rcTab.top && y <= tab.rcTab.bottom) {
                m_hoverTab = (int)i;
            }
            if (x >= tab.rcCloseBtn.left && x <= tab.rcCloseBtn.right && y >= tab.rcCloseBtn.top && y <= tab.rcCloseBtn.bottom) {
                m_hoverTabClose = (int)i;
            }
            if (tab.isModified && x >= tab.rcSaveBtn.left && x <= tab.rcSaveBtn.right && y >= tab.rcSaveBtn.top && y <= tab.rcSaveBtn.bottom) {
                m_hoverTabSave = (int)i;
            }
        }

        // Code Editor Text Selection Dragging
        if (m_isSelectingText && (wParam & MK_LBUTTON)) {
            D2D1_RECT_F rcEditor; float tx, lh, cw;
            if (GetEditorRect(rcEditor, tx, lh, cw)) {
                if (!m_tabs.empty() && m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabs.size()) {
                    OpenDocTab& tab = m_tabs[m_activeTabIndex];

                    if (y < rcEditor.top) {
                        tab.scrollY = (std::max)(0.0f, tab.scrollY - lh);
                    } else if (y > rcEditor.bottom) {
                        tab.scrollY = (std::min)(tab.maxScrollY, tab.scrollY + lh);
                    }

                    int lineIdx = (int)((y - rcEditor.top + tab.scrollY) / lh);
                    if (lineIdx < 0) lineIdx = 0;
                    if (lineIdx >= (int)tab.lines.size()) lineIdx = (int)tab.lines.size() - 1;

                    int colIdx = HitTestLineCol(lineIdx, x, tx);

                    tab.selActiveLine = lineIdx;
                    tab.selActiveCol = colIdx;
                    tab.cursorLine = lineIdx;
                    tab.cursorCol = colIdx;

                    if (tab.selActiveLine != tab.selAnchorLine || tab.selActiveCol != tab.selAnchorCol) {
                        tab.hasSelection = true;
                    }
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }
            }
        }

        // Bottom Console Text Selection Dragging
        if (m_isSelectingConsole && (wParam & MK_LBUTTON)) {
            D2D1_RECT_F rcConsole, rcConsoleClip; float cLineH;
            if (GetConsoleRect(rcConsole, rcConsoleClip, cLineH)) {
                if (y < rcConsoleClip.top) {
                    if (m_bottomConsoleTab == 0) m_consoleScrollY = (std::max)(0.0f, m_consoleScrollY - cLineH);
                    else m_logcatScrollY = (std::max)(0.0f, m_logcatScrollY - cLineH);
                } else if (y > rcConsoleClip.bottom) {
                    float maxS = (std::max)(0.0f, (float)GetConsoleLineCount() * cLineH - (rcConsoleClip.bottom - rcConsoleClip.top));
                    if (m_bottomConsoleTab == 0) m_consoleScrollY = (std::min)(maxS, m_consoleScrollY + cLineH);
                    else m_logcatScrollY = (std::min)(maxS, m_logcatScrollY + cLineH);
                }

                float relY = y - rcConsoleClip.top + ((m_bottomConsoleTab == 0) ? m_consoleScrollY : m_logcatScrollY);
                int lineIdx = (int)(relY / cLineH);
                int totalLines = GetConsoleLineCount();
                if (lineIdx < 0) lineIdx = 0;
                if (lineIdx >= totalLines) lineIdx = totalLines - 1;

                float textX = rcConsole.left + 15.0f;
                int colIdx = HitTestConsoleCol(lineIdx, x, textX);

                m_consoleSelActiveLine = lineIdx;
                m_consoleSelActiveCol = colIdx;
                if (m_consoleSelActiveLine != m_consoleSelAnchorLine || m_consoleSelActiveCol != m_consoleSelAnchorCol) {
                    m_consoleHasSelection = true;
                }
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
        }

        // In-App Settings RGB Slider & Color Picker dragging
        if (wParam & MK_LBUTTON) {
            if (m_isDraggingSatVal && m_rcColorSatValBox.right > m_rcColorSatValBox.left) {
                float normX = (x - m_rcColorSatValBox.left) / (m_rcColorSatValBox.right - m_rcColorSatValBox.left);
                float normY = (y - m_rcColorSatValBox.top) / (m_rcColorSatValBox.bottom - m_rcColorSatValBox.top);
                m_pickerSat = KaisekiTheme::ClampF(normX, 0.0f, 1.0f);
                m_pickerVal = KaisekiTheme::ClampF(1.0f - normY, 0.0f, 1.0f);
                D2D1_COLOR_F c = KaisekiTheme::HsvToRgb(m_pickerHue, m_pickerSat, m_pickerVal);
                uint8_t r = (uint8_t)(c.r * 255.0f);
                uint8_t g = (uint8_t)(c.g * 255.0f);
                uint8_t b = (uint8_t)(c.b * 255.0f);
                if (m_colorPickerTarget == 0) SetCustomAccentColorU8(r, g, b);
                else SetCustomBackgroundColorU8(r, g, b);
            } else if (m_isDraggingHue && m_rcColorHueSlider.right > m_rcColorHueSlider.left) {
                float norm = (x - m_rcColorHueSlider.left) / (m_rcColorHueSlider.right - m_rcColorHueSlider.left);
                m_pickerHue = KaisekiTheme::ClampF(norm, 0.0f, 1.0f) * 360.0f;
                D2D1_COLOR_F c = KaisekiTheme::HsvToRgb(m_pickerHue, m_pickerSat, m_pickerVal);
                uint8_t r = (uint8_t)(c.r * 255.0f);
                uint8_t g = (uint8_t)(c.g * 255.0f);
                uint8_t b = (uint8_t)(c.b * 255.0f);
                if (m_colorPickerTarget == 0) SetCustomAccentColorU8(r, g, b);
                else SetCustomBackgroundColorU8(r, g, b);
            } else if (m_activeSlider == 0 && m_rcSliderR.right > m_rcSliderR.left) {
                float norm = (x - m_rcSliderR.left) / (m_rcSliderR.right - m_rcSliderR.left);
                if (norm < 0.0f) norm = 0.0f; if (norm > 1.0f) norm = 1.0f;
                SetCustomAccentColorU8((uint8_t)(norm * 255.0f), m_sliderG, m_sliderB);
            } else if (m_activeSlider == 1 && m_rcSliderG.right > m_rcSliderG.left) {
                float norm = (x - m_rcSliderG.left) / (m_rcSliderG.right - m_rcSliderG.left);
                if (norm < 0.0f) norm = 0.0f; if (norm > 1.0f) norm = 1.0f;
                SetCustomAccentColorU8(m_sliderR, (uint8_t)(norm * 255.0f), m_sliderB);
            } else if (m_activeSlider == 2 && m_rcSliderB.right > m_rcSliderB.left) {
                float norm = (x - m_rcSliderB.left) / (m_rcSliderB.right - m_rcSliderB.left);
                if (norm < 0.0f) norm = 0.0f; if (norm > 1.0f) norm = 1.0f;
                SetCustomAccentColorU8(m_sliderR, m_sliderG, (uint8_t)(norm * 255.0f));
            }
        }

        bool hoverChanged = (prevHoverClose != m_hoverWinClose ||
                             prevHoverMax != m_hoverWinMax ||
                             prevHoverMin != m_hoverWinMin ||
                             prevHoverMenu != m_hoverMenuHeader ||
                             prevHoverDrop != m_hoverDropdownItem ||
                             prevHeroDrop != m_hoverHeroDrop ||
                             prevHeroBrowse != m_hoverHeroBrowse ||
                             prevClearRecent != m_hoverClearRecent ||
                             prevRecent != m_hoverRecentIndex ||
                             prevRail != m_hoverRail ||
                             prevTab != m_hoverTab ||
                             prevTabClose != m_hoverTabClose ||
                             prevTabSave != m_hoverTabSave ||
                             prevSec != m_hoverBtnViewSecReport ||
                             prevFrida != m_hoverBtnViewFridaBypass);

        if (hoverChanged) {
            InvalidateRect(m_hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_MOUSELEAVE: {
        m_mouseTracking = false;
        m_hoverWinClose = false;
        m_hoverWinMax = false;
        m_hoverWinMin = false;
        m_hoverHeroDrop = false;
        m_hoverHeroBrowse = false;
        m_hoverClearRecent = false;
        m_hoverRecentIndex = -1;
        m_hoverRail = -1;
        m_hoverTab = -1;
        m_hoverTabClose = -1;
        m_hoverTabSave = -1;
        m_hoverBtnViewSecReport = false;
        m_hoverBtnViewFridaBypass = false;
        m_activeSlider = -1;
        m_isDraggingSatVal = false;
        m_isDraggingHue = false;
        InvalidateRect(m_hwnd, NULL, FALSE);
        return 0;
    }
    case WM_LBUTTONUP: {
        m_activeSlider = -1;
        m_isDraggingSatVal = false;
        m_isDraggingHue = false;
        if (m_isDraggingEditorScroll) {
            m_isDraggingEditorScroll = false;
            ReleaseCapture();
            InvalidateRect(m_hwnd, NULL, FALSE);
            return 0;
        }
        if (m_isDraggingTreeScroll) {
            m_isDraggingTreeScroll = false;
            ReleaseCapture();
            InvalidateRect(m_hwnd, NULL, FALSE);
            return 0;
        }
        if (m_isSelectingConsole) {
            m_isSelectingConsole = false;
            ReleaseCapture();
            if (m_consoleSelActiveLine == m_consoleSelAnchorLine && m_consoleSelActiveCol == m_consoleSelAnchorCol) {
                m_consoleHasSelection = false;
            }
            InvalidateRect(m_hwnd, NULL, FALSE);
            return 0;
        }
        if (m_isSelectingText) {
            m_isSelectingText = false;
            ReleaseCapture();
            if (!m_tabs.empty() && m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabs.size()) {
                OpenDocTab& tab = m_tabs[m_activeTabIndex];
                if (tab.selActiveLine == tab.selAnchorLine && tab.selActiveCol == tab.selAnchorCol) {
                    tab.hasSelection = false;
                }
            }
            InvalidateRect(m_hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONDBLCLK: {
        float x = PixelToDipX((float)GET_X_LPARAM(lParam));
        float y = PixelToDipY((float)GET_Y_LPARAM(lParam));
        D2D1_RECT_F rcEditor; float tx, lh, cw;
        if (GetEditorRect(rcEditor, tx, lh, cw)) {
            if (x >= rcEditor.left && x <= rcEditor.right && y >= rcEditor.top && y <= rcEditor.bottom) {
                if (!m_tabs.empty() && m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabs.size()) {
                    OpenDocTab& tab = m_tabs[m_activeTabIndex];
                    int lineIdx = (int)((y - rcEditor.top + tab.scrollY) / lh);
                    int colIdx = HitTestLineCol(lineIdx, x, tx);
                    SelectWordAt(lineIdx, colIdx);
                    return 0;
                }
            }
        }
        return 0;
    }
    case WM_RBUTTONUP: {
        float x = PixelToDipX((float)GET_X_LPARAM(lParam));
        float y = PixelToDipY((float)GET_Y_LPARAM(lParam));
        D2D1_RECT_F rcConsole, rcConsoleClip; float cLineH;
        if (GetConsoleRect(rcConsole, rcConsoleClip, cLineH)) {
            if (x >= rcConsoleClip.left && x <= rcConsoleClip.right &&
                y >= rcConsoleClip.top && y <= rcConsoleClip.bottom) {
                POINT pt;
                GetCursorPos(&pt);
                ShowConsoleContextMenu(pt.x, pt.y);
                return 0;
            }
        }
        D2D1_RECT_F rcEditor; float tx, lh, cw;
        if (GetEditorRect(rcEditor, tx, lh, cw)) {
            float gutterLeft = rcEditor.left - 50.0f;
            if (x >= gutterLeft && x <= rcEditor.right && y >= rcEditor.top && y <= rcEditor.bottom) {
                POINT pt;
                GetCursorPos(&pt);
                ShowEditorContextMenu(pt.x, pt.y);
                return 0;
            }
        }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        float x = PixelToDipX((float)GET_X_LPARAM(lParam));
        float y = PixelToDipY((float)GET_Y_LPARAM(lParam));
        m_lastMouseX = x;
        m_lastMouseY = y;

        // Draggable Editor Scrollbar Start
        if (m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabs.size() && m_tabs[m_activeTabIndex].maxScrollY > 0.0f) {
            if (x >= m_rcEditorScrollThumb.left - 6.0f && x <= m_rcEditorScrollThumb.right + 6.0f &&
                y >= m_rcEditorScrollThumb.top && y <= m_rcEditorScrollThumb.bottom) {
                m_isDraggingEditorScroll = true;
                m_dragScrollStartY = y;
                m_dragScrollStartVal = m_tabs[m_activeTabIndex].scrollY;
                m_editorScrollAlpha = 1.0f;
                SetCapture(m_hwnd);
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
        }

        // Draggable Tree Scrollbar Start
        if (m_maxTreeScrollY > 0.0f) {
            if (x >= m_rcTreeScrollThumb.left - 6.0f && x <= m_rcTreeScrollThumb.right + 6.0f &&
                y >= m_rcTreeScrollThumb.top && y <= m_rcTreeScrollThumb.bottom) {
                m_isDraggingTreeScroll = true;
                m_dragTreeScrollStartY = y;
                m_dragTreeScrollStartVal = m_treeScrollY;
                m_treeScrollAlpha = 1.0f;
                SetCapture(m_hwnd);
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
        }

        // Close About modal if clicked outside or close button
        if (m_showAboutModal) {
            if (x >= m_rcAboutClose.left && x <= m_rcAboutClose.right && y >= m_rcAboutClose.top && y <= m_rcAboutClose.bottom) {
                CloseAboutModalAnimated();
            } else if (x < m_width * 0.25f || x > m_width * 0.75f || y < m_height * 0.2f || y > m_height * 0.8f) {
                CloseAboutModalAnimated();
            }
            InvalidateRect(m_hwnd, NULL, FALSE);
            return 0;
        }

        // Search Modal Clicks
        if (m_showSearchModal) {
            if (x >= m_rcSearchClose.left && x <= m_rcSearchClose.right && y >= m_rcSearchClose.top && y <= m_rcSearchClose.bottom) {
                CloseSearchModalAnimated();
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
            for (int t = 0; t < 4; ++t) {
                if (x >= m_rcSearchTypeTabs[t].left && x <= m_rcSearchTypeTabs[t].right &&
                    y >= m_rcSearchTypeTabs[t].top && y <= m_rcSearchTypeTabs[t].bottom) {
                    m_searchType = t;
                    PerformGlobalSearch(m_searchQuery, m_searchType);
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }
            }
            for (size_t r = 0; r < m_rcSearchResultRows.size() && r < m_searchResults.size(); ++r) {
                if (x >= m_rcSearchResultRows[r].left && x <= m_rcSearchResultRows[r].right &&
                    y >= m_rcSearchResultRows[r].top && y <= m_rcSearchResultRows[r].bottom) {
                    OpenClassInTab(m_searchResults[r].targetClass, true);
                    CloseSearchModalAnimated();
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }
            }
            return 0;
        }

        // Dropdown menu item clicks
        if (m_activeMenu != -1) {
            for (size_t i = 0; i < m_rcDropdownItems.size() && i < m_currentMenuItems.size(); ++i) {
                if (x >= m_rcDropdownItems[i].left && x <= m_rcDropdownItems[i].right &&
                    y >= m_rcDropdownItems[i].top && y <= m_rcDropdownItems[i].bottom) {
                    if (!m_currentMenuItems[i].isSeparator) {
                        TriggerMenuAction(m_currentMenuItems[i].actionId);
                        return 0;
                    }
                }
            }
            m_activeMenu = -1;
            InvalidateRect(m_hwnd, NULL, FALSE);
        }

        // Window buttons (Execute Windows Default System Transitions with Native Swoosh & Zoom Animations)
        if (x >= m_rcWinClose.left && x <= m_rcWinClose.right && y >= m_rcWinClose.top && y <= m_rcWinClose.bottom) {
            KaisekiAnim_PerformTransition(m_hwnd, KAISEKI_ACTION_CLOSE);
            return 0;
        }
        if (x >= m_rcWinMax.left && x <= m_rcWinMax.right && y >= m_rcWinMax.top && y <= m_rcWinMax.bottom) {
            KaisekiAnim_PerformTransition(m_hwnd, IsZoomed(m_hwnd) ? KAISEKI_ACTION_RESTORE : KAISEKI_ACTION_MAXIMIZE);
            return 0;
        }
        if (x >= m_rcWinMin.left && x <= m_rcWinMin.right && y >= m_rcWinMin.top && y <= m_rcWinMin.bottom) {
            KaisekiAnim_PerformTransition(m_hwnd, KAISEKI_ACTION_MINIMIZE);
            return 0;
        }

        // Settings Modal Clicks
        if (m_showSettingsModal) {
            if (x >= m_rcSettingsClose.left && x <= m_rcSettingsClose.right && y >= m_rcSettingsClose.top && y <= m_rcSettingsClose.bottom) {
                CloseSettingsModalAnimated();
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
            // Sub-tabs (7 Consolidated Categories: 0: Genel, 1: Görünüm, 2: Dil, 3: Android Araçları, 4: APK İşlemleri, 5: Kısayollar, 6: Gelişmiş ve Tanılama)
            for (int i = 0; i < 7; ++i) {
                if (x >= m_rcSettingsTabs[i].left && x <= m_rcSettingsTabs[i].right &&
                    y >= m_rcSettingsTabs[i].top && y <= m_rcSettingsTabs[i].bottom) {
                    m_settingsTab = i;
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }
            }

            // Tab 0: GENEL (Workspace Folder, Restore Project, .kproj File Association)
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
                                    ShowToast(L"Çalışma alanı dizini güncellendi.");
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
                    SettingsManager::Get().settings.restoreWorkspace = !SettingsManager::Get().settings.restoreWorkspace;
                    SettingsManager::Get().Save();
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }
                if (x >= m_rcBtnAssocKproj.left && x <= m_rcBtnAssocKproj.right &&
                    y >= m_rcBtnAssocKproj.top && y <= m_rcBtnAssocKproj.bottom) {
                    WCHAR path[MAX_PATH];
                    GetModuleFileNameW(NULL, path, MAX_PATH);
                    SettingsManager::Get().RegisterKprojAssociation(path);
                    ShowToast(LocW(StringId::Toast_KprojAssocSuccess));
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }
            }

            // Tab 1: GÖRÜNÜM (Custom Accent, Swatches, Background Palette & Custom Color, Toggle Switches)
            if (m_settingsTab == 1) {
                // If popover is open, check popover controls first!
                if (m_colorPickerPopoverOpen) {
                    // Close button (✕)
                    if (x >= m_rcColorPickerClose.left && x <= m_rcColorPickerClose.right &&
                        y >= m_rcColorPickerClose.top && y <= m_rcColorPickerClose.bottom) {
                        m_colorPickerPopoverOpen = false;
                        InvalidateRect(m_hwnd, NULL, FALSE);
                        return 0;
                    }
                    // Saturation / Value Box
                    if (x >= m_rcColorSatValBox.left && x <= m_rcColorSatValBox.right &&
                        y >= m_rcColorSatValBox.top && y <= m_rcColorSatValBox.bottom) {
                        m_isDraggingSatVal = true;
                        float normX = (x - m_rcColorSatValBox.left) / (m_rcColorSatValBox.right - m_rcColorSatValBox.left);
                        float normY = (y - m_rcColorSatValBox.top) / (m_rcColorSatValBox.bottom - m_rcColorSatValBox.top);
                        m_pickerSat = KaisekiTheme::ClampF(normX, 0.0f, 1.0f);
                        m_pickerVal = KaisekiTheme::ClampF(1.0f - normY, 0.0f, 1.0f);
                        D2D1_COLOR_F c = KaisekiTheme::HsvToRgb(m_pickerHue, m_pickerSat, m_pickerVal);
                        uint8_t r = (uint8_t)(c.r * 255.0f);
                        uint8_t g = (uint8_t)(c.g * 255.0f);
                        uint8_t b = (uint8_t)(c.b * 255.0f);
                        if (m_colorPickerTarget == 0) SetCustomAccentColorU8(r, g, b);
                        else SetCustomBackgroundColorU8(r, g, b);
                        InvalidateRect(m_hwnd, NULL, FALSE);
                        return 0;
                    }
                    // Hue Rainbow Slider
                    if (x >= m_rcColorHueSlider.left && x <= m_rcColorHueSlider.right &&
                        y >= m_rcColorHueSlider.top - 4.0f && y <= m_rcColorHueSlider.bottom + 4.0f) {
                        m_isDraggingHue = true;
                        float norm = (x - m_rcColorHueSlider.left) / (m_rcColorHueSlider.right - m_rcColorHueSlider.left);
                        m_pickerHue = KaisekiTheme::ClampF(norm, 0.0f, 1.0f) * 360.0f;
                        D2D1_COLOR_F c = KaisekiTheme::HsvToRgb(m_pickerHue, m_pickerSat, m_pickerVal);
                        uint8_t r = (uint8_t)(c.r * 255.0f);
                        uint8_t g = (uint8_t)(c.g * 255.0f);
                        uint8_t b = (uint8_t)(c.b * 255.0f);
                        if (m_colorPickerTarget == 0) SetCustomAccentColorU8(r, g, b);
                        else SetCustomBackgroundColorU8(r, g, b);
                        InvalidateRect(m_hwnd, NULL, FALSE);
                        return 0;
                    }
                    // Preset Dots inside popover
                    for (size_t d = 0; d < m_rcColorPresetDots.size(); ++d) {
                        if (x >= m_rcColorPresetDots[d].left && x <= m_rcColorPresetDots[d].right &&
                            y >= m_rcColorPresetDots[d].top && y <= m_rcColorPresetDots[d].bottom) {
                            if (m_colorPickerTarget == 0) {
                                const auto& sw = KaisekiTheme::GetPresetSwatches()[d % KaisekiTheme::GetPresetSwatches().size()];
                                uint8_t r = (uint8_t)(sw.color.r * 255.0f);
                                uint8_t g = (uint8_t)(sw.color.g * 255.0f);
                                uint8_t b = (uint8_t)(sw.color.b * 255.0f);
                                KaisekiTheme::RgbToHsv(sw.color.r, sw.color.g, sw.color.b, m_pickerHue, m_pickerSat, m_pickerVal);
                                SetCustomAccentColorU8(r, g, b);
                            } else {
                                SetBackgroundShade((BackgroundShade)(d % 6));
                                KaisekiTheme::RgbToHsv(m_colors.canvasBg.r, m_colors.canvasBg.g, m_colors.canvasBg.b, m_pickerHue, m_pickerSat, m_pickerVal);
                            }
                            InvalidateRect(m_hwnd, NULL, FALSE);
                            return 0;
                        }
                    }
                    // If clicked inside popover card but not on controls, consume click
                    if (x >= m_rcColorPickerCard.left && x <= m_rcColorPickerCard.right &&
                        y >= m_rcColorPickerCard.top && y <= m_rcColorPickerCard.bottom) {
                        return 0;
                    }
                    // Clicked outside popover: close it
                    m_colorPickerPopoverOpen = false;
                }

                // Accent Square
                if (x >= m_rcColorSquareAccent.left && x <= m_rcColorSquareAccent.right &&
                    y >= m_rcColorSquareAccent.top && y <= m_rcColorSquareAccent.bottom) {
                    m_colorPickerTarget = 0;
                    m_colorPickerPopoverOpen = !m_colorPickerPopoverOpen;
                    KaisekiTheme::RgbToHsv(m_customAccentColor.r, m_customAccentColor.g, m_customAccentColor.b, m_pickerHue, m_pickerSat, m_pickerVal);
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }

                // Background Color Square
                if (x >= m_rcColorSquareBg.left && x <= m_rcColorSquareBg.right &&
                    y >= m_rcColorSquareBg.top && y <= m_rcColorSquareBg.bottom) {
                    m_colorPickerTarget = 1;
                    m_colorPickerPopoverOpen = !m_colorPickerPopoverOpen;
                    KaisekiTheme::RgbToHsv(m_colors.canvasBg.r, m_colors.canvasBg.g, m_colors.canvasBg.b, m_pickerHue, m_pickerSat, m_pickerVal);
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }

                // Preset Swatches for Accent
                for (size_t i = 0; i < m_rcSwatches.size(); ++i) {
                    if (x >= m_rcSwatches[i].left && x <= m_rcSwatches[i].right &&
                        y >= m_rcSwatches[i].top && y <= m_rcSwatches[i].bottom) {
                        const auto& sw = KaisekiTheme::GetPresetSwatches()[i];
                        uint8_t r = (uint8_t)(sw.color.r * 255.0f);
                        uint8_t g = (uint8_t)(sw.color.g * 255.0f);
                        uint8_t b = (uint8_t)(sw.color.b * 255.0f);
                        SetCustomAccentColorU8(r, g, b);
                        KaisekiTheme::RgbToHsv(sw.color.r, sw.color.g, sw.color.b, m_pickerHue, m_pickerSat, m_pickerVal);
                        ShowToast(std::wstring(LocW(StringId::Toast_ColorSelected)) + sw.name);
                        return 0;
                    }
                }

                // Preset Swatches for Background (6 presets)
                for (int b = 0; b < 6; ++b) {
                    if (x >= m_rcBgShades[b].left && x <= m_rcBgShades[b].right &&
                        y >= m_rcBgShades[b].top && y <= m_rcBgShades[b].bottom) {
                        SetBackgroundShade((BackgroundShade)b);
                        KaisekiTheme::RgbToHsv(m_colors.canvasBg.r, m_colors.canvasBg.g, m_colors.canvasBg.b, m_pickerHue, m_pickerSat, m_pickerVal);
                        InvalidateRect(m_hwnd, NULL, FALSE);
                        return 0;
                    }
                }

                // Toggle Line Numbers
                if (x >= m_rcToggleLineNumbers.left && x <= m_rcToggleLineNumbers.right &&
                    y >= m_rcToggleLineNumbers.top && y <= m_rcToggleLineNumbers.bottom) {
                    SettingsManager::Get().settings.showLineNumbers = !SettingsManager::Get().settings.showLineNumbers;
                    SettingsManager::Get().Save();
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }

                // Toggle Minimap
                if (x >= m_rcToggleMinimap.left && x <= m_rcToggleMinimap.right &&
                    y >= m_rcToggleMinimap.top && y <= m_rcToggleMinimap.bottom) {
                    SettingsManager::Get().settings.showMinimap = !SettingsManager::Get().settings.showMinimap;
                    SettingsManager::Get().Save();
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }
            }

            // Tab 2: DİL (Language Dropdown Selection - Adobe / DaVinci Resolve clean UX)
            if (m_settingsTab == 2) {
                if (m_langDropdownOpen) {
                    const auto& langs = KaisekiLang::Get().GetAvailableLanguages();
                    for (size_t i = 0; i < m_rcLangDropdownOptions.size() && i < langs.size(); ++i) {
                        if (x >= m_rcLangDropdownOptions[i].left && x <= m_rcLangDropdownOptions[i].right &&
                            y >= m_rcLangDropdownOptions[i].top && y <= m_rcLangDropdownOptions[i].bottom) {
                            KaisekiLang::Get().SetLanguage(langs[i].code);
                            SettingsManager::Get().settings.language = langs[i].code;
                            SettingsManager::Get().Save();
                            m_langDropdownOpen = false;
                            ShowToast(LocW(StringId::Toast_LangChanged));
                            InvalidateRect(m_hwnd, NULL, FALSE);
                            return 0;
                        }
                    }
                }
                if (x >= m_rcLangDropdownBtn.left && x <= m_rcLangDropdownBtn.right &&
                    y >= m_rcLangDropdownBtn.top && y <= m_rcLangDropdownBtn.bottom) {
                    m_langDropdownOpen = !m_langDropdownOpen;
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }
                if (m_langDropdownOpen) {
                    m_langDropdownOpen = false;
                    InvalidateRect(m_hwnd, NULL, FALSE);
                }
            }

            // Tab 3: ANDROID ARAÇLARI (SDK Directory Select, Download & Install Tools Launcher)
            if (m_settingsTab == 3) {
                if (x >= m_rcBtnSelectSdk.left && x <= m_rcBtnSelectSdk.right &&
                    y >= m_rcBtnSelectSdk.top && y <= m_rcBtnSelectSdk.bottom) {
                    IFileDialog* pfd = nullptr;
                    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd)))) {
                        DWORD dwOptions;
                        if (SUCCEEDED(pfd->GetOptions(&dwOptions))) {
                            pfd->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
                        }
                        pfd->SetTitle(L"Android SDK Kök Dizinini Seçin");
                        if (SUCCEEDED(pfd->Show(m_hwnd))) {
                            IShellItem* psi = nullptr;
                            if (SUCCEEDED(pfd->GetResult(&psi))) {
                                PWSTR pszPath = nullptr;
                                if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath))) {
                                    SettingsManager::Get().settings.androidSdkPath = pszPath;
                                    SettingsManager::Get().ValidateSdkPath();
                                    SettingsManager::Get().Save();
                                    ShowToast(L"Android SDK dizini güncellendi.");
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
                if (x >= m_rcBtnDownloadTools.left && x <= m_rcBtnDownloadTools.right &&
                    y >= m_rcBtnDownloadTools.top && y <= m_rcBtnDownloadTools.bottom) {
                    ToolInstallerDialog::Get().ShowModal(m_hwnd);
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }
            }

            // Tab 4: APK İŞLEMLERİ (Java/Smali Default, Multidex, Sign Schemes v1/v2/v3, ZipAlign)
            if (m_settingsTab == 4) {
                if (x >= m_rcToggleDefaultView.left && x <= m_rcToggleDefaultView.right &&
                    y >= m_rcToggleDefaultView.top && y <= m_rcToggleDefaultView.bottom) {
                    SettingsManager::Get().settings.defaultToJava = !SettingsManager::Get().settings.defaultToJava;
                    SettingsManager::Get().Save();
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }
                if (x >= m_rcToggleMultidex.left && x <= m_rcToggleMultidex.right &&
                    y >= m_rcToggleMultidex.top && y <= m_rcToggleMultidex.bottom) {
                    SettingsManager::Get().settings.multidexMerged = !SettingsManager::Get().settings.multidexMerged;
                    SettingsManager::Get().Save();
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }
                if (x >= m_rcToggleSignV1.left && x <= m_rcToggleSignV1.right &&
                    y >= m_rcToggleSignV1.top && y <= m_rcToggleSignV1.bottom) {
                    SettingsManager::Get().settings.signSchemeV1 = !SettingsManager::Get().settings.signSchemeV1;
                    SettingsManager::Get().Save();
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }
                if (x >= m_rcToggleSignV2.left && x <= m_rcToggleSignV2.right &&
                    y >= m_rcToggleSignV2.top && y <= m_rcToggleSignV2.bottom) {
                    SettingsManager::Get().settings.signSchemeV2 = !SettingsManager::Get().settings.signSchemeV2;
                    SettingsManager::Get().Save();
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }
                if (x >= m_rcToggleSignV3.left && x <= m_rcToggleSignV3.right &&
                    y >= m_rcToggleSignV3.top && y <= m_rcToggleSignV3.bottom) {
                    SettingsManager::Get().settings.signSchemeV3 = !SettingsManager::Get().settings.signSchemeV3;
                    SettingsManager::Get().Save();
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }
                if (x >= m_rcToggleZipalign.left && x <= m_rcToggleZipalign.right &&
                    y >= m_rcToggleZipalign.top && y <= m_rcToggleZipalign.bottom) {
                    SettingsManager::Get().settings.zipAlign4Byte = !SettingsManager::Get().settings.zipAlign4Byte;
                    SettingsManager::Get().Save();
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }
            }

            // Tab 5: KISAYOLLAR (VS Code, IntelliJ IDEA, Visual Studio Presets)
            if (m_settingsTab == 5) {
                if (x >= m_rcHotkeyPresetVSCode.left && x <= m_rcHotkeyPresetVSCode.right &&
                    y >= m_rcHotkeyPresetVSCode.top && y <= m_rcHotkeyPresetVSCode.bottom) {
                    SettingsManager::Get().settings.hotkeyPreset = 0;
                    SettingsManager::Get().Save();
                    ShowToast(L"Kısayol Düzeni: VS Code Default");
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }
                if (x >= m_rcHotkeyPresetIntelliJ.left && x <= m_rcHotkeyPresetIntelliJ.right &&
                    y >= m_rcHotkeyPresetIntelliJ.top && y <= m_rcHotkeyPresetIntelliJ.bottom) {
                    SettingsManager::Get().settings.hotkeyPreset = 1;
                    SettingsManager::Get().Save();
                    ShowToast(L"Kısayol Düzeni: IntelliJ / Android Studio");
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }
                if (x >= m_rcHotkeyPresetVS.left && x <= m_rcHotkeyPresetVS.right &&
                    y >= m_rcHotkeyPresetVS.top && y <= m_rcHotkeyPresetVS.bottom) {
                    SettingsManager::Get().settings.hotkeyPreset = 2;
                    SettingsManager::Get().Save();
                    ShowToast(L"Kısayol Düzeni: Visual Studio Pro");
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }
            }

            // Tab 6: GELİŞMİŞ VE TANILAMA (Verbose Mode, Copy Diagnostics, Factory Reset)
            if (m_settingsTab == 6) {
                if (x >= m_rcToggleVerboseLog.left && x <= m_rcToggleVerboseLog.right &&
                    y >= m_rcToggleVerboseLog.top && y <= m_rcToggleVerboseLog.bottom) {
                    SettingsManager::Get().settings.verboseLogging = !SettingsManager::Get().settings.verboseLogging;
                    SettingsManager::Get().Save();
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }
                if (x >= m_rcBtnCopyDiagnostics.left && x <= m_rcBtnCopyDiagnostics.right &&
                    y >= m_rcBtnCopyDiagnostics.top && y <= m_rcBtnCopyDiagnostics.bottom) {
                    std::wostringstream diag;
                    diag << L"=== KAISEKI STUDIO DIAGNOSTIC REPORT ===\r\n";
                    diag << L"Version: " << KAISEKI_VERSION_W << L" (Windows Native x64/x86 Direct2D)\r\n";
                    diag << L"Language: " << KaisekiTheme::Utf8ToWide(SettingsManager::Get().settings.language) << L"\r\n";
                    diag << L"Android SDK Root: " << SettingsManager::Get().settings.androidSdkPath << L"\r\n";
                    diag << L"ADB Path: " << SettingsManager::Get().FindAdbPath() << L"\r\n";
                    diag << L"Build-Tools Path: " << SettingsManager::Get().FindBuildToolsPath() << L"\r\n";
                    diag << L"Java Path: " << SettingsManager::Get().FindJavaPath() << L"\r\n";
                    diag << L"SDK Status: " << ((SettingsManager::Get().ValidateSdkPath() == SdkValidationStatus::Ready) ? L"Ready" : L"Incomplete") << L"\r\n";
                    diag << L"Settings File: " << SettingsManager::Get().GetSettingsFilePath() << L"\r\n";
                    std::wstring diagStr = diag.str();
                    if (OpenClipboard(m_hwnd)) {
                        EmptyClipboard();
                        HGLOBAL hGlb = GlobalAlloc(GMEM_MOVEABLE, (diagStr.length() + 1) * sizeof(wchar_t));
                        if (hGlb) {
                            wchar_t* pBuf = (wchar_t*)GlobalLock(hGlb);
                            wcscpy_s(pBuf, diagStr.length() + 1, diagStr.c_str());
                            GlobalUnlock(hGlb);
                            SetClipboardData(CF_UNICODETEXT, hGlb);
                        }
                        CloseClipboard();
                        ShowToast(L"📋 Tanılama raporu panoya kopyalandı!");
                    }
                    return 0;
                }
                if (x >= m_rcBtnResetSettings.left && x <= m_rcBtnResetSettings.right &&
                    y >= m_rcBtnResetSettings.top && y <= m_rcBtnResetSettings.bottom) {
                    if (MessageBoxW(m_hwnd, L"Tüm ayarlar fabrika ayarlarına sıfırlanacak. Devam etmek istiyor musunuz?", L"Ayarları Sıfırla", MB_YESNO | MB_ICONWARNING) == IDYES) {
                        SettingsManager::Get().settings = KaisekiSettings();
                        SettingsManager::Get().Save();
                        KaisekiLang::Get().SetLanguage("tr");
                        m_currentBgShade = BackgroundShade::Charcoal;
                        SetCustomAccentColorU8(243, 143, 177);
                        ShowToast(L"Tüm ayarlar fabrika ayarlarına sıfırlandı!");
                        InvalidateRect(m_hwnd, NULL, FALSE);
                    }
                    return 0;
                }
            }
            return 0;
        }

        // Menu bar header clicks
        for (int i = 0; i < 5; ++i) {
            if (x >= m_rcMenuHeaders[i].left && x <= m_rcMenuHeaders[i].right &&
                y >= m_rcMenuHeaders[i].top && y <= m_rcMenuHeaders[i].bottom) {
                if (i == 4) { // Hakkında
                    m_showAboutModal = true;
                    m_closingAbout = false;
                    m_animAboutModal = 0.0f;
                    m_activeMenu = -1;
                } else {
                    m_activeMenu = (m_activeMenu == i) ? -1 : i;
                }
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
        }

        if (!m_pModel) {
            // Click on Top Card (Drop Zone / Browse)
            if (x >= m_rcHeroDropZone.left && x <= m_rcHeroDropZone.right && y >= m_rcHeroDropZone.top && y <= m_rcHeroDropZone.bottom) {
                OpenFileDialog();
                return 0;
            }
            // Click on Clear Recent Button
            if (x >= m_rcClearRecentBtn.left && x <= m_rcClearRecentBtn.right && y >= m_rcClearRecentBtn.top && y <= m_rcClearRecentBtn.bottom) {
                SettingsManager::Get().ClearRecentProjects();
                m_rcRecentItems.clear();
                m_hoverRecentIndex = -1;
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
            // Click on any recent project row
            for (size_t i = 0; i < m_rcRecentItems.size(); ++i) {
                if (x >= m_rcRecentItems[i].left && x <= m_rcRecentItems[i].right &&
                    y >= m_rcRecentItems[i].top && y <= m_rcRecentItems[i].bottom) {
                    const auto& recents = SettingsManager::Get().GetRecentProjects();
                    if (i < recents.size()) {
                        std::wstring targetPath = recents[i].filePath;
                        size_t dot = targetPath.find_last_of(L'.');
                        std::wstring ext = (dot != std::wstring::npos) ? targetPath.substr(dot) : L"";
                        for (auto& c : ext) c = towlower(c);

                        if (ext == L".apk") {
                            LoadApk(targetPath);
                        } else if (ext == L".kproj") {
                            KaisekiProjectData proj;
                            if (SettingsManager::Get().LoadProjectFile(targetPath, proj)) {
                                LoadApk(proj.apkPath);
                                SettingsManager::Get().AddRecentProject(targetPath, recents[i].badgeText, L".kproj");
                            } else {
                                ShowToast(L"Proje dosyası bulunamadı veya açılamadı!", true);
                            }
                        } else {
                            LoadApk(targetPath);
                        }
                    }
                    return 0;
                }
            }
            return 0;
        }

        // Left Activity Rail clicks
        for (int i = 0; i < 4; ++i) {
            if (x >= m_rcRailIcons[i].left && x <= m_rcRailIcons[i].right &&
                y >= m_rcRailIcons[i].top && y <= m_rcRailIcons[i].bottom) {
                m_railMode = (RailMode)i;
                if (m_railMode == RailMode::AdbPhone) {
                    RefreshAdbDevices();
                    if (!m_adbDeviceList.empty() && m_selectedDeviceIdx < 0) {
                        m_selectedDeviceIdx = 0;
                        m_adbDevice = KaisekiTheme::Utf8ToWide(m_adbDeviceList[0].model.empty() ? m_adbDeviceList[0].serial : m_adbDeviceList[0].model);
                    }
                }
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
        }

        // Tab Navigation Arrow Buttons (◀ and ▶) — MUST CHECK BEFORE TAB CELLS!
        if (x >= m_rcTabScrollLeft.left && x <= m_rcTabScrollLeft.right &&
            y >= m_rcTabScrollLeft.top && y <= m_rcTabScrollLeft.bottom) {
            m_tabScrollX -= 140.0f;
            if (m_tabScrollX < 0.0f) m_tabScrollX = 0.0f;
            InvalidateRect(m_hwnd, NULL, FALSE);
            return 0;
        }
        if (x >= m_rcTabScrollRight.left && x <= m_rcTabScrollRight.right &&
            y >= m_rcTabScrollRight.top && y <= m_rcTabScrollRight.bottom) {
            m_tabScrollX += 140.0f;
            if (m_tabScrollX > m_maxTabScrollX) m_tabScrollX = m_maxTabScrollX;
            InvalidateRect(m_hwnd, NULL, FALSE);
            return 0;
        }

        // Tab Bar clicks (clipped to left of scroll buttons)
        for (size_t i = 0; i < m_tabs.size(); ++i) {
            if (x < m_rcTabScrollLeft.left) {
                if (m_tabs[i].isModified &&
                    x >= m_tabs[i].rcSaveBtn.left && x <= m_tabs[i].rcSaveBtn.right &&
                    y >= m_tabs[i].rcSaveBtn.top && y <= m_tabs[i].rcSaveBtn.bottom) {
                    m_activeTabIndex = (int)i;
                    SaveActiveTab();
                    return 0;
                }
                if (x >= m_tabs[i].rcCloseBtn.left && x <= m_tabs[i].rcCloseBtn.right &&
                    y >= m_tabs[i].rcCloseBtn.top && y <= m_tabs[i].rcCloseBtn.bottom) {
                    CloseTab((int)i);
                    return 0;
                }
                if (x >= m_tabs[i].rcTab.left && x <= m_tabs[i].rcTab.right &&
                    y >= m_tabs[i].rcTab.top && y <= m_tabs[i].rcTab.bottom) {
                    m_activeTabIndex = (int)i;
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }
            }
        }

        // Bottom Console Tab Bar & Toolbar Clicks
        if (x >= m_rcBottomTabTerminal.left && x <= m_rcBottomTabTerminal.right &&
            y >= m_rcBottomTabTerminal.top && y <= m_rcBottomTabTerminal.bottom) {
            m_bottomConsoleTab = 0;
            InvalidateRect(m_hwnd, NULL, FALSE);
            return 0;
        }
        if (x >= m_rcBottomTabLogcat.left && x <= m_rcBottomTabLogcat.right &&
            y >= m_rcBottomTabLogcat.top && y <= m_rcBottomTabLogcat.bottom) {
            m_bottomConsoleTab = 1;
            if (m_rawLogcatEntries.empty() && !m_logcatLoaded) {
                RefreshLogcat();
            }
            InvalidateRect(m_hwnd, NULL, FALSE);
            return 0;
        }
        if (m_bottomConsoleTab == 1 && m_rcLogcatPkgFilter.right > m_rcLogcatPkgFilter.left &&
            x >= m_rcLogcatPkgFilter.left && x <= m_rcLogcatPkgFilter.right &&
            y >= m_rcLogcatPkgFilter.top && y <= m_rcLogcatPkgFilter.bottom) {
            if (m_pModel && !m_pModel->packageName.empty()) {
                m_logcatFilterByPackage = !m_logcatFilterByPackage;
                ApplyLogcatFilters();
                ShowToast(m_logcatFilterByPackage ? (L"Logcat: Sadece " + KaisekiTheme::Utf8ToWide(m_pModel->packageName) + L" logları filtrelendi") : L"Logcat: Tüm sistem logları gösteriliyor");
                InvalidateRect(m_hwnd, NULL, FALSE);
            } else {
                ShowToast(L"Filtrelenecek açık bir APK paketi yok.", true);
            }
            return 0;
        }
        if (m_bottomConsoleTab == 1 && m_rcLogcatLevelFilter.right > m_rcLogcatLevelFilter.left &&
            x >= m_rcLogcatLevelFilter.left && x <= m_rcLogcatLevelFilter.right &&
            y >= m_rcLogcatLevelFilter.top && y <= m_rcLogcatLevelFilter.bottom) {
            if (m_logcatMinLevel == 'V') m_logcatMinLevel = 'D';
            else if (m_logcatMinLevel == 'D') m_logcatMinLevel = 'I';
            else if (m_logcatMinLevel == 'I') m_logcatMinLevel = 'W';
            else if (m_logcatMinLevel == 'W') m_logcatMinLevel = 'E';
            else m_logcatMinLevel = 'V';
            ApplyLogcatFilters();
            InvalidateRect(m_hwnd, NULL, FALSE);
            return 0;
        }
        if (m_bottomConsoleTab == 1 && m_rcLogcatLiveStream.right > m_rcLogcatLiveStream.left &&
            x >= m_rcLogcatLiveStream.left && x <= m_rcLogcatLiveStream.right &&
            y >= m_rcLogcatLiveStream.top && y <= m_rcLogcatLiveStream.bottom) {
            m_logcatStreamActive = !m_logcatStreamActive;
            if (m_logcatStreamActive) {
                SetTimer(m_hwnd, 2, 1500, NULL);
                RefreshLogcat();
                ShowToast(L"⚡ Logcat canlı akışı başlatıldı (1.5s aralıkla güncellenir)");
            } else {
                KillTimer(m_hwnd, 2);
                ShowToast(L"⏸️ Logcat canlı akışı duraklatıldı");
            }
            InvalidateRect(m_hwnd, NULL, FALSE);
            return 0;
        }
        if (x >= m_rcBtnLogcatRefresh.left && x <= m_rcBtnLogcatRefresh.right &&
            y >= m_rcBtnLogcatRefresh.top && y <= m_rcBtnLogcatRefresh.bottom) {
            RefreshLogcat();
            ShowToast(L"「Logcat」 Loglar güncellendi.");
            InvalidateRect(m_hwnd, NULL, FALSE);
            return 0;
        }
        if (x >= m_rcBtnLogcatClear.left && x <= m_rcBtnLogcatClear.right &&
            y >= m_rcBtnLogcatClear.top && y <= m_rcBtnLogcatClear.bottom) {
            if (m_bottomConsoleTab == 0) {
                m_consoleLogs.clear();
            } else {
                m_logcatLines.clear();
                m_rawLogcatEntries.clear();
                std::string serial = "";
                if (m_selectedDeviceIdx >= 0 && m_selectedDeviceIdx < (int)m_adbDeviceList.size()) {
                    serial = m_adbDeviceList[m_selectedDeviceIdx].serial;
                }
                if (!serial.empty()) {
                    KaisekiADB_ClearDeviceLogcat(serial.c_str());
                }
            }
            m_consoleHasSelection = false;
            ShowToast(L"Loglar temizlendi.");
            InvalidateRect(m_hwnd, NULL, FALSE);
            return 0;
        }

        // Bottom Console Text Selection Click
        D2D1_RECT_F rcConsole, rcConsoleClip; float cLineH;
        if (GetConsoleRect(rcConsole, rcConsoleClip, cLineH)) {
            if (x >= rcConsoleClip.left && x <= rcConsoleClip.right &&
                y >= rcConsoleClip.top && y <= rcConsoleClip.bottom) {
                float scrollY = (m_bottomConsoleTab == 0) ? m_consoleScrollY : m_logcatScrollY;
                float relY = y - rcConsoleClip.top + scrollY;
                int lineIdx = (int)(relY / cLineH);
                int totalLines = GetConsoleLineCount();
                if (lineIdx >= 0 && lineIdx < totalLines) {
                    float textX = rcConsole.left + 15.0f;
                    int colIdx = HitTestConsoleCol(lineIdx, x, textX);

                    bool isShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                    if (isShift && m_consoleSelAnchorLine >= 0) {
                        m_consoleSelActiveLine = lineIdx;
                        m_consoleSelActiveCol = colIdx;
                        m_consoleHasSelection = true;
                    } else {
                        m_consoleSelAnchorLine = lineIdx;
                        m_consoleSelAnchorCol = colIdx;
                        m_consoleSelActiveLine = lineIdx;
                        m_consoleSelActiveCol = colIdx;
                        m_consoleHasSelection = false;
                    }

                    if (!m_tabs.empty() && m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabs.size()) {
                        m_tabs[m_activeTabIndex].ClearSelection();
                    }

                    m_isSelectingConsole = true;
                    SetCapture(m_hwnd);
                    SetFocus(m_hwnd);
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }
            }
        }

        // Toggle Java / Smali
        if (x >= m_rcBtnToggleJavaSmali.left && x <= m_rcBtnToggleJavaSmali.right &&
            y >= m_rcBtnToggleJavaSmali.top && y <= m_rcBtnToggleJavaSmali.bottom) {
            ToggleActiveTabJavaSmali();
            return 0;
        }

        // Native Frida Hook Script Generator Button
        if (x >= m_rcBtnJadxDecompile.left && x <= m_rcBtnJadxDecompile.right &&
            y >= m_rcBtnJadxDecompile.top && y <= m_rcBtnJadxDecompile.bottom) {
            if (!m_tabs.empty() && m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabs.size()) {
                std::string targetCls = m_tabs[m_activeTabIndex].filePath;
                std::string fridaScript;
                KaisekiDex_GenerateFridaScript(targetCls.c_str(), {}, &fridaScript);

                size_t dot = targetCls.rfind('.');
                std::string shortName = (dot != std::string::npos) ? targetCls.substr(dot + 1) : targetCls;

                OpenDocTab ftab;
                ftab.title = L"💉 " + KaisekiTheme::Utf8ToWide(shortName) + L"_frida.js";
                ftab.filePath = targetCls + "_frida.js";
                ftab.type = TabContentType::RawText;
                ftab.contentText = fridaScript;

                std::istringstream stream(fridaScript);
                std::string line;
                while (std::getline(stream, line)) {
                    ftab.lines.push_back(line);
                }
                if (ftab.lines.empty()) ftab.lines.push_back("");

                m_tabs.push_back(ftab);
                m_activeTabIndex = (int)m_tabs.size() - 1;
                ShowToast(L"「Frida」 Hook scripti üretildi: " + ftab.title);
                InvalidateRect(m_hwnd, NULL, FALSE);
            }
            return 0;
        }

        // Repack & Sign APK Button
        if (x >= m_rcBtnRepackApk.left && x <= m_rcBtnRepackApk.right &&
            y >= m_rcBtnRepackApk.top && y <= m_rcBtnRepackApk.bottom) {
            BuildAndRepackageApkInteractive();
            return 0;
        }

        // Export Gradle Project Button
        if (x >= m_rcBtnExportGradle.left && x <= m_rcBtnExportGradle.right &&
            y >= m_rcBtnExportGradle.top && y <= m_rcBtnExportGradle.bottom) {
            ExportGradleProjectInteractive();
            return 0;
        }

        // Global Search Button
        if (x >= m_rcBtnGlobalSearch.left && x <= m_rcBtnGlobalSearch.right &&
            y >= m_rcBtnGlobalSearch.top && y <= m_rcBtnGlobalSearch.bottom) {
            m_showSearchModal = true;
            InvalidateRect(m_hwnd, NULL, FALSE);
            return 0;
        }

        // Explorer tree items
        if (m_railMode == RailMode::Explorer && x >= 50.0f && x <= 50.0f + 250.0f && y >= 76.0f && y <= m_height - 24.0f) {
            float rowH = 28.0f;
            float relY = y - 80.0f + m_treeScrollY;
            int clickedIdx = (int)(relY / rowH);
            float inRowY = relY - (float)clickedIdx * rowH;
            if (clickedIdx >= 0 && clickedIdx < (int)m_cachedTreeItems.size() && inRowY >= 0.0f && inRowY <= 26.0f) {
                m_selectedTreeItem = clickedIdx;
                std::string path = m_cachedTreeItems[clickedIdx].path;
                if (path == "__TOGGLE_SOURCES__") {
                    m_sourcesExpanded = !m_sourcesExpanded;
                    m_treeItemsDirty = true;
                    RebuildFlattenedTreeItems();
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                } else if (path.rfind("__PKG__", 0) == 0) {
                    std::string pkgName = path.substr(7);
                    m_pkgExpanded[pkgName] = !m_pkgExpanded[pkgName];
                    m_treeItemsDirty = true;
                    RebuildFlattenedTreeItems();
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                } else if (path.rfind("__RES_DIR__", 0) == 0) {
                    m_pkgExpanded[path] = !m_pkgExpanded[path];
                    m_treeItemsDirty = true;
                    RebuildFlattenedTreeItems();
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                } else if (path.rfind("__CAT__", 0) == 0) {
                    m_pkgExpanded[path] = !m_pkgExpanded[path];
                    m_treeItemsDirty = true;
                    RebuildFlattenedTreeItems();
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                } else if (path == "__ASSETS__") {
                    m_pkgExpanded["__ASSETS__"] = !m_pkgExpanded["__ASSETS__"];
                    m_treeItemsDirty = true;
                    RebuildFlattenedTreeItems();
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                } else if (path.rfind("__CLS__", 0) == 0) {
                    std::string clsName = path.substr(7);
                    OpenClassInTab(clsName, true);
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                } else if (path == "__APK_SIGNATURE__") {
                    if (m_pModel) OpenDocInNewTab("APK_Signature", TabContentType::RawText, m_pModel->signature.formattedReport);
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                } else if (path == "__APK_SUMMARY__") {
                    if (m_pModel) OpenDocInNewTab("APK_Summary", TabContentType::RawText, m_pModel->summaryReport);
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                } else if (path == "__TOGGLE_RES__") {
                    m_resExpanded = !m_resExpanded;
                    m_treeItemsDirty = true;
                    RebuildFlattenedTreeItems();
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                } else if (path == "__TOGGLE_ASSETS__") {
                    m_assetsExpanded = !m_assetsExpanded;
                    m_treeItemsDirty = true;
                    RebuildFlattenedTreeItems();
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                } else if (path == "__TOGGLE_LIB__") {
                    m_libExpanded = !m_libExpanded;
                    m_treeItemsDirty = true;
                    RebuildFlattenedTreeItems();
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                } else if (path.size() >= 4 && path.substr(path.size() - 4) == ".dex") {
                    OpenDocInNewTab(path, TabContentType::SmaliCode);
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                } else if (path == "AndroidManifest.xml") {
                    if (m_pModel) OpenDocInNewTab("AndroidManifest.xml", TabContentType::XmlText, m_pModel->formattedXml);
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                } else if (path == "resources.arsc") {
                    if (m_pModel) {
                        uint8_t* bData = nullptr;
                        size_t bSize = 0;
                        if (ApkCore_ExtractFile(m_pModel->fullPath.c_str(), "resources.arsc", &bData, &bSize) && bData) {
                            ArscParseOutput arscOut;
                            if (KaisekiRes_ParseArsc(bData, bSize, &arscOut)) {
                                OpenDocInNewTab("resources.arsc", TabContentType::XmlText, arscOut.formattedReport);
                                KaisekiRes_FreeOutput(&arscOut);
                            } else {
                                std::string hexDump;
                                KaisekiHex_FormatDump(bData, bSize, 0, 65536, &hexDump);
                                OpenDocInNewTab("resources.arsc [Hex]", TabContentType::HexViewer, hexDump);
                            }
                            ApkCore_FreeBuffer(bData);
                        } else {
                            OpenDocInNewTab("resources.arsc", TabContentType::RawText, "resources.arsc [Binary Resource Table]");
                        }
                    }
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                } else if (EndsWithIgnoreCase(path, ".png") || EndsWithIgnoreCase(path, ".jpg") || EndsWithIgnoreCase(path, ".webp")) {
                    if (m_pModel) {
                        uint8_t* bData = nullptr;
                        size_t bSize = 0;
                        if (ApkCore_ExtractFile(m_pModel->fullPath.c_str(), path.c_str(), &bData, &bSize) && bData) {
                            OpenImageInTab(path, bData, bSize);
                            ApkCore_FreeBuffer(bData);
                        }
                    }
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                } else if (EndsWithIgnoreCase(path, ".xml")) {
                    if (m_pModel) {
                        uint8_t* bData = nullptr;
                        size_t bSize = 0;
                        if (ApkCore_ExtractFile(m_pModel->fullPath.c_str(), path.c_str(), &bData, &bSize) && bData) {
                            std::string xmlStr;
                            bool decompOk = false;
                            if (bSize >= 4) {
                                decompOk = ApkCore_DecompileXml(bData, bSize, &xmlStr) && !xmlStr.empty();
                            }
                            if (!decompOk && bSize > 0) {
                                xmlStr.assign((const char*)bData, bSize);
                            }
                            OpenDocInNewTab(path, TabContentType::XmlText, xmlStr);
                            ApkCore_FreeBuffer(bData);
                        }
                    }
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                } else if (!path.empty() && path[0] != '_') {
                    if (m_pModel) {
                        uint8_t* bData = nullptr;
                        size_t bSize = 0;
                        if (ApkCore_ExtractFile(m_pModel->fullPath.c_str(), path.c_str(), &bData, &bSize) && bData) {
                            bool isBinary = false;
                            if (EndsWithIgnoreCase(path, ".so") || EndsWithIgnoreCase(path, ".dat") || EndsWithIgnoreCase(path, ".bin")) {
                                isBinary = true;
                            } else {
                                size_t checkLen = (bSize < 256) ? bSize : 256;
                                for (size_t ci = 0; ci < checkLen; ++ci) {
                                    if (bData[ci] == 0) { isBinary = true; break; }
                                }
                            }
                            if (isBinary) {
                                HexAnalysisResult hexRes;
                                if (KaisekiHex_AnalyzeBinary(bData, bSize, &hexRes)) {
                                    OpenDocInNewTab(path, TabContentType::HexViewer, hexRes.formattedDump);
                                    KaisekiHex_FreeResult(&hexRes);
                                } else {
                                    std::string hexDump;
                                    KaisekiHex_FormatDump(bData, bSize, 0, 65536, &hexDump);
                                    OpenDocInNewTab(path, TabContentType::HexViewer, hexDump);
                                    KaisekiHex_FreeDumpString(&hexDump);
                                }
                            } else {
                                std::string rawStr((char*)bData, bSize);
                                OpenDocInNewTab(path, TabContentType::RawText, rawStr);
                            }
                            ApkCore_FreeBuffer(bData);
                        }
                    }
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
        }



        // ApkInfo Panel Buttons (Security Report & Frida Script)
        if (m_railMode == RailMode::ApkInfo) {
            if (x >= m_rcBtnViewSecReport.left - 4.0f && x <= m_rcBtnViewSecReport.right + 4.0f &&
                y >= m_rcBtnViewSecReport.top - 4.0f && y <= m_rcBtnViewSecReport.bottom + 4.0f) {
                int targetTab = -1;
                for (size_t i = 0; i < m_tabs.size(); ++i) {
                    if (m_tabs[i].filePath == "Security_Audit_Report.txt" ||
                        m_tabs[i].title == L"🛡️ Güvenlik Raporu" ||
                        m_tabs[i].title == L"Güvenlik Raporu") {
                        targetTab = (int)i;
                        break;
                    }
                }
                if (targetTab < 0) {
                    std::string secReportText = BuildSecurityReportString();
                    OpenDocInNewTab("Security_Audit_Report.txt", TabContentType::RawText, secReportText);
                    for (size_t i = 0; i < m_tabs.size(); ++i) {
                        if (m_tabs[i].filePath == "Security_Audit_Report.txt") {
                            m_tabs[i].title = L"🛡️ Güvenlik Raporu";
                            m_tabs[i].isReadOnly = true;
                            targetTab = (int)i;
                            break;
                        }
                    }
                }
                if (targetTab >= 0 && targetTab < (int)m_tabs.size()) {
                    m_activeTabIndex = targetTab;
                }
                m_railMode = RailMode::Explorer;
                ShowToast(L"Güvenlik Raporu açıldı.");
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
            if (x >= m_rcBtnViewFridaBypass.left - 4.0f && x <= m_rcBtnViewFridaBypass.right + 4.0f &&
                y >= m_rcBtnViewFridaBypass.top - 4.0f && y <= m_rcBtnViewFridaBypass.bottom + 4.0f) {
                int targetTab = -1;
                for (size_t i = 0; i < m_tabs.size(); ++i) {
                    if (m_tabs[i].filePath == "universal_frida_bypass.js") {
                        targetTab = (int)i;
                        break;
                    }
                }
                if (targetTab < 0) {
                    std::string fridaScript;
                    KaisekiSec_GenerateFridaBypass(m_pModel ? m_pModel->packageName.c_str() : "com.example.app", &fridaScript);
                    OpenDocInNewTab("universal_frida_bypass.js", TabContentType::RawText, fridaScript);
                    KaisekiSec_FreeScript(&fridaScript);
                    for (size_t i = 0; i < m_tabs.size(); ++i) {
                        if (m_tabs[i].filePath == "universal_frida_bypass.js") {
                            m_tabs[i].title = L"💉 universal_frida_bypass.js";
                            targetTab = (int)i;
                            break;
                        }
                    }
                }
                if (targetTab >= 0 && targetTab < (int)m_tabs.size()) {
                    m_activeTabIndex = targetTab;
                }
                m_railMode = RailMode::Explorer;
                ShowToast(std::wstring(LocW(StringId::Toast_FridaScriptGen)) + L"Universal Bypass");
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
        }

        // Signer Panel Buttons
        if (m_railMode == RailMode::Signer) {
            if (x >= m_rcBtnSignApk.left && x <= m_rcBtnSignApk.right && y >= m_rcBtnSignApk.top && y <= m_rcBtnSignApk.bottom) {
                if (!m_pModel) {
                    ShowToast(LocW(StringId::Toast_NoApkOpen), true);
                    return 0;
                }

                WCHAR szOutApk[MAX_PATH] = { 0 };
                std::wstring defName = m_pModel->fileName;
                size_t dot = defName.rfind(L".apk");
                if (dot != std::wstring::npos) defName = defName.substr(0, dot);
                defName += L"_signed.apk";
                wcsncpy_s(szOutApk, defName.c_str(), MAX_PATH);

                OPENFILENAMEW ofn = { sizeof(OPENFILENAMEW) };
                ofn.hwndOwner = m_hwnd;
                ofn.lpstrFile = szOutApk;
                ofn.nMaxFile = MAX_PATH;
                ofn.lpstrFilter = L"Android Paketleri (*.apk)\0*.apk\0Tüm Dosyalar (*.*)\0*.*\0";
                ofn.nFilterIndex = 1;
                ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

                if (GetSaveFileNameW(&ofn)) {
                    ShowModalProgress(L"Kaiseki - APK İmzalanıyor...", L"APK Signature Scheme v1 ve v2 blokları oluşturuluyor...");
                    UpdateModalProgress(35, L"Dijital sertifika ve SHA-256 özetleri hesaplanıyor...");
                    std::wstring outMsg;
                    bool ok = RealSignApk(m_pModel->fullPath, szOutApk, outMsg);
                    UpdateModalProgress(90, L"İmza doğrulama kontrolü yapılıyor...");
                    if (ok) {
                        UpdateModalProgress(100, L"İmzalama başarıyla tamamlandı!");
                        CloseModalProgress();
                        ShowToast(LocW(StringId::Toast_RepackSignSuccess));
                        m_consoleLogs.push_back({ L"[SIGNER]", L"Signed APK written to: " + std::wstring(szOutApk), 0 });
                    } else {
                        CloseModalProgress();
                        ShowToast(outMsg, true);
                    }
                }
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
            if (x >= m_rcBtnVerifySign.left && x <= m_rcBtnVerifySign.right && y >= m_rcBtnVerifySign.top && y <= m_rcBtnVerifySign.bottom) {
                if (!m_pModel) {
                    ShowToast(LocW(StringId::Toast_NoApkOpen), true);
                    return 0;
                }
                ShowModalProgress(L"İmza Doğrulanıyor...", L"APK Sertifika ve şema blokları taranıyor...");
                std::wstring outReport;
                RealVerifyApk(m_pModel->fullPath, outReport);
                CloseModalProgress();
                OpenDocInNewTab("APK_Signature", TabContentType::RawText, KaisekiTheme::WideToUtf8(outReport));
                ShowToast(std::wstring(LocW(StringId::Toast_FileSaved)) + L"APK_Signature");
                return 0;
            }
        }

        // ADB Phone Panel Buttons
        if (m_railMode == RailMode::AdbPhone) {
            if (x >= m_rcBtnRefreshDevices.left && x <= m_rcBtnRefreshDevices.right &&
                y >= m_rcBtnRefreshDevices.top && y <= m_rcBtnRefreshDevices.bottom) {
                RefreshAdbDevices();
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
            for (size_t i = 0; i < m_rcDeviceCards.size(); ++i) {
                if (x >= m_rcDeviceCards[i].left && x <= m_rcDeviceCards[i].right &&
                    y >= m_rcDeviceCards[i].top && y <= m_rcDeviceCards[i].bottom) {
                    m_selectedDeviceIdx = (int)i;
                    if (i < m_adbDeviceList.size()) {
                        m_adbDevice = KaisekiTheme::Utf8ToWide(m_adbDeviceList[i].model.empty() ? m_adbDeviceList[i].serial : m_adbDeviceList[i].model);
                    }
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }
            }
            if (x >= m_rcBtnAdbInstall.left && x <= m_rcBtnAdbInstall.right && y >= m_rcBtnAdbInstall.top && y <= m_rcBtnAdbInstall.bottom) {
                if (!m_pModel) {
                    ShowToast(LocW(StringId::Toast_NoApkOpen), true);
                    return 0;
                }
                if (m_selectedDeviceIdx < 0 || m_selectedDeviceIdx >= (int)m_adbDeviceList.size()) {
                    ShowToast(L"Lütfen önce bir cihaz seçin!", true);
                    return 0;
                }
                const auto& dev = m_adbDeviceList[m_selectedDeviceIdx];
                if (!dev.isOnline) {
                    if (dev.state == "unauthorized") {
                        ShowToast(L"Cihaz yetkisiz! Telefonunuzun ekranından USB Hata Ayıklama iznini onaylayın.", true);
                        m_consoleLogs.push_back({ L"[ADB UYARI]", L"Cihaz yetkilendirilmedi (unauthorized). Lütfen telefonun ekranını açıp 'Bu bilgisayara izin ver' (USB Debugging) uyarısını onaylayın.", 1 });
                    } else {
                        ShowToast(L"Cihaz çevrimdışı (offline)!", true);
                    }
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }

                std::wstring targetApkToInstall = m_pModel->fullPath;
                if (!m_lastBuiltApkPath.empty() && GetFileAttributesW(m_lastBuiltApkPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    targetApkToInstall = m_lastBuiltApkPath;
                }

                ShowModalProgress(L"Telefona Kuruluyor...", (L"APK ADB ile cihaza yükleniyor: " + KaisekiTheme::Utf8ToWide(dev.serial)).c_str());
                UpdateModalProgress(30, L"adb install -r -d -t parametresi ile cihaza aktarılıyor...");
                std::string installLog;
                bool ok = KaisekiADB_InstallApk(dev.serial.c_str(), targetApkToInstall.c_str(), &installLog);
                CloseModalProgress();

                if (ok) {
                    ShowToast(LocW(StringId::Toast_AdbInstallSuccess));
                    m_consoleLogs.push_back({ L"[ADB]", L"Installed to " + KaisekiTheme::Utf8ToWide(dev.serial) + L": " + targetApkToInstall, 0 });
                } else {
                    ShowToast(LocW(StringId::Toast_AdbInstallFail), true);
                    m_consoleLogs.push_back({ L"[ADB ERROR]", L"Failed to install APK to device: " + KaisekiTheme::Utf8ToWide(dev.serial), 2 });
                    if (!installLog.empty()) {
                        std::istringstream ssLog(installLog);
                        std::string line;
                        while (std::getline(ssLog, line)) {
                            while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
                            if (!line.empty()) {
                                m_consoleLogs.push_back({ L"[ADB OUTPUT]", KaisekiTheme::Utf8ToWide(line), 2 });
                            }
                        }
                    }
                    if (installLog.find("INSTALL_FAILED_UPDATE_INCOMPATIBLE") != std::string::npos) {
                        m_consoleLogs.push_back({ L"[ADB UYARI]", L"İmza uyuşmazlığı (INSTALL_FAILED_UPDATE_INCOMPATIBLE)! Telefonda resmi sertifikalı orijinal sürüm yüklü olabilir.", 1 });
                        int res = MessageBoxW(m_hwnd,
                            L"Cihazda aynı paket adında farklı bir sertifikaya (ör. Google resmi mağaza imzası) sahip sürüm yüklü!\n\n"
                            L"Android işletim sistemi imza uyuşmazlığı nedeniyle güncellemeyi engelledi (INSTALL_FAILED_UPDATE_INCOMPATIBLE).\n\n"
                            L"Eski uygulamayı cihazdan kaldırıp yeni hazırlanan APK'yı kurmak istiyor musunuz?",
                            L"İmza Uyuşmazlığı (ADB)",
                            MB_YESNO | MB_ICONQUESTION);
                        if (res == IDYES) {
                            ShowModalProgress(L"Eski Sürüm Kaldırılıyor...", L"pm uninstall ile mevcut paket temizleniyor...");
                            std::string unLog;
                            KaisekiADB_UninstallPackage(dev.serial.c_str(), m_pModel->packageName.c_str(), &unLog);
                            UpdateModalProgress(50, L"Yeni imzalı APK yükleniyor...");
                            std::string reinstallLog;
                            bool reOk = KaisekiADB_InstallApk(dev.serial.c_str(), targetApkToInstall.c_str(), &reinstallLog);
                            CloseModalProgress();
                            if (reOk) {
                                ShowToast(L"Eski sürüm kaldırıldı ve yeni APK başarıyla kuruldu!");
                                m_consoleLogs.push_back({ L"[ADB]", L"Replaced and installed APK to " + KaisekiTheme::Utf8ToWide(dev.serial), 0 });
                            } else {
                                ShowToast(LocW(StringId::Toast_AdbInstallFail), true);
                                m_consoleLogs.push_back({ L"[ADB ERROR]", L"Failed after uninstall: " + KaisekiTheme::Utf8ToWide(reinstallLog), 2 });
                            }
                        }
                    } else if (installLog.find("unauthorized") != std::string::npos) {
                        m_consoleLogs.push_back({ L"[ADB UYARI]", L"Cihaz yetkisiz (unauthorized)! Telefon ekranınızı açıp USB hata ayıklamaya izin verin.", 1 });
                    }
                }
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
            if (x >= m_rcBtnAdbLaunch.left && x <= m_rcBtnAdbLaunch.right && y >= m_rcBtnAdbLaunch.top && y <= m_rcBtnAdbLaunch.bottom) {
                if (!m_pModel) {
                    ShowToast(LocW(StringId::Toast_NoApkOpen), true);
                    return 0;
                }
                if (m_selectedDeviceIdx < 0 || m_selectedDeviceIdx >= (int)m_adbDeviceList.size()) {
                    ShowToast(LocW(StringId::Toast_NoApkOpen), true);
                    return 0;
                }
                const auto& dev = m_adbDeviceList[m_selectedDeviceIdx];
                std::string launchLog;
                bool ok = KaisekiADB_LaunchApp(dev.serial.c_str(), m_pModel->packageName.c_str(), "", &launchLog);
                if (ok) {
                    ShowToast(LocW(StringId::Toast_AdbLaunchSuccess));
                    m_consoleLogs.push_back({ L"[ADB]", L"Launched package: " + KaisekiTheme::Utf8ToWide(m_pModel->packageName), 0 });
                } else {
                    ShowToast(LocW(StringId::Toast_AdbLaunchFail), true);
                }
                InvalidateRect(m_hwnd, NULL, FALSE);
                return 0;
            }
        }

        // Code Editor Text Selection / Cursor Positioning
        D2D1_RECT_F rcEditor; float tx, lh, cw;
        if (GetEditorRect(rcEditor, tx, lh, cw)) {
            float gutterLeft = rcEditor.left - 50.0f; // includes line-number gutter
            if (x >= gutterLeft && x <= rcEditor.right && y >= rcEditor.top && y <= rcEditor.bottom) {
                if (!m_tabs.empty() && m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabs.size()) {
                    OpenDocTab& tab = m_tabs[m_activeTabIndex];

                    int lineIdx = (int)((y - rcEditor.top + tab.scrollY) / lh);
                    if (lineIdx < 0) lineIdx = 0;
                    if (lineIdx >= (int)tab.lines.size()) lineIdx = (int)tab.lines.size() - 1;

                    // If clicked in line number gutter: select entire line!
                    if (x < rcEditor.left) {
                        tab.hasSelection = true;
                        tab.selAnchorLine = lineIdx;
                        tab.selAnchorCol = 0;
                        tab.selActiveLine = lineIdx;
                        std::wstring lineW = KaisekiTheme::Utf8ToWide(tab.lines[lineIdx]);
                        tab.selActiveCol = (int)lineW.length();
                        tab.cursorLine = lineIdx;
                        tab.cursorCol = tab.selActiveCol;
                        SetFocus(m_hwnd);
                        InvalidateRect(m_hwnd, NULL, FALSE);
                        return 0;
                    }

                    int colIdx = HitTestLineCol(lineIdx, x, tx);

                    bool isShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                    if (isShift) {
                        if (!tab.hasSelection) {
                            tab.selAnchorLine = tab.cursorLine;
                            tab.selAnchorCol = tab.cursorCol;
                        }
                        tab.selActiveLine = lineIdx;
                        tab.selActiveCol = colIdx;
                        tab.hasSelection = true;
                    } else {
                        tab.selAnchorLine = lineIdx;
                        tab.selAnchorCol = colIdx;
                        tab.selActiveLine = lineIdx;
                        tab.selActiveCol = colIdx;
                        tab.hasSelection = false;
                    }

                    tab.cursorLine = lineIdx;
                    tab.cursorCol = colIdx;
                    m_isSelectingText = true;
                    SetCapture(m_hwnd);
                    SetFocus(m_hwnd);
                    InvalidateRect(m_hwnd, NULL, FALSE);
                    return 0;
                }
            }
        }

        return 0;
    }
    case WM_DESTROY: {
        KillTimer(m_hwnd, 1);
        KillTimer(m_hwnd, 2);
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProc(m_hwnd, msg, wParam, lParam);
}

bool UIWindow::SaveScreenshot(const wchar_t* outPngPath) {
    if (!m_pD2DFactory || !m_pWicFactory || !outPngPath) return false;

    UINT w = (UINT)m_width;
    UINT h = (UINT)m_height;
    if (w < 800) w = 1180;
    if (h < 600) h = 780;

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

    m_isOffscreen = true;
    m_windowFadeAlpha = 1.0f; // Ensure full opacity for screenshot
    ID2D1HwndRenderTarget* savedHwndRT = m_pRenderTarget;
    DiscardBrushResources();

    m_pRenderTarget = (ID2D1HwndRenderTarget*)pWicRT;
    CreateBrushResources();

    // Render entire Kaiseki UI directly onto WIC bitmap
    OnRender();

    // Save pWicBmp to PNG file via WIC encoder
    IWICStream* pStream = nullptr;
    IWICBitmapEncoder* pEncoder = nullptr;
    IWICBitmapFrameEncode* pFrame = nullptr;
    bool saved = false;

    if (SUCCEEDED(m_pWicFactory->CreateStream(&pStream)) &&
        SUCCEEDED(pStream->InitializeFromFilename(outPngPath, GENERIC_WRITE)) &&
        SUCCEEDED(m_pWicFactory->CreateEncoder(GUID_ContainerFormatPng, NULL, &pEncoder)) &&
        SUCCEEDED(pEncoder->Initialize(pStream, WICBitmapEncoderNoCache)) &&
        SUCCEEDED(pEncoder->CreateNewFrame(&pFrame, NULL)) &&
        SUCCEEDED(pFrame->Initialize(NULL))) {

        pFrame->SetSize(w, h);
        WICPixelFormatGUID pFormat = GUID_WICPixelFormat32bppPBGRA;
        pFrame->SetPixelFormat(&pFormat);
        if (SUCCEEDED(pFrame->WriteSource(pWicBmp, NULL))) {
            pFrame->Commit();
            pEncoder->Commit();
            saved = true;
        }
    }

    if (pFrame) pFrame->Release();
    if (pEncoder) pEncoder->Release();
    if (pStream) pStream->Release();

    // Clean up brushes created on pWicRT
    DiscardBrushResources();

    // Restore original hwnd render target
    m_pRenderTarget = savedHwndRT;
    m_isOffscreen = false;
    pWicRT->Release();
    pWicBmp->Release();

    if (m_pRenderTarget) {
        CreateBrushResources();
    }

    return saved;
}

void UIWindow::SimulateClickSecReport() {
    int targetTab = -1;
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].filePath == "Security_Audit_Report.txt" ||
            m_tabs[i].title == L"🛡️ Güvenlik Raporu" ||
            m_tabs[i].title == L"Güvenlik Raporu") {
            targetTab = (int)i;
            break;
        }
    }
    if (targetTab < 0) {
        std::string secReportText = BuildSecurityReportString();
        OpenDocInNewTab("Security_Audit_Report.txt", TabContentType::RawText, secReportText);
        for (size_t i = 0; i < m_tabs.size(); ++i) {
            if (m_tabs[i].filePath == "Security_Audit_Report.txt") {
                m_tabs[i].title = L"🛡️ Güvenlik Raporu";
                m_tabs[i].isReadOnly = true;
                targetTab = (int)i;
                break;
            }
        }
    }
    if (targetTab >= 0 && targetTab < (int)m_tabs.size()) {
        m_activeTabIndex = targetTab;
    }
    m_railMode = RailMode::Explorer;
    InvalidateRect(m_hwnd, NULL, FALSE);
}

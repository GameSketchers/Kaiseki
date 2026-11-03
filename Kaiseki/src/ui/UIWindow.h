#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>

#ifdef DrawText
#undef DrawText
#endif

#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wincodec.h>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include "KaisekiTheme.h"
#include "VectorIcons.h"
#include "FontManager.h"
#include "SettingsManager.h"
#include "KaisekiLang.h"
#include "ApkCoreApi.h"
#include "KaisekiDexApi.h"
#include "KaisekiSecApi.h"
#include "KaisekiDiffApi.h"
#include "KaisekiHexApi.h"
#include "KaisekiResApi.h"
#include "KaisekiAnimApi.h"
#include "KaisekiAdbApi.h"

enum class RailMode {
    Explorer = 0,   // Proje Gezgini & Editör
    ApkInfo = 1,    // APK Bilgisi & Güvenlik
    Signer = 2,     // APK İmzalama & Mühür
    AdbPhone = 3,   // Telefona Kur (ADB)
    Settings = 4    // Ayarlar & Renk Paleti
};

enum class TabContentType {
    SmaliCode,
    JavaCode,
    XmlText,
    ImagePreview,
    RawText,
    HexViewer
};

struct EditorSnapshot {
    std::vector<std::string> lines;
    int cursorLine = 0;
    int cursorCol = 0;
    bool hasSelection = false;
    int selAnchorLine = 0;
    int selAnchorCol = 0;
    int selActiveLine = 0;
    int selActiveCol = 0;
};

struct OpenDocTab {
    std::wstring title;
    std::string filePath;
    TabContentType type;
    std::string contentText;
    std::string javaAlternative;
    std::vector<std::string> lines;
    bool isModified = false;
    bool isJavaMode = false;
    bool isReadOnly = false;
    std::string sourceDex;

    // Image preview fields
    ID2D1Bitmap* pImageBitmap = nullptr;
    UINT imgWidth = 0;
    UINT imgHeight = 0;
    size_t imgBytes = 0;

    // View & scroll
    float scrollY = 0.0f;
    float maxScrollY = 0.0f;
    int cursorLine = 0;
    int cursorCol = 0;
    bool isEditorFocused = false;

    // Text selection
    bool hasSelection = false;
    int selAnchorLine = 0;
    int selAnchorCol = 0;
    int selActiveLine = 0;
    int selActiveCol = 0;

    // Undo / Redo Stacks
    std::vector<EditorSnapshot> undoStack;
    std::vector<EditorSnapshot> redoStack;

    void PushUndoState() {
        EditorSnapshot snap;
        snap.lines = lines;
        snap.cursorLine = cursorLine;
        snap.cursorCol = cursorCol;
        snap.hasSelection = hasSelection;
        snap.selAnchorLine = selAnchorLine;
        snap.selAnchorCol = selAnchorCol;
        snap.selActiveLine = selActiveLine;
        snap.selActiveCol = selActiveCol;
        undoStack.push_back(snap);
        if (undoStack.size() > 100) {
            undoStack.erase(undoStack.begin());
        }
        redoStack.clear();
    }

    bool Undo() {
        if (undoStack.empty()) return false;
        EditorSnapshot current;
        current.lines = lines;
        current.cursorLine = cursorLine;
        current.cursorCol = cursorCol;
        current.hasSelection = hasSelection;
        current.selAnchorLine = selAnchorLine;
        current.selAnchorCol = selAnchorCol;
        current.selActiveLine = selActiveLine;
        current.selActiveCol = selActiveCol;
        redoStack.push_back(current);

        EditorSnapshot prev = undoStack.back();
        undoStack.pop_back();

        lines = prev.lines;
        cursorLine = prev.cursorLine;
        cursorCol = prev.cursorCol;
        hasSelection = prev.hasSelection;
        selAnchorLine = prev.selAnchorLine;
        selAnchorCol = prev.selAnchorCol;
        selActiveLine = prev.selActiveLine;
        selActiveCol = prev.selActiveCol;

        std::string newText = "";
        for (size_t i = 0; i < lines.size(); ++i) {
            newText += lines[i];
            if (i + 1 < lines.size()) newText += "\n";
        }
        if (isJavaMode) javaAlternative = newText;
        else contentText = newText;
        isModified = true;
        return true;
    }

    bool Redo() {
        if (redoStack.empty()) return false;
        EditorSnapshot current;
        current.lines = lines;
        current.cursorLine = cursorLine;
        current.cursorCol = cursorCol;
        current.hasSelection = hasSelection;
        current.selAnchorLine = selAnchorLine;
        current.selAnchorCol = selAnchorCol;
        current.selActiveLine = selActiveLine;
        current.selActiveCol = selActiveCol;
        undoStack.push_back(current);

        EditorSnapshot next = redoStack.back();
        redoStack.pop_back();

        lines = next.lines;
        cursorLine = next.cursorLine;
        cursorCol = next.cursorCol;
        hasSelection = next.hasSelection;
        selAnchorLine = next.selAnchorLine;
        selAnchorCol = next.selAnchorCol;
        selActiveLine = next.selActiveLine;
        selActiveCol = next.selActiveCol;

        std::string newText = "";
        for (size_t i = 0; i < lines.size(); ++i) {
            newText += lines[i];
            if (i + 1 < lines.size()) newText += "\n";
        }
        if (isJavaMode) javaAlternative = newText;
        else contentText = newText;
        isModified = true;
        return true;
    }

    void ClearSelection() {
        hasSelection = false;
        selAnchorLine = 0; selAnchorCol = 0;
        selActiveLine = 0; selActiveCol = 0;
    }

    void GetNormalizedSelection(int& outStartLine, int& outStartCol, int& outEndLine, int& outEndCol) const {
        if (selAnchorLine < selActiveLine || (selAnchorLine == selActiveLine && selAnchorCol <= selActiveCol)) {
            outStartLine = selAnchorLine;
            outStartCol = selAnchorCol;
            outEndLine = selActiveLine;
            outEndCol = selActiveCol;
        } else {
            outStartLine = selActiveLine;
            outStartCol = selActiveCol;
            outEndLine = selAnchorLine;
            outEndCol = selAnchorCol;
        }
    }

    std::wstring GetSelectedText() const {
        if (!hasSelection || lines.empty()) return L"";
        int sLine, sCol, eLine, eCol;
        GetNormalizedSelection(sLine, sCol, eLine, eCol);
        if (sLine < 0 || sLine >= (int)lines.size()) return L"";
        if (eLine >= (int)lines.size()) eLine = (int)lines.size() - 1;

        std::wstring result = L"";
        for (int l = sLine; l <= eLine; ++l) {
            std::wstring lineW = KaisekiTheme::Utf8ToWide(lines[l]);
            int startC = (l == sLine) ? sCol : 0;
            int endC = (l == eLine) ? eCol : (int)lineW.length();

            if (startC < 0) startC = 0;
            if (endC < 0) endC = 0;
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

    bool DeleteSelection() {
        if (!hasSelection || lines.empty() || isReadOnly) return false;
        PushUndoState();
        int sLine, sCol, eLine, eCol;
        GetNormalizedSelection(sLine, sCol, eLine, eCol);
        if (sLine < 0) sLine = 0;
        if (eLine >= (int)lines.size()) eLine = (int)lines.size() - 1;

        std::wstring sLineW = KaisekiTheme::Utf8ToWide(lines[sLine]);
        std::wstring eLineW = KaisekiTheme::Utf8ToWide(lines[eLine]);

        if (sCol < 0) sCol = 0;
        if (eCol < 0) eCol = 0;
        if (sCol > (int)sLineW.length()) sCol = (int)sLineW.length();
        if (eCol > (int)eLineW.length()) eCol = (int)eLineW.length();

        std::wstring prefix = sLineW.substr(0, sCol);
        std::wstring suffix = eLineW.substr(eCol);

        std::wstring merged = prefix + suffix;
        lines[sLine] = KaisekiTheme::WideToUtf8(merged);

        if (eLine > sLine) {
            lines.erase(lines.begin() + sLine + 1, lines.begin() + eLine + 1);
        }

        cursorLine = sLine;
        cursorCol = sCol;
        ClearSelection();

        std::string newText = "";
        for (size_t i = 0; i < lines.size(); ++i) {
            newText += lines[i];
            if (i + 1 < lines.size()) newText += "\n";
        }
        if (isJavaMode) javaAlternative = newText;
        else contentText = newText;
        isModified = true;
        return true;
    }

    void InsertText(const std::wstring& wText) {
        if (wText.empty() || isReadOnly) return;
        if (lines.empty()) lines.push_back("");

        if (hasSelection) {
            DeleteSelection();
        } else {
            PushUndoState();
        }

        if (cursorLine < 0) cursorLine = 0;
        if (cursorLine >= (int)lines.size()) cursorLine = (int)lines.size() - 1;

        std::wstring curLineW = KaisekiTheme::Utf8ToWide(lines[cursorLine]);
        if (cursorCol < 0) cursorCol = 0;
        if (cursorCol > (int)curLineW.length()) cursorCol = (int)curLineW.length();

        std::vector<std::wstring> insertLines;
        std::wstring curSeg = L"";
        for (size_t i = 0; i < wText.length(); ++i) {
            wchar_t c = wText[i];
            if (c == L'\r') {
                if (i + 1 < wText.length() && wText[i+1] == L'\n') {
                    ++i;
                }
                insertLines.push_back(curSeg);
                curSeg.clear();
            } else if (c == L'\n') {
                insertLines.push_back(curSeg);
                curSeg.clear();
            } else {
                curSeg += c;
            }
        }
        insertLines.push_back(curSeg);

        std::wstring before = curLineW.substr(0, cursorCol);
        std::wstring after = curLineW.substr(cursorCol);

        if (insertLines.size() == 1) {
            std::wstring merged = before + insertLines[0] + after;
            lines[cursorLine] = KaisekiTheme::WideToUtf8(merged);
            cursorCol = (int)(before.length() + insertLines[0].length());
        } else {
            std::wstring firstLine = before + insertLines[0];
            std::wstring lastLine = insertLines.back() + after;
            lines[cursorLine] = KaisekiTheme::WideToUtf8(firstLine);

            std::vector<std::string> toInsert;
            for (size_t i = 1; i < insertLines.size() - 1; ++i) {
                toInsert.push_back(KaisekiTheme::WideToUtf8(insertLines[i]));
            }
            toInsert.push_back(KaisekiTheme::WideToUtf8(lastLine));

            lines.insert(lines.begin() + cursorLine + 1, toInsert.begin(), toInsert.end());
            cursorLine = cursorLine + (int)insertLines.size() - 1;
            cursorCol = (int)insertLines.back().length();
        }

        ClearSelection();

        std::string newText = "";
        for (size_t i = 0; i < lines.size(); ++i) {
            newText += lines[i];
            if (i + 1 < lines.size()) newText += "\n";
        }
        if (isJavaMode) javaAlternative = newText;
        else contentText = newText;
        isModified = true;
    }

    // Hit-test rects
    D2D1_RECT_F rcTab = { 0 };
    D2D1_RECT_F rcCloseBtn = { 0 };
    D2D1_RECT_F rcSaveBtn = { 0 };
};

struct ConsoleLogEntry {
    std::wstring tag;
    std::wstring message;
    int type; // 0: SUCCESS, 1: INFO, 2: WARN, 3: NORMAL
};

struct DropdownItemDef {
    std::wstring text;
    std::wstring shortcut;
    int actionId;
    bool isSeparator;
};

class UIWindow {
public:
    UIWindow();
    ~UIWindow();

    bool Create(HINSTANCE hInstance, int nCmdShow, const wchar_t* initialApk);
    int Run();
    HWND GetHwnd() const { return m_hwnd; }
    void SetRailMode(RailMode mode) { m_railMode = mode; }
    void SetSettingsTab(int tab) { m_settingsTab = tab; }
    void SetActiveMenu(int menuIdx) { m_activeMenu = menuIdx; if (m_hwnd) InvalidateRect(m_hwnd, NULL, FALSE); }
    void OpenSettingsModalDirect();
    void OpenSettingsModal(int initialTab = -1);
    void SimulateEdit(const wchar_t* text) {
        if (!m_tabs.empty() && m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabs.size()) {
            m_tabs[m_activeTabIndex].InsertText(text);
        }
    }
    void SetTestSelection(int startLine, int startCol, int endLine, int endCol);

    bool IsTitleBarControl(float x, float y) const;
    void SetTheme(AppTheme theme);
    void SetCustomAccentColorU8(uint8_t r, uint8_t g, uint8_t b);
    void SetCustomBackgroundColorU8(uint8_t r, uint8_t g, uint8_t b);
    void SetBackgroundShade(BackgroundShade shade);
    void OpenInVisualStudio();

    inline float DipToPixelX(float dipX) const { return dipX * (m_dpiX / 96.0f); }
    inline float DipToPixelY(float dipY) const { return dipY * (m_dpiY / 96.0f); }
    inline float PixelToDipX(float px) const { return px * (96.0f / m_dpiX); }
    inline float PixelToDipY(float py) const { return py * (96.0f / m_dpiY); }

    void LoadApk(const std::wstring& path);
    void OpenDocInNewTab(const std::string& path, TabContentType type, const std::string& content = "");
    std::string BuildSecurityReportString();
    void OpenImageInTab(const std::string& path, const uint8_t* imgData, size_t imgSize);
    void CloseTab(int index);
    void SaveTab(int index);
    void SaveAllTabs();
    void SaveActiveTab();
    void PasteClipboardText();
    void CutSelectedText();

    void SaveKaisekiProject();
    void OpenKaisekiProject();
    void CloseProject();
    void ShowWelcomeScreen();
    void ExportGradleProjectInteractive();
    void BuildAndRepackageApkInteractive();
    void OpenClassInTab(const std::string& fullClassName, bool decompileWithNativeEngine = true);
    void PerformGlobalSearch(const std::wstring& query, int type);
    bool RealSignApk(const std::wstring& inApk, const std::wstring& outApk, std::wstring& outMsg);
    bool RealVerifyApk(const std::wstring& inApk, std::wstring& outReport);
    void RefreshAdbDevices();
    void RefreshLogcat();

    void BuildPackageTree();
    void BuildResourcesTree();
    void DecompileAllDexToTemp();
    void RebuildFlattenedTreeItems();

    void ShowModalProgress(const wchar_t* title, const wchar_t* initialStatus);
    void UpdateModalProgress(int percent, const wchar_t* status);
    void CloseModalProgress();
    bool SaveScreenshot(const wchar_t* outPngPath);
    void SimulateClickSecReport();
    void CompareApksInteractive();
    void OpenHexViewerInteractive();
    void OpenResourceTableInteractive();
    void FindXrefsInteractive();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    HRESULT CreateDeviceIndependentResources();
    HRESULT CreateDeviceResources();
    void CreateBrushResources();
    void DiscardBrushResources();
    void DiscardDeviceResources();
    void RecreateThemeBrushes();

    void OnRender();
    void OnResize(UINT width, UINT height);
    void UpdateWindowRegion();
    void OnDropFiles(HDROP hDrop);

    // Render Subroutines
    void RenderTopTitleBar(float w);
    void RenderTopMenuBar(float w);
    void RenderDropdownMenu(float w, float h);
    void RenderActivityRail(float h);
    void RenderInitialHeroDropZone(float w, float h);
    void RenderProgressModal(float w, float h);
    void RenderSettingsModal(float w, float h);
    void RenderSearchModal(float w, float h);
    void RenderColorWheelPopover(float x, float y);
    void DrawToggleSwitch(const D2D1_RECT_F& rc, bool isOn, const wchar_t* label, const wchar_t* subtext = nullptr);
    void RenderAboutModal(float w, float h);
    void RenderExplorerView(float x, float y, float w, float h);
    void RenderApkInfoView(float x, float y, float w, float h);
    void RenderSignerView(float x, float y, float w, float h);
    void RenderAdbPhoneView(float x, float y, float w, float h);
    void RenderSettingsView(float x, float y, float w, float h);
    void RenderDocumentTabBar(float x, float y, float w);
    void RenderDocumentContent(float x, float y, float w, float h);
    void RenderBottomConsole(float x, float y, float w, float h);
    void RenderBottomStatusBar(float w, float h);
    void RenderToast(float w, float h);

    void OpenFileDialog();
    void CreateIconBitmap();
    void LoadAppLogoBitmap();
    void ShowToast(const std::wstring& msg, bool isError = false);
    void RefreshConsoleLogs();
    void HandleCharInput(wchar_t ch);
    void HandleKeyDown(WPARAM key);
    void TriggerMenuAction(int actionId);
    bool UpdateAnimations();
    void ApplySyntaxHighlighting(bool isJava, TabContentType type, const std::wstring& line, IDWriteTextLayout* pLayout);
    void CopySelectedTextToClipboard();
    void ToggleActiveTabJavaSmali();
    bool GetEditorRect(D2D1_RECT_F& outEditorRect, float& outTextX, float& outLineH, float& outCharW) const;
    int HitTestLineCol(int lineIdx, float mouseX, float textX) const;
    float GetCharXPos(IDWriteTextLayout* pLayout, const std::wstring& lineW, int col, float textX, float charW) const;
    void SelectWordAt(int lineIdx, int colIdx);
    void ShowEditorContextMenu(int screenX, int screenY);

    // Bottom Console Selection & Logcat Methods
    bool GetConsoleRect(D2D1_RECT_F& outConsoleRect, D2D1_RECT_F& outTextRect, float& outLineH) const;
    std::wstring GetConsoleLineText(int index) const;
    int GetConsoleLineCount() const;
    int HitTestConsoleCol(int lineIdx, float mouseX, float textX) const;
    void GetNormalizedConsoleSelection(int& outStartLine, int& outStartCol, int& outEndLine, int& outEndCol) const;
    std::wstring GetSelectedConsoleText() const;
    void CopyConsoleSelectionToClipboard();
    void SelectAllConsole();
    void ShowConsoleContextMenu(int screenX, int screenY);
    void ApplyLogcatFilters();
    void CloseSettingsModalAnimated();
    void CloseSearchModalAnimated();
    void CloseAboutModalAnimated();

private:
    HWND m_hwnd = nullptr;
    HINSTANCE m_hInstance = nullptr;

    ID2D1Factory* m_pD2DFactory = nullptr;
    IDWriteFactory* m_pDWriteFactory = nullptr;
    IWICImagingFactory* m_pWicFactory = nullptr;
    ID2D1HwndRenderTarget* m_pRenderTarget = nullptr;

    FontManager m_fonts;

    // Theme & In-App Custom Palette (Default Soft Pastel Pink from User)
    AppTheme m_currentTheme = AppTheme::KaisekiDark;
    D2D1_COLOR_F m_customAccentColor = D2D1::ColorF(1.000f, 0.500f, 0.720f, 1.0f); // Default Sakura Pink
    KaisekiTheme::ThemeColors m_colors;

    // Slider Dragging State in Settings
    int m_activeSlider = -1; // 0: Red, 1: Green, 2: Blue
    uint8_t m_sliderR = 255;
    uint8_t m_sliderG = 228;
    uint8_t m_sliderB = 255;

    // Settings Sub-Tabs (Consolidated 7 Categories: 0: Genel, 1: Görünüm, 2: Dil, 3: Android Araçları, 4: APK İşlemleri, 5: Kısayollar, 6: Gelişmiş ve Tanılama)
    int m_settingsTab = 0;
    D2D1_RECT_F m_rcSettingsTabs[7];
    D2D1_RECT_F m_rcBtnSelectWorkspace = { 0, 0, 0, 0 };
    D2D1_RECT_F m_rcToggleRestoreWorkspace = { 0, 0, 0, 0 };
    D2D1_RECT_F m_rcBtnSelectSdk = { 0, 0, 0, 0 };
    D2D1_RECT_F m_rcBtnDownloadTools = { 0, 0, 0, 0 };
    D2D1_RECT_F m_rcLangDropdownBtn = { 0, 0, 0, 0 };
    bool m_langDropdownOpen = false;
    std::vector<D2D1_RECT_F> m_rcLangDropdownOptions;
    std::vector<D2D1_RECT_F> m_rcLangCards;
    D2D1_RECT_F m_rcBtnOpenLangFolder = { 0, 0, 0, 0 };
    D2D1_RECT_F m_rcBtnRefreshLangs = { 0, 0, 0, 0 };
    D2D1_RECT_F m_rcHotkeyPresetVSCode = { 0, 0, 0, 0 };
    D2D1_RECT_F m_rcHotkeyPresetIntelliJ = { 0, 0, 0, 0 };
    D2D1_RECT_F m_rcHotkeyPresetVS = { 0, 0, 0, 0 };
    D2D1_RECT_F m_rcBtnTestSdkTools = { 0, 0, 0, 0 };
    D2D1_RECT_F m_rcBtnRestartAdbServer = { 0, 0, 0, 0 };
    D2D1_RECT_F m_rcBtnTrimRam = { 0, 0, 0, 0 };
    D2D1_RECT_F m_rcBtnCopyDiagnostics = { 0, 0, 0, 0 };
    D2D1_RECT_F m_rcBtnResetSettings = { 0, 0, 0, 0 };
    D2D1_RECT_F m_rcToggleDefaultView = { 0, 0, 0, 0 };
    D2D1_RECT_F m_rcToggleMultidex = { 0, 0, 0, 0 };
    D2D1_RECT_F m_rcToggleVerboseLog = { 0, 0, 0, 0 };
    D2D1_RECT_F m_rcToggleMinimap = { 0, 0, 0, 0 };
    D2D1_RECT_F m_rcToggleLineNumbers = { 0, 0, 0, 0 };
    D2D1_RECT_F m_rcToggleWordWrap = { 0, 0, 0, 0 };
    D2D1_RECT_F m_rcToggleZipalign = { 0, 0, 0, 0 };
    D2D1_RECT_F m_rcToggleSignV1 = { 0, 0, 0, 0 };
    D2D1_RECT_F m_rcToggleSignV2 = { 0, 0, 0, 0 };
    D2D1_RECT_F m_rcToggleSignV3 = { 0, 0, 0, 0 };
    D2D1_RECT_F m_rcToggleAutoInstall = { 0, 0, 0, 0 };

    // Smooth Launch & Transition Animation
    DWORD m_launchStartTime = 0;
    DWORD m_lastAnimTick = 0;
    float m_viewAlpha = 1.0f;
    float m_windowFadeAlpha = 0.0f;
    float m_animRailY = 50.0f;
    float m_railHoverAlpha[5] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    float m_animSettingsModal = 0.0f;
    bool m_closingSettings = false;
    float m_animSearchModal = 0.0f;
    bool m_closingSearch = false;
    float m_animAboutModal = 0.0f;
    bool m_closingAbout = false;
    float m_animDropdown = 0.0f;
    float m_pulsePhase = 0.0f;
    float m_scannerX = -120.0f;
    float m_toastAnim = 0.0f;
    float m_tabUnderlineX = 0.0f;
    bool m_isOffscreen = false;
    float m_tabUnderlineW = 0.0f;
    float m_animTabScrollX = 0.0f;
    float m_animTreeScrollY = 0.0f;
    float m_hoverWinCloseAlpha = 0.0f;
    float m_hoverWinMaxAlpha = 0.0f;
    float m_hoverWinMinAlpha = 0.0f;
    float m_hoverHeroDropAlpha = 0.0f;

    // Direct2D Brushes
    ID2D1SolidColorBrush* m_bCanvas = nullptr;
    ID2D1SolidColorBrush* m_bPanel = nullptr;
    ID2D1SolidColorBrush* m_bCard = nullptr;
    ID2D1SolidColorBrush* m_bCardHover = nullptr;
    ID2D1SolidColorBrush* m_bWell = nullptr;
    ID2D1SolidColorBrush* m_bBorderSubtle = nullptr;
    ID2D1SolidColorBrush* m_bBorderHover = nullptr;
    ID2D1SolidColorBrush* m_bAccent = nullptr;
    ID2D1SolidColorBrush* m_bAccentLight = nullptr;
    ID2D1SolidColorBrush* m_bTextPrimary = nullptr;
    ID2D1SolidColorBrush* m_bTextSecondary = nullptr;
    ID2D1SolidColorBrush* m_bTextMuted = nullptr;
    ID2D1SolidColorBrush* m_bBadgeBg = nullptr;
    ID2D1SolidColorBrush* m_bSuccess = nullptr;
    ID2D1SolidColorBrush* m_bWarning = nullptr;
    ID2D1SolidColorBrush* m_bCloseRed = nullptr;
    ID2D1SolidColorBrush* m_bRailBg = nullptr;
    ID2D1SolidColorBrush* m_bMenuBg = nullptr;
    ID2D1SolidColorBrush* m_bAccentLine = nullptr;
    ID2D1SolidColorBrush* m_bBtnCompile = nullptr;
    ID2D1SolidColorBrush* m_bBtnAnalyze = nullptr;
    ID2D1SolidColorBrush* m_bChartBar = nullptr;
    ID2D1SolidColorBrush* m_bWatermark = nullptr;
    ID2D1SolidColorBrush* m_bConsoleBg = nullptr;

    // Antigravity Pill Tab Brushes
    ID2D1SolidColorBrush* m_bTabActiveBg = nullptr;
    ID2D1SolidColorBrush* m_bTabActiveBorder = nullptr;
    ID2D1SolidColorBrush* m_bTabHoverBg = nullptr;

    // Fixed Authentic IDE Syntax Highlighting Brushes
    ID2D1SolidColorBrush* m_bSyntaxKeyword = nullptr;
    ID2D1SolidColorBrush* m_bSyntaxRegister = nullptr;
    ID2D1SolidColorBrush* m_bSyntaxString = nullptr;
    ID2D1SolidColorBrush* m_bSyntaxNumber = nullptr;
    ID2D1SolidColorBrush* m_bSyntaxComment = nullptr;
    ID2D1SolidColorBrush* m_bSyntaxAnnotation = nullptr;
    ID2D1SolidColorBrush* m_bSyntaxType = nullptr;
    ID2D1SolidColorBrush* m_bSyntaxTag = nullptr;
    ID2D1SolidColorBrush* m_bSyntaxAttr = nullptr;
    ID2D1SolidColorBrush* m_bSelection = nullptr;
    bool m_isSelectingText = false;

    ID2D1Bitmap* m_pIconBitmap = nullptr;
    ID2D1Bitmap* m_pAppLogoBitmap = nullptr;
    ApkDataModel* m_pModel = nullptr;

    // KaisekiSec Security Report
    SecReport m_secReport;
    bool m_secReportLoaded = false;
    D2D1_RECT_F m_rcBtnViewSecReport = { 0 };
    D2D1_RECT_F m_rcBtnViewFridaBypass = { 0 };
    bool m_hoverBtnViewSecReport = false;
    bool m_hoverBtnViewFridaBypass = false;

    // Separate Progress Modal Window State
    bool m_isLoading = false;
    bool m_showProgressModal = false;
    float m_loadProgress = 0.0f;
    std::wstring m_loadStatusText = L"APK Çözümleniyor...";
    D2D1_RECT_F m_rcProgressClose = { 0 };

    // Separate Settings Modal Window State
    bool m_showSettingsModal = false;
    D2D1_RECT_F m_rcSettingsClose = { 0 };
    bool m_colorPickerPopoverOpen = false;
    int m_colorPickerTarget = 0; // 0: Accent, 1: Background surface
    D2D1_RECT_F m_rcColorSquareAccent = { 0 };
    D2D1_RECT_F m_rcColorSquareBg = { 0 };
    D2D1_RECT_F m_rcColorPickerCard = { 0 };
    D2D1_RECT_F m_rcColorPickerClose = { 0 };
    D2D1_RECT_F m_rcColorSatValBox = { 0 };
    D2D1_RECT_F m_rcColorHueSlider = { 0 };
    D2D1_RECT_F m_rcColorPreviewBox = { 0 };
    std::vector<D2D1_RECT_F> m_rcColorPresetDots;
    bool m_isDraggingSatVal = false;
    bool m_isDraggingHue = false;
    float m_pickerHue = 330.0f;
    float m_pickerSat = 0.50f;
    float m_pickerVal = 1.00f;
    BackgroundShade m_currentBgShade = BackgroundShade::Charcoal;
    D2D1_COLOR_F m_customBgColor = D2D1::ColorF(0.106f, 0.110f, 0.133f, 1.0f);
    D2D1_RECT_F m_rcBgShades[6];

    // Scrollbars (Auto-hiding overlay + Draggable)
    float m_editorScrollAlpha = 0.0f;
    bool m_isDraggingEditorScroll = false;
    float m_dragScrollStartY = 0.0f;
    float m_dragScrollStartVal = 0.0f;
    D2D1_RECT_F m_rcEditorScrollTrack = { 0 };
    D2D1_RECT_F m_rcEditorScrollThumb = { 0 };

    float m_treeScrollAlpha = 0.0f;
    bool m_isDraggingTreeScroll = false;
    float m_dragTreeScrollStartY = 0.0f;
    float m_dragTreeScrollStartVal = 0.0f;
    D2D1_RECT_F m_rcTreeScrollTrack = { 0 };
    D2D1_RECT_F m_rcTreeScrollThumb = { 0 };

    float m_lastMouseX = 0.0f;
    float m_lastMouseY = 0.0f;

    // About Modal
    bool m_showAboutModal = false;
    D2D1_RECT_F m_rcAboutClose = { 0 };

    // Window Controls (No Corner Theme Button)
    D2D1_RECT_F m_rcWinClose = { 0 };
    D2D1_RECT_F m_rcWinMax = { 0 };
    D2D1_RECT_F m_rcWinMin = { 0 };
    bool m_hoverWinClose = false;
    bool m_hoverWinMax = false;
    bool m_hoverWinMin = false;

    // Top Menu Bar (Dosya, İncele, Araçlar, Görünüm, Ayarlar, Hakkında)
    D2D1_RECT_F m_rcMenuHeaders[6];
    int m_activeMenu = -1; // -1: closed, 0: Dosya, 1: İncele, 2: Araçlar, 3: Görünüm, 4: Ayarlar, 5: Hakkında
    int m_hoverMenuHeader = -1;
    D2D1_RECT_F m_rcMenuDropdown = { 0 };
    std::vector<D2D1_RECT_F> m_rcDropdownItems;
    std::vector<DropdownItemDef> m_currentMenuItems;
    int m_hoverDropdownItem = -1;

    // Hero Drag-Drop & Welcome Screen
    D2D1_RECT_F m_rcHeroDropZone = { 0 };
    D2D1_RECT_F m_rcHeroBrowseBtn = { 0 };
    D2D1_RECT_F m_rcClearRecentBtn = { 0 };
    std::vector<D2D1_RECT_F> m_rcRecentItems;
    bool m_hoverHeroDrop = false;
    bool m_hoverHeroBrowse = false;
    bool m_hoverClearRecent = false;
    int m_hoverRecentIndex = -1;

    // Left Rail (5 Modes)
    RailMode m_railMode = RailMode::Explorer;
    D2D1_RECT_F m_rcRailIcons[5];
    int m_hoverRail = -1;

    // Explorer Tree
    struct TreeItemDef {
        int depth;
        std::wstring label;
        std::string path;
        int iconType;
    };

    struct PkgTreeNode {
        std::string name;
        std::string fullPkgKey;
        std::map<std::string, PkgTreeNode> children;
        std::vector<PackageClassItem> classes;
        int totalClassCount = 0;

        int ComputeClassCounts() {
            int cnt = (int)classes.size();
            for (auto& pair : children) {
                cnt += pair.second.ComputeClassCounts();
            }
            totalClassCount = cnt;
            return cnt;
        }

        void Flatten(std::vector<TreeItemDef>& outItems, const std::map<std::string, bool>& pkgExpanded, int depth) const {
            auto itExp = pkgExpanded.find(fullPkgKey);
            bool isExp = (itExp != pkgExpanded.end() && itExp->second);
            std::wstring arrow = isExp ? L"▼ " : L"▶ ";
            std::wstring title = arrow + KaisekiTheme::Utf8ToWide(name);
            if (totalClassCount > 0) {
                title += L" (" + std::to_wstring(totalClassCount) + L")";
            }

            outItems.push_back({ depth, title, "__PKG__" + fullPkgKey, 0 });

            if (isExp) {
                for (const auto& pair : children) {
                    pair.second.Flatten(outItems, pkgExpanded, depth + 1);
                }
                for (const auto& cls : classes) {
                    std::string dName = !cls.className.empty() ? cls.className : cls.fullClassName;
                    std::wstring clsTitle = KaisekiTheme::Utf8ToWide(dName);
                    outItems.push_back({ depth + 1, clsTitle, "__CLS__" + cls.fullClassName, 2 });
                }
            }
        }
    };

    PkgTreeNode m_pkgTreeRoot;
    bool m_pkgTreeBuilt = false;

    struct ResFileLeaf {
        std::string fileName;
        std::string fullPath;
        int iconType;
    };

    struct ResTreeNode {
        std::string name;
        std::string fullDirKey;
        std::map<std::string, ResTreeNode> children;
        std::vector<ResFileLeaf> files;
        int totalFileCount = 0;

        int ComputeFileCounts() {
            int cnt = (int)files.size();
            for (auto& pair : children) {
                cnt += pair.second.ComputeFileCounts();
            }
            totalFileCount = cnt;
            return cnt;
        }

        void Flatten(std::vector<TreeItemDef>& outItems, const std::map<std::string, bool>& expMap, int depth) const {
            auto itExp = expMap.find("__RES_DIR__" + fullDirKey);
            bool isExp = (itExp != expMap.end() && itExp->second);
            std::wstring arrow = isExp ? L"▼ " : L"▶ ";
            std::wstring title = arrow + KaisekiTheme::Utf8ToWide(name);
            if (totalFileCount > 0) {
                title += L" (" + std::to_wstring(totalFileCount) + L")";
            }

            outItems.push_back({ depth, title, "__RES_DIR__" + fullDirKey, 0 });

            if (isExp) {
                for (const auto& pair : children) {
                    pair.second.Flatten(outItems, expMap, depth + 1);
                }
                for (const auto& f : files) {
                    outItems.push_back({ depth + 1, KaisekiTheme::Utf8ToWide(f.fileName), f.fullPath, f.iconType });
                }
            }
        }
    };

    ResTreeNode m_resTreeRoot;
    bool m_resTreeBuilt = false;

    struct DecompileCacheEntry {
        std::string javaCode;
        std::string smaliCode;
        std::string fridaCode;
    };
    std::unordered_map<std::string, DecompileCacheEntry> m_decompiledClasses;
    std::wstring m_workspaceTempDir;
    std::vector<TreeItemDef> m_cachedTreeItems;
    bool m_treeItemsDirty = true;

    float m_treeScrollY = 0.0f;
    float m_maxTreeScrollY = 0.0f;
    D2D1_RECT_F m_rcTreeTab[2];
    int m_treeTab = 0;
    bool m_resExpanded = true;
    bool m_assetsExpanded = false;
    bool m_libExpanded = false;
    std::vector<D2D1_RECT_F> m_treeItemRects;
    std::vector<std::string> m_treeItemPaths;
    int m_selectedTreeItem = -1;
    int m_hoverTreeItem = -1;

    // Multi-Tab Document System
    std::vector<OpenDocTab> m_tabs;
    int m_activeTabIndex = 0;
    int m_hoverTab = -1;
    int m_hoverTabClose = -1;
    int m_hoverTabSave = -1;
    float m_tabScrollX = 0.0f;
    float m_maxTabScrollX = 0.0f;
    D2D1_RECT_F m_rcTabScrollLeft = { 0 };
    D2D1_RECT_F m_rcTabScrollRight = { 0 };
    bool m_hoverTabScrollLeft = false;
    bool m_hoverTabScrollRight = false;
    D2D1_RECT_F m_rcBtnToggleJavaSmali = { 0 };
    D2D1_RECT_F m_rcBtnJadxDecompile = { 0 };
    D2D1_RECT_F m_rcBtnRepackApk = { 0 };
    D2D1_RECT_F m_rcBtnExportGradle = { 0 };
    D2D1_RECT_F m_rcBtnGlobalSearch = { 0 };
    bool m_hoverBtnToggleJavaSmali = false;
    bool m_hoverBtnJadxDecompile = false;
    bool m_hoverBtnRepackApk = false;
    bool m_hoverBtnExportGradle = false;
    bool m_hoverBtnGlobalSearch = false;

    // JADX Global Search Modal (Ctrl+Shift+F)
    struct SearchResultItem {
        std::string title;
        std::string location;
        int line = 0;
        std::string snippet;
        std::string targetClass;
    };
    bool m_showSearchModal = false;
    std::wstring m_searchQuery;
    int m_searchType = 0; // 0: Sınıflar, 1: Metotlar, 2: Kod Metni, 3: Kaynaklar
    std::vector<SearchResultItem> m_searchResults;
    int m_hoverSearchResult = -1;
    D2D1_RECT_F m_rcSearchInput = { 0 };
    D2D1_RECT_F m_rcSearchTypeTabs[4];
    D2D1_RECT_F m_rcSearchClose = { 0 };
    std::vector<D2D1_RECT_F> m_rcSearchResultRows;

    // JADX Navigation & Deobfuscation
    bool m_useDeobfuscation = false;
    std::wstring m_lastBuiltApkPath;
    std::map<std::string, bool> m_pkgExpanded;
    bool m_sourcesExpanded = true;
    bool m_signatureExpanded = false;
    bool m_summaryExpanded = false;

    // Console & Logcat Panel
    int m_bottomConsoleTab = 0; // 0: Terminal, 1: Logcat
    D2D1_RECT_F m_rcBottomTabTerminal = { 0 };
    D2D1_RECT_F m_rcBottomTabLogcat = { 0 };
    D2D1_RECT_F m_rcBtnLogcatRefresh = { 0 };
    D2D1_RECT_F m_rcBtnLogcatClear = { 0 };
    D2D1_RECT_F m_rcLogcatPkgFilter = { 0 };
    D2D1_RECT_F m_rcLogcatLevelFilter = { 0 };
    D2D1_RECT_F m_rcLogcatLiveStream = { 0 };
    std::vector<std::wstring> m_logcatLines;
    std::vector<AdbLogcatEntry> m_rawLogcatEntries;
    bool m_logcatFilterByPackage = true;
    char m_logcatMinLevel = 'V';
    std::string m_appPid = "";
    bool m_logcatStreamActive = false;
    bool m_logcatLoaded = false;
    D2D1_RECT_F m_rcConsoleTabs[3];
    int m_consoleTab = 0;
    int m_hoverConsoleTab = -1;
    std::vector<ConsoleLogEntry> m_consoleLogs;
    bool m_consoleCollapsed = false;
    float m_consoleScrollY = 0.0f;
    float m_logcatScrollY = 0.0f;

    // Bottom Console Selection State
    bool m_isSelectingConsole = false;
    bool m_consoleHasSelection = false;
    int m_consoleSelAnchorLine = -1;
    int m_consoleSelAnchorCol = -1;
    int m_consoleSelActiveLine = -1;
    int m_consoleSelActiveCol = -1;

    // Signer
    std::wstring m_signerKeystorePath = L"kaiseki_release.keystore";
    std::wstring m_signerAlias = L"kaiseki_key";
    D2D1_RECT_F m_rcBtnSignApk = { 0 };
    D2D1_RECT_F m_rcBtnVerifySign = { 0 };
    bool m_hoverBtnSignApk = false;
    bool m_hoverBtnVerifySign = false;

    // ADB Phone Device Selector
    struct AdbDeviceDef {
        std::string serial;
        std::string model;
        std::string product;
        std::string state;
        bool isOnline;
    };
    std::vector<AdbDeviceDef> m_adbDeviceList;
    int m_selectedDeviceIdx = -1;
    D2D1_RECT_F m_rcBtnRefreshDevices = { 0 };
    std::vector<D2D1_RECT_F> m_rcDeviceCards;
    int m_hoverDeviceCard = -1;
    std::wstring m_adbDevice = L"Cihaz aranıyor...";
    D2D1_RECT_F m_rcBtnAdbInstall = { 0 };
    D2D1_RECT_F m_rcBtnAdbLaunch = { 0 };

    // Settings Panel Controls
    std::vector<D2D1_RECT_F> m_rcSwatches;
    D2D1_RECT_F m_rcSliderR = { 0 };
    D2D1_RECT_F m_rcSliderG = { 0 };
    D2D1_RECT_F m_rcSliderB = { 0 };
    D2D1_RECT_F m_rcBtnAssocKproj = { 0 };
    D2D1_RECT_F m_rcBtnExportGradleSettings = { 0 };
    D2D1_RECT_F m_rcBtnToggleDeobf = { 0 };

    // Toast
    std::wstring m_toastText;
    bool m_toastIsError = false;
    DWORD m_toastStartTime = 0;

    // DPI Scaling Factors
    float m_dpiX = 96.0f;
    float m_dpiY = 96.0f;
    float m_dpiScale = 1.0f;
    bool m_mouseTracking = false;

    float m_width = 1180.0f;
    float m_height = 780.0f;
};

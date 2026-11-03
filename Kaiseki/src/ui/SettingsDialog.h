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
#include "KaisekiTheme.h"
#include "KaisekiLang.h"
#include "SettingsManager.h"
#include "FontManager.h"

// ============================================================================
// Kaiseki — Settings & Preferences Native Modal Dialog
// Runs as a separate top-level Windows dialog in front of the main window.
// While open, the main window is disabled (EnableWindow(hParent, FALSE)).
// ============================================================================

class SettingsDialog {
public:
    static SettingsDialog& Get() {
        static SettingsDialog instance;
        return instance;
    }

    void ShowModal(HWND hParent, int initialTab = 0);
    void Close();
    void SetTab(int tabIndex) { m_settingsTab = tabIndex; }
    void SetLangDropdownOpen(bool open) { m_langDropdownOpen = open; m_animLangDropdown = open ? 1.0f : 0.0f; }
    int GetTab() const { return m_settingsTab; }
    bool IsOpen() const { return m_hwnd != NULL; }

    bool SaveScreenshot(const wchar_t* outPngPath);

private:
    SettingsDialog();
    ~SettingsDialog();

    static LRESULT CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    HRESULT CreateDeviceResources();
    void DiscardDeviceResources();
    void CreateBrushes();

    void OnRender();
    void OnResize(UINT width, UINT height);

    void DrawToggleSwitch(const D2D1_RECT_F& rc, bool isOn, const wchar_t* label, const wchar_t* subtext = nullptr);
    void RenderColorWheelPopover(float cx, float cy);
    void LoadAppLogoBitmap();

    float PixelToDipX(float px) const { return (m_dpiX > 0.0f) ? (px * 96.0f / m_dpiX) : px; }
    float PixelToDipY(float py) const { return (m_dpiY > 0.0f) ? (py * 96.0f / m_dpiY) : py; }

    void ApplyTheme();
    void SetCustomAccentColorU8(uint8_t r, uint8_t g, uint8_t b);
    void SetCustomBackgroundColorU8(uint8_t r, uint8_t g, uint8_t b);
    void SetBackgroundShade(BackgroundShade shade);

    void UpdateAnimations(float dt);
    void DrawCountryFlag(const std::string& code, const D2D1_RECT_F& rc);
    void DrawIdeLogo(int ideIndex, const D2D1_RECT_F& rc);
    void DrawToggleSwitch(const D2D1_RECT_F& rc, bool state, float animVal, const wchar_t* label, const wchar_t* subtext);

    HWND m_hwnd = NULL;
    HWND m_hParent = NULL;
    int m_width = 1040;
    int m_height = 680;
    float m_dpiX = 96.0f;
    float m_dpiY = 96.0f;
    float m_dpiScale = 1.0f;

    ID2D1Factory* m_pD2DFactory = nullptr;
    IDWriteFactory* m_pDWriteFactory = nullptr;
    IWICImagingFactory* m_pWicFactory = nullptr;
    ID2D1HwndRenderTarget* m_pRenderTarget = nullptr;
    ID2D1Bitmap* m_pAppLogoBitmap = nullptr;

    KaisekiTheme::ThemeColors m_colors;
    AppTheme m_currentTheme = AppTheme::KaisekiDark;
    BackgroundShade m_currentBgShade = BackgroundShade::Charcoal;
    D2D1_COLOR_F m_customAccentColor = D2D1::ColorF(1.0f, 0.50f, 0.72f, 1.0f);
    D2D1_COLOR_F m_customBgColor = D2D1::ColorF(0.094f, 0.098f, 0.118f, 1.0f);

    ID2D1SolidColorBrush* m_bPanel = nullptr;
    ID2D1SolidColorBrush* m_bCard = nullptr;
    ID2D1SolidColorBrush* m_bCardHover = nullptr;
    ID2D1SolidColorBrush* m_bWell = nullptr;
    ID2D1SolidColorBrush* m_bBorderSubtle = nullptr;
    ID2D1SolidColorBrush* m_bTextPrimary = nullptr;
    ID2D1SolidColorBrush* m_bTextSecondary = nullptr;
    ID2D1SolidColorBrush* m_bTextMuted = nullptr;
    ID2D1SolidColorBrush* m_bAccent = nullptr;
    ID2D1SolidColorBrush* m_bSuccess = nullptr;
    ID2D1SolidColorBrush* m_bWarning = nullptr;
    ID2D1SolidColorBrush* m_bCloseRed = nullptr;
    ID2D1SolidColorBrush* m_bBtnCompile = nullptr;

    FontManager m_fonts;

    int m_settingsTab = 0; // 0..6
    D2D1_RECT_F m_rcSettingsTabs[7] = { {0} };
    D2D1_RECT_F m_rcSettingsClose = { 0 };
    bool m_hoverClose = false;

    // Tab 0
    D2D1_RECT_F m_rcBtnSelectWorkspace = { 0 };
    D2D1_RECT_F m_rcToggleRestoreWorkspace = { 0 };
    D2D1_RECT_F m_rcBtnAssocKproj = { 0 };

    // Tab 1
    D2D1_RECT_F m_rcAccentSwatches[12] = { {0} };
    D2D1_RECT_F m_rcAccentPreviewBox = { 0 };
    D2D1_RECT_F m_rcBgPreviewBox = { 0 };
    D2D1_RECT_F m_rcBgShades[8] = { {0} };
    D2D1_RECT_F m_rcToggleLineNumbers = { 0 };
    D2D1_RECT_F m_rcToggleMinimap = { 0 };

    // Tab 2
    D2D1_RECT_F m_rcLangDropdownBtn = { 0 };
    bool m_langDropdownOpen = false;
    int m_hoverLangOption = -1;

    // Tab 3
    D2D1_RECT_F m_rcBtnSelectSdk = { 0 };
    D2D1_RECT_F m_rcBtnSelectAdb = { 0 };
    D2D1_RECT_F m_rcBtnSelectBuildTools = { 0 };
    D2D1_RECT_F m_rcBtnSelectJava = { 0 };
    D2D1_RECT_F m_rcBtnDownloadTools = { 0 };

    // Switch Animation state variables (0.0f = OFF, 1.0f = ON)
    float m_animAutoWorkspace = 1.0f;
    float m_animLineNumbers = 1.0f;
    float m_animMinimap = 0.0f;
    float m_animSchemeV1 = 1.0f;
    float m_animSchemeV2 = 1.0f;
    float m_animSchemeV3 = 1.0f;
    float m_animZipAlign = 1.0f;
    float m_animVerboseLog = 0.0f;

    // Animated Dropdown State (0.0f = closed, 1.0f = fully open)
    float m_animLangDropdown = 0.0f;
    bool m_animating = false;

    // Tab 4
    D2D1_RECT_F m_rcToggleDefaultView = { 0 };
    D2D1_RECT_F m_rcToggleMultidexTree = { 0 };
    D2D1_RECT_F m_rcBtnDecompileThreads = { 0 };
    D2D1_RECT_F m_rcToggleSchemeV1 = { 0 };
    D2D1_RECT_F m_rcToggleSchemeV2 = { 0 };
    D2D1_RECT_F m_rcToggleSchemeV3 = { 0 };
    D2D1_RECT_F m_rcToggleZipAlign = { 0 };

    // Tab 5
    D2D1_RECT_F m_rcPresetVSCode = { 0 };
    D2D1_RECT_F m_rcPresetIDEA = { 0 };
    D2D1_RECT_F m_rcPresetVS = { 0 };
    D2D1_RECT_F m_rcPresetCustom = { 0 };
    D2D1_RECT_F m_rcBtnResetHotkeys = { 0 };
    D2D1_RECT_F m_rcShortcutBadges[16] = { {0} };
    int m_editingShortcutIndex = -1;

    // Tab 6
    D2D1_RECT_F m_rcToggleVerboseLog = { 0 };
    D2D1_RECT_F m_rcBtnCopyDiagnostics = { 0 };
    D2D1_RECT_F m_rcBtnResetSettings = { 0 };

    // Color picker popover state
    bool m_colorPickerPopoverOpen = false;
    int m_colorPickerTarget = 0; // 0 = Accent, 1 = Background
    float m_pickerHue = 330.0f;
    float m_pickerSat = 0.50f;
    float m_pickerVal = 1.00f;
    uint8_t m_sliderR = 255;
    uint8_t m_sliderG = 128;
    uint8_t m_sliderB = 184;

    D2D1_RECT_F m_rcColorPickerCard = { 0 };
    D2D1_RECT_F m_rcColorPickerClose = { 0 };
    D2D1_RECT_F m_rcColorSatValBox = { 0 };
    D2D1_RECT_F m_rcColorHueSlider = { 0 };
    D2D1_RECT_F m_rcColorPreviewBox = { 0 };
    D2D1_RECT_F m_rcColorPresetDots[8] = { {0} };
    bool m_isDraggingSatVal = false;
    bool m_isDraggingHue = false;
};

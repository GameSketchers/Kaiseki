#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>

struct ToolComponent {
    std::wstring name;
    std::wstring description;
    std::wstring url;
    std::wstring relativeDest;
    std::wstring testFile;
    bool selected = true;
    bool installed = false;
};

class ToolInstallerDialog {
public:
    static ToolInstallerDialog& Get() {
        static ToolInstallerDialog instance;
        return instance;
    }

    void ShowModal(HWND hParent);
    void Close();
    bool IsShowing() const { return m_hwnd != NULL; }
    bool SaveScreenshot(const wchar_t* outPngPath);
    void PaintContent(HDC hdc, int w, int h);

private:
    ToolInstallerDialog();
    ~ToolInstallerDialog();

    static LRESULT CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void StartInstallation();
    void CancelInstallation();
    void WorkerThreadFunc();
    void UpdateComponentStatus();

    HWND m_hwnd = NULL;
    HWND m_hParent = NULL;
    HFONT m_hFontTitle = NULL;
    HFONT m_hFontBody = NULL;
    HFONT m_hFontBold = NULL;
    HFONT m_hFontSmall = NULL;

    std::wstring m_destFolder;
    std::vector<ToolComponent> m_components;

    std::atomic<bool> m_isInstalling{ false };
    std::atomic<bool> m_cancelRequested{ false };
    std::atomic<int> m_percent{ 0 };
    std::mutex m_textMutex;
    std::wstring m_statusText;
    std::wstring m_detailText;

    RECT m_rcBtnBrowse = { 0 };
    RECT m_rcBtnInstall = { 0 };
    RECT m_rcBtnClose = { 0 };
    RECT m_rcHeaderClose = { 0 };
    RECT m_rcCheckbox[4] = { {0} };
    bool m_hoverClose = false;
};

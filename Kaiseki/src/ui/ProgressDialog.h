#pragma once
#include <windows.h>
#include <string>

class KaisekiProgressDialog {
public:
    static KaisekiProgressDialog& Get() {
        static KaisekiProgressDialog instance;
        return instance;
    }

    void Show(HWND hParent, const wchar_t* title, const wchar_t* initialStatus);
    void Update(int percent, const wchar_t* statusText);
    void Close();
    void PumpMessages();
    bool IsShowing() const { return m_hwnd != NULL; }

private:
    KaisekiProgressDialog();
    ~KaisekiProgressDialog();

    static LRESULT CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND m_hwnd = NULL;
    HWND m_hParent = NULL;
    HFONT m_hFontTitle = NULL;
    HFONT m_hFontBody = NULL;
    HFONT m_hFontBold = NULL;
    HFONT m_hFontSmall = NULL;
    HICON m_hIcon = NULL;

    std::wstring m_title;
    std::wstring m_status;
    int m_percent = 0;
    float m_animPercent = 0.0f;
    float m_animSweep = 0.0f;
    float m_alpha = 0.0f;
};

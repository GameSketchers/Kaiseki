#include "UIWindow.h"
#include "KaisekiVersion.h"
#include <iomanip>
#include <sstream>
#include <map>
#include <functional>

void UIWindow::OnRender() {
    HRESULT hr = CreateDeviceResources();
    if (FAILED(hr) || !m_pRenderTarget) return;

    if (!m_isOffscreen) {
        RECT rcClient;
        GetClientRect(m_hwnd, &rcClient);
        UINT curW = (UINT)(rcClient.right - rcClient.left);
        UINT curH = (UINT)(rcClient.bottom - rcClient.top);
        D2D1_SIZE_U rtSize = m_pRenderTarget->GetPixelSize();
        if (curW > 0 && curH > 0 && (rtSize.width != curW || rtSize.height != curH)) {
            OnResize(curW, curH);
        }
    }

    m_pRenderTarget->BeginDraw();
    if (m_windowFadeAlpha < 1.0f) {
        float winScale = 0.985f + 0.015f * KaisekiTheme::EaseOutCubic(m_windowFadeAlpha);
        m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Scale(winScale, winScale, D2D1::Point2F(m_width * 0.5f, m_height * 0.5f)));
    } else {
        m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
    }
    m_pRenderTarget->Clear(m_colors.canvasBg);

    // Top Titlebar & Menu Bar
    RenderTopTitleBar(m_width);

    // If no APK loaded and in Explorer view, show the clean drag-and-drop / click-to-browse hero area!
    if (!m_pModel && !m_isLoading) {
        RenderInitialHeroDropZone(m_width, m_height);
    } else {
        RenderActivityRail(m_height - 24.0f);

        float contentX = 50.0f;
        float contentY = 40.0f;
        float contentW = m_width - 50.0f;
        float contentH = m_height - 40.0f - 24.0f;

        switch (m_railMode) {
        case RailMode::Explorer:
            if (!m_pModel && !m_isLoading) {
                RenderInitialHeroDropZone(m_width, m_height - 24.0f);
            } else {
                RenderExplorerView(contentX, contentY, contentW, contentH);
            }
            break;
        case RailMode::ApkInfo:
            RenderApkInfoView(contentX, contentY, contentW, contentH);
            break;
        case RailMode::Signer:
            RenderSignerView(contentX, contentY, contentW, contentH);
            break;
        case RailMode::AdbPhone:
            RenderAdbPhoneView(contentX, contentY, contentW, contentH);
            break;
        case RailMode::Settings:
            RenderSettingsView(contentX, contentY, contentW, contentH);
            break;
        }
    }

    // Separate Settings Modal Popup
    if (m_showSettingsModal) {
        RenderSettingsModal(m_width, m_height);
    }

    // Active Dropdown Menu Popup (Must render on top of workspace)
    if (m_activeMenu != -1) {
        RenderDropdownMenu(m_width, m_height);
    }

    // About Modal Popup
    if (m_showAboutModal) {
        RenderAboutModal(m_width, m_height);
    }

    // Global Search Modal Popup (JADX Search Engine - Ctrl+Shift+F)
    if (m_showSearchModal) {
        RenderSearchModal(m_width, m_height);
    }

    // Permanent IDE Bottom Status Bar (Integrated status notifications) - only when project is open
    if (m_pModel) {
        RenderBottomStatusBar(m_width, m_height);
    }

    // Modern Overlock: Smooth Rounded Outer Border when not maximized
    if (!m_isOffscreen && !IsZoomed(m_hwnd)) {
        D2D1_ROUNDED_RECT rcWindowBorder = D2D1::RoundedRect(
            D2D1::RectF(0.5f, 0.5f, m_width - 0.5f, m_height - 0.5f),
            8.0f, 8.0f
        );
        m_pRenderTarget->DrawRoundedRectangle(&rcWindowBorder, m_bBorderSubtle, 1.2f);
    }

    hr = m_pRenderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
    }
}

void UIWindow::RenderTopTitleBar(float w) {
    D2D1_RECT_F rcBar = D2D1::RectF(0, 0, w, 40.0f);
    m_pRenderTarget->FillRectangle(&rcBar, m_bMenuBg);
    m_pRenderTarget->DrawLine(D2D1::Point2F(0, 40.0f), D2D1::Point2F(w, 40.0f), m_bBorderSubtle, 1.0f);

    // App Logo in Title Bar (Japanese anime cyberpunk logo)
    if (m_pAppLogoBitmap) {
        D2D1_RECT_F rcLogo = D2D1::RectF(8.0f, 6.0f, 36.0f, 34.0f);
        m_pRenderTarget->DrawBitmap(m_pAppLogoBitmap, &rcLogo);
    } else {
        // Fallback Kamon Crest
        VectorIcons::DrawKamon(m_pRenderTarget, 22.0f, 20.0f, 10.0f, m_bAccent, 1.4f);
    }

    // Title
    if (m_fonts.title) {
        m_pRenderTarget->DrawText(L"Kaiseki", 7, m_fonts.title, D2D1::RectF(42.0f, 9.0f, 150.0f, 33.0f), m_bTextPrimary);
    }

    // Top Menu Bar items (File, Inspect, Tools, View, About) - ONLY WHEN PROJECT LOADED
    if (m_pModel) {
        const wchar_t* menuNames[5] = {
            LocW(StringId::Menu_File),
            LocW(StringId::Menu_Inspect),
            LocW(StringId::Menu_Tools),
            LocW(StringId::Menu_View),
            LocW(StringId::Menu_About)
        };
        float menuX = 180.0f;
        float menuY = 8.0f;
        float menuH = 26.0f;

        for (int i = 0; i < 5; ++i) {
            float menuW = (float)wcslen(menuNames[i]) * 8.2f + 16.0f;
            if (menuW < 52.0f) menuW = 52.0f;

            m_rcMenuHeaders[i] = D2D1::RectF(menuX, menuY, menuX + menuW, menuY + menuH);
            bool isActive = (m_activeMenu == i);
            bool isHover = (m_hoverMenuHeader == i);

            if (isActive) {
                KaisekiTheme::DrawCard(m_pRenderTarget, m_rcMenuHeaders[i], m_bWell, m_bAccent, 4.0f);
            } else if (isHover) {
                KaisekiTheme::DrawCard(m_pRenderTarget, m_rcMenuHeaders[i], m_bCardHover, nullptr, 4.0f);
            }

            if (m_fonts.label) {
                m_pRenderTarget->DrawText(menuNames[i], (UINT32)wcslen(menuNames[i]), m_fonts.label,
                    D2D1::RectF(menuX + 8.0f, menuY + 4.0f, menuX + menuW, menuY + menuH),
                    isActive ? m_bAccent : (isHover ? m_bTextPrimary : m_bTextSecondary));
            }

            menuX += menuW + 4.0f;
        }

        // Center Active APK Name badge
        if (m_fonts.subhead) {
            std::wstring apkTag = L"[" + m_pModel->fileName + L"]";
            float tagW = (float)apkTag.length() * 8.5f + 16.0f;
            float tagX = menuX + 16.0f;
            if (tagX + tagW < w - 160.0f) {
                D2D1_RECT_F rcBadge = D2D1::RectF(tagX, 8.0f, tagX + tagW, 32.0f);
                KaisekiTheme::DrawCard(m_pRenderTarget, rcBadge, m_bBadgeBg, m_bBorderSubtle, 4.0f);
                m_pRenderTarget->DrawText(apkTag.c_str(), (UINT32)apkTag.length(), m_fonts.smallText,
                    D2D1::RectF(tagX + 8.0f, 10.0f, tagX + tagW, 30.0f), m_bAccent);
            }
        }
    } else {
        for (int i = 0; i < 6; ++i) {
            m_rcMenuHeaders[i] = D2D1::RectF(0, 0, 0, 0);
        }
    }

    // Right-side Window Controls with smooth hover transitions
    float btnW = 44.0f;
    float rx = w;

    // Close Button
    m_rcWinClose = D2D1::RectF(rx - btnW, 0, rx, 40.0f);
    if (m_hoverWinCloseAlpha > 0.02f) {
        ID2D1SolidColorBrush* bCloseBg = nullptr;
        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.85f, 0.15f, 0.20f, m_hoverWinCloseAlpha), &bCloseBg);
        if (bCloseBg) {
            m_pRenderTarget->FillRectangle(&m_rcWinClose, bCloseBg);
            bCloseBg->Release();
        }
        VectorIcons::DrawWinClose(m_pRenderTarget, m_rcWinClose, m_bTextPrimary);
    } else {
        VectorIcons::DrawWinClose(m_pRenderTarget, m_rcWinClose, m_bTextSecondary);
    }
    rx -= btnW;

    // Maximize Button
    m_rcWinMax = D2D1::RectF(rx - btnW, 0, rx, 40.0f);
    if (m_hoverWinMaxAlpha > 0.02f) {
        ID2D1SolidColorBrush* bHover = nullptr;
        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.20f, 0.22f, 0.28f, m_hoverWinMaxAlpha), &bHover);
        if (bHover) {
            m_pRenderTarget->FillRectangle(&m_rcWinMax, bHover);
            bHover->Release();
        }
        VectorIcons::DrawWinMaximize(m_pRenderTarget, m_rcWinMax, IsZoomed(m_hwnd), m_bTextPrimary);
    } else {
        VectorIcons::DrawWinMaximize(m_pRenderTarget, m_rcWinMax, IsZoomed(m_hwnd), m_bTextSecondary);
    }
    rx -= btnW;

    // Minimize Button
    m_rcWinMin = D2D1::RectF(rx - btnW, 0, rx, 40.0f);
    if (m_hoverWinMinAlpha > 0.02f) {
        ID2D1SolidColorBrush* bHover = nullptr;
        m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.20f, 0.22f, 0.28f, m_hoverWinMinAlpha), &bHover);
        if (bHover) {
            m_pRenderTarget->FillRectangle(&m_rcWinMin, bHover);
            bHover->Release();
        }
        VectorIcons::DrawWinMinimize(m_pRenderTarget, m_rcWinMin, m_bTextPrimary);
    } else {
        VectorIcons::DrawWinMinimize(m_pRenderTarget, m_rcWinMin, m_bTextSecondary);
    }
}

void UIWindow::RenderDropdownMenu(float w, float h) {
    using MenuItem = DropdownItemDef;
    if (m_activeMenu < 0 || m_activeMenu >= 6) return;

    m_currentMenuItems.clear();
    m_rcDropdownItems.clear();

    bool isEn = (KaisekiLang::Get().GetLanguage() == AppLanguage::English);

    if (m_activeMenu == 0) { // File
        m_currentMenuItems = isEn ? std::vector<MenuItem>{
            { L"📂  Open APK File...", L"Ctrl+O", 101, false },
            { L"💾  Save Active File", L"Ctrl+S", 106, false },
            { L"📚  Save All Files", L"Ctrl+Shift+S", 107, false },
            { L"───────────────────────────", L"", 0, true },
            { L"📦  Save Kaiseki Project (.kproj)", L"", 103, false },
            { L"📋  Open Kaiseki Project (.kproj)...", L"Ctrl+Shift+O", 102, false },
            { L"📤  Export Project (Gradle / Android Studio)...", L"Ctrl+E", 104, false },
            { L"───────────────────────────", L"", 0, true },
            { L"🏠  Close Project / Start Screen", L"Ctrl+W", 108, false },
            { L"🚪  Exit", L"Alt+F4", 105, false }
        } : std::vector<MenuItem>{
            { L"📂  APK Dosyası Aç...", L"Ctrl+O", 101, false },
            { L"💾  Aktif Dosyayı Kaydet", L"Ctrl+S", 106, false },
            { L"📚  Tüm Dosyaları Kaydet", L"Ctrl+Shift+S", 107, false },
            { L"───────────────────────────", L"", 0, true },
            { L"📦  Kaiseki Projesini Kaydet (.kproj)", L"", 103, false },
            { L"📋  Kaiseki Projesi Aç (.kproj)...", L"Ctrl+Shift+O", 102, false },
            { L"📤  Projeyi Dışa Aktar (Gradle / Android Studio)...", L"Ctrl+E", 104, false },
            { L"───────────────────────────", L"", 0, true },
            { L"🏠  Projeyi Kapat / Başlangıç Ekranı", L"Ctrl+W", 108, false },
            { L"🚪  Çıkış", L"Alt+F4", 105, false }
        };
    } else if (m_activeMenu == 1) { // Inspect / Search
        m_currentMenuItems = isEn ? std::vector<MenuItem>{
            { L"🔍  Search Text (Code & XML)...", L"Ctrl+Shift+F", 201, false },
            { L"🏷️  Search Class...", L"Ctrl+N", 202, false },
            { L"🔗  Find Xrefs (Cross References)...", L"Ctrl+X", 207, false },
            { L"📦  Open ARSC Resource Table...", L"Ctrl+R", 208, false },
            { L"───────────────────────────", L"", 0, true },
            { L"🚀  Go to Main Activity", L"Ctrl+Shift+A", 203, false },
            { L"📜  Go to AndroidManifest.xml", L"Ctrl+Shift+M", 204, false },
            { L"🛡️  Go to APK Signatures", L"F5", 205, false },
            { L"ℹ️  Go to APK Summary", L"F6", 206, false }
        } : std::vector<MenuItem>{
            { L"🔍  Metin Ara (Kod & XML)...", L"Ctrl+Shift+F", 201, false },
            { L"🏷️  Sınıf Ara (Class Search)...", L"Ctrl+N", 202, false },
            { L"🔗  Çapraz Referans / Xref Bul...", L"Ctrl+X", 207, false },
            { L"📦  ARSC Kaynak Tablosunu Aç...", L"Ctrl+R", 208, false },
            { L"───────────────────────────", L"", 0, true },
            { L"🚀  Ana Activity'ye Git", L"Ctrl+Shift+A", 203, false },
            { L"📜  AndroidManifest.xml'e Git", L"Ctrl+Shift+M", 204, false },
            { L"🛡️  APK İmzasına Git", L"F5", 205, false },
            { L"ℹ️  APK Özetine Git", L"F6", 206, false }
        };
    } else if (m_activeMenu == 2) { // Tools
        m_currentMenuItems = isEn ? std::vector<MenuItem>{
            { L"⚡  Decompile All Classes (Native Engine)", L"F7", 301, false },
            { L"🔨  Repackage & Sign APK", L"F8", 311, false },
            { L"🔄  Toggle Deobfuscation (JOBF)", L"Ctrl+Shift+D", 302, false },
            { L"⚖️  Binary APK Diff...", L"Ctrl+D", 310, false },
            { L"🗺️  Load ProGuard / R8 Mapping (.txt)...", L"", 309, false },
            { L"───────────────────────────", L"", 0, true },
            { L"⚡  Generate Frida Hook Script", L"Ctrl+Shift+H", 306, false },
            { L"🧩  Generate Xposed / LSPosed Module", L"", 307, false },
            { L"📊  Generate Control Flow Graph (CFG)", L"Ctrl+G", 308, false },
            { L"───────────────────────────", L"", 0, true },
            { L"✍️  Sign APK (v1/v2/v3 Scheme)", L"F9", 303, false },
            { L"📱  Deploy to Device (ADB)", L"F10", 304, false },
            { L"🔗  Register .kproj File Association", L"", 305, false }
        } : std::vector<MenuItem>{
            { L"⚡  Yerel DexEngine ile Decompile Et (Tüm Sınıflar)", L"F7", 301, false },
            { L"🔨  Değişiklikleri APK Olarak Paketle & İmzala", L"F8", 311, false },
            { L"🔄  Deobfuscation (Otomatik İsimlendirme) Aç/Kapat", L"Ctrl+Shift+D", 302, false },
            { L"⚖️  İki APK'yı Karşılaştır (APK Diff)...", L"Ctrl+D", 310, false },
            { L"🗺️  ProGuard / R8 Mapping Yükle (.txt)...", L"", 309, false },
            { L"───────────────────────────", L"", 0, true },
            { L"⚡  Frida Hook Scripti Üret", L"Ctrl+Shift+H", 306, false },
            { L"🧩  Xposed / LSPosed Modülü Üret", L"", 307, false },
            { L"📊  Kontrol Akış Grafiği (CFG) Oluştur", L"Ctrl+G", 308, false },
            { L"───────────────────────────", L"", 0, true },
            { L"✍️  APK İmzala (v1/v2/v3 Scheme)", L"F9", 303, false },
            { L"📱  Telefona Yükle (ADB Deploy)", L"F10", 304, false },
            { L"🔗  .kproj Dosya İlişkilendirmesini Kaydet", L"", 305, false }
        };
    } else if (m_activeMenu == 3) { // View
        m_currentMenuItems = isEn ? std::vector<MenuItem>{
            { L"📁  Project Explorer (Source Tree)", L"Ctrl+1", 401, false },
            { L"ℹ️  APK Info Panel", L"Ctrl+2", 402, false },
            { L"✍️  Signer Panel", L"Ctrl+3", 403, false },
            { L"📱  Device Deployment (ADB)", L"Ctrl+4", 404, false },
            { L"🧮  Hex Viewer (BinEd Binary Inspector)", L"Ctrl+H", 406, false },
            { L"⚙️  Settings Panel", L"Ctrl+,", 405, false }
        } : std::vector<MenuItem>{
            { L"📁  Paket Gezgini (Kaynak Ağacı)", L"Ctrl+1", 401, false },
            { L"ℹ️  APK Bilgi Paneli", L"Ctrl+2", 402, false },
            { L"✍️  İmzalayıcı Paneli", L"Ctrl+3", 403, false },
            { L"📱  Telefona Dağıtım (ADB)", L"Ctrl+4", 404, false },
            { L"🧮  Hex Viewer (BinEd İkili Görüntüleyici)", L"Ctrl+H", 406, false },
            { L"⚙️  Ayarlar Paneli", L"Ctrl+,", 405, false }
        };
    }

    if (m_currentMenuItems.empty()) return;

    float dx = m_rcMenuHeaders[m_activeMenu].left;
    float dy = 38.0f;
    float dw = 420.0f;
    float itemH = 30.0f;

    // Ensure dropdown fits inside window client area
    if (dx + dw > w - 12.0f) {
        dx = (std::max)(10.0f, w - dw - 12.0f);
    }

    float dh = 8.0f + m_currentMenuItems.size() * itemH;

    float anim = KaisekiTheme::ClampF(m_animDropdown, 0.0f, 1.0f);
    float scale = 0.94f + 0.06f * KaisekiTheme::EaseOutCubic(anim);
    m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale, D2D1::Point2F(dx, dy)));

    m_rcMenuDropdown = D2D1::RectF(dx, dy, dx + dw, dy + dh);
    KaisekiTheme::DrawCard(m_pRenderTarget, m_rcMenuDropdown, m_bCard, m_bAccent, 6.0f, 1.2f);

    float iy = dy + 4.0f;
    for (size_t i = 0; i < m_currentMenuItems.size(); ++i) {
        const auto& it = m_currentMenuItems[i];
        D2D1_RECT_F rcItem = D2D1::RectF(dx + 4.0f, iy, dx + dw - 4.0f, iy + itemH);
        m_rcDropdownItems.push_back(rcItem);

        if (it.isSeparator) {
            m_pRenderTarget->DrawLine(D2D1::Point2F(dx + 8.0f, iy + itemH * 0.5f), D2D1::Point2F(dx + dw - 8.0f, iy + itemH * 0.5f), m_bBorderSubtle, 1.0f);
        } else {
            bool isHov = (m_hoverDropdownItem == (int)i);
            if (isHov) {
                KaisekiTheme::DrawCard(m_pRenderTarget, rcItem, m_bCardHover, nullptr, 4.0f);
            }

            if (m_fonts.label) {
                m_pRenderTarget->DrawText(it.text.c_str(), (UINT32)it.text.length(), m_fonts.label,
                    D2D1::RectF(rcItem.left + 12.0f, iy + 6.0f, rcItem.right - 105.0f, iy + itemH),
                    isHov ? m_bTextPrimary : m_bTextSecondary);

                if (!it.shortcut.empty()) {
                    m_pRenderTarget->DrawText(it.shortcut.c_str(), (UINT32)it.shortcut.length(), m_fonts.smallText,
                        D2D1::RectF(rcItem.right - 100.0f, iy + 7.0f, rcItem.right - 12.0f, iy + itemH),
                        m_bTextMuted);
                }
            }
        }
        iy += itemH;
    }
    m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
}

void UIWindow::RenderAboutModal(float w, float h) {
    if (m_animAboutModal <= 0.001f) return;
    float anim = KaisekiTheme::ClampF(m_animAboutModal, 0.0f, 1.0f);
    float overlayAlpha = 0.70f * anim;

    D2D1_RECT_F rcScreen = D2D1::RectF(0, 0, w, h);
    ID2D1SolidColorBrush* pOverlay = nullptr;
    m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, overlayAlpha), &pOverlay);
    if (pOverlay) {
        m_pRenderTarget->FillRectangle(&rcScreen, pOverlay);
        pOverlay->Release();
    }

    float cardW = 540.0f;
    float cardH = 340.0f;
    float cx = (w - cardW) * 0.5f;
    float cy = (h - cardH) * 0.5f;
    D2D1_RECT_F rcModal = D2D1::RectF(cx, cy, cx + cardW, cy + cardH);

    float scale = 0.88f + 0.12f * KaisekiTheme::EaseOutBack(anim);
    m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale, D2D1::Point2F(cx + cardW * 0.5f, cy + cardH * 0.5f)));

    KaisekiTheme::DrawCard(m_pRenderTarget, rcModal, m_bCard, m_bAccent, 8.0f, 1.5f);

    if (m_pAppLogoBitmap) {
        D2D1_RECT_F rcLogoModal = D2D1::RectF(cx + 22.0f, cy + 24.0f, cx + 70.0f, cy + 72.0f);
        m_pRenderTarget->DrawBitmap(m_pAppLogoBitmap, &rcLogoModal);
    } else {
        VectorIcons::DrawKamon(m_pRenderTarget, cx + 45.0f, cy + 50.0f, 20.0f, m_bAccent, 1.8f);
    }

    if (m_fonts.hero) {
        m_pRenderTarget->DrawText(L"Kaiseki", 7, m_fonts.hero,
            D2D1::RectF(cx + 80.0f, cy + 22.0f, cx + cardW - 20.0f, cy + 54.0f), m_bTextPrimary);
    }
    if (m_fonts.subhead) {
        std::wstring subTitle = std::wstring(KAISEKI_FULL_VERSION_WSTRING) + L" (High Performance Android Reverse Engineering Suite)";
        m_pRenderTarget->DrawText(subTitle.c_str(), (UINT32)subTitle.length(), m_fonts.subhead,
            D2D1::RectF(cx + 80.0f, cy + 56.0f, cx + cardW - 20.0f, cy + 80.0f), m_bAccent);
    }

    if (m_fonts.body) {
        AppLanguage appLang = KaisekiLang::Get().GetLanguage();
        std::wstring desc;
        if (appLang == AppLanguage::Turkish) {
            desc = L"Geliştirici: anonimbiri  •  GitHub: github.com/GameSketchers/Kaiseki\n"
                   L"Saf Modern C++ (C++17), Windows Direct2D ve DirectWrite ile sıfırdan geliştirilmiş bağımsız Android tersine mühendislik ve APK düzenleme platformu.\n\n"
                   L"• Modüler Mimari: KaisekiCore, KaisekiDex, KaisekiUI, KaisekiSec\n"
                   L"• Dalvik DEX -> Smali & Yüksek Seviyeli Java Dekompilasyonu\n"
                   L"• Binary AXML & ARSC Kaynak Çözümleme Motoru\n"
                   L"• APK Yeniden Paketleme, v1/v2/v3 Blok İmzalama ve Hizalama\n"
                   L"• Canlı ADB Cihaz Dağıtımı ve Android Logcat Filtreleme";
        } else if (appLang == AppLanguage::German) {
            desc = L"Entwickler: anonimbiri  •  GitHub: github.com/GameSketchers/Kaiseki\n"
                   L"Unabhängige Android APK Reverse-Engineering- und Modding-Suite, von Grund auf in modernem C++ (C++17) und Direct2D entwickelt.\n\n"
                   L"• Modulare Architektur: KaisekiCore, KaisekiDex, KaisekiUI, KaisekiSec\n"
                   L"• Dalvik DEX -> Smali & High-Level Java Dekompilierung\n"
                   L"• Binärer AXML & ARSC Ressourcen-Dekodierer\n"
                   L"• APK-Neuverpackung, v1/v2/v3 Signatur-Engine & Zipalign\n"
                   L"• Live ADB-Gerätebereitstellung & Logcat-Filterung";
        } else if (appLang == AppLanguage::Japanese) {
            desc = L"開発者: anonimbiri  •  GitHub: github.com/GameSketchers/Kaiseki\n"
                   L"モダンC++ (C++17) とDirect2Dでゼロから開発された高機能Android APKリバースエンジニアリング＆逆アセンブルスイート。\n\n"
                   L"• モジュール設計: KaisekiCore, KaisekiDex, KaisekiUI, KaisekiSec\n"
                   L"• Dalvik DEX -> Smali & 高水準Java逆コンパイル\n"
                   L"• バイナリ AXML & ARSC リソース解析エンジン\n"
                   L"• APK再構築・v1/v2/v3 署名ブロック生成 & Zipalign\n"
                   L"• リアルタイムADB実機インストール & Logcatフィルタリング";
        } else {
            desc = L"Developer: anonimbiri  •  GitHub: github.com/GameSketchers/Kaiseki\n"
                   L"High-performance standalone Android APK reverse-engineering & modding suite built from scratch in pure Modern C++ (C++17) and Direct2D.\n\n"
                   L"• Modular Architecture: KaisekiCore, KaisekiDex, KaisekiUI, KaisekiSec\n"
                   L"• Dalvik DEX -> Smali & High-Level Java AST Decompiler\n"
                   L"• Binary AXML & ARSC Resource Decoding Engine\n"
                   L"• APK Repackaging, v1/v2/v3 Signature Block Engine & Zipalign\n"
                   L"• Live ADB Device Deployment & Filtered Android Logcat";
        }

        m_pRenderTarget->DrawText(desc.c_str(), (UINT32)desc.length(), m_fonts.body,
            D2D1::RectF(cx + 30.0f, cy + 96.0f, cx + cardW - 30.0f, cy + 285.0f), m_bTextSecondary);
    }

    m_rcAboutClose = D2D1::RectF(cx + cardW - 120.0f, cy + cardH - 44.0f, cx + cardW - 24.0f, cy + cardH - 14.0f);
    KaisekiTheme::DrawCard(m_pRenderTarget, m_rcAboutClose, m_bAccent, nullptr, 4.0f);
    if (m_fonts.button) {
        bool isTr = (KaisekiLang::Get().GetLanguage() == AppLanguage::Turkish);
        const wchar_t* btnCloseTxt = isTr ? L"Kapat" : L"Close";
        m_pRenderTarget->DrawText(btnCloseTxt, (UINT32)wcslen(btnCloseTxt), m_fonts.button,
            D2D1::RectF(m_rcAboutClose.left + 22.0f, m_rcAboutClose.top + 5.0f, m_rcAboutClose.right, m_rcAboutClose.bottom), m_bTextPrimary);
    }
    m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
}

void UIWindow::RenderSearchModal(float w, float h) {
    if (m_animSearchModal <= 0.001f) return;
    float anim = KaisekiTheme::ClampF(m_animSearchModal, 0.0f, 1.0f);
    float overlayAlpha = 0.70f * anim;

    D2D1_RECT_F rcBackdrop = D2D1::RectF(0.0f, 0.0f, w, h);
    ID2D1SolidColorBrush* bBackdrop = nullptr;
    m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, overlayAlpha), &bBackdrop);
    if (bBackdrop) {
        m_pRenderTarget->FillRectangle(&rcBackdrop, bBackdrop);
        bBackdrop->Release();
    }

    float modalW = KaisekiTheme::MinF(780.0f, w - 80.0f);
    float modalH = KaisekiTheme::MinF(540.0f, h - 80.0f);
    float mx = (w - modalW) * 0.5f;
    float my = (h - modalH) * 0.5f;

    float scale = 0.88f + 0.12f * KaisekiTheme::EaseOutBack(anim);
    m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale, D2D1::Point2F(mx + modalW * 0.5f, my + modalH * 0.5f)));

    D2D1_RECT_F rcModal = D2D1::RectF(mx, my, mx + modalW, my + modalH);
    KaisekiTheme::DrawCard(m_pRenderTarget, rcModal, m_bPanel, m_bAccent, 10.0f, 1.5f);

    // Header
    float hdrH = 46.0f;
    D2D1_RECT_F rcHdr = D2D1::RectF(mx, my, mx + modalW, my + hdrH);
    m_pRenderTarget->FillRectangle(&rcHdr, m_bCard);
    m_pRenderTarget->DrawLine(D2D1::Point2F(mx, my + hdrH), D2D1::Point2F(mx + modalW, my + hdrH), m_bBorderSubtle, 1.0f);

    if (m_fonts.title) {
        std::wstring sTitle = L"🔍 Genel Arama (Kaiseki Semantik Kod Arama - Ctrl+Shift+F)";
        m_pRenderTarget->DrawText(sTitle.c_str(), (UINT32)sTitle.length(), m_fonts.title,
            D2D1::RectF(mx + 20.0f, my + 10.0f, mx + modalW - 60.0f, my + hdrH), m_bTextPrimary);
    }

    // Close button
    m_rcSearchClose = D2D1::RectF(mx + modalW - 40.0f, my + 8.0f, mx + modalW - 10.0f, my + 38.0f);
    VectorIcons::DrawTabClose(m_pRenderTarget, m_rcSearchClose, m_bTextMuted);

    // Search Type Tabs: Sınıflar, Metotlar, Kod Metni, Kaynaklar
    float tabY = my + hdrH + 12.0f;
    const wchar_t* tabTitles[] = { L"🏷️ Sınıflar (Classes)", L"⚡ Metotlar (Methods)", L"📝 Kod Metni (Text)", L"🎨 Kaynaklar (Resources)" };
    float tabW = (modalW - 40.0f) / 4.0f;
    for (int t = 0; t < 4; ++t) {
        m_rcSearchTypeTabs[t] = D2D1::RectF(mx + 20.0f + t * tabW, tabY, mx + 20.0f + (t + 1) * tabW - 4.0f, tabY + 30.0f);
        bool isActive = (m_searchType == t);
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcSearchTypeTabs[t], isActive ? m_bCardHover : m_bWell, isActive ? m_bAccent : m_bBorderSubtle, 4.0f);
        if (m_fonts.smallText) {
            m_pRenderTarget->DrawText(tabTitles[t], (UINT32)wcslen(tabTitles[t]), m_fonts.smallText,
                D2D1::RectF(m_rcSearchTypeTabs[t].left + 6.0f, tabY + 6.0f, m_rcSearchTypeTabs[t].right, tabY + 28.0f),
                isActive ? m_bAccent : m_bTextSecondary);
        }
    }

    // Input Box
    float inputY = tabY + 38.0f;
    m_rcSearchInput = D2D1::RectF(mx + 20.0f, inputY, mx + modalW - 20.0f, inputY + 36.0f);
    KaisekiTheme::DrawCard(m_pRenderTarget, m_rcSearchInput, m_bWell, m_bAccent, 4.0f, 1.2f);

    std::wstring displaySearch = m_searchQuery.empty() ? L"Aramak istediğiniz terimi yazın (örn: MainActivity, onClick, https)..." : (m_searchQuery + L"|");
    if (m_fonts.body) {
        m_pRenderTarget->DrawText(displaySearch.c_str(), (UINT32)displaySearch.length(), m_fonts.body,
            D2D1::RectF(m_rcSearchInput.left + 12.0f, inputY + 8.0f, m_rcSearchInput.right - 12.0f, inputY + 32.0f),
            m_searchQuery.empty() ? m_bTextMuted : m_bTextPrimary);
    }

    // Results List Header
    float listY = inputY + 44.0f;
    float listH = my + modalH - listY - 14.0f;
    D2D1_RECT_F rcList = D2D1::RectF(mx + 20.0f, listY, mx + modalW - 20.0f, listY + listH);
    KaisekiTheme::DrawCard(m_pRenderTarget, rcList, m_bWell, m_bBorderSubtle, 4.0f);

    m_rcSearchResultRows.clear();
    if (m_searchResults.empty()) {
        if (m_fonts.subhead) {
            std::wstring noRes = m_searchQuery.empty() ? L"Arama yapmak için yukarıya bir ifade yazın." : L"Hiçbir eşleşme bulunamadı.";
            m_pRenderTarget->DrawText(noRes.c_str(), (UINT32)noRes.length(), m_fonts.subhead,
                D2D1::RectF(rcList.left + 20.0f, listY + 30.0f, rcList.right, listY + 60.0f), m_bTextMuted);
        }
    } else {
        float rowH = 26.0f;
        size_t maxRows = (size_t)(listH / rowH);
        for (size_t r = 0; r < m_searchResults.size() && r < maxRows; ++r) {
            float ry = listY + r * rowH;
            D2D1_RECT_F rcRow = D2D1::RectF(rcList.left, ry, rcList.right, ry + rowH);
            m_rcSearchResultRows.push_back(rcRow);

            bool isHov = (m_hoverSearchResult == (int)r);
            if (isHov) {
                m_pRenderTarget->FillRectangle(&rcRow, m_bCardHover);
            }

            const auto& item = m_searchResults[r];
            std::wstring wTitle = KaisekiTheme::Utf8ToWide(item.title);
            std::wstring wLoc = KaisekiTheme::Utf8ToWide(item.location);
            std::wstring wSnip = KaisekiTheme::Utf8ToWide(item.snippet);

            if (m_fonts.smallText) {
                // Title
                m_pRenderTarget->DrawText(wTitle.c_str(), (UINT32)wTitle.length(), m_fonts.smallText,
                    D2D1::RectF(rcRow.left + 10.0f, ry + 4.0f, rcRow.left + 220.0f, ry + rowH), m_bAccent);
                // Location
                m_pRenderTarget->DrawText(wLoc.c_str(), (UINT32)wLoc.length(), m_fonts.smallText,
                    D2D1::RectF(rcRow.left + 225.0f, ry + 4.0f, rcRow.left + 440.0f, ry + rowH), m_bTextSecondary);
                // Snippet
                m_pRenderTarget->DrawText(wSnip.c_str(), (UINT32)wSnip.length(), m_fonts.smallText,
                    D2D1::RectF(rcRow.left + 445.0f, ry + 4.0f, rcRow.right - 10.0f, ry + rowH), m_bTextMuted);
            }
        }
    }
    m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
}

void UIWindow::RenderSettingsModal(float w, float h) {
    if (m_animSettingsModal <= 0.001f) return;
    float anim = KaisekiTheme::ClampF(m_animSettingsModal, 0.0f, 1.0f);
    float overlayAlpha = 0.70f * anim;

    D2D1_RECT_F rcScreen = D2D1::RectF(0, 0, w, h);
    ID2D1SolidColorBrush* pOverlay = nullptr;
    m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, overlayAlpha), &pOverlay);
    if (pOverlay) {
        m_pRenderTarget->FillRectangle(&rcScreen, pOverlay);
        pOverlay->Release();
    }

    float cardW = 900.0f;
    float cardH = 580.0f;
    float cx = (w - cardW) * 0.5f;
    float cy = (h - cardH) * 0.5f;
    D2D1_RECT_F rcModal = D2D1::RectF(cx, cy, cx + cardW, cy + cardH);

    float scale = 0.88f + 0.12f * KaisekiTheme::EaseOutBack(anim);
    m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale, D2D1::Point2F(cx + cardW * 0.5f, cy + cardH * 0.5f)));

    KaisekiTheme::DrawCard(m_pRenderTarget, rcModal, m_bCard, m_bAccent, 8.0f, 1.5f);

    // Modal Header (App Icon in Title)
    if (m_pAppLogoBitmap) {
        D2D1_RECT_F rcAppIcon = D2D1::RectF(cx + 18.0f, cy + 12.0f, cx + 42.0f, cy + 36.0f);
        m_pRenderTarget->DrawBitmap(m_pAppLogoBitmap, &rcAppIcon);
    } else {
        VectorIcons::DrawKamon(m_pRenderTarget, cx + 28.0f, cy + 24.0f, 12.0f, m_bAccent);
    }
    if (m_fonts.title) {
        const wchar_t* settModalTitle = LocW(StringId::Settings_Title);
        m_pRenderTarget->DrawText(settModalTitle, (UINT32)wcslen(settModalTitle), m_fonts.title,
            D2D1::RectF(cx + 50.0f, cy + 14.0f, cx + 500.0f, cy + 38.0f), m_bTextPrimary);
    }

    m_rcSettingsClose = D2D1::RectF(cx + cardW - 38.0f, cy + 14.0f, cx + cardW - 14.0f, cy + 38.0f);
    VectorIcons::DrawTabClose(m_pRenderTarget, m_rcSettingsClose, m_bTextMuted);

    m_pRenderTarget->DrawLine(D2D1::Point2F(cx, cy + 48.0f), D2D1::Point2F(cx + cardW, cy + 48.0f), m_bBorderSubtle, 1.0f);

    // Left Sub-Tabs (Width 210px) - 7 Consolidated Categories
    float railW = 210.0f;
    D2D1_RECT_F rcTabsRail = D2D1::RectF(cx, cy + 48.0f, cx + railW, cy + cardH);
    m_pRenderTarget->FillRectangle(&rcTabsRail, m_bPanel);
    m_pRenderTarget->DrawLine(D2D1::Point2F(cx + railW, cy + 48.0f), D2D1::Point2F(cx + railW, cy + cardH), m_bBorderSubtle, 1.0f);

    const wchar_t* subTabs[7] = {
        LocW(StringId::Settings_TabGeneral),      // 0: Genel
        LocW(StringId::Settings_TabTheme),        // 1: Görünüm
        LocW(StringId::Settings_TabLanguage),     // 2: Dil
        LocW(StringId::Settings_TabTools),        // 3: Android Araçları
        LocW(StringId::Settings_TabDecompile),    // 4: APK İşlemleri
        LocW(StringId::Settings_TabHotkeys),      // 5: Kısayollar
        LocW(StringId::Settings_TabAdvanced)      // 6: Gelişmiş ve Tanılama
    };
    float tabY = cy + 54.0f;
    float tabH = 38.0f;

    for (int i = 0; i < 7; ++i) {
        m_rcSettingsTabs[i] = D2D1::RectF(cx + 8.0f, tabY, cx + railW - 8.0f, tabY + tabH);
        bool isAct = (m_settingsTab == i);

        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcSettingsTabs[i], isAct ? m_bCardHover : m_bPanel, isAct ? m_bAccent : nullptr, 6.0f);

        if (m_fonts.label) {
            m_pRenderTarget->DrawText(subTabs[i], (UINT32)wcslen(subTabs[i]), m_fonts.label,
                D2D1::RectF(m_rcSettingsTabs[i].left + 12.0f, tabY + 9.0f, m_rcSettingsTabs[i].right - 4.0f, tabY + tabH),
                isAct ? m_bAccent : m_bTextSecondary);
        }
        tabY += tabH + 6.0f;
    }

    // Right Content Panel
    float contentX = cx + railW + 24.0f;
    float contentY = cy + 60.0f;
    float contentW = cardW - railW - 48.0f;

    if (m_settingsTab == 0) {
        // TAB 0: GENEL (Workspace, Restore, .kproj)
        if (m_fonts.subhead) {
            m_pRenderTarget->DrawText(L"1. Varsayılan Çalışma Alanı (Workspace):", 40, m_fonts.subhead,
                D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 22.0f), m_bTextPrimary);
        }
        contentY += 28.0f;

        std::wstring wsPath = SettingsManager::Get().settings.workspacePath;
        if (wsPath.empty()) wsPath = SettingsManager::Get().GetDefaultTempDir();
        D2D1_RECT_F rcWsBox = D2D1::RectF(contentX, contentY, contentX + contentW - 130.0f, contentY + 36.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, rcWsBox, m_bWell, m_bBorderSubtle, 5.0f);
        if (m_fonts.body) {
            m_pRenderTarget->DrawText(wsPath.c_str(), (UINT32)wsPath.length(), m_fonts.body,
                D2D1::RectF(rcWsBox.left + 10.0f, contentY + 8.0f, rcWsBox.right - 10.0f, rcWsBox.bottom), m_bTextPrimary);
        }
        m_rcBtnSelectWorkspace = D2D1::RectF(contentX + contentW - 120.0f, contentY, contentX + contentW, contentY + 36.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBtnSelectWorkspace, m_bCardHover, m_bAccent, 5.0f);
        if (m_fonts.button) {
            m_pRenderTarget->DrawText(L"📂 Klasör Seç", 13, m_fonts.button,
                D2D1::RectF(m_rcBtnSelectWorkspace.left + 8.0f, contentY + 8.0f, m_rcBtnSelectWorkspace.right, m_rcBtnSelectWorkspace.bottom), m_bTextPrimary);
        }
        contentY += 48.0f;

        if (m_fonts.subhead) {
            m_pRenderTarget->DrawText(L"2. Oturum ve Proje Hatırlama:", 30, m_fonts.subhead,
                D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 22.0f), m_bTextPrimary);
        }
        contentY += 28.0f;

        m_rcToggleRestoreWorkspace = D2D1::RectF(contentX, contentY, contentX + 460.0f, contentY + 38.0f);
        bool restoreWs = SettingsManager::Get().settings.restoreWorkspace;
        DrawToggleSwitch(m_rcToggleRestoreWorkspace, restoreWs, L"Önceki Çalışma Alanını Otomatik Yükle", L"Açılışta son açık APK projesini ve sekmeleri geri yükler.");
        contentY += 52.0f;

        if (m_fonts.subhead) {
            m_pRenderTarget->DrawText(L"3. Windows Dosya İlişkilendirmesi:", 33, m_fonts.subhead,
                D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 22.0f), m_bTextPrimary);
        }
        contentY += 28.0f;
        m_rcBtnAssocKproj = D2D1::RectF(contentX, contentY, contentX + 380.0f, contentY + 40.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBtnAssocKproj, m_bCardHover, m_bAccent, 6.0f);
        if (m_fonts.button) {
            const wchar_t* szAssoc = L"🔗  .kproj Dosyalarını Kaiseki ile İlişkilendir";
            m_pRenderTarget->DrawText(szAssoc, (UINT32)wcslen(szAssoc), m_fonts.button,
                D2D1::RectF(m_rcBtnAssocKproj.left + 14.0f, contentY + 10.0f, m_rcBtnAssocKproj.right, contentY + 36.0f), m_bTextPrimary);
        }
        contentY += 52.0f;

        if (m_fonts.subhead) {
            m_pRenderTarget->DrawText(L"4. Yapılandırma Dosyası (%APPDATA%):", 35, m_fonts.subhead,
                D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 22.0f), m_bTextPrimary);
        }
        contentY += 28.0f;
        std::wstring cfgPath = L"%APPDATA%\\Kaiseki\\settings.xml";
        if (m_fonts.code) {
            m_pRenderTarget->DrawText(cfgPath.c_str(), (UINT32)cfgPath.length(), m_fonts.code,
                D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 22.0f), m_bAccent);
        }
    } else if (m_settingsTab == 1) {
        // TAB 1: GÖRÜNÜM (Accent Palette, Background Palette & Custom Colors, Toggle Switches)
        if (m_fonts.subhead) {
            m_pRenderTarget->DrawText(L"1. Vurgu Rengi (Accent Palette):", 31, m_fonts.subhead,
                D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 22.0f), m_bTextPrimary);
        }
        contentY += 28.0f;

        m_rcColorSquareAccent = D2D1::RectF(contentX, contentY, contentX + 38.0f, contentY + 38.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcColorSquareAccent, m_bAccent, m_bBorderHover, 6.0f, 2.0f);

        if (m_fonts.body) {
            std::wstring hexCode = KaisekiTheme::ColorToHexW(m_customAccentColor);
            std::wstring lbl = L"Seçili Renk: " + hexCode + L"  (Tıklayarak Renk Seçiciyi Açın)";
            m_pRenderTarget->DrawText(lbl.c_str(), (UINT32)lbl.length(), m_fonts.body,
                D2D1::RectF(contentX + 50.0f, contentY + 8.0f, contentX + contentW, contentY + 32.0f), m_bTextPrimary);
        }
        contentY += 48.0f;

        // Swatches
        m_rcSwatches.clear();
        const auto& swatches = KaisekiTheme::GetPresetSwatches();
        float swW = 86.0f;
        float swH = 44.0f;
        for (size_t i = 0; i < swatches.size() && i < 12; ++i) {
            int row = (int)i / 6;
            int col = (int)i % 6;
            float sx = contentX + col * (swW + 8.0f);
            float sy = contentY + row * (swH + 8.0f);
            D2D1_RECT_F rcSw = D2D1::RectF(sx, sy, sx + swW, sy + swH);
            m_rcSwatches.push_back(rcSw);

            ID2D1SolidColorBrush* bSw = nullptr;
            m_pRenderTarget->CreateSolidColorBrush(swatches[i].color, &bSw);
            if (bSw) {
                KaisekiTheme::DrawCard(m_pRenderTarget, rcSw, m_bWell, m_bBorderSubtle, 4.0f);
                m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(sx + 4.0f, sy + 4.0f, sx + swW - 4.0f, sy + 22.0f), 3.0f, 3.0f), bSw);
                bSw->Release();
            }
            if (m_fonts.smallText) {
                m_pRenderTarget->DrawText(swatches[i].name, (UINT32)wcslen(swatches[i].name), m_fonts.smallText,
                    D2D1::RectF(sx + 4.0f, sy + 25.0f, sx + swW, sy + 41.0f), m_bTextSecondary);
            }
        }
        contentY += 112.0f;

        // 2. Arka Plan Yüzey Tonları (Background Palette)
        if (m_fonts.subhead) {
            m_pRenderTarget->DrawText(L"2. Arka Plan Yüzey Tonları (Background Palette):", 47, m_fonts.subhead,
                D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 22.0f), m_bTextPrimary);
        }
        contentY += 28.0f;

        m_rcColorSquareBg = D2D1::RectF(contentX, contentY, contentX + 38.0f, contentY + 38.0f);
        ID2D1SolidColorBrush* bBgPreview = nullptr;
        m_pRenderTarget->CreateSolidColorBrush(m_colors.canvasBg, &bBgPreview);
        if (bBgPreview) {
            KaisekiTheme::DrawCard(m_pRenderTarget, m_rcColorSquareBg, bBgPreview, m_bBorderHover, 6.0f, 2.0f);
            bBgPreview->Release();
        }

        if (m_fonts.body) {
            std::wstring bgHex = KaisekiTheme::ColorToHexW(m_colors.canvasBg);
            std::wstring lbl = L"Seçili Renk: " + bgHex + L"  (Tıklayarak Renk Seçiciyi Açın)";
            m_pRenderTarget->DrawText(lbl.c_str(), (UINT32)lbl.length(), m_fonts.body,
                D2D1::RectF(contentX + 50.0f, contentY + 8.0f, contentX + contentW, contentY + 32.0f), m_bTextPrimary);
        }
        contentY += 46.0f;

        // 6 Preset background shade swatches
        struct BgPresetInfo { const wchar_t* name; D2D1_COLOR_F color; };
        BgPresetInfo bgPresets[6] = {
            { L"Kömür", D2D1::ColorF(0.106f, 0.110f, 0.133f) },        // #1B1C22
            { L"OLED Siyah", D2D1::ColorF(0.043f, 0.043f, 0.051f) },   // #0B0B0D
            { L"Gece Mavisi", D2D1::ColorF(0.063f, 0.078f, 0.122f) },  // #10141F
            { L"İmparatorluk", D2D1::ColorF(0.094f, 0.067f, 0.102f) }, // #18111A
            { L"Derin Arduvaz", D2D1::ColorF(0.070f, 0.075f, 0.086f) },// #121316
            { L"Koyu Obsidyen", D2D1::ColorF(0.055f, 0.067f, 0.090f) } // #0E1117
        };

        float bgCardW = 95.0f;
        float bgCardH = 40.0f;
        for (int b = 0; b < 6; ++b) {
            float sx = contentX + b * (bgCardW + 8.0f);
            float sy = contentY;
            D2D1_RECT_F rcBg = D2D1::RectF(sx, sy, sx + bgCardW, sy + bgCardH);
            m_rcBgShades[b] = rcBg;
            bool isCur = ((int)m_currentBgShade == b);

            ID2D1SolidColorBrush* bBgCard = nullptr;
            m_pRenderTarget->CreateSolidColorBrush(bgPresets[b].color, &bBgCard);
            if (bBgCard) {
                KaisekiTheme::DrawCard(m_pRenderTarget, rcBg, bBgCard, isCur ? m_bAccent : m_bBorderSubtle, 5.0f, isCur ? 2.0f : 1.0f);
                bBgCard->Release();
            }
            if (m_fonts.smallText) {
                m_pRenderTarget->DrawText(bgPresets[b].name, (UINT32)wcslen(bgPresets[b].name), m_fonts.smallText,
                    D2D1::RectF(sx + 4.0f, sy + 12.0f, sx + bgCardW - 4.0f, sy + 30.0f), isCur ? m_bAccent : m_bTextPrimary);
            }
        }
        contentY += 52.0f;

        // 3. Kod Editörü Tercihleri (Sliding Toggle Switches)
        if (m_fonts.subhead) {
            m_pRenderTarget->DrawText(L"3. Kod Editörü Tercihleri:", 26, m_fonts.subhead,
                D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 22.0f), m_bTextPrimary);
        }
        contentY += 28.0f;

        m_rcToggleLineNumbers = D2D1::RectF(contentX, contentY, contentX + 270.0f, contentY + 36.0f);
        bool showLnum = SettingsManager::Get().settings.showLineNumbers;
        DrawToggleSwitch(m_rcToggleLineNumbers, showLnum, L"Satır Numaraları", showLnum ? L"Açık" : L"Kapalı");

        m_rcToggleMinimap = D2D1::RectF(contentX + 285.0f, contentY, contentX + 575.0f, contentY + 36.0f);
        bool showMmap = SettingsManager::Get().settings.showMinimap;
        DrawToggleSwitch(m_rcToggleMinimap, showMmap, L"Kod Haritası (Minimap)", showMmap ? L"Açık" : L"Kapalı");

        if (m_colorPickerPopoverOpen) {
            float popoverY = (m_colorPickerTarget == 0) ? (contentY - 260.0f) : (contentY - 200.0f);
            RenderColorWheelPopover(contentX + 20.0f, popoverY);
        }
    } else if (m_settingsTab == 2) {
        // TAB 2: DİL (Compact Dropdown Popover)
        if (m_fonts.subhead) {
            m_pRenderTarget->DrawText(L"Uygulama Arayüz Dili (Language):", 32, m_fonts.subhead,
                D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 24.0f), m_bTextPrimary);
        }
        contentY += 28.0f;

        if (m_fonts.label) {
            const wchar_t* lDesc = LocW(StringId::Settings_LangDesc);
            m_pRenderTarget->DrawText(lDesc, (UINT32)wcslen(lDesc), m_fonts.label,
                D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 36.0f), m_bTextSecondary);
        }
        contentY += 42.0f;

        // Compact Dropdown: Uygulama Dili: [ Türkçe (Türkiye) ▼ ]
        if (m_fonts.body) {
            m_pRenderTarget->DrawText(L"Uygulama Dili:", 14, m_fonts.body,
                D2D1::RectF(contentX, contentY + 7.0f, contentX + 130.0f, contentY + 32.0f), m_bTextPrimary);
        }

        m_rcLangDropdownBtn = D2D1::RectF(contentX + 135.0f, contentY, contentX + 420.0f, contentY + 36.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcLangDropdownBtn, m_bCardHover, m_langDropdownOpen ? m_bAccent : m_bBorderHover, 5.0f);

        std::string curCode = KaisekiLang::Get().GetLanguageCode();
        std::wstring curDisplay = L"Türkçe (Türkiye)";
        if (curCode == "en") curDisplay = L"English (United States)";
        else if (curCode == "de") curDisplay = L"Deutsch (Deutschland)";
        else if (curCode == "ja") curDisplay = L"日本語 (Japan)";
        else {
            const auto& langs = KaisekiLang::Get().GetAvailableLanguages();
            for (const auto& l : langs) {
                if (l.code == curCode) { curDisplay = l.displayName; break; }
            }
        }

        if (m_fonts.body) {
            m_pRenderTarget->DrawText(curDisplay.c_str(), (UINT32)curDisplay.length(), m_fonts.body,
                D2D1::RectF(m_rcLangDropdownBtn.left + 14.0f, contentY + 8.0f, m_rcLangDropdownBtn.right - 30.0f, m_rcLangDropdownBtn.bottom), m_bTextPrimary);
            m_pRenderTarget->DrawText(m_langDropdownOpen ? L"▲" : L"▼", 1, m_fonts.smallText,
                D2D1::RectF(m_rcLangDropdownBtn.right - 26.0f, contentY + 10.0f, m_rcLangDropdownBtn.right - 8.0f, m_rcLangDropdownBtn.bottom), m_bAccent);
        }
        contentY += 56.0f;

        if (m_fonts.smallText) {
            const wchar_t* szLangDesc = L"Kaiseki çok dilli arayüz motoru (Türkçe, English, Deutsch, 日本語) desteklemektedir.";
            m_pRenderTarget->DrawText(szLangDesc, (UINT32)wcslen(szLangDesc), m_fonts.smallText,
                D2D1::RectF(contentX, contentY + 8.0f, contentX + contentW, contentY + 28.0f), m_bTextMuted);
        }

        // Render Dropdown Popover if open
        if (m_langDropdownOpen) {
            const auto& langs = KaisekiLang::Get().GetAvailableLanguages();
            float popW = m_rcLangDropdownBtn.right - m_rcLangDropdownBtn.left;
            float optH = 34.0f;
            float popH = (float)langs.size() * optH + 8.0f;
            D2D1_RECT_F rcPop = D2D1::RectF(m_rcLangDropdownBtn.left, m_rcLangDropdownBtn.bottom + 2.0f, m_rcLangDropdownBtn.left + popW, m_rcLangDropdownBtn.bottom + 2.0f + popH);
            KaisekiTheme::DrawCard(m_pRenderTarget, rcPop, m_bWell, m_bAccent, 6.0f, 1.5f);

            m_rcLangDropdownOptions.clear();
            for (size_t i = 0; i < langs.size(); ++i) {
                float oy = rcPop.top + 4.0f + i * optH;
                D2D1_RECT_F rcOpt = D2D1::RectF(rcPop.left + 4.0f, oy, rcPop.right - 4.0f, oy + optH);
                m_rcLangDropdownOptions.push_back(rcOpt);

                bool isSel = (langs[i].code == curCode);
                if (isSel) {
                    m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(rcOpt, 4.0f, 4.0f), m_bCardHover);
                }
                if (m_fonts.body) {
                    m_pRenderTarget->DrawText(langs[i].displayName.c_str(), (UINT32)langs[i].displayName.length(), m_fonts.body,
                        D2D1::RectF(rcOpt.left + 10.0f, oy + 7.0f, rcOpt.right - 10.0f, oy + optH), isSel ? m_bAccent : m_bTextPrimary);
                }
            }
        }
    } else if (m_settingsTab == 3) {
        // TAB 3: ANDROID ARAÇLARI (SDK Path, Validation Badge, Component Status, Download Dialog Launcher)
        if (m_fonts.subhead) {
            m_pRenderTarget->DrawText(L"Android SDK ve Araçlar Durumu:", 31, m_fonts.subhead,
                D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 22.0f), m_bTextPrimary);
        }
        contentY += 28.0f;

        // SDK Path TextBox + Browse Button + Live Status Badge
        std::wstring sdkPath = SettingsManager::Get().settings.androidSdkPath;
        if (sdkPath.empty()) sdkPath = SettingsManager::Get().FindAndroidSdkRoot();
        D2D1_RECT_F rcSdkBox = D2D1::RectF(contentX, contentY, contentX + contentW - 250.0f, contentY + 36.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, rcSdkBox, m_bWell, m_bBorderSubtle, 5.0f);
        if (m_fonts.body) {
            std::wstring dispPath = sdkPath.empty() ? L"(Android SDK dizini ayarlanmadı)" : sdkPath;
            m_pRenderTarget->DrawText(dispPath.c_str(), (UINT32)dispPath.length(), m_fonts.body,
                D2D1::RectF(rcSdkBox.left + 10.0f, contentY + 8.0f, rcSdkBox.right - 10.0f, rcSdkBox.bottom),
                sdkPath.empty() ? m_bTextMuted : m_bTextPrimary);
        }

        m_rcBtnSelectSdk = D2D1::RectF(contentX + contentW - 240.0f, contentY, contentX + contentW - 120.0f, contentY + 36.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBtnSelectSdk, m_bCardHover, m_bAccent, 5.0f);
        if (m_fonts.button) {
            m_pRenderTarget->DrawText(L"📂 Klasör Seç", 13, m_fonts.button,
                D2D1::RectF(m_rcBtnSelectSdk.left + 8.0f, contentY + 8.0f, m_rcBtnSelectSdk.right, m_rcBtnSelectSdk.bottom), m_bTextPrimary);
        }

        // Live Status Badge
        auto sdkStatus = SettingsManager::Get().ValidateSdkPath();
        D2D1_RECT_F rcBadge = D2D1::RectF(contentX + contentW - 110.0f, contentY, contentX + contentW, contentY + 36.0f);
        ID2D1SolidColorBrush* bStatusCol = (sdkStatus == SdkValidationStatus::Ready) ? m_bSuccess :
            (sdkStatus == SdkValidationStatus::MissingComponents ? m_bWarning : m_bCloseRed);
        KaisekiTheme::DrawCard(m_pRenderTarget, rcBadge, m_bWell, bStatusCol, 5.0f);
        if (m_fonts.button) {
            const wchar_t* badgeTxt = (sdkStatus == SdkValidationStatus::Ready) ? L"✔ Hazır" :
                (sdkStatus == SdkValidationStatus::MissingComponents ? L"⚠ Eksik" : L"✖ Geçersiz");
            m_pRenderTarget->DrawText(badgeTxt, (UINT32)wcslen(badgeTxt), m_fonts.button,
                D2D1::RectF(rcBadge.left + 8.0f, contentY + 8.0f, rcBadge.right, rcBadge.bottom), bStatusCol);
        }
        contentY += 48.0f;

        // Components Status Table
        std::wstring adb = SettingsManager::Get().FindAdbPath();
        std::wstring bt = SettingsManager::Get().FindBuildToolsPath();
        std::wstring java = SettingsManager::Get().FindJavaPath();

        auto drawCompRow = [&](const wchar_t* name, const std::wstring& path, bool ok) {
            D2D1_RECT_F rcRow = D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 34.0f);
            KaisekiTheme::DrawCard(m_pRenderTarget, rcRow, m_bWell, ok ? m_bSuccess : m_bBorderSubtle, 4.0f);
            if (m_fonts.body) {
                m_pRenderTarget->DrawText(name, (UINT32)wcslen(name), m_fonts.body,
                    D2D1::RectF(rcRow.left + 10.0f, contentY + 7.0f, rcRow.left + 220.0f, rcRow.bottom), ok ? m_bTextPrimary : m_bTextMuted);
            }
            if (m_fonts.smallText) {
                std::wstring pStr = ok ? (L"✓ " + path) : L"✕ Bulunamadı";
                m_pRenderTarget->DrawText(pStr.c_str(), (UINT32)pStr.length(), m_fonts.smallText,
                    D2D1::RectF(rcRow.left + 225.0f, contentY + 8.0f, rcRow.right - 10.0f, rcRow.bottom), ok ? m_bSuccess : m_bWarning);
            }
            contentY += 40.0f;
        };

        drawCompRow(L"Platform-Tools (ADB):", adb, !adb.empty());
        drawCompRow(L"Build-Tools (apksigner / zipalign):", bt, !bt.empty());
        drawCompRow(L"Java JDK Runtime (java.exe):", java, !java.empty());
        contentY += 12.0f;

        // Tool Installer Dialog Button
        m_rcBtnDownloadTools = D2D1::RectF(contentX, contentY, contentX + 380.0f, contentY + 44.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBtnDownloadTools, m_bBtnCompile ? m_bBtnCompile : m_bAccent, nullptr, 6.0f);
        if (m_fonts.button) {
            const wchar_t* szDownTools = L"⬇️  Android Araçlarını İndir ve Kur...";
            m_pRenderTarget->DrawText(szDownTools, (UINT32)wcslen(szDownTools), m_fonts.button,
                D2D1::RectF(m_rcBtnDownloadTools.left + 14.0f, contentY + 11.0f, m_rcBtnDownloadTools.right, contentY + 38.0f), m_bTextPrimary);
        }
        contentY += 52.0f;

        D2D1_RECT_F rcToolDesc = D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 44.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, rcToolDesc, m_bWell, m_bBorderSubtle, 5.0f);
        if (m_fonts.smallText) {
            std::wstring td = L"Google resmi repository sunucularından platform-tools (adb.exe), cmdline-tools ve build-tools (apksigner, zipalign)\n"
                              L"paketlerini otomatik indirir, doğrular ve kurar. Harici yazılım gerektirmez.";
            m_pRenderTarget->DrawText(td.c_str(), (UINT32)td.length(), m_fonts.smallText,
                D2D1::RectF(rcToolDesc.left + 10.0f, contentY + 6.0f, rcToolDesc.right - 10.0f, rcToolDesc.bottom), m_bTextSecondary);
        }
    } else if (m_settingsTab == 4) {
        // TAB 4: APK İŞLEMLERİ (Decompilation & Signing)
        if (m_fonts.subhead) {
            m_pRenderTarget->DrawText(L"1. Dalvik Bytecode & Dekompilasyon:", 36, m_fonts.subhead,
                D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 22.0f), m_bTextPrimary);
        }
        contentY += 28.0f;

        m_rcToggleDefaultView = D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 40.0f);
        bool defJava = SettingsManager::Get().settings.defaultToJava;
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcToggleDefaultView, defJava ? m_bWell : m_bCard, defJava ? m_bAccent : m_bBorderSubtle, 5.0f);
        if (m_fonts.body) {
            std::wstring dv = defJava
                ? L"✓ Varsayılan Görünüm: [Java AST Dekompilasyonu] (Tıklayarak Smali yapın)"
                : L"✕ Varsayılan Görünüm: [Smali Dalvik Bayt Kodu] (Tıklayarak Java yapın)";
            m_pRenderTarget->DrawText(dv.c_str(), (UINT32)dv.length(), m_fonts.body,
                D2D1::RectF(m_rcToggleDefaultView.left + 12.0f, contentY + 9.0f, m_rcToggleDefaultView.right, m_rcToggleDefaultView.bottom), m_bTextPrimary);
        }
        contentY += 48.0f;

        m_rcToggleMultidex = D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 40.0f);
        bool multiD = SettingsManager::Get().settings.multidexMerged;
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcToggleMultidex, multiD ? m_bWell : m_bCard, multiD ? m_bSuccess : m_bBorderSubtle, 5.0f);
        if (m_fonts.body) {
            std::wstring md = multiD
                ? L"✓ Çoklu DEX (Multidex): classes.dex, classes2.dex tek ağaçta birleştirildi (Aktif)"
                : L"✕ Çoklu DEX: Her DEX ayrı klasör olarak listelenir";
            m_pRenderTarget->DrawText(md.c_str(), (UINT32)md.length(), m_fonts.body,
                D2D1::RectF(m_rcToggleMultidex.left + 12.0f, contentY + 9.0f, m_rcToggleMultidex.right, m_rcToggleMultidex.bottom), m_bSuccess);
        }
        contentY += 56.0f;

        if (m_fonts.subhead) {
            m_pRenderTarget->DrawText(L"2. Dijital İmza Şemaları (Signature Schemes) & Paketleme:", 57, m_fonts.subhead,
                D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 22.0f), m_bTextPrimary);
        }
        contentY += 28.0f;

        m_rcToggleSignV1 = D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 36.0f);
        bool sV1 = SettingsManager::Get().settings.signSchemeV1;
        DrawToggleSwitch(m_rcToggleSignV1, sV1, L"Scheme v1 (JAR İmzalama)", L"META-INF MANIFEST.MF & CERT.RSA (Android 2.3+)");
        contentY += 42.0f;

        m_rcToggleSignV2 = D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 36.0f);
        bool sV2 = SettingsManager::Get().settings.signSchemeV2;
        DrawToggleSwitch(m_rcToggleSignV2, sV2, L"Scheme v2 (APK Signature Scheme v2)", L"APK Signing Block 42 doğrulaması (Android 7.0+)");
        contentY += 42.0f;

        m_rcToggleSignV3 = D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 36.0f);
        bool sV3 = SettingsManager::Get().settings.signSchemeV3;
        DrawToggleSwitch(m_rcToggleSignV3, sV3, L"Scheme v3 (Key Rotation Desteği)", L"Android 9.0+ Güvenlik Standardı");
        contentY += 42.0f;

        m_rcToggleZipalign = D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 36.0f);
        bool za = SettingsManager::Get().settings.zipAlign4Byte;
        DrawToggleSwitch(m_rcToggleZipalign, za, L"4-Bayt Hizalaması (ZipAlign)", L"Uncompressed .so ve kaynaklar için 4 bayt bellek hizalaması");
        contentY += 42.0f;
    } else if (m_settingsTab == 5) {
        // TAB 5: KISAYOLLAR (Presets & Visual Key Cards)
        bool isEn = (KaisekiLang::Get().GetLanguage() == AppLanguage::English);
        if (m_fonts.subhead) {
            const wchar_t* hkTitle = isEn ? L"⌨️ Keybindings & Hotkey Configuration" : L"⌨️ Kısayollar ve Tuş Düzeni";
            m_pRenderTarget->DrawText(hkTitle, (UINT32)wcslen(hkTitle), m_fonts.subhead,
                D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 22.0f), m_bTextPrimary);
        }
        contentY += 24.0f;

        float btnW = (contentW - 16.0f) / 3.0f;
        float btnH = 40.0f;
        int activePreset = SettingsManager::Get().settings.hotkeyPreset;

        m_rcHotkeyPresetVSCode = D2D1::RectF(contentX, contentY, contentX + btnW, contentY + btnH);
        m_rcHotkeyPresetIntelliJ = D2D1::RectF(contentX + btnW + 8.0f, contentY, contentX + btnW * 2.0f + 8.0f, contentY + btnH);
        m_rcHotkeyPresetVS = D2D1::RectF(contentX + (btnW + 8.0f) * 2.0f, contentY, contentX + contentW, contentY + btnH);

        auto drawPresetCard = [&](const D2D1_RECT_F& rc, const wchar_t* title, const wchar_t* desc, bool isActive) {
            KaisekiTheme::DrawCard(m_pRenderTarget, rc, isActive ? m_bWell : m_bCardHover, isActive ? m_bAccent : m_bBorderSubtle, 6.0f, isActive ? 2.0f : 1.0f);
            if (m_fonts.button) {
                m_pRenderTarget->DrawText(title, (UINT32)wcslen(title), m_fonts.button,
                    D2D1::RectF(rc.left + 10.0f, rc.top + 4.0f, rc.right - 8.0f, rc.top + 20.0f), isActive ? m_bAccent : m_bTextPrimary);
            }
            if (m_fonts.smallText) {
                m_pRenderTarget->DrawText(desc, (UINT32)wcslen(desc), m_fonts.smallText,
                    D2D1::RectF(rc.left + 10.0f, rc.top + 21.0f, rc.right - 8.0f, rc.bottom - 3.0f), m_bTextSecondary);
            }
        };

        drawPresetCard(m_rcHotkeyPresetVSCode, L"VS Code Default", isEn ? L"Standard IDE Keymap" : L"Standart Modern Düzen", activePreset == 0);
        drawPresetCard(m_rcHotkeyPresetIntelliJ, L"IntelliJ IDEA", isEn ? L"Android Studio Keymap" : L"Android Geliştirici Düzeni", activePreset == 1);
        drawPresetCard(m_rcHotkeyPresetVS, L"Visual Studio", isEn ? L"Classic C++ Keymap" : L"Klasik C++ Düzeni", activePreset == 2);
        contentY += btnH + 14.0f;

        // Two Category Cards
        float catW = (contentW - 14.0f) * 0.5f;
        float catH = 250.0f;
        D2D1_RECT_F rcCatLeft = D2D1::RectF(contentX, contentY, contentX + catW, contentY + catH);
        D2D1_RECT_F rcCatRight = D2D1::RectF(contentX + catW + 14.0f, contentY, contentX + contentW, contentY + catH);

        KaisekiTheme::DrawCard(m_pRenderTarget, rcCatLeft, m_bCard, m_bBorderSubtle, 8.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, rcCatRight, m_bCard, m_bBorderSubtle, 8.0f);

        if (m_fonts.button) {
            m_pRenderTarget->DrawText(isEn ? L"📁 File, Project & Editor" : L"📁 Dosya, Proje ve Düzenleyici", 30, m_fonts.button,
                D2D1::RectF(rcCatLeft.left + 14.0f, rcCatLeft.top + 10.0f, rcCatLeft.right - 14.0f, rcCatLeft.top + 30.0f), m_bAccent);
            m_pRenderTarget->DrawText(isEn ? L"⚡ Search, Build & Tools" : L"⚡ Arama, Derleme ve Araçlar", 28, m_fonts.button,
                D2D1::RectF(rcCatRight.left + 14.0f, rcCatRight.top + 10.0f, rcCatRight.right - 14.0f, rcCatRight.top + 30.0f), m_bAccent);
        }

        struct KeyDef { std::wstring key; std::wstring desc; };
        std::vector<KeyDef> leftItems = {
            { L"Ctrl + O", isEn ? L"Open APK or Project" : L"APK veya Proje Aç..." },
            { L"Ctrl + S", isEn ? L"Save Active Document" : L"Aktif Dosyayı Kaydet" },
            { L"Ctrl + Shift + S", isEn ? L"Save All Open Documents" : L"Tüm Açık Dosyaları Kaydet" },
            { L"Ctrl + Z", isEn ? L"Undo Edit Action" : L"Geri Al (Undo)" },
            { L"Ctrl + Y", isEn ? L"Redo Edit Action" : L"Yinele (Redo)" },
            { L"Ctrl + C / V", isEn ? L"Copy / Paste Text" : L"Kopyala / Yapıştır" },
            { L"Ctrl + A", isEn ? L"Select All Content" : L"Tümünü Seç" },
            { L"Esc", isEn ? L"Close Active Modals" : L"Pencereleri Kapat" }
        };
        std::vector<KeyDef> rightItems = {
            { L"Ctrl + Shift + F", isEn ? L"Global Code/Text Search" : L"Genel Kod ve Metin Arama" },
            { L"Ctrl + N", isEn ? L"Quick Class Search" : L"Sınıf ve Metot Arama" },
            { L"F8", isEn ? L"Rebuild & Sign APK" : L"APK Paketle ve İmzala" },
            { L"F5", isEn ? L"Inspect APK Signature" : L"APK İmza Bilgileri" },
            { L"F6", isEn ? L"Security Score Audit" : L"Güvenlik Skor Denetimi" },
            { L"Ctrl + ,", isEn ? L"Preferences & Settings" : L"Ayarlar ve Tercihler" },
            { L"Tab", isEn ? L"4-Space Code Indent" : L"4 Boşluk Kod Girintisi" },
            { L"F1", isEn ? L"About Kaiseki" : L"Kaiseki Hakkında" }
        };

        auto renderShortcutRows = [&](float startX, float startY, float width, const std::vector<KeyDef>& items) {
            float ry = startY;
            for (const auto& item : items) {
                D2D1_RECT_F rRow = D2D1::RectF(startX + 8.0f, ry, startX + width - 8.0f, ry + 24.0f);
                D2D1_RECT_F rBadge = D2D1::RectF(rRow.left, rRow.top + 1.0f, rRow.left + 105.0f, rRow.bottom - 1.0f);
                KaisekiTheme::DrawCard(m_pRenderTarget, rBadge, m_bWell, m_bBorderSubtle, 4.0f);
                if (m_fonts.smallText) {
                    m_pRenderTarget->DrawText(item.key.c_str(), (UINT32)item.key.length(), m_fonts.smallText,
                        D2D1::RectF(rBadge.left + 4.0f, rBadge.top + 2.0f, rBadge.right - 4.0f, rBadge.bottom), m_bAccent);
                    m_pRenderTarget->DrawText(item.desc.c_str(), (UINT32)item.desc.length(), m_fonts.smallText,
                        D2D1::RectF(rRow.left + 114.0f, rRow.top + 2.0f, rRow.right - 4.0f, rRow.bottom), m_bTextPrimary);
                }
                ry += 26.0f;
            }
        };

        renderShortcutRows(rcCatLeft.left, rcCatLeft.top + 36.0f, catW, leftItems);
        renderShortcutRows(rcCatRight.left, rcCatRight.top + 36.0f, catW, rightItems);
    } else if (m_settingsTab == 6) {
        // TAB 6: GELİŞMİŞ VE TANILAMA (Verbose Logging, Diagnostics, Factory Reset)
        if (m_fonts.subhead) {
            m_pRenderTarget->DrawText(L"Gelişmiş Tanılama ve Sistem Araçları:", 38, m_fonts.subhead,
                D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 22.0f), m_bTextPrimary);
        }
        contentY += 28.0f;

        m_rcToggleVerboseLog = D2D1::RectF(contentX, contentY, contentX + contentW, contentY + 36.0f);
        bool vl = SettingsManager::Get().settings.verboseLogging;
        DrawToggleSwitch(m_rcToggleVerboseLog, vl, L"Ayrıntılı Hata Ayıklama Günlüğü (Verbose Logging)", L"Tüm sistem işlemlerini ve ayrıştırma detaylarını konsola döker");
        contentY += 54.0f;

        m_rcBtnCopyDiagnostics = D2D1::RectF(contentX, contentY, contentX + 380.0f, contentY + 42.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBtnCopyDiagnostics, m_bCardHover, m_bAccent, 6.0f);
        if (m_fonts.button) {
            const wchar_t* szDiag = L"📋  Sistem Tanılama Raporunu Panoya Kopyala";
            m_pRenderTarget->DrawText(szDiag, (UINT32)wcslen(szDiag), m_fonts.button,
                D2D1::RectF(m_rcBtnCopyDiagnostics.left + 14.0f, contentY + 10.0f, m_rcBtnCopyDiagnostics.right, contentY + 36.0f), m_bTextPrimary);
        }
        contentY += 56.0f;

        m_rcBtnResetSettings = D2D1::RectF(contentX, contentY, contentX + 380.0f, contentY + 42.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBtnResetSettings, m_bCard, m_bCloseRed, 6.0f);
        if (m_fonts.button) {
            const wchar_t* szReset = L"⚠️  Tüm Ayarları Fabrika Ayarlarına Sıfırla";
            m_pRenderTarget->DrawText(szReset, (UINT32)wcslen(szReset), m_fonts.button,
                D2D1::RectF(m_rcBtnResetSettings.left + 14.0f, contentY + 10.0f, m_rcBtnResetSettings.right, contentY + 36.0f), m_bCloseRed);
        }
    }
    m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
}

void UIWindow::DrawToggleSwitch(const D2D1_RECT_F& rc, bool isOn, const wchar_t* label, const wchar_t* subtext) {
    float trackW = 40.0f;
    float trackH = 22.0f;
    float trackX = rc.left + 4.0f;
    float trackY = rc.top + (rc.bottom - rc.top - trackH) * 0.5f;
    D2D1_RECT_F rcTrack = D2D1::RectF(trackX, trackY, trackX + trackW, trackY + trackH);

    // Track Background
    ID2D1SolidColorBrush* bTrack = isOn ? m_bAccent : m_bCardHover;
    ID2D1SolidColorBrush* bTrackBorder = isOn ? m_bAccent : m_bBorderSubtle;
    m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(rcTrack, 11.0f, 11.0f), bTrack);
    m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(rcTrack, 11.0f, 11.0f), bTrackBorder, 1.0f);

    // Sliding Thumb Circle
    float thumbRadius = 7.5f;
    float thumbX = isOn ? (rcTrack.right - 11.5f) : (rcTrack.left + 11.5f);
    float thumbY = trackY + trackH * 0.5f;
    m_pRenderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(thumbX, thumbY), thumbRadius, thumbRadius), m_bTextPrimary);

    // Text Label
    float textX = trackX + trackW + 12.0f;
    if (m_fonts.body) {
        float textY = (subtext && subtext[0]) ? (rc.top + 2.0f) : (rc.top + (rc.bottom - rc.top - 18.0f) * 0.5f);
        m_pRenderTarget->DrawText(label, (UINT32)wcslen(label), m_fonts.body,
            D2D1::RectF(textX, textY, rc.right, textY + 20.0f), isOn ? m_bTextPrimary : m_bTextSecondary);
    }
    if (subtext && subtext[0] && m_fonts.smallText) {
        m_pRenderTarget->DrawText(subtext, (UINT32)wcslen(subtext), m_fonts.smallText,
            D2D1::RectF(textX, rc.top + 20.0f, rc.right, rc.bottom), m_bTextMuted);
    }
}

void UIWindow::RenderColorWheelPopover(float x, float y) {
    float popW = 290.0f;
    float popH = 320.0f;
    m_rcColorPickerCard = D2D1::RectF(x, y, x + popW, y + popH);
    KaisekiTheme::DrawCard(m_pRenderTarget, m_rcColorPickerCard, m_bCard, m_bAccent, 10.0f, 1.5f);

    // Header Title
    const wchar_t* pTitle = (m_colorPickerTarget == 0) ? L"🎨 Vurgu Rengi Seçici" : L"🎨 Arka Plan Tonu Seçici";
    if (m_fonts.label) {
        m_pRenderTarget->DrawText(pTitle, (UINT32)wcslen(pTitle), m_fonts.label,
            D2D1::RectF(x + 14.0f, y + 10.0f, x + popW - 36.0f, y + 28.0f), m_bTextPrimary);
    }

    // Close Button (✕)
    m_rcColorPickerClose = D2D1::RectF(x + popW - 30.0f, y + 8.0f, x + popW - 10.0f, y + 28.0f);
    if (m_fonts.button) {
        m_pRenderTarget->DrawText(L"✕", 1, m_fonts.button, m_rcColorPickerClose, m_bTextMuted);
    }

    // 2D Saturation / Value Gradient Box
    float satX = x + 14.0f;
    float satY = y + 34.0f;
    float satW = popW - 28.0f;
    float satH = 136.0f;
    m_rcColorSatValBox = D2D1::RectF(satX, satY, satX + satW, satY + satH);

    // 1) Horizontal Linear Gradient: White -> Pure Hue
    D2D1_GRADIENT_STOP stopsH[2];
    stopsH[0].position = 0.0f;
    stopsH[0].color = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
    stopsH[1].position = 1.0f;
    stopsH[1].color = KaisekiTheme::HsvToRgb(m_pickerHue, 1.0f, 1.0f);

    ID2D1GradientStopCollection* pStopsH = nullptr;
    ID2D1LinearGradientBrush* pBrH = nullptr;
    if (SUCCEEDED(m_pRenderTarget->CreateGradientStopCollection(stopsH, 2, &pStopsH))) {
        D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES propH;
        propH.startPoint = D2D1::Point2F(satX, satY);
        propH.endPoint = D2D1::Point2F(satX + satW, satY);
        m_pRenderTarget->CreateLinearGradientBrush(propH, pStopsH, &pBrH);
        pStopsH->Release();
    }

    if (pBrH) {
        m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(m_rcColorSatValBox, 6.0f, 6.0f), pBrH);
        pBrH->Release();
    }

    // 2) Vertical Linear Gradient: Transparent -> Black
    D2D1_GRADIENT_STOP stopsV[2];
    stopsV[0].position = 0.0f;
    stopsV[0].color = D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f);
    stopsV[1].position = 1.0f;
    stopsV[1].color = D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f);

    ID2D1GradientStopCollection* pStopsV = nullptr;
    ID2D1LinearGradientBrush* pBrV = nullptr;
    if (SUCCEEDED(m_pRenderTarget->CreateGradientStopCollection(stopsV, 2, &pStopsV))) {
        D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES propV;
        propV.startPoint = D2D1::Point2F(satX, satY);
        propV.endPoint = D2D1::Point2F(satX, satY + satH);
        m_pRenderTarget->CreateLinearGradientBrush(propV, pStopsV, &pBrV);
        pStopsV->Release();
    }

    if (pBrV) {
        m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(m_rcColorSatValBox, 6.0f, 6.0f), pBrV);
        pBrV->Release();
    }

    m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(m_rcColorSatValBox, 6.0f, 6.0f), m_bBorderSubtle, 1.0f);

    // Sat/Val Cursor Thumb
    float curThumbX = satX + m_pickerSat * satW;
    float curThumbY = satY + (1.0f - m_pickerVal) * satH;
    m_pRenderTarget->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(curThumbX, curThumbY), 6.0f, 6.0f), m_bWell, 3.0f);
    m_pRenderTarget->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(curThumbX, curThumbY), 6.0f, 6.0f), m_bTextPrimary, 1.5f);

    // Hue Rainbow Slider Bar
    float hueY = satY + satH + 12.0f;
    float hueH = 16.0f;
    m_rcColorHueSlider = D2D1::RectF(satX, hueY, satX + satW, hueY + hueH);

    float segW = satW / 6.0f;
    D2D1_COLOR_F spectrum[7] = {
        D2D1::ColorF(1.0f, 0.0f, 0.0f),
        D2D1::ColorF(1.0f, 1.0f, 0.0f),
        D2D1::ColorF(0.0f, 1.0f, 0.0f),
        D2D1::ColorF(0.0f, 1.0f, 1.0f),
        D2D1::ColorF(0.0f, 0.0f, 1.0f),
        D2D1::ColorF(1.0f, 0.0f, 1.0f),
        D2D1::ColorF(1.0f, 0.0f, 0.0f)
    };
    for (int s = 0; s < 6; ++s) {
        D2D1_GRADIENT_STOP stopsSeg[2];
        stopsSeg[0].position = 0.0f;
        stopsSeg[0].color = spectrum[s];
        stopsSeg[1].position = 1.0f;
        stopsSeg[1].color = spectrum[s + 1];

        ID2D1GradientStopCollection* pStopsSeg = nullptr;
        ID2D1LinearGradientBrush* pBrSeg = nullptr;
        if (SUCCEEDED(m_pRenderTarget->CreateGradientStopCollection(stopsSeg, 2, &pStopsSeg))) {
            D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES propSeg;
            propSeg.startPoint = D2D1::Point2F(satX + s * segW, hueY);
            propSeg.endPoint = D2D1::Point2F(satX + (s + 1) * segW, hueY);
            m_pRenderTarget->CreateLinearGradientBrush(propSeg, pStopsSeg, &pBrSeg);
            pStopsSeg->Release();
        }
        if (pBrSeg) {
            D2D1_RECT_F rSeg = D2D1::RectF(satX + s * segW, hueY, satX + (s + 1) * segW, hueY + hueH);
            m_pRenderTarget->FillRectangle(rSeg, pBrSeg);
            pBrSeg->Release();
        }
    }
    m_pRenderTarget->DrawRoundedRectangle(D2D1::RoundedRect(m_rcColorHueSlider, 8.0f, 8.0f), m_bBorderSubtle, 1.0f);

    // Hue Slider Thumb Handle
    float hueThumbX = satX + (m_pickerHue / 360.0f) * satW;
    m_pRenderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(hueThumbX, hueY + hueH * 0.5f), 7.0f, 7.0f), m_bTextPrimary);
    m_pRenderTarget->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(hueThumbX, hueY + hueH * 0.5f), 7.0f, 7.0f), m_bWell, 1.5f);

    // Color Preview & Hex / RGB
    float prevY = hueY + hueH + 12.0f;
    m_rcColorPreviewBox = D2D1::RectF(satX, prevY, satX + 38.0f, prevY + 30.0f);
    D2D1_COLOR_F currCol = KaisekiTheme::HsvToRgb(m_pickerHue, m_pickerSat, m_pickerVal);
    ID2D1SolidColorBrush* bPrev = nullptr;
    m_pRenderTarget->CreateSolidColorBrush(currCol, &bPrev);
    if (bPrev) {
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcColorPreviewBox, bPrev, m_bBorderHover, 5.0f);
        bPrev->Release();
    }

    D2D1_RECT_F rcHexBox = D2D1::RectF(satX + 46.0f, prevY, satX + 155.0f, prevY + 30.0f);
    KaisekiTheme::DrawCard(m_pRenderTarget, rcHexBox, m_bWell, m_bBorderSubtle, 5.0f);
    std::wstring hexStr = KaisekiTheme::ColorToHexW(currCol);
    if (m_fonts.button) {
        m_pRenderTarget->DrawText(hexStr.c_str(), (UINT32)hexStr.length(), m_fonts.button,
            D2D1::RectF(rcHexBox.left + 10.0f, prevY + 5.0f, rcHexBox.right, prevY + 28.0f), m_bTextPrimary);
    }

    int rVal = (int)(currCol.r * 255.0f);
    int gVal = (int)(currCol.g * 255.0f);
    int bVal = (int)(currCol.b * 255.0f);
    std::wstring rgbStr = L"R:" + std::to_wstring(rVal) + L" G:" + std::to_wstring(gVal) + L" B:" + std::to_wstring(bVal);
    if (m_fonts.smallText) {
        m_pRenderTarget->DrawText(rgbStr.c_str(), (UINT32)rgbStr.length(), m_fonts.smallText,
            D2D1::RectF(satX + 162.0f, prevY + 7.0f, satX + satW, prevY + 28.0f), m_bTextSecondary);
    }

    // Quick Palette Dots Row
    float dotY = prevY + 40.0f;
    m_rcColorPresetDots.clear();
    if (m_fonts.smallText) {
        m_pRenderTarget->DrawText(L"Hızlı Renkler:", 13, m_fonts.smallText,
            D2D1::RectF(satX, dotY, satX + 70.0f, dotY + 18.0f), m_bTextSecondary);
    }

    float dotStartX = satX + 74.0f;
    float dotRadius = 9.0f;
    float dotStep = 23.0f;
    if (m_colorPickerTarget == 0) {
        const auto& swatches = KaisekiTheme::GetPresetSwatches();
        for (size_t d = 0; d < 8 && d < swatches.size(); ++d) {
            float cx = dotStartX + d * dotStep;
            float cy = dotY + 9.0f;
            D2D1_RECT_F rcDot = D2D1::RectF(cx - dotRadius, cy - dotRadius, cx + dotRadius, cy + dotRadius);
            m_rcColorPresetDots.push_back(rcDot);

            ID2D1SolidColorBrush* bDot = nullptr;
            m_pRenderTarget->CreateSolidColorBrush(swatches[d].color, &bDot);
            if (bDot) {
                m_pRenderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), dotRadius, dotRadius), bDot);
                m_pRenderTarget->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), dotRadius, dotRadius), m_bBorderSubtle, 1.0f);
                bDot->Release();
            }
        }
    } else {
        D2D1_COLOR_F bgColors[6] = {
            D2D1::ColorF(0.106f, 0.110f, 0.133f), // Charcoal
            D2D1::ColorF(0.043f, 0.043f, 0.051f), // OLED
            D2D1::ColorF(0.063f, 0.078f, 0.122f), // Midnight Navy
            D2D1::ColorF(0.094f, 0.067f, 0.102f), // Plum
            D2D1::ColorF(0.070f, 0.075f, 0.086f), // Slate
            D2D1::ColorF(0.055f, 0.067f, 0.090f)  // Obsidian
        };
        for (int d = 0; d < 6; ++d) {
            float cx = dotStartX + d * dotStep;
            float cy = dotY + 9.0f;
            D2D1_RECT_F rcDot = D2D1::RectF(cx - dotRadius, cy - dotRadius, cx + dotRadius, cy + dotRadius);
            m_rcColorPresetDots.push_back(rcDot);

            ID2D1SolidColorBrush* bDot = nullptr;
            m_pRenderTarget->CreateSolidColorBrush(bgColors[d], &bDot);
            if (bDot) {
                m_pRenderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), dotRadius, dotRadius), bDot);
                m_pRenderTarget->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), dotRadius, dotRadius), m_bBorderSubtle, 1.0f);
                bDot->Release();
            }
        }
    }
}

void UIWindow::RenderInitialHeroDropZone(float w, float h) {
    SettingsManager::Get().RefreshTimeAgos();

    float cardW = KaisekiTheme::MinF(740.0f, w - 80.0f);
    if (cardW < 420.0f) cardW = 420.0f;
    float cardX = (w - cardW) * 0.5f;

    float card1H = 240.0f;
    float card2H = 270.0f;
    float gap = 18.0f;
    float totalH = card1H + gap + card2H;
    float startY = (h - totalH) * 0.5f;
    if (startY < 48.0f) startY = 48.0f;

    float card1Y = startY;
    float card2Y = card1Y + card1H + gap;

    m_rcHeroDropZone = D2D1::RectF(cardX, card1Y, cardX + cardW, card1Y + card1H);
    m_rcHeroBrowseBtn = m_rcHeroDropZone; // Entire card is clickable

    // ========================================================
    // CARD 1: TOP DRAG & DROP / CLICK TO OPEN
    // ========================================================
    float breath = 0.5f + 0.5f * sinf(m_pulsePhase);
    ID2D1SolidColorBrush* borderB = (m_hoverHeroDrop || m_hoverHeroDropAlpha > 0.05f) ? m_bAccent : m_bBorderSubtle;
    ID2D1SolidColorBrush* bgB = (m_hoverHeroDrop || m_hoverHeroDropAlpha > 0.05f) ? m_bCardHover : m_bCard;
    float strokeW = (m_hoverHeroDrop ? 2.2f : 1.0f) + (m_hoverHeroDrop ? breath * 0.5f : 0.0f);

    KaisekiTheme::DrawCard(m_pRenderTarget, m_rcHeroDropZone, bgB, borderB, 14.0f, strokeW);

    // Pill Tag in Top-Right Corner of Card 1: • .apk  • .kproj
    D2D1_RECT_F rcPill = D2D1::RectF(cardX + cardW - 124.0f, card1Y + 16.0f, cardX + cardW - 18.0f, card1Y + 40.0f);
    KaisekiTheme::DrawCard(m_pRenderTarget, rcPill, m_bBadgeBg, m_bBorderSubtle, 10.0f, 1.0f);
    // Draw pink dot
    m_pRenderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(rcPill.left + 12.0f, rcPill.top + 12.0f), 2.5f, 2.5f), m_bAccent);
    if (m_fonts.smallText) {
        const wchar_t* pillText = L".apk  • .kproj";
        m_pRenderTarget->DrawText(pillText, (UINT32)wcslen(pillText), m_fonts.smallText,
            D2D1::RectF(rcPill.left + 20.0f, rcPill.top + 4.0f, rcPill.right, rcPill.bottom), m_bTextSecondary);
    }

    // Center rounded squircle badge with document icon matching Image 2
    float cx = cardX + cardW * 0.5f;
    float cy = card1Y + 70.0f;
    D2D1_RECT_F rcIconBadge = D2D1::RectF(cx - 26.0f, cy - 26.0f, cx + 26.0f, cy + 26.0f);
    ID2D1SolidColorBrush* bIconBg = nullptr;
    m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.20f, 0.11f, 0.17f, 0.95f), &bIconBg);
    if (bIconBg) {
        KaisekiTheme::DrawCard(m_pRenderTarget, rcIconBadge, bIconBg, m_bAccent, 14.0f, 1.6f);
        bIconBg->Release();
    }

    // Document Icon inside squircle
    VectorIcons::DrawDocument(m_pRenderTarget, cx - 11.0f, cy - 13.0f, 22.0f, m_bAccent, 1.8f);

    // Title: Kaiseki
    if (m_fonts.title) {
        D2D1_RECT_F rcTitle = D2D1::RectF(cardX + 20.0f, card1Y + 108.0f, cardX + cardW - 20.0f, card1Y + 138.0f);
        DWRITE_TEXT_ALIGNMENT oldAlign = m_fonts.title->GetTextAlignment();
        m_fonts.title->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_pRenderTarget->DrawText(L"Kaiseki", 7, m_fonts.title, rcTitle, m_bTextPrimary);
        m_fonts.title->SetTextAlignment(oldAlign);
    }

    // Subtitle: .apk veya .kproj Dosyasını Buraya Sürükleyin veya Seçmek İçin Tıklayın
    if (m_fonts.subhead) {
        D2D1_RECT_F rcSub = D2D1::RectF(cardX + 20.0f, card1Y + 142.0f, cardX + cardW - 20.0f, card1Y + 168.0f);
        const wchar_t* subText = L".apk veya .kproj Dosyasını Buraya Sürükleyin veya Seçmek İçin Tıklayın";
        DWRITE_TEXT_ALIGNMENT oldAlign = m_fonts.subhead->GetTextAlignment();
        m_fonts.subhead->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_pRenderTarget->DrawText(subText, (UINT32)wcslen(subText), m_fonts.subhead, rcSub, m_bTextPrimary);
        m_fonts.subhead->SetTextAlignment(oldAlign);
    }

    // Detail text: Sadece DEX Yüklenir • Dalvik DEX to Smali • Yerel DexEngine (.kproj)
    if (m_fonts.smallText) {
        D2D1_RECT_F rcDetail = D2D1::RectF(cardX + 20.0f, card1Y + 172.0f, cardX + cardW - 20.0f, card1Y + 196.0f);
        const wchar_t* detailText = L"Sadece DEX Yüklenir • Dalvik DEX to Smali • Yerel DexEngine (.kproj)";
        DWRITE_TEXT_ALIGNMENT oldAlign = m_fonts.smallText->GetTextAlignment();
        m_fonts.smallText->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_pRenderTarget->DrawText(detailText, (UINT32)wcslen(detailText), m_fonts.smallText, rcDetail, m_bTextMuted);
        m_fonts.smallText->SetTextAlignment(oldAlign);
    }

    // ========================================================
    // CARD 2: BOTTOM RECENT PROJECTS (.KPROJ & .APK)
    // ========================================================
    D2D1_RECT_F rcCard2 = D2D1::RectF(cardX, card2Y, cardX + cardW, card2Y + card2H);
    KaisekiTheme::DrawCard(m_pRenderTarget, rcCard2, m_bCard, m_bBorderSubtle, 14.0f, 1.0f);

    // Card 2 Header: 🕒 SON DOSYALAR [ 3 ] ... Geçmişi Temizle
    VectorIcons::DrawClock(m_pRenderTarget, cardX + 26.0f, card2Y + 22.0f, 6.5f, m_bAccent, 1.5f);

    bool isEn = (KaisekiLang::Get().GetLanguage() == AppLanguage::English);
    const wchar_t* hdrTitle = isEn ? L"RECENT FILES" : L"SON DOSYALAR";

    float titleW = 0.0f;
    if (m_pDWriteFactory && m_fonts.label) {
        IDWriteTextLayout* pLayout = nullptr;
        if (SUCCEEDED(m_pDWriteFactory->CreateTextLayout(hdrTitle, (UINT32)wcslen(hdrTitle), m_fonts.label, 400.0f, 30.0f, &pLayout))) {
            DWRITE_TEXT_METRICS tm;
            if (SUCCEEDED(pLayout->GetMetrics(&tm))) {
                titleW = tm.width;
            }
            pLayout->Release();
        }
    }
    if (titleW <= 0.0f) titleW = (float)wcslen(hdrTitle) * 7.5f;

    float titleX = cardX + 38.0f;
    float titleY = card2Y + 12.0f;
    if (m_fonts.label) {
        m_pRenderTarget->DrawText(hdrTitle, (UINT32)wcslen(hdrTitle), m_fonts.label,
            D2D1::RectF(titleX, titleY, titleX + titleW + 4.0f, titleY + 20.0f), m_bTextPrimary);
    }

    const auto& recents = SettingsManager::Get().recentProjects;
    std::wstring countStr = std::to_wstring(recents.size());
    float pillX = titleX + titleW + 8.0f;
    float pillW = (countStr.length() > 1) ? 24.0f : 18.0f;
    float pillH = 16.0f;
    float pillY = card2Y + 14.0f;
    D2D1_RECT_F rcCountPill = D2D1::RectF(pillX, pillY, pillX + pillW, pillY + pillH);
    KaisekiTheme::DrawCard(m_pRenderTarget, rcCountPill, m_bBadgeBg, nullptr, 5.0f);
    if (m_fonts.smallText) {
        DWRITE_TEXT_ALIGNMENT oldAlign = m_fonts.smallText->GetTextAlignment();
        DWRITE_PARAGRAPH_ALIGNMENT oldPara = m_fonts.smallText->GetParagraphAlignment();
        m_fonts.smallText->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_fonts.smallText->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        m_pRenderTarget->DrawText(countStr.c_str(), (UINT32)countStr.length(), m_fonts.smallText,
            rcCountPill, m_bTextSecondary);
        m_fonts.smallText->SetTextAlignment(oldAlign);
        m_fonts.smallText->SetParagraphAlignment(oldPara);
    }

    // "Geçmişi Temizle" button in top-right of Card 2
    m_rcClearRecentBtn = D2D1::RectF(cardX + cardW - 120.0f, card2Y + 12.0f, cardX + cardW - 18.0f, card2Y + 32.0f);
    if (m_fonts.smallText) {
        const wchar_t* clrText = L"Geçmişi Temizle";
        DWRITE_TEXT_ALIGNMENT oldAlign = m_fonts.smallText->GetTextAlignment();
        m_fonts.smallText->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
        m_pRenderTarget->DrawText(clrText, (UINT32)wcslen(clrText), m_fonts.smallText,
            m_rcClearRecentBtn, m_hoverClearRecent ? m_bAccent : m_bTextMuted);
        m_fonts.smallText->SetTextAlignment(oldAlign);
    }

    // Recent items list
    m_rcRecentItems.clear();
    if (recents.empty()) {
        if (m_fonts.body) {
            const wchar_t* emptyMsg = L"Henüz açılmış bir proje veya APK bulunmuyor. Yeni bir APK sürükleyip başlayın.";
            DWRITE_TEXT_ALIGNMENT oldAlign = m_fonts.body->GetTextAlignment();
            m_fonts.body->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_pRenderTarget->DrawText(emptyMsg, (UINT32)wcslen(emptyMsg), m_fonts.body,
                D2D1::RectF(cardX + 20.0f, card2Y + 100.0f, cardX + cardW - 20.0f, card2Y + 140.0f), m_bTextMuted);
            m_fonts.body->SetTextAlignment(oldAlign);
        }
    } else {
        int maxShow = 3;
        for (int i = 0; i < (int)recents.size() && i < maxShow; ++i) {
            float rowY = card2Y + 44.0f + (float)i * 68.0f;
            float rowH = 58.0f;
            D2D1_RECT_F rcRow = D2D1::RectF(cardX + 16.0f, rowY, cardX + cardW - 16.0f, rowY + rowH);
            m_rcRecentItems.push_back(rcRow);

            bool isRowHover = (m_hoverRecentIndex == i);
            ID2D1SolidColorBrush* rBg = isRowHover ? m_bCardHover : m_bWell;
            ID2D1SolidColorBrush* rBrd = isRowHover ? m_bAccent : m_bBorderSubtle;
            KaisekiTheme::DrawCard(m_pRenderTarget, rcRow, rBg, rBrd, 8.0f, isRowHover ? 1.4f : 1.0f);

            // Left icon squircle
            D2D1_RECT_F rcIconSq = D2D1::RectF(rcRow.left + 12.0f, rcRow.top + 10.0f, rcRow.left + 48.0f, rcRow.bottom - 10.0f);
            ID2D1SolidColorBrush* bItemIconBg = nullptr;
            m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.20f, 0.11f, 0.17f, 0.90f), &bItemIconBg);
            if (bItemIconBg) {
                KaisekiTheme::DrawCard(m_pRenderTarget, rcIconSq, bItemIconBg, m_bAccent, 8.0f, 1.0f);
                bItemIconBg->Release();
            }
            VectorIcons::DrawDocument(m_pRenderTarget, rcIconSq.left + 9.0f, rcIconSq.top + 8.0f, 18.0f, m_bAccent, 1.4f);

            // File Name & Badge
            float textX = rcRow.left + 58.0f;
            float topY = rcRow.top + 9.0f;

            if (m_fonts.label) {
                const auto& fn = recents[i].fileName;
                float nameW = 0.0f;
                if (m_pDWriteFactory && m_fonts.label) {
                    IDWriteTextLayout* pLayout = nullptr;
                    if (SUCCEEDED(m_pDWriteFactory->CreateTextLayout(fn.c_str(), (UINT32)fn.length(), m_fonts.label, 600.0f, 30.0f, &pLayout))) {
                        DWRITE_TEXT_METRICS tm;
                        if (SUCCEEDED(pLayout->GetMetrics(&tm))) {
                            nameW = tm.width;
                        }
                        pLayout->Release();
                    }
                }
                if (nameW <= 0.0f) nameW = (float)fn.length() * 7.0f;

                m_pRenderTarget->DrawText(fn.c_str(), (UINT32)fn.length(), m_fonts.label,
                    D2D1::RectF(textX, topY, textX + nameW + 2.0f, topY + 20.0f), m_bTextPrimary);

                // Badge next to name (e.g. classes.dex (4 DEX))
                if (!recents[i].badgeText.empty()) {
                    const auto& bText = recents[i].badgeText;
                    float bTextW = 0.0f;
                    if (m_pDWriteFactory && m_fonts.smallText) {
                        IDWriteTextLayout* pLayout = nullptr;
                        if (SUCCEEDED(m_pDWriteFactory->CreateTextLayout(bText.c_str(), (UINT32)bText.length(), m_fonts.smallText, 300.0f, 20.0f, &pLayout))) {
                            DWRITE_TEXT_METRICS tm;
                            if (SUCCEEDED(pLayout->GetMetrics(&tm))) {
                                bTextW = tm.width;
                            }
                            pLayout->Release();
                        }
                    }
                    if (bTextW <= 0.0f) bTextW = (float)bText.length() * 6.0f;

                    float bX = textX + nameW + 8.0f;
                    float bW = bTextW + 12.0f; // 6px padding on left & right
                    float bH = 16.0f;
                    float bY = topY + 2.0f;    // perfectly aligned with label baseline

                    D2D1_RECT_F rcBadge = D2D1::RectF(bX, bY, bX + bW, bY + bH);
                    ID2D1SolidColorBrush* bPillFill = nullptr;
                    m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.25f, 0.11f, 0.19f, 0.90f), &bPillFill);
                    if (bPillFill) {
                        KaisekiTheme::DrawCard(m_pRenderTarget, rcBadge, bPillFill, m_bAccent, 4.0f, 0.8f);
                        bPillFill->Release();
                    }
                    if (m_fonts.smallText) {
                        DWRITE_TEXT_ALIGNMENT oldAlign = m_fonts.smallText->GetTextAlignment();
                        DWRITE_PARAGRAPH_ALIGNMENT oldPara = m_fonts.smallText->GetParagraphAlignment();
                        m_fonts.smallText->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                        m_fonts.smallText->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                        m_pRenderTarget->DrawText(bText.c_str(), (UINT32)bText.length(),
                            m_fonts.smallText, rcBadge, m_bAccent);
                        m_fonts.smallText->SetTextAlignment(oldAlign);
                        m_fonts.smallText->SetParagraphAlignment(oldPara);
                    }
                }
            }

            // Path on second line (with path ellipsis)
            if (m_fonts.smallText) {
                float botY = rcRow.top + 32.0f;
                D2D1_RECT_F rcPath = D2D1::RectF(textX, botY, rcRow.right - 140.0f, botY + 18.0f);
                m_pRenderTarget->DrawText(recents[i].filePath.c_str(), (UINT32)recents[i].filePath.length(),
                    m_fonts.smallText, rcPath, m_bTextMuted);
            }

            // Right block: extension, timestamp, and chevron arrow >
            float rRight = rcRow.right - 16.0f;

            // Chevron > (centered vertically with the right block)
            if (m_fonts.label) {
                DWRITE_TEXT_ALIGNMENT oldAlign = m_fonts.label->GetTextAlignment();
                DWRITE_PARAGRAPH_ALIGNMENT oldPara = m_fonts.label->GetParagraphAlignment();
                m_fonts.label->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                m_fonts.label->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                m_pRenderTarget->DrawText(L">", 1, m_fonts.label,
                    D2D1::RectF(rRight - 14.0f, rcRow.top + 18.0f, rRight, rcRow.top + 40.0f),
                    isRowHover ? m_bAccentLight : m_bAccent);
                m_fonts.label->SetTextAlignment(oldAlign);
                m_fonts.label->SetParagraphAlignment(oldPara);
            }

            // Extension pill (.kproj or .apk)
            if (m_fonts.smallText) {
                D2D1_RECT_F rcExt = D2D1::RectF(rRight - 80.0f, topY + 1.0f, rRight - 28.0f, topY + 19.0f);
                DWRITE_TEXT_ALIGNMENT oldAlign = m_fonts.smallText->GetTextAlignment();
                m_fonts.smallText->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
                m_pRenderTarget->DrawText(recents[i].fileType.c_str(), (UINT32)recents[i].fileType.length(),
                    m_fonts.smallText, rcExt, m_bTextSecondary);

                // Time ago (e.g. 14 dk önce)
                D2D1_RECT_F rcTime = D2D1::RectF(rRight - 120.0f, rcRow.top + 31.0f, rRight - 28.0f, rcRow.top + 48.0f);
                m_pRenderTarget->DrawText(recents[i].timeAgo.c_str(), (UINT32)recents[i].timeAgo.length(),
                    m_fonts.smallText, rcTime, m_bTextMuted);
                m_fonts.smallText->SetTextAlignment(oldAlign);
            }
        }
    }
}

void UIWindow::RenderProgressModal(float w, float h) {
    // Sub-window native KaisekiProgressDialog handles progress rendering with zero flicker.
    // In-window modal removed to avoid duplicate progress bars.
}

void UIWindow::RenderBottomStatusBar(float w, float h) {
    float barH = 24.0f;
    float y = h - barH;
    D2D1_RECT_F rcBar = D2D1::RectF(0, y, w, h);

    // 1. Sleek IDE Bottom Status Bar background (#121216)
    m_pRenderTarget->FillRectangle(&rcBar, m_bMenuBg);
    m_pRenderTarget->DrawLine(D2D1::Point2F(0, y), D2D1::Point2F(w, y), m_bBorderSubtle, 1.0f);

    // 2. Left Status: Green Pulse Dot + Message with Breathing Aura
    float itemX = 14.0f;
    ID2D1SolidColorBrush* bDot = m_pModel ? m_bSuccess : m_bTextMuted;

    m_pRenderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(itemX + 4.0f, y + 12.0f), 3.5f, 3.5f), bDot);

    bool isTr = (KaisekiLang::Get().GetLanguage() == AppLanguage::Turkish);
    if (m_fonts.smallText) {
        std::wstring statusMsg;
        bool hasActiveToast = (!m_toastText.empty() && (GetTickCount() - m_toastStartTime < 4000));
        if (hasActiveToast) {
            statusMsg = m_toastText;
        } else if (m_pModel) {
            statusMsg = (isTr ? L"Hazır • " : L"Ready • ") + m_pModel->fileName + (m_pModel->packageName.empty() ? L"" : L" (" + KaisekiTheme::Utf8ToWide(m_pModel->packageName) + L")");
        } else {
            statusMsg = isTr ? L"Hazır • APK bekleniyor" : L"Ready • Waiting for APK";
        }
        ID2D1SolidColorBrush* bStatusText = (hasActiveToast && m_toastIsError) ? m_bCloseRed : m_bTextSecondary;
        m_pRenderTarget->DrawText(statusMsg.c_str(), (UINT32)statusMsg.length(), m_fonts.smallText,
            D2D1::RectF(itemX + 16.0f, y + 4.0f, w * 0.5f, y + barH), bStatusText);
    }

    // 3. Right Status: Details
    if (m_fonts.smallText) {
        std::wstring rightInfo;
        if (m_pModel) {
            rightInfo = L"UTF-8  │  Multidex (" + std::to_wstring(m_pModel->totalClasses) + (isTr ? L" Sınıf)  │  " : L" Classes)  │  ") + L"x64 Standalone  │  Kaiseki " KAISEKI_FULL_VERSION_WSTRING;
        } else {
            rightInfo = isTr ? L"UTF-8  │  x64 Yerel Motor  │  Kaiseki " KAISEKI_FULL_VERSION_WSTRING : L"UTF-8  │  x64 Native Engine  │  Kaiseki " KAISEKI_FULL_VERSION_WSTRING;
        }

        m_pRenderTarget->DrawText(rightInfo.c_str(), (UINT32)rightInfo.length(), m_fonts.smallText,
            D2D1::RectF(w * 0.45f, y + 4.0f, w - 16.0f, y + barH), m_bTextMuted);
    }
}

void UIWindow::RenderActivityRail(float h) {
    D2D1_RECT_F rcRail = D2D1::RectF(0, 40.0f, 50.0f, h);
    m_pRenderTarget->FillRectangle(&rcRail, m_bRailBg);
    m_pRenderTarget->DrawLine(D2D1::Point2F(50.0f, 40.0f), D2D1::Point2F(50.0f, h), m_bBorderSubtle, 1.0f);

    float iconSize = 40.0f;
    float startY = 50.0f;
    float spacing = 48.0f;

    // Smoothly sliding active indicator pill with spring lerp!
    m_pRenderTarget->FillRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(0, m_animRailY + 6.0f, 3.5f, m_animRailY + iconSize - 6.0f), 2.0f, 2.0f),
        m_bAccent);

    // Top 4 Primary Workspace Rails
    for (int i = 0; i < 4; ++i) {
        float iy = startY + i * spacing;
        m_rcRailIcons[i] = D2D1::RectF(5.0f, iy, 45.0f, iy + iconSize);

        bool active = ((int)m_railMode == i);
        float hovAlpha = m_railHoverAlpha[i];

        if (active) {
            KaisekiTheme::DrawCard(m_pRenderTarget, m_rcRailIcons[i], m_bCardHover, nullptr, 6.0f);
        } else if (hovAlpha > 0.02f) {
            ID2D1SolidColorBrush* bHover = nullptr;
            m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.18f, 0.20f, 0.25f, hovAlpha), &bHover);
            if (bHover) {
                KaisekiTheme::DrawCard(m_pRenderTarget, m_rcRailIcons[i], bHover, nullptr, 6.0f);
                bHover->Release();
            }
        }

        ID2D1SolidColorBrush* bIcon = active ? m_bAccent : ((hovAlpha > 0.5f) ? m_bTextPrimary : m_bTextSecondary);
        float cx = 25.0f;
        float cy = iy + 20.0f;

        switch (i) {
        case 0: VectorIcons::DrawRailExplorer(m_pRenderTarget, cx, cy, 20.0f, bIcon); break;
        case 1: VectorIcons::DrawRailInfo(m_pRenderTarget, cx, cy, 20.0f, bIcon); break;
        case 2: VectorIcons::DrawRailSigner(m_pRenderTarget, cx, cy, 20.0f, bIcon); break;
        case 3: VectorIcons::DrawRailPhone(m_pRenderTarget, cx, cy, 20.0f, bIcon); break;
        }
    }
}

void UIWindow::RenderExplorerView(float x, float y, float w, float h) {
    float treeW = 260.0f;
    float consoleH = m_consoleCollapsed ? 32.0f : 140.0f;
    float docH = h - consoleH;
    float docW = w - treeW;

    // 1. Left Tree Panel
    D2D1_RECT_F rcTree = D2D1::RectF(x, y, x + treeW, y + h);
    m_pRenderTarget->FillRectangle(&rcTree, m_bPanel);
    m_pRenderTarget->DrawLine(D2D1::Point2F(x + treeW, y), D2D1::Point2F(x + treeW, y + h), m_bBorderSubtle, 1.0f);

    if (m_fonts.label) {
        m_pRenderTarget->DrawText(L"PROJE GEZGİNİ (APK)", 19, m_fonts.label, D2D1::RectF(x + 14.0f, y + 12.0f, x + treeW, y + 32.0f), m_bTextMuted);
    }

    if (m_treeItemsDirty || (!m_pkgTreeBuilt && m_pModel)) {
        RebuildFlattenedTreeItems();
    }

    float itemH = 26.0f;
    float rowH = itemH + 2.0f;
    float visibleTreeH = h - 42.0f;
    float totalTreeH = (float)m_cachedTreeItems.size() * rowH;
    m_maxTreeScrollY = (totalTreeH > visibleTreeH) ? (totalTreeH - visibleTreeH) : 0.0f;
    if (m_treeScrollY > m_maxTreeScrollY) m_treeScrollY = m_maxTreeScrollY;
    if (m_treeScrollY < 0.0f) m_treeScrollY = 0.0f;

    D2D1_RECT_F rcTreeClip = D2D1::RectF(x, y + 36.0f, x + treeW, y + h);
    m_pRenderTarget->PushAxisAlignedClip(rcTreeClip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    int startIdx = (int)(m_treeScrollY / rowH);
    if (startIdx < 0) startIdx = 0;
    int visibleCount = (int)(visibleTreeH / rowH) + 2;
    int endIdx = (std::min)((int)m_cachedTreeItems.size(), startIdx + visibleCount);

    for (int i = startIdx; i < endIdx; ++i) {
        const auto& it = m_cachedTreeItems[i];
        float curItemY = y + 40.0f - m_treeScrollY + (float)i * rowH;
        D2D1_RECT_F rItem = D2D1::RectF(x + 4.0f, curItemY, x + treeW - 4.0f, curItemY + itemH);

        bool isSel = (m_selectedTreeItem == i);
        if (isSel) {
            KaisekiTheme::DrawCard(m_pRenderTarget, rItem, m_bCardHover, m_bAccent, 4.0f, 1.0f);
        }

        float indX = x + 12.0f + it.depth * 14.0f;
        if (it.iconType == 0) {
            VectorIcons::DrawFolder(m_pRenderTarget, indX, curItemY + 6.0f, 14.0f, m_bAccentLight);
        } else if (it.iconType == 2) {
            VectorIcons::DrawCodeIcon(m_pRenderTarget, indX, curItemY + 6.0f, 14.0f, m_bAccent);
        } else {
            VectorIcons::DrawDocument(m_pRenderTarget, indX, curItemY + 6.0f, 14.0f, m_bTextSecondary);
        }

        if (m_fonts.body) {
            m_pRenderTarget->DrawText(it.label.c_str(), (UINT32)it.label.length(), m_fonts.body,
                D2D1::RectF(indX + 20.0f, curItemY + 4.0f, x + treeW - 6.0f, curItemY + itemH),
                isSel ? m_bTextPrimary : m_bTextSecondary);
        }
    }

    m_pRenderTarget->PopAxisAlignedClip();

    if (m_maxTreeScrollY > 0.0f) {
        float trackH = visibleTreeH;
        float thumbH = KaisekiTheme::MaxF(24.0f, (visibleTreeH / totalTreeH) * trackH);
        float thumbY = y + 38.0f + (m_treeScrollY / m_maxTreeScrollY) * (trackH - thumbH);
        m_rcTreeScrollTrack = D2D1::RectF(x + treeW - 12.0f, y + 38.0f, x + treeW - 2.0f, y + 38.0f + trackH);
        m_rcTreeScrollThumb = D2D1::RectF(x + treeW - 8.0f, thumbY, x + treeW - 2.0f, thumbY + thumbH);

        bool isTreeHover = (m_lastMouseX >= x + treeW - 24.0f && m_lastMouseX <= x + treeW && m_lastMouseY >= y + 38.0f && m_lastMouseY <= y + 38.0f + trackH);
        if (isTreeHover || m_isDraggingTreeScroll) {
            m_treeScrollAlpha = 1.0f;
        }

        if (m_treeScrollAlpha > 0.05f) {
            ID2D1SolidColorBrush* bThumb = nullptr;
            float alpha = m_treeScrollAlpha * (m_isDraggingTreeScroll ? 0.85f : (isTreeHover ? 0.70f : 0.40f));
            m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(m_colors.accent.r, m_colors.accent.g, m_colors.accent.b, alpha), &bThumb);
            if (bThumb) {
                KaisekiTheme::DrawCard(m_pRenderTarget, m_rcTreeScrollThumb, bThumb, nullptr, 3.0f);
                bThumb->Release();
            }
        }
    } else {
        m_rcTreeScrollTrack = D2D1::RectF(0, 0, 0, 0);
        m_rcTreeScrollThumb = D2D1::RectF(0, 0, 0, 0);
    }

    // 2. Document Tab Bar & Editor Content
    float docX = x + treeW;
    RenderDocumentTabBar(docX, y, docW);
    RenderDocumentContent(docX, y + 36.0f, docW, docH - 36.0f);

    // 3. Bottom Console
    RenderBottomConsole(docX, y + docH, docW, consoleH);
}

void UIWindow::RenderDocumentTabBar(float x, float y, float w) {
    D2D1_RECT_F rcBar = D2D1::RectF(x, y, x + w, y + 36.0f);
    m_pRenderTarget->FillRectangle(&rcBar, m_bMenuBg);
    m_pRenderTarget->DrawLine(D2D1::Point2F(x, y + 36.0f), D2D1::Point2F(x + w, y + 36.0f), m_bBorderSubtle, 1.0f);

    float navAreaW = 56.0f;
    float tabsVisibleW = w - navAreaW;

    float totalTabsW = 0.0f;
    for (const auto& tab : m_tabs) {
        float tabW = 160.0f;
        if (tab.title.length() > 14) tabW = (float)tab.title.length() * 8.5f + 48.0f;
        if (tab.isModified) tabW += 24.0f;
        totalTabsW += tabW + 6.0f;
    }

    m_maxTabScrollX = (totalTabsW > tabsVisibleW) ? (totalTabsW - tabsVisibleW) : 0.0f;

    // Auto scroll to make active tab fully visible
    if (m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabs.size()) {
        float activeLeft = 0.0f;
        for (int t = 0; t < m_activeTabIndex; ++t) {
            float tw = 160.0f;
            if (m_tabs[t].title.length() > 14) tw = (float)m_tabs[t].title.length() * 8.5f + 48.0f;
            if (m_tabs[t].isModified) tw += 24.0f;
            activeLeft += tw + 6.0f;
        }
        float curTw = 160.0f;
        if (m_tabs[m_activeTabIndex].title.length() > 14) curTw = (float)m_tabs[m_activeTabIndex].title.length() * 8.5f + 48.0f;
        if (m_tabs[m_activeTabIndex].isModified) curTw += 24.0f;

        if (activeLeft < m_tabScrollX) {
            m_tabScrollX = activeLeft;
        } else if (activeLeft + curTw > m_tabScrollX + tabsVisibleW) {
            m_tabScrollX = activeLeft + curTw - tabsVisibleW;
        }
        m_animTabScrollX = m_tabScrollX;
    }

    if (m_tabScrollX > m_maxTabScrollX) m_tabScrollX = m_maxTabScrollX;
    if (m_tabScrollX < 0.0f) m_tabScrollX = 0.0f;

    D2D1_RECT_F rcTabClip = D2D1::RectF(x, y, x + tabsVisibleW, y + 36.0f);
    m_pRenderTarget->PushAxisAlignedClip(rcTabClip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    float tabX = x + 8.0f - m_animTabScrollX;
    float pillH = 26.0f;
    float pillY = y + 5.0f;

    for (size_t i = 0; i < m_tabs.size(); ++i) {
        auto& tab = m_tabs[i];
        bool isActive = ((int)i == m_activeTabIndex);
        bool isHover = (m_hoverTab == (int)i);

        float tabW = 160.0f;
        if (tab.title.length() > 14) tabW = (float)tab.title.length() * 8.5f + 48.0f;
        if (tab.isModified) tabW += 24.0f;

        tab.rcTab = D2D1::RectF(tabX, pillY, tabX + tabW, pillY + pillH);

        // Antigravity dark pill tab style (rounded rectangle radius 6px)
        D2D1_ROUNDED_RECT pillR = D2D1::RoundedRect(tab.rcTab, 6.0f, 6.0f);
        if (isActive) {
            m_pRenderTarget->FillRoundedRectangle(&pillR, m_bTabActiveBg);
            m_pRenderTarget->DrawRoundedRectangle(&pillR, m_bTabActiveBorder, 1.0f);
        } else if (isHover) {
            m_pRenderTarget->FillRoundedRectangle(&pillR, m_bTabHoverBg);
        }

        // Left Badge Icon
        float iconX = tabX + 9.0f;
        float iconY = pillY + 5.0f;
        if (tab.filePath.size() >= 4 && tab.filePath.substr(tab.filePath.size() - 4) == ".cpp") {
            if (m_fonts.smallText) {
                m_pRenderTarget->DrawText(L"C++", 3, m_fonts.smallText, D2D1::RectF(iconX, iconY, iconX + 22.0f, iconY + 16.0f), m_bSyntaxKeyword);
            }
        } else if (tab.type == TabContentType::SmaliCode || (tab.filePath.size() >= 4 && tab.filePath.substr(tab.filePath.size() - 4) == ".dex")) {
            if (m_fonts.smallText) {
                m_pRenderTarget->DrawText(L"{}", 2, m_fonts.smallText, D2D1::RectF(iconX, iconY, iconX + 18.0f, iconY + 16.0f), m_bSyntaxRegister);
            }
        } else if (tab.type == TabContentType::XmlText || (tab.filePath.size() >= 4 && tab.filePath.substr(tab.filePath.size() - 4) == ".xml")) {
            if (m_fonts.smallText) {
                m_pRenderTarget->DrawText(L"< >", 3, m_fonts.smallText, D2D1::RectF(iconX, iconY, iconX + 18.0f, iconY + 16.0f), m_bSyntaxString);
            }
        } else if (tab.type == TabContentType::ImagePreview) {
            VectorIcons::DrawDocument(m_pRenderTarget, iconX, iconY, 12.0f, m_bSyntaxAnnotation);
        } else if (tab.type == TabContentType::HexViewer) {
            if (m_fonts.smallText) {
                m_pRenderTarget->DrawText(L"0x", 2, m_fonts.smallText, D2D1::RectF(iconX, iconY, iconX + 18.0f, iconY + 16.0f), m_bWarning);
            }
        } else {
            VectorIcons::DrawDocument(m_pRenderTarget, iconX, iconY, 12.0f, m_bTextMuted);
        }

        // Close ✕ button (always on the right)
        tab.rcCloseBtn = D2D1::RectF(tabX + tabW - 20.0f, pillY + 5.0f, tabX + tabW - 4.0f, pillY + pillH - 5.0f);
        bool isCloseHov = (m_hoverTabClose == (int)i);
        VectorIcons::DrawTabClose(m_pRenderTarget, tab.rcCloseBtn, isCloseHov ? m_bCloseRed : m_bTextMuted);

        // Save Diskette Icon Button ONLY if modified and NOT read-only
        if (tab.isModified && !tab.isReadOnly) {
            tab.rcSaveBtn = D2D1::RectF(tab.rcCloseBtn.left - 20.0f, pillY + 4.0f, tab.rcCloseBtn.left - 2.0f, pillY + pillH - 4.0f);
            bool isSaveHov = (m_hoverTabSave == (int)i);
            if (isSaveHov) {
                D2D1_ROUNDED_RECT saveHoverR = D2D1::RoundedRect(tab.rcSaveBtn, 4.0f, 4.0f);
                m_pRenderTarget->FillRoundedRectangle(&saveHoverR, m_bCardHover);
            }
            VectorIcons::DrawSaveDisk(m_pRenderTarget, tab.rcSaveBtn.left + 2.0f, tab.rcSaveBtn.top + 2.0f, 14.0f, isSaveHov ? m_bTextPrimary : m_bAccent, 1.3f);
        } else {
            tab.rcSaveBtn = D2D1::RectF(0.0f, 0.0f, 0.0f, 0.0f);
        }

        // Italic Tab Title
        float textX = iconX + 22.0f;
        float textW = (tab.isModified && !tab.isReadOnly) ? (tab.rcSaveBtn.left - 4.0f) : (tab.rcCloseBtn.left - 4.0f);
        IDWriteTextFormat* tf = m_fonts.tabItalic ? m_fonts.tabItalic : m_fonts.body;
        if (tf) {
            m_pRenderTarget->DrawText(tab.title.c_str(), (UINT32)tab.title.length(), tf,
                D2D1::RectF(textX, pillY + 4.0f, textW, pillY + pillH),
                isActive ? m_bTextPrimary : m_bTextSecondary);
        }

        tabX += tabW + 6.0f;
    }

    // Sliding active tab indicator underline
    if (m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabs.size()) {
        float activeTabX = m_tabs[m_activeTabIndex].rcTab.left;
        float activeTabW = m_tabs[m_activeTabIndex].rcTab.right - m_tabs[m_activeTabIndex].rcTab.left;
        if (activeTabW > 10.0f) {
            if (m_tabUnderlineW < 1.0f) {
                m_tabUnderlineX = activeTabX;
                m_tabUnderlineW = activeTabW;
            } else {
                m_tabUnderlineX += (activeTabX - m_tabUnderlineX) * 0.28f;
                m_tabUnderlineW += (activeTabW - m_tabUnderlineW) * 0.28f;
            }
            m_pRenderTarget->FillRoundedRectangle(
                D2D1::RoundedRect(D2D1::RectF(m_tabUnderlineX + 8.0f, y + 33.0f, m_tabUnderlineX + m_tabUnderlineW - 8.0f, y + 36.0f), 1.5f, 1.5f),
                m_bAccent);
        }
    }

    m_pRenderTarget->PopAxisAlignedClip();

    // Render Navigation Arrow Buttons (< and >)
    float btnY = y + 5.0f;
    float btnH = 26.0f;
    m_rcTabScrollLeft = D2D1::RectF(x + w - 52.0f, btnY, x + w - 28.0f, btnY + btnH);
    m_rcTabScrollRight = D2D1::RectF(x + w - 26.0f, btnY, x + w - 2.0f, btnY + btnH);

    KaisekiTheme::DrawCard(m_pRenderTarget, m_rcTabScrollLeft, m_hoverTabScrollLeft ? m_bCardHover : m_bCard, m_bBorderSubtle, 4.0f);
    KaisekiTheme::DrawCard(m_pRenderTarget, m_rcTabScrollRight, m_hoverTabScrollRight ? m_bCardHover : m_bCard, m_bBorderSubtle, 4.0f);

    if (m_fonts.button) {
        m_pRenderTarget->DrawText(L"◀", 1, m_fonts.button, D2D1::RectF(m_rcTabScrollLeft.left + 4.0f, btnY + 4.0f, m_rcTabScrollLeft.right, btnY + btnH), m_hoverTabScrollLeft ? m_bAccent : m_bTextSecondary);
        m_pRenderTarget->DrawText(L"▶", 1, m_fonts.button, D2D1::RectF(m_rcTabScrollRight.left + 5.0f, btnY + 4.0f, m_rcTabScrollRight.right, btnY + btnH), m_hoverTabScrollRight ? m_bAccent : m_bTextSecondary);
    }
}

void UIWindow::CopySelectedTextToClipboard() {
    if (m_tabs.empty() || m_activeTabIndex < 0 || m_activeTabIndex >= (int)m_tabs.size()) return;
    OpenDocTab& tab = m_tabs[m_activeTabIndex];
    if (!tab.hasSelection) return;

    std::wstring selText = tab.GetSelectedText();
    if (selText.empty()) return;

    if (OpenClipboard(m_hwnd)) {
        EmptyClipboard();
        size_t bytes = (selText.length() + 1) * sizeof(wchar_t);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (hMem) {
            void* pMem = GlobalLock(hMem);
            if (pMem) {
                memcpy(pMem, selText.c_str(), bytes);
                GlobalUnlock(hMem);
                SetClipboardData(CF_UNICODETEXT, hMem);
            }
        }
        CloseClipboard();
        ShowToast(L"📋 Metin panoya kopyalandı (Ctrl+C)");
    }
}

void UIWindow::ApplySyntaxHighlighting(bool isJava, TabContentType type, const std::wstring& line, IDWriteTextLayout* pLayout) {
    if (line.empty() || !pLayout) return;
    UINT32 lineLen = (UINT32)line.length();

    // 1. XML Highlighting
    if (type == TabContentType::XmlText || (!line.empty() && line[0] == L'<' && line.find(L"xml") != std::wstring::npos)) {
        // Comments <!-- ... -->
        size_t cOpen = line.find(L"<!--");
        if (cOpen != std::wstring::npos) {
            size_t cClose = line.find(L"-->", cOpen);
            size_t endPos = (cClose != std::wstring::npos) ? cClose + 3 : lineLen;
            if (m_bSyntaxComment) pLayout->SetDrawingEffect(m_bSyntaxComment, DWRITE_TEXT_RANGE{ (UINT32)cOpen, (UINT32)(endPos - cOpen) });
            return;
        }
        // Tags <...> </... /> and attributes
        bool inTag = false;
        size_t tagStart = 0;
        for (size_t idx = 0; idx < lineLen; ++idx) {
            if (line[idx] == L'<') {
                inTag = true;
                tagStart = idx;
            } else if (line[idx] == L'>') {
                if (inTag) {
                    if (m_bSyntaxTag) pLayout->SetDrawingEffect(m_bSyntaxTag, DWRITE_TEXT_RANGE{ (UINT32)tagStart, (UINT32)(idx - tagStart + 1) });
                    inTag = false;
                }
            } else if (line[idx] == L'"') {
                size_t qEnd = line.find(L'"', idx + 1);
                if (qEnd != std::wstring::npos) {
                    if (m_bSyntaxString) pLayout->SetDrawingEffect(m_bSyntaxString, DWRITE_TEXT_RANGE{ (UINT32)idx, (UINT32)(qEnd - idx + 1) });
                    idx = qEnd;
                }
            } else if (inTag && (iswalnum(line[idx]) || line[idx] == L':' || line[idx] == L'-')) {
                size_t aStart = idx;
                while (idx < lineLen && (iswalnum(line[idx]) || line[idx] == L':' || line[idx] == L'-')) idx++;
                if (idx < lineLen && line[idx] == L'=') {
                    if (m_bSyntaxAttr) pLayout->SetDrawingEffect(m_bSyntaxAttr, DWRITE_TEXT_RANGE{ (UINT32)aStart, (UINT32)(idx - aStart) });
                }
                idx--;
            }
        }
        return;
    }

    // 2. Full-line comment checks
    size_t firstNonSpace = line.find_first_not_of(L" \t");
    if (firstNonSpace != std::wstring::npos) {
        // Java/C++ comment // or /* or *
        if (isJava) {
            if (line.compare(firstNonSpace, 2, L"//") == 0 || line.compare(firstNonSpace, 2, L"/*") == 0 || line[firstNonSpace] == L'*') {
                if (m_bSyntaxComment) pLayout->SetDrawingEffect(m_bSyntaxComment, DWRITE_TEXT_RANGE{ (UINT32)firstNonSpace, (UINT32)(lineLen - firstNonSpace) });
                return;
            }
        } else {
            // Smali comment #
            if (line[firstNonSpace] == L'#') {
                if (m_bSyntaxComment) pLayout->SetDrawingEffect(m_bSyntaxComment, DWRITE_TEXT_RANGE{ (UINT32)firstNonSpace, (UINT32)(lineLen - firstNonSpace) });
                return;
            }
        }
    }

    // 3. Scan line tokens
    size_t pos = 0;
    while (pos < lineLen) {
        wchar_t ch = line[pos];

        // Whitespace
        if (ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n') {
            pos++;
            continue;
        }

        // Check for line comments starting mid-line
        if (isJava && ch == L'/' && pos + 1 < lineLen && line[pos + 1] == L'/') {
            if (m_bSyntaxComment) pLayout->SetDrawingEffect(m_bSyntaxComment, DWRITE_TEXT_RANGE{ (UINT32)pos, (UINT32)(lineLen - pos) });
            break;
        }
        if (!isJava && ch == L'#') {
            if (m_bSyntaxComment) pLayout->SetDrawingEffect(m_bSyntaxComment, DWRITE_TEXT_RANGE{ (UINT32)pos, (UINT32)(lineLen - pos) });
            break;
        }

        // String literals "..." or '...'
        if (ch == L'"' || ch == L'\'') {
            size_t qStart = pos;
            wchar_t quoteChar = ch;
            pos++;
            while (pos < lineLen) {
                if (line[pos] == L'\\' && pos + 1 < lineLen) {
                    pos += 2; // skip escape
                } else if (line[pos] == quoteChar) {
                    pos++;
                    break;
                } else {
                    pos++;
                }
            }
            if (m_bSyntaxString) pLayout->SetDrawingEffect(m_bSyntaxString, DWRITE_TEXT_RANGE{ (UINT32)qStart, (UINT32)(pos - qStart) });
            continue;
        }

        // Annotations: @Override, @NonNull, @annotation
        if (ch == L'@') {
            size_t aStart = pos;
            pos++;
            while (pos < lineLen && (iswalnum(line[pos]) || line[pos] == L'_')) {
                pos++;
            }
            if (pos > aStart + 1) {
                if (m_bSyntaxAnnotation) pLayout->SetDrawingEffect(m_bSyntaxAnnotation, DWRITE_TEXT_RANGE{ (UINT32)aStart, (UINT32)(pos - aStart) });
            }
            continue;
        }

        // Smali directives starting with '.' (e.g. .class, .super, .method, .line)
        if (!isJava && ch == L'.') {
            size_t dStart = pos;
            pos++;
            while (pos < lineLen && (iswalnum(line[pos]) || line[pos] == L'-' || line[pos] == L'_')) {
                pos++;
            }
            if (m_bSyntaxKeyword) pLayout->SetDrawingEffect(m_bSyntaxKeyword, DWRITE_TEXT_RANGE{ (UINT32)dStart, (UINT32)(pos - dStart) });
            continue;
        }

        // Smali type descriptors: Ljava/lang/String;
        if (!isJava && ch == L'L' && pos + 1 < lineLen && (line.find(L'/', pos) != std::wstring::npos || line.find(L';', pos) != std::wstring::npos)) {
            size_t tStart = pos;
            while (pos < lineLen && line[pos] != L';' && line[pos] != L' ' && line[pos] != L'(' && line[pos] != L')' && line[pos] != L',') {
                pos++;
            }
            if (pos < lineLen && line[pos] == L';') pos++;
            if (m_bSyntaxType) pLayout->SetDrawingEffect(m_bSyntaxType, DWRITE_TEXT_RANGE{ (UINT32)tStart, (UINT32)(pos - tStart) });
            continue;
        }

        // Numbers: hex 0x... or decimal
        if ((ch == L'0' && pos + 1 < lineLen && (line[pos + 1] == L'x' || line[pos + 1] == L'X')) || iswdigit(ch)) {
            size_t nStart = pos;
            if (ch == L'0' && pos + 1 < lineLen && (line[pos + 1] == L'x' || line[pos + 1] == L'X')) {
                pos += 2;
                while (pos < lineLen && (iswxdigit(line[pos]) || line[pos] == L'L' || line[pos] == L'l')) pos++;
            } else {
                while (pos < lineLen && (iswalnum(line[pos]) || line[pos] == L'.' || line[pos] == L'f' || line[pos] == L'F' || line[pos] == L'L' || line[pos] == L'l')) pos++;
            }
            if (m_bSyntaxNumber) pLayout->SetDrawingEffect(m_bSyntaxNumber, DWRITE_TEXT_RANGE{ (UINT32)nStart, (UINT32)(pos - nStart) });
            continue;
        }

        // Words / Identifiers
        if (iswalpha(ch) || ch == L'_' || ch == L'-') {
            size_t wStart = pos;
            while (pos < lineLen && (iswalnum(line[pos]) || line[pos] == L'_' || line[pos] == L'-')) {
                pos++;
            }
            std::wstring word = line.substr(wStart, pos - wStart);

            if (isJava) {
                // Java Keywords Check
                static const std::wstring javaKeywords[] = {
                    L"abstract", L"assert", L"boolean", L"break", L"byte", L"case", L"catch", L"char", L"class",
                    L"const", L"continue", L"default", L"do", L"double", L"else", L"enum", L"extends", L"final",
                    L"finally", L"float", L"for", L"goto", L"if", L"implements", L"import", L"instanceof", L"int",
                    L"interface", L"long", L"native", L"new", L"package", L"private", L"protected", L"public",
                    L"return", L"short", L"static", L"strictfp", L"super", L"switch", L"synchronized", L"this",
                    L"throw", L"throws", L"transient", L"try", L"void", L"volatile", L"while", L"true", L"false", L"null"
                };
                bool isKw = false;
                for (const auto& kw : javaKeywords) {
                    if (word == kw) {
                        isKw = true;
                        break;
                    }
                }

                if (isKw) {
                    if (m_bSyntaxKeyword) pLayout->SetDrawingEffect(m_bSyntaxKeyword, DWRITE_TEXT_RANGE{ (UINT32)wStart, (UINT32)(pos - wStart) });
                } else if (iswupper(word[0])) {
                    // Type / Class name (starts with capital letter)
                    if (m_bSyntaxType) pLayout->SetDrawingEffect(m_bSyntaxType, DWRITE_TEXT_RANGE{ (UINT32)wStart, (UINT32)(pos - wStart) });
                } else {
                    // Check if function / method invocation (followed by '(')
                    size_t nextPos = pos;
                    while (nextPos < lineLen && (line[nextPos] == L' ' || line[nextPos] == L'\t')) nextPos++;
                    if (nextPos < lineLen && line[nextPos] == L'(') {
                        if (m_bSyntaxTag) pLayout->SetDrawingEffect(m_bSyntaxTag, DWRITE_TEXT_RANGE{ (UINT32)wStart, (UINT32)(pos - wStart) });
                    }
                }
            } else {
                // Smali Identifiers / Registers / Opcodes
                if ((word[0] == L'v' || word[0] == L'p') && word.length() >= 2 && iswdigit(word[1])) {
                    // Register v0, p1
                    if (m_bSyntaxRegister) pLayout->SetDrawingEffect(m_bSyntaxRegister, DWRITE_TEXT_RANGE{ (UINT32)wStart, (UINT32)(pos - wStart) });
                } else {
                    static const std::wstring smaliOpcodes[] = {
                        L"invoke-virtual", L"invoke-direct", L"invoke-static", L"invoke-super", L"invoke-interface",
                        L"const-string", L"const-class", L"const/4", L"const/16", L"const", L"const-wide",
                        L"return-void", L"return", L"return-object", L"move-result", L"move-result-object",
                        L"sget-object", L"sget", L"sput-object", L"sput", L"iget-object", L"iget", L"iput-object", L"iput",
                        L"new-instance", L"check-cast", L"instance-of", L"new-array", L"array-length", L"goto",
                        L"if-eqz", L"if-nez", L"if-eq", L"if-ne", L"if-lt", L"if-ge", L"if-gt", L"if-le"
                    };
                    bool isOp = false;
                    for (const auto& op : smaliOpcodes) {
                        if (word == op) {
                            isOp = true;
                            break;
                        }
                    }
                    if (isOp) {
                        if (m_bSyntaxKeyword) pLayout->SetDrawingEffect(m_bSyntaxKeyword, DWRITE_TEXT_RANGE{ (UINT32)wStart, (UINT32)(pos - wStart) });
                    }
                }
            }
            continue;
        }

        pos++;
    }
}

void UIWindow::RenderDocumentContent(float x, float y, float w, float h) {
    D2D1_RECT_F rcContent = D2D1::RectF(x, y, x + w, y + h);
    m_pRenderTarget->FillRectangle(&rcContent, m_bWell);

    if (m_tabs.empty() || m_activeTabIndex < 0 || m_activeTabIndex >= (int)m_tabs.size()) {
        if (m_fonts.subhead) {
            m_pRenderTarget->DrawText(L"Açık bir dosya yok.", 18, m_fonts.subhead, D2D1::RectF(x + 30.0f, y + 40.0f, x + w, y + 100.0f), m_bTextMuted);
        }
        return;
    }

    OpenDocTab& tab = m_tabs[m_activeTabIndex];

    float tbH = 30.0f;
    D2D1_RECT_F rcToolbar = D2D1::RectF(x, y, x + w, y + tbH);
    m_pRenderTarget->FillRectangle(&rcToolbar, m_bPanel);
    m_pRenderTarget->DrawLine(D2D1::Point2F(x, y + tbH), D2D1::Point2F(x + w, y + tbH), m_bBorderSubtle, 1.0f);

    float btnX = x + 10.0f;
    // Toggle Java / Smali Button
    if (tab.type == TabContentType::SmaliCode || tab.type == TabContentType::JavaCode) {
        m_rcBtnToggleJavaSmali = D2D1::RectF(btnX, y + 4.0f, btnX + 160.0f, y + 26.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBtnToggleJavaSmali, m_bCardHover, m_bAccent, 4.0f);
        if (m_fonts.smallText) {
            const wchar_t* btnLbl = tab.isJavaMode ? L"⇄ Smali Görünümüne Geç" : L"⇄ Java Dekompile Görünümü";
            m_pRenderTarget->DrawText(btnLbl, (UINT32)wcslen(btnLbl), m_fonts.smallText,
                D2D1::RectF(m_rcBtnToggleJavaSmali.left + 6.0f, y + 6.0f, m_rcBtnToggleJavaSmali.right, y + 26.0f), m_bTextPrimary);
        }
        btnX += 170.0f;

        // Native Frida Hook Script Generator Button
        m_rcBtnJadxDecompile = D2D1::RectF(btnX, y + 4.0f, btnX + 160.0f, y + 26.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBtnJadxDecompile, m_bCard, m_bBorderSubtle, 4.0f);
        if (m_fonts.smallText) {
            m_pRenderTarget->DrawText(L"💉 Frida Hook Scripti", 21, m_fonts.smallText,
                D2D1::RectF(m_rcBtnJadxDecompile.left + 8.0f, y + 6.0f, m_rcBtnJadxDecompile.right, y + 26.0f), m_bAccent);
        }
        btnX += 170.0f;
    }

    // Repackage & Sign APK Button (F8)
    m_rcBtnRepackApk = D2D1::RectF(btnX, y + 4.0f, btnX + 190.0f, y + 26.0f);
    KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBtnRepackApk, tab.isModified ? m_bBtnCompile : m_bCard, tab.isModified ? nullptr : m_bBorderSubtle, 4.0f);
    if (m_fonts.smallText) {
        std::wstring rText = L"🔨 APK Paketle & İmzala (F8)";
        m_pRenderTarget->DrawText(rText.c_str(), (UINT32)rText.length(), m_fonts.smallText,
            D2D1::RectF(m_rcBtnRepackApk.left + 8.0f, y + 6.0f, m_rcBtnRepackApk.right, y + 26.0f), tab.isModified ? m_bTextPrimary : m_bAccent);
    }
    btnX += 200.0f;

    // Export Gradle Project Button
    m_rcBtnExportGradle = D2D1::RectF(btnX, y + 4.0f, btnX + 175.0f, y + 26.0f);
    KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBtnExportGradle, m_bCard, m_bBorderSubtle, 4.0f);
    if (m_fonts.smallText) {
        m_pRenderTarget->DrawText(L"📦 Projeyi Dışa Aktar (Gradle)", 30, m_fonts.smallText,
            D2D1::RectF(m_rcBtnExportGradle.left + 8.0f, y + 6.0f, m_rcBtnExportGradle.right, y + 26.0f), m_bTextPrimary);
    }
    btnX += 185.0f;

    // JADX Global Search Button (Only draw if space permits without overlapping right status)
    if (btnX + 165.0f < x + w - 160.0f) {
        m_rcBtnGlobalSearch = D2D1::RectF(btnX, y + 4.0f, btnX + 165.0f, y + 26.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBtnGlobalSearch, m_bCard, m_bBorderSubtle, 4.0f);
        if (m_fonts.smallText) {
            m_pRenderTarget->DrawText(L"🔍 Ara (Ctrl+Shift+F)", 20, m_fonts.smallText,
                D2D1::RectF(m_rcBtnGlobalSearch.left + 8.0f, y + 6.0f, m_rcBtnGlobalSearch.right, y + 26.0f), m_bTextSecondary);
        }
        btnX += 175.0f;
    } else {
        m_rcBtnGlobalSearch = D2D1::RectF(0, 0, 0, 0);
    }

    if (m_fonts.smallText && (x + w - 170.0f > btnX)) {
        std::wstring stat = L"Toplam: " + std::to_wstring(tab.lines.size()) + L" satır";
        m_pRenderTarget->DrawText(stat.c_str(), (UINT32)stat.length(), m_fonts.smallText,
            D2D1::RectF(x + w - 160.0f, y + 7.0f, x + w - 10.0f, y + 26.0f), m_bTextMuted);
    }

    if (tab.type == TabContentType::ImagePreview) {
        if (tab.pImageBitmap) {
            float imgW = (float)tab.imgWidth;
            float imgH = (float)tab.imgHeight;

            float maxView = 256.0f;
            float scale = 1.0f;
            if (imgW > maxView || imgH > maxView) {
                scale = KaisekiTheme::MinF(maxView / imgW, maxView / imgH);
            } else if (imgW < 96.0f && imgH < 96.0f) {
                scale = 2.0f;
            }
            float renderW = imgW * scale;
            float renderH = imgH * scale;

            float cx = x + (w - renderW) * 0.5f;
            float cy = y + tbH + (h - tbH - renderH) * 0.4f;

            D2D1_RECT_F rcImgCard = D2D1::RectF(cx - 20.0f, cy - 20.0f, cx + renderW + 20.0f, cy + renderH + 80.0f);
            KaisekiTheme::DrawCard(m_pRenderTarget, rcImgCard, m_bCard, m_bBorderSubtle, 8.0f);

            D2D1_RECT_F rcDest = D2D1::RectF(cx, cy, cx + renderW, cy + renderH);
            m_pRenderTarget->DrawBitmap(tab.pImageBitmap, &rcDest);

            if (m_fonts.body) {
                std::wstring dimStr = std::to_wstring(tab.imgWidth) + L" × " + std::to_wstring(tab.imgHeight) + L" px";
                m_pRenderTarget->DrawText(dimStr.c_str(), (UINT32)dimStr.length(), m_fonts.body,
                    D2D1::RectF(cx - 10.0f, cy + renderH + 10.0f, cx + renderW + 10.0f, cy + renderH + 30.0f), m_bTextPrimary);
            }
            if (m_fonts.smallText) {
                std::wstring meta = L"Format: PNG (32-bit RGBA) • Boyut: " + std::to_wstring(tab.imgBytes) + L" bayt";
                m_pRenderTarget->DrawText(meta.c_str(), (UINT32)meta.length(), m_fonts.smallText,
                    D2D1::RectF(cx - 10.0f, cy + renderH + 32.0f, cx + renderW + 10.0f, cy + renderH + 55.0f), m_bTextSecondary);
            }
        }
        return;
    }

    // Code Editor
    float editorY = y + tbH;
    float editorH = h - tbH;
    float gutterW = 50.0f;
    float lineH = 20.0f;

    D2D1_RECT_F rcGutter = D2D1::RectF(x, editorY, x + gutterW, editorY + editorH);
    m_pRenderTarget->FillRectangle(&rcGutter, m_bPanel);
    m_pRenderTarget->DrawLine(D2D1::Point2F(x + gutterW, editorY), D2D1::Point2F(x + gutterW, editorY + editorH), m_bBorderSubtle, 1.0f);

    float textX = x + gutterW + 10.0f;
    int startLine = (int)(tab.scrollY / lineH);
    int visibleCount = (int)(editorH / lineH) + 2;

    tab.maxScrollY = KaisekiTheme::MaxF(0.0f, (float)tab.lines.size() * lineH - editorH);

    float charW = m_fonts.codeCharWidth > 0.0f ? m_fonts.codeCharWidth : 7.2f;

    D2D1_RECT_F rcEditorClip = D2D1::RectF(x, editorY, x + w, editorY + editorH);
    m_pRenderTarget->PushAxisAlignedClip(rcEditorClip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    for (int i = startLine; i < startLine + visibleCount && i < (int)tab.lines.size(); ++i) {
        float ly = editorY + (i * lineH) - tab.scrollY;
        if (ly + lineH < editorY || ly > editorY + editorH) continue;

        if (m_fonts.code) {
            std::wstring lnum = std::to_wstring(i + 1);
            m_pRenderTarget->DrawText(lnum.c_str(), (UINT32)lnum.length(), m_fonts.code,
                D2D1::RectF(x + 5.0f, ly, x + gutterW - 8.0f, ly + lineH), m_bTextMuted);
        }

        // Active line background highlight (when not read-only and no multi-line selection)
        if (!tab.isReadOnly && i == tab.cursorLine && !tab.hasSelection) {
            D2D1_RECT_F rcActiveLine = D2D1::RectF(textX - 4.0f, ly, x + w, ly + lineH);
            m_pRenderTarget->FillRectangle(&rcActiveLine, m_bCard);
        }

        std::wstring lineW = KaisekiTheme::Utf8ToWide(tab.lines[i]);
        while (!lineW.empty() && (lineW.back() == L'\r' || lineW.back() == L'\n')) {
            lineW.pop_back();
        }
        IDWriteTextLayout* pLayout = nullptr;
        if (m_pDWriteFactory && m_fonts.code && !lineW.empty()) {
            m_pDWriteFactory->CreateTextLayout(
                lineW.c_str(),
                (UINT32)lineW.length(),
                m_fonts.code,
                10000.0f,
                lineH,
                &pLayout
            );
            if (pLayout) {
                pLayout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            }
        }

        // Draw Selection Highlight Rectangle for this line!
        if (tab.hasSelection) {
            int sLine, sCol, eLine, eCol;
            tab.GetNormalizedSelection(sLine, sCol, eLine, eCol);
            if (i >= sLine && i <= eLine) {
                int selStartCol = (i == sLine) ? sCol : 0;
                int selEndCol = (i == eLine) ? eCol : (int)lineW.length();
                if (selStartCol < 0) selStartCol = 0;
                if (selEndCol > (int)lineW.length()) selEndCol = (int)lineW.length();

                float selX1 = GetCharXPos(pLayout, lineW, selStartCol, textX, charW);
                float selX2 = GetCharXPos(pLayout, lineW, selEndCol, textX, charW);

                if (selX2 <= selX1 && i < eLine) {
                    selX2 = selX1 + charW; // visually show newline selected
                }
                if (selX2 > selX1 && m_bSelection) {
                    D2D1_RECT_F rcSel = D2D1::RectF(selX1, ly, selX2, ly + lineH);
                    m_pRenderTarget->FillRectangle(&rcSel, m_bSelection);
                }
            }
        }

        // Draw Code Line with DirectWrite Token-level Syntax Highlighting
        if (pLayout) {
            ApplySyntaxHighlighting(tab.isJavaMode, tab.type, lineW, pLayout);
            m_pRenderTarget->DrawTextLayout(D2D1::Point2F(textX, ly), pLayout, m_bTextPrimary);
        } else if (m_fonts.code && !lineW.empty()) {
            m_pRenderTarget->DrawText(lineW.c_str(), (UINT32)lineW.length(), m_fonts.code,
                D2D1::RectF(textX, ly, x + w - 10.0f, ly + lineH), m_bTextPrimary);
        }

        // Draw Blinking/Solid Cursor (ONLY IF NOT READ ONLY)
        if (!tab.isReadOnly && i == tab.cursorLine) {
            float cursorX = GetCharXPos(pLayout, lineW, tab.cursorCol, textX, charW);
            m_pRenderTarget->DrawLine(D2D1::Point2F(cursorX, ly + 2.0f), D2D1::Point2F(cursorX, ly + lineH - 2.0f), m_bAccent, 1.8f);
        }

        if (pLayout) {
            pLayout->Release();
            pLayout = nullptr;
        }
    }
    m_pRenderTarget->PopAxisAlignedClip();

    // Code Editor Auto-Hiding Overlay Scrollbar (Visible on Hover/Scroll & Draggable)
    if (tab.maxScrollY > 0.0f) {
        float trackH = editorH;
        float totalH = (float)tab.lines.size() * lineH;
        float thumbH = KaisekiTheme::MaxF(28.0f, (editorH / totalH) * trackH);
        float thumbY = editorY + (tab.scrollY / tab.maxScrollY) * (trackH - thumbH);

        m_rcEditorScrollTrack = D2D1::RectF(x + w - 12.0f, editorY, x + w - 2.0f, editorY + editorH);
        m_rcEditorScrollThumb = D2D1::RectF(x + w - 10.0f, thumbY, x + w - 3.0f, thumbY + thumbH);

        bool isEditorHover = (m_lastMouseX >= x + w - 24.0f && m_lastMouseX <= x + w && m_lastMouseY >= editorY && m_lastMouseY <= editorY + editorH);
        if (isEditorHover || m_isDraggingEditorScroll) {
            m_editorScrollAlpha = 1.0f;
        }

        if (m_editorScrollAlpha > 0.05f) {
            ID2D1SolidColorBrush* bThumb = nullptr;
            float alpha = m_editorScrollAlpha * (m_isDraggingEditorScroll ? 0.85f : (isEditorHover ? 0.70f : 0.40f));
            m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(m_colors.accent.r, m_colors.accent.g, m_colors.accent.b, alpha), &bThumb);
            if (bThumb) {
                KaisekiTheme::DrawCard(m_pRenderTarget, m_rcEditorScrollThumb, bThumb, nullptr, 3.0f);
                bThumb->Release();
            }
        }
    } else {
        m_rcEditorScrollTrack = D2D1::RectF(0, 0, 0, 0);
        m_rcEditorScrollThumb = D2D1::RectF(0, 0, 0, 0);
    }
}

void UIWindow::RenderBottomConsole(float x, float y, float w, float h) {
    D2D1_RECT_F rcConsole = D2D1::RectF(x, y, x + w, y + h);
    m_pRenderTarget->FillRectangle(&rcConsole, m_bConsoleBg);
    m_pRenderTarget->DrawLine(D2D1::Point2F(x, y), D2D1::Point2F(x + w, y), m_bBorderSubtle, 1.0f);

    // Tab 1: Terminal / Sistem
    m_rcBottomTabTerminal = D2D1::RectF(x + 10.0f, y + 4.0f, x + 125.0f, y + 26.0f);
    bool isTermAct = (m_bottomConsoleTab == 0);
    KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBottomTabTerminal, isTermAct ? m_bCardHover : m_bCard, isTermAct ? m_bAccent : nullptr, 4.0f);
    if (m_fonts.smallText) {
        m_pRenderTarget->DrawText(L"📟 Kaiseki Terminal", 19, m_fonts.smallText,
            D2D1::RectF(m_rcBottomTabTerminal.left + 8.0f, y + 6.0f, m_rcBottomTabTerminal.right, y + 24.0f),
            isTermAct ? m_bAccent : m_bTextSecondary);
    }

    // Tab 2: Android Logcat
    m_rcBottomTabLogcat = D2D1::RectF(x + 130.0f, y + 4.0f, x + 260.0f, y + 26.0f);
    bool isLogcatAct = (m_bottomConsoleTab == 1);
    KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBottomTabLogcat, isLogcatAct ? m_bCardHover : m_bCard, isLogcatAct ? m_bAccent : nullptr, 4.0f);
    if (m_fonts.smallText) {
        m_pRenderTarget->DrawText(L"📱 Android Logcat", 17, m_fonts.smallText,
            D2D1::RectF(m_rcBottomTabLogcat.left + 8.0f, y + 6.0f, m_rcBottomTabLogcat.right, y + 24.0f),
            isLogcatAct ? m_bAccent : m_bTextSecondary);
    }

    // Logcat-specific Filter & Stream Controls
    if (isLogcatAct) {
        float fx = x + 270.0f;

        // 1. Package Filter Button
        std::wstring pkgStr;
        if (m_pModel && !m_pModel->packageName.empty()) {
            pkgStr = m_logcatFilterByPackage ? (L"✓ Paket: " + KaisekiTheme::Utf8ToWide(m_pModel->packageName)) : L"📦 Paket: Tümü (Cihaz)";
        } else {
            pkgStr = L"📦 Paket: Tümü (Cihaz)";
        }
        float pkgBtnW = (float)pkgStr.length() * 6.5f + 16.0f;
        if (pkgBtnW > 280.0f) pkgBtnW = 280.0f;
        if (pkgBtnW < 140.0f) pkgBtnW = 140.0f;

        m_rcLogcatPkgFilter = D2D1::RectF(fx, y + 4.0f, fx + pkgBtnW, y + 26.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcLogcatPkgFilter,
            m_logcatFilterByPackage ? m_bCardHover : m_bCard,
            m_logcatFilterByPackage ? m_bAccent : m_bBorderSubtle, 4.0f);
        if (m_fonts.smallText) {
            m_pRenderTarget->DrawText(pkgStr.c_str(), (UINT32)pkgStr.length(), m_fonts.smallText,
                D2D1::RectF(m_rcLogcatPkgFilter.left + 6.0f, y + 6.0f, m_rcLogcatPkgFilter.right - 4.0f, y + 24.0f),
                m_logcatFilterByPackage ? m_bAccent : m_bTextSecondary);
        }
        fx += pkgBtnW + 8.0f;

        // 2. Level Filter Button
        std::wstring lvlStr = (m_logcatMinLevel == 'V' ? L"Seviye: Tümü ▾" :
                              (m_logcatMinLevel == 'D' ? L"Seviye: Debug+ ▾" :
                              (m_logcatMinLevel == 'I' ? L"Seviye: Info+ ▾" :
                              (m_logcatMinLevel == 'W' ? L"Seviye: Warn+ ▾" : L"Seviye: Error ▾"))));
        float lvlBtnW = 100.0f;
        m_rcLogcatLevelFilter = D2D1::RectF(fx, y + 4.0f, fx + lvlBtnW, y + 26.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcLogcatLevelFilter, m_bCard, m_bBorderSubtle, 4.0f);
        if (m_fonts.smallText) {
            m_pRenderTarget->DrawText(lvlStr.c_str(), (UINT32)lvlStr.length(), m_fonts.smallText,
                D2D1::RectF(m_rcLogcatLevelFilter.left + 6.0f, y + 6.0f, m_rcLogcatLevelFilter.right, y + 24.0f), m_bTextPrimary);
        }
        fx += lvlBtnW + 8.0f;

        // 3. Live Stream Auto-Refresh Button
        std::wstring liveStr = m_logcatStreamActive ? L"⚡ Canlı: Açık" : L"⏸️ Canlı: Kapalı";
        float liveBtnW = 95.0f;
        m_rcLogcatLiveStream = D2D1::RectF(fx, y + 4.0f, fx + liveBtnW, y + 26.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcLogcatLiveStream,
            m_logcatStreamActive ? m_bCardHover : m_bCard,
            m_logcatStreamActive ? m_bSuccess : m_bBorderSubtle, 4.0f);
        if (m_fonts.smallText) {
            m_pRenderTarget->DrawText(liveStr.c_str(), (UINT32)liveStr.length(), m_fonts.smallText,
                D2D1::RectF(m_rcLogcatLiveStream.left + 6.0f, y + 6.0f, m_rcLogcatLiveStream.right, y + 24.0f),
                m_logcatStreamActive ? m_bSuccess : m_bTextSecondary);
        }
    } else {
        m_rcLogcatPkgFilter = { 0 };
        m_rcLogcatLevelFilter = { 0 };
        m_rcLogcatLiveStream = { 0 };
    }

    // Refresh & Clear Action Buttons
    m_rcBtnLogcatRefresh = D2D1::RectF(x + w - 165.0f, y + 4.0f, x + w - 85.0f, y + 26.0f);
    KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBtnLogcatRefresh, m_bCard, m_bBorderSubtle, 4.0f);
    if (m_fonts.smallText) {
        m_pRenderTarget->DrawText(L"🔄 Yenile", 9, m_fonts.smallText,
            D2D1::RectF(m_rcBtnLogcatRefresh.left + 8.0f, y + 6.0f, m_rcBtnLogcatRefresh.right, y + 24.0f), m_bTextPrimary);
    }

    m_rcBtnLogcatClear = D2D1::RectF(x + w - 80.0f, y + 4.0f, x + w - 10.0f, y + 26.0f);
    KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBtnLogcatClear, m_bCard, m_bBorderSubtle, 4.0f);
    if (m_fonts.smallText) {
        m_pRenderTarget->DrawText(L"🗑️ Temizle", 10, m_fonts.smallText,
            D2D1::RectF(m_rcBtnLogcatClear.left + 8.0f, y + 6.0f, m_rcBtnLogcatClear.right, y + 24.0f), m_bTextMuted);
    }

    float clipTop = y + 30.0f;
    float clipBottom = y + h - 4.0f;
    D2D1_RECT_F rcLogClip = D2D1::RectF(x, clipTop, x + w, clipBottom);
    m_pRenderTarget->PushAxisAlignedClip(&rcLogClip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    float logH = 18.0f;
    float cw = m_fonts.codeCharWidth > 0.0f ? m_fonts.codeCharWidth : 7.2f;

    if (m_bottomConsoleTab == 0) {
        float logY = clipTop + 2.0f - m_consoleScrollY;
        for (int i = 0; i < (int)m_consoleLogs.size(); ++i) {
            const auto& log = m_consoleLogs[i];
            if (logY + logH >= clipTop && logY <= clipBottom) {
                std::wstring fullLine = log.tag + L"  " + log.message;

                // Selection highlight
                if (m_consoleHasSelection) {
                    int sLine, sCol, eLine, eCol;
                    GetNormalizedConsoleSelection(sLine, sCol, eLine, eCol);
                    if (i >= sLine && i <= eLine) {
                        int selStartCol = (i == sLine) ? sCol : 0;
                        int selEndCol = (i == eLine) ? eCol : (int)fullLine.length();
                        if (selStartCol < 0) selStartCol = 0;
                        if (selEndCol > (int)fullLine.length()) selEndCol = (int)fullLine.length();

                        float selX1 = x + 15.0f + (float)selStartCol * cw;
                        float selX2 = x + 15.0f + (float)selEndCol * cw;
                        if (selX2 <= selX1 && i < eLine) selX2 = selX1 + cw;
                        if (selX2 > selX1 && m_bSelection) {
                            D2D1_RECT_F rcSel = D2D1::RectF(selX1, logY, selX2, logY + logH);
                            m_pRenderTarget->FillRectangle(&rcSel, m_bSelection);
                        }
                    }
                }

                ID2D1SolidColorBrush* tagBrush = m_bAccent;
                if (log.type == 0) tagBrush = m_bSuccess;
                else if (log.type == 1) tagBrush = m_bBtnAnalyze;
                else if (log.type == 2) tagBrush = m_bWarning;

                if (m_fonts.code) {
                    float tagW = (float)log.tag.length() * cw;
                    m_pRenderTarget->DrawText(log.tag.c_str(), (UINT32)log.tag.length(), m_fonts.code,
                        D2D1::RectF(x + 15.0f, logY, x + 15.0f + tagW, logY + logH), tagBrush);

                    m_pRenderTarget->DrawText(log.message.c_str(), (UINT32)log.message.length(), m_fonts.code,
                        D2D1::RectF(x + 15.0f + tagW + 2.0f * cw, logY, x + w - 10.0f, logY + logH), m_bTextPrimary);
                }
            }
            logY += logH;
        }
    } else {
        // Tab 1: Live Android Logcat Streaming
        if (m_logcatLines.empty()) {
            if (!m_logcatLoaded) {
                RefreshLogcat();
            }
            if (m_logcatLines.empty()) {
                if (m_fonts.code) {
                    m_pRenderTarget->DrawText(L"📱 Bağlı cihazdan Logcat akışı bekleniyor... [Yenile] butonuna tıklayarak logları çekebilirsiniz.",
                        88, m_fonts.code, D2D1::RectF(x + 15.0f, clipTop + 10.0f, x + w - 10.0f, clipTop + 30.0f), m_bTextMuted);
                }
                m_pRenderTarget->PopAxisAlignedClip();
                return;
            }
        }

        float logY = clipTop + 2.0f - m_logcatScrollY;
        for (int i = 0; i < (int)m_logcatLines.size(); ++i) {
            const auto& line = m_logcatLines[i];
            if (logY + logH >= clipTop && logY <= clipBottom) {
                // Selection highlight
                if (m_consoleHasSelection) {
                    int sLine, sCol, eLine, eCol;
                    GetNormalizedConsoleSelection(sLine, sCol, eLine, eCol);
                    if (i >= sLine && i <= eLine) {
                        int selStartCol = (i == sLine) ? sCol : 0;
                        int selEndCol = (i == eLine) ? eCol : (int)line.length();
                        if (selStartCol < 0) selStartCol = 0;
                        if (selEndCol > (int)line.length()) selEndCol = (int)line.length();

                        float selX1 = x + 15.0f + (float)selStartCol * cw;
                        float selX2 = x + 15.0f + (float)selEndCol * cw;
                        if (selX2 <= selX1 && i < eLine) selX2 = selX1 + cw;
                        if (selX2 > selX1 && m_bSelection) {
                            D2D1_RECT_F rcSel = D2D1::RectF(selX1, logY, selX2, logY + logH);
                            m_pRenderTarget->FillRectangle(&rcSel, m_bSelection);
                        }
                    }
                }

                ID2D1SolidColorBrush* bLine = m_bTextPrimary;
                if (line.find(L" E ") != std::wstring::npos || line.find(L"E/") != std::wstring::npos || line.find(L"FATAL") != std::wstring::npos) {
                    bLine = m_bCloseRed;
                } else if (line.find(L" W ") != std::wstring::npos || line.find(L"W/") != std::wstring::npos) {
                    bLine = m_bWarning;
                } else if (line.find(L" I ") != std::wstring::npos || line.find(L"I/") != std::wstring::npos) {
                    bLine = m_bBtnAnalyze;
                } else if (line.find(L" D ") != std::wstring::npos || line.find(L"D/") != std::wstring::npos) {
                    bLine = m_bSuccess;
                }

                if (m_fonts.code) {
                    m_pRenderTarget->DrawText(line.c_str(), (UINT32)line.length(), m_fonts.code,
                        D2D1::RectF(x + 15.0f, logY, x + w - 10.0f, logY + logH), bLine);
                }
            }
            logY += logH;
        }
    }
    m_pRenderTarget->PopAxisAlignedClip();
}

void UIWindow::RenderApkInfoView(float x, float y, float w, float h) {
    if (!m_pModel) {
        if (m_fonts.subhead) {
            m_pRenderTarget->DrawText(L"Yüklü APK bulunamadı. Lütfen bir APK açın.", 41, m_fonts.subhead, D2D1::RectF(x + 30.0f, y + 40.0f, x + w, y + 100.0f), m_bTextMuted);
        }
        return;
    }

    float pad = 24.0f;
    float curY = y + pad;

    // Header Card
    D2D1_RECT_F rcHeader = D2D1::RectF(x + pad, curY, x + w - pad, curY + 90.0f);
    KaisekiTheme::DrawCard(m_pRenderTarget, rcHeader, m_bCard, m_bBorderSubtle, 8.0f);

    if (!m_pIconBitmap && m_pModel && !m_pModel->iconBytes.empty()) {
        CreateIconBitmap();
    }

    D2D1_ROUNDED_RECT rcIconBox = D2D1::RoundedRect(
        D2D1::RectF(rcHeader.left + 14.0f, rcHeader.top + 11.0f, rcHeader.left + 82.0f, rcHeader.top + 79.0f),
        10.0f, 10.0f
    );
    m_pRenderTarget->FillRoundedRectangle(&rcIconBox, m_bWell);
    m_pRenderTarget->DrawRoundedRectangle(&rcIconBox, m_bBorderSubtle, 1.0f);

    if (m_pIconBitmap) {
        D2D1_RECT_F rcIcon = D2D1::RectF(rcHeader.left + 16.0f, rcHeader.top + 13.0f, rcHeader.left + 80.0f, rcHeader.top + 77.0f);
        m_pRenderTarget->DrawBitmap(m_pIconBitmap, &rcIcon);
    } else {
        // Fallback Vector Android Bugdroid mascot in accent pink
        VectorIcons::DrawAndroidLogo(m_pRenderTarget, rcHeader.left + 48.0f, rcHeader.top + 44.0f, 22.0f, m_bAccent);
    }

    std::wstring appName = KaisekiTheme::Utf8ToWide(m_pModel->appLabel.empty() ? "Kaiseki" : m_pModel->appLabel);
    std::wstring pkgName = KaisekiTheme::Utf8ToWide(m_pModel->packageName);
    std::wstring verName = L"v" + KaisekiTheme::Utf8ToWide(m_pModel->versionName) + L" (Build " + KaisekiTheme::Utf8ToWide(m_pModel->versionCode) + L")";

    if (m_fonts.hero) {
        m_pRenderTarget->DrawText(appName.c_str(), (UINT32)appName.length(), m_fonts.hero,
            D2D1::RectF(rcHeader.left + 95.0f, rcHeader.top + 14.0f, rcHeader.right - 20.0f, rcHeader.top + 46.0f), m_bTextPrimary);
    }
    if (m_fonts.subhead) {
        std::wstring sub = pkgName + L"  •  " + verName;
        m_pRenderTarget->DrawText(sub.c_str(), (UINT32)sub.length(), m_fonts.subhead,
            D2D1::RectF(rcHeader.left + 95.0f, rcHeader.top + 50.0f, rcHeader.right - 20.0f, rcHeader.top + 76.0f), m_bAccent);
    }

    curY += 105.0f;

    // Stat Cards
    float colW = (w - pad * 2.0f - 36.0f) / 4.0f;
    float cardH = 80.0f;

    struct StatCardDef {
        std::wstring title;
        std::wstring val;
        ID2D1SolidColorBrush* bVal;
    };

    std::wstring sdkVal = L"SDK " + KaisekiTheme::Utf8ToWide(m_pModel->targetSdk) + L" (Min " + KaisekiTheme::Utf8ToWide(m_pModel->minSdk) + L")";
    std::wstring dexVal = std::to_wstring(m_pModel->totalClasses) + L" Sınıf / " + std::to_wstring(m_pModel->totalMethods) + L" Metot";
    std::wstring sizeVal = std::to_wstring(m_pModel->fileSize / 1024) + L" KB";
    std::wstring packerVal = KaisekiTheme::Utf8ToWide(m_secReport.packer.name);
    ID2D1SolidColorBrush* bPackerColor = m_secReport.packer.isPacked ? m_bWarning : m_bSuccess;

    std::vector<StatCardDef> stats = {
        { L"HEDEF PLATFORM", sdkVal, m_bAccent },
        { L"DEX METRİKLERİ", dexVal, m_bSuccess },
        { L"PAKET BOYUTU", sizeVal, m_bBtnAnalyze },
        { L"KORUMA & ŞİFRELEME", packerVal, bPackerColor }
    };

    for (int i = 0; i < 4; ++i) {
        float cx = x + pad + i * (colW + 12.0f);
        D2D1_RECT_F rcCard = D2D1::RectF(cx, curY, cx + colW, curY + cardH);
        KaisekiTheme::DrawCard(m_pRenderTarget, rcCard, m_bCard, m_bBorderSubtle, 6.0f);

        if (m_fonts.label) {
            m_pRenderTarget->DrawText(stats[i].title.c_str(), (UINT32)stats[i].title.length(), m_fonts.label,
                D2D1::RectF(cx + 14.0f, curY + 12.0f, cx + colW - 10.0f, curY + 30.0f), m_bTextMuted);
        }
        if (m_fonts.subhead) {
            m_pRenderTarget->DrawText(stats[i].val.c_str(), (UINT32)stats[i].val.length(), m_fonts.subhead,
                D2D1::RectF(cx + 14.0f, curY + 36.0f, cx + colW - 10.0f, curY + 66.0f), stats[i].bVal);
        }
    }

    curY += 95.0f;

    // Hashes Card
    D2D1_RECT_F rcHashes = D2D1::RectF(x + pad, curY, x + w - pad, curY + 75.0f);
    KaisekiTheme::DrawCard(m_pRenderTarget, rcHashes, m_bCard, m_bBorderSubtle, 6.0f);

    if (m_fonts.label) {
        m_pRenderTarget->DrawText(L"GÜVENLİK VE SAĞLAMA ÖZETLERİ (HASH)", 36, m_fonts.label,
            D2D1::RectF(rcHashes.left + 14.0f, curY + 10.0f, rcHashes.right, curY + 28.0f), m_bTextMuted);
    }
    if (m_fonts.code) {
        std::wstring sha = L"SHA-256: " + KaisekiTheme::Utf8ToWide(m_pModel->sha256);
        std::wstring md5 = L"MD5:     " + KaisekiTheme::Utf8ToWide(m_pModel->md5);
        m_pRenderTarget->DrawText(sha.c_str(), (UINT32)sha.length(), m_fonts.code,
            D2D1::RectF(rcHashes.left + 14.0f, curY + 32.0f, rcHashes.right, curY + 50.0f), m_bTextPrimary);
        m_pRenderTarget->DrawText(md5.c_str(), (UINT32)md5.length(), m_fonts.code,
            D2D1::RectF(rcHashes.left + 14.0f, curY + 50.0f, rcHashes.right, curY + 68.0f), m_bTextSecondary);
    }

    curY += 90.0f;

    // KaisekiSec Security & Vulnerability Summary Card
    float secCardH = 88.0f;
    D2D1_RECT_F rcSec = D2D1::RectF(x + pad, curY, x + w - pad, curY + secCardH);
    ID2D1SolidColorBrush* bSecBorder = (m_secReport.riskScore >= 80) ? m_bSuccess : ((m_secReport.riskScore >= 60) ? m_bWarning : m_bCloseRed);
    KaisekiTheme::DrawCard(m_pRenderTarget, rcSec, m_bCard, bSecBorder, 6.0f, 1.2f);

    if (m_fonts.label) {
        m_pRenderTarget->DrawText(L"KAISEKI GÜVENLİK VE ZAFİYET DENETİMİ (KaisekiSec Engine)", 56, m_fonts.label,
            D2D1::RectF(rcSec.left + 14.0f, curY + 8.0f, rcSec.right, curY + 26.0f), m_bTextMuted);
    }

    // Security Score Badge
    std::wstring scoreStr = L"Güvenlik Skoru: " + std::to_wstring(m_secReport.riskScore) + L"/100 [" + KaisekiTheme::Utf8ToWide(m_secReport.riskGrade) + L"]";
    if (m_fonts.subhead) {
        m_pRenderTarget->DrawText(scoreStr.c_str(), (UINT32)scoreStr.length(), m_fonts.subhead,
            D2D1::RectF(rcSec.left + 14.0f, curY + 28.0f, rcSec.left + 350.0f, curY + 50.0f), bSecBorder);
    }

    std::wstring breakdown = L"Kritik: " + std::to_wstring(m_secReport.criticalCount) +
                             L"  |  Yüksek: " + std::to_wstring(m_secReport.highCount) +
                             L"  |  Orta: " + std::to_wstring(m_secReport.mediumCount) +
                             L"  |  Düşük: " + std::to_wstring(m_secReport.lowCount);
    if (m_fonts.smallText) {
        m_pRenderTarget->DrawText(breakdown.c_str(), (UINT32)breakdown.length(), m_fonts.smallText,
            D2D1::RectF(rcSec.left + 14.0f, curY + 50.0f, rcSec.left + 350.0f, curY + 68.0f), m_bTextSecondary);

        std::wstring encInfo = L"Şifreleme & Yükleyici: " + KaisekiTheme::Utf8ToWide(m_secReport.packer.encryptionAlg);
        m_pRenderTarget->DrawText(encInfo.c_str(), (UINT32)encInfo.length(), m_fonts.smallText,
            D2D1::RectF(rcSec.left + 14.0f, curY + 68.0f, rcSec.left + 450.0f, curY + 86.0f), m_bAccent);
    }

    // Action Buttons on right side of Security Card
    float btnW = 185.0f;
    float btnH = 34.0f;
    float btnY = curY + 26.0f;
    m_rcBtnViewFridaBypass = D2D1::RectF(rcSec.right - btnW - 14.0f, btnY, rcSec.right - 14.0f, btnY + btnH);
    m_rcBtnViewSecReport = D2D1::RectF(m_rcBtnViewFridaBypass.left - btnW - 12.0f, btnY, m_rcBtnViewFridaBypass.left - 12.0f, btnY + btnH);

    KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBtnViewSecReport, m_hoverBtnViewSecReport ? m_bCardHover : m_bCard, m_hoverBtnViewSecReport ? m_bAccent : m_bBorderSubtle, 4.0f, m_hoverBtnViewSecReport ? 1.4f : 1.0f);
    KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBtnViewFridaBypass, m_hoverBtnViewFridaBypass ? m_bCardHover : m_bCard, m_bAccent, 4.0f, m_hoverBtnViewFridaBypass ? 1.6f : 1.0f);

    if (m_fonts.smallText) {
        m_pRenderTarget->DrawText(L"Güvenlik Raporu Aç", 19, m_fonts.smallText,
            D2D1::RectF(m_rcBtnViewSecReport.left + 12.0f, btnY + 8.0f, m_rcBtnViewSecReport.right, btnY + btnH),
            m_hoverBtnViewSecReport ? m_bAccent : m_bTextPrimary);
        m_pRenderTarget->DrawText(L"Evrensel Frida Bypass", 22, m_fonts.smallText,
            D2D1::RectF(m_rcBtnViewFridaBypass.left + 12.0f, btnY + 8.0f, m_rcBtnViewFridaBypass.right, btnY + btnH), m_bAccent);
    }

    curY += secCardH + 14.0f;

    // Declared Permissions Badges (Clean, Professional Typography & Vector Dots - Zero Emojis)
    D2D1_RECT_F rcPerms = D2D1::RectF(x + pad, curY, x + w - pad, y + h - pad);
    KaisekiTheme::DrawCard(m_pRenderTarget, rcPerms, m_bCard, m_bBorderSubtle, 6.0f);

    if (m_fonts.label) {
        m_pRenderTarget->DrawText(L"TANIMLANMIŞ İZİNLER (USES-PERMISSION)", 37, m_fonts.label,
            D2D1::RectF(rcPerms.left + 14.0f, curY + 10.0f, rcPerms.right, curY + 28.0f), m_bTextMuted);
    }

    float badgeX = rcPerms.left + 14.0f;
    float badgeY = curY + 36.0f;

    for (const auto& perm : m_pModel->permissions) {
        std::string pUpper = perm;
        for (char& c : pUpper) c = (char)toupper((unsigned char)c);

        ID2D1SolidColorBrush* bIconColor = m_bAccentLight;

        if (pUpper.find("INTERNET") != std::string::npos || pUpper.find("WIFI") != std::string::npos || pUpper.find("NETWORK") != std::string::npos) {
            bIconColor = m_bBtnAnalyze;
        } else if (pUpper.find("CAMERA") != std::string::npos) {
            bIconColor = m_bWarning;
        } else if (pUpper.find("LOCATION") != std::string::npos || pUpper.find("GPS") != std::string::npos) {
            bIconColor = m_bSuccess;
        } else if (pUpper.find("STORAGE") != std::string::npos || pUpper.find("MEDIA") != std::string::npos || pUpper.find("WRITE_EXTERNAL") != std::string::npos) {
            bIconColor = m_bAccent;
        } else if (pUpper.find("AUDIO") != std::string::npos || pUpper.find("RECORD") != std::string::npos || pUpper.find("MICROPHONE") != std::string::npos) {
            bIconColor = m_bAccentLight;
        } else if (pUpper.find("BLUETOOTH") != std::string::npos) {
            bIconColor = m_bBtnAnalyze;
        } else if (pUpper.find("PHONE") != std::string::npos || pUpper.find("CALL") != std::string::npos || pUpper.find("SMS") != std::string::npos || pUpper.find("CONTACTS") != std::string::npos) {
            bIconColor = m_bWarning;
        } else if (pUpper.find("NOTIF") != std::string::npos || pUpper.find("VIBRATE") != std::string::npos) {
            bIconColor = m_bAccentLight;
        } else if (pUpper.find("BIOMETRIC") != std::string::npos || pUpper.find("FINGERPRINT") != std::string::npos) {
            bIconColor = m_bSuccess;
        } else if (pUpper.find("BOOT") != std::string::npos || pUpper.find("WAKE_LOCK") != std::string::npos || pUpper.find("FOREGROUND") != std::string::npos) {
            bIconColor = m_bTextSecondary;
        }

        std::wstring pW = KaisekiTheme::Utf8ToWide(perm);
        float pWLen = (float)pW.length() * 7.5f + 32.0f;
        if (badgeX + pWLen > rcPerms.right - 14.0f) {
            badgeX = rcPerms.left + 14.0f;
            badgeY += 28.0f;
        }
        if (badgeY + 24.0f > rcPerms.bottom - 10.0f) break;

        D2D1_RECT_F rcBadge = D2D1::RectF(badgeX, badgeY, badgeX + pWLen, badgeY + 22.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, rcBadge, m_bBadgeBg, m_bBorderSubtle, 4.0f);

        // Professional geometric vector permission icon (Zero emojis, Direct2D native)
        float icx = badgeX + 11.5f;
        float icy = badgeY + 11.0f;
        if (pUpper.find("INTERNET") != std::string::npos || pUpper.find("WIFI") != std::string::npos || pUpper.find("NETWORK") != std::string::npos) {
            VectorIcons::DrawIconGlobe(m_pRenderTarget, icx, icy, 4.8f, bIconColor);
        } else if (pUpper.find("CAMERA") != std::string::npos) {
            VectorIcons::DrawIconCamera(m_pRenderTarget, icx, icy, 9.5f, bIconColor);
        } else if (pUpper.find("LOCATION") != std::string::npos || pUpper.find("GPS") != std::string::npos) {
            VectorIcons::DrawIconLocation(m_pRenderTarget, icx, icy, 9.5f, bIconColor);
        } else if (pUpper.find("STORAGE") != std::string::npos || pUpper.find("MEDIA") != std::string::npos || pUpper.find("WRITE_EXTERNAL") != std::string::npos) {
            VectorIcons::DrawIconStorage(m_pRenderTarget, icx, icy, 9.5f, bIconColor);
        } else if (pUpper.find("AUDIO") != std::string::npos || pUpper.find("RECORD") != std::string::npos || pUpper.find("MICROPHONE") != std::string::npos) {
            VectorIcons::DrawIconMicrophone(m_pRenderTarget, icx, icy, 9.5f, bIconColor);
        } else if (pUpper.find("BLUETOOTH") != std::string::npos) {
            VectorIcons::DrawIconBluetooth(m_pRenderTarget, icx, icy, 9.5f, bIconColor);
        } else if (pUpper.find("PHONE") != std::string::npos || pUpper.find("CALL") != std::string::npos || pUpper.find("SMS") != std::string::npos || pUpper.find("CONTACTS") != std::string::npos) {
            VectorIcons::DrawIconPhone(m_pRenderTarget, icx, icy, 9.5f, bIconColor);
        } else if (pUpper.find("BIOMETRIC") != std::string::npos || pUpper.find("FINGERPRINT") != std::string::npos) {
            VectorIcons::DrawIconFingerprint(m_pRenderTarget, icx, icy, 9.5f, bIconColor);
        } else if (pUpper.find("BOOT") != std::string::npos || pUpper.find("WAKE_LOCK") != std::string::npos || pUpper.find("FOREGROUND") != std::string::npos) {
            VectorIcons::DrawIconHardware(m_pRenderTarget, icx, icy, 9.5f, bIconColor);
        } else {
            VectorIcons::DrawIconShield(m_pRenderTarget, icx, icy, 9.5f, bIconColor);
        }

        if (m_fonts.smallText) {
            m_pRenderTarget->DrawText(pW.c_str(), (UINT32)pW.length(), m_fonts.smallText,
                D2D1::RectF(badgeX + 22.0f, badgeY + 3.0f, rcBadge.right - 4.0f, rcBadge.bottom), m_bTextPrimary);
        }
        badgeX += pWLen + 8.0f;
    }
}

void UIWindow::RenderSignerView(float x, float y, float w, float h) {
    float pad = 24.0f;
    float curY = y + pad;

    if (m_fonts.hero) {
        const wchar_t* titleStr = LocW(StringId::Signer_Title);
        m_pRenderTarget->DrawText(titleStr, (UINT32)wcslen(titleStr), m_fonts.hero,
            D2D1::RectF(x + pad, curY, x + w - pad, curY + 40.0f), m_bTextPrimary);
    }
    curY += 50.0f;

    float formW = KaisekiTheme::MinF(580.0f, w - pad * 2.0f);
    D2D1_RECT_F rcForm = D2D1::RectF(x + pad, curY, x + pad + formW, curY + 340.0f);
    KaisekiTheme::DrawCard(m_pRenderTarget, rcForm, m_bCard, m_bBorderSubtle, 8.0f);

    float fy = curY + 16.0f;

    if (m_fonts.label) m_pRenderTarget->DrawText(L"Hedef APK Dosyası:", 19, m_fonts.label, D2D1::RectF(rcForm.left + 18.0f, fy, rcForm.right - 18.0f, fy + 20.0f), m_bTextMuted);
    fy += 22.0f;
    D2D1_RECT_F rcInpApk = D2D1::RectF(rcForm.left + 18.0f, fy, rcForm.right - 18.0f, fy + 32.0f);
    KaisekiTheme::DrawCard(m_pRenderTarget, rcInpApk, m_bWell, m_bBorderSubtle, 4.0f);
    if (m_fonts.body) {
        std::wstring apkName = m_pModel ? m_pModel->fileName : L"Henüz bir APK seçilmedi";
        m_pRenderTarget->DrawText(apkName.c_str(), (UINT32)apkName.length(), m_fonts.body,
            D2D1::RectF(rcInpApk.left + 10.0f, fy + 6.0f, rcInpApk.right, rcInpApk.bottom), m_pModel ? m_bTextPrimary : m_bTextMuted);
    }
    fy += 44.0f;

    if (m_fonts.label) m_pRenderTarget->DrawText(L"Anahtar Deposu (Keystore / JKS):", 33, m_fonts.label, D2D1::RectF(rcForm.left + 18.0f, fy, rcForm.right - 18.0f, fy + 20.0f), m_bTextMuted);
    fy += 22.0f;
    D2D1_RECT_F rcInpKs = D2D1::RectF(rcForm.left + 18.0f, fy, rcForm.right - 18.0f, fy + 32.0f);
    KaisekiTheme::DrawCard(m_pRenderTarget, rcInpKs, m_bWell, m_bBorderSubtle, 4.0f);
    if (m_fonts.body) {
        m_pRenderTarget->DrawText(m_signerKeystorePath.c_str(), (UINT32)m_signerKeystorePath.length(), m_fonts.body,
            D2D1::RectF(rcInpKs.left + 10.0f, fy + 6.0f, rcInpKs.right, rcInpKs.bottom), m_bTextPrimary);
    }
    fy += 44.0f;

    if (m_fonts.label) m_pRenderTarget->DrawText(L"Anahtar Takma Adı (Key Alias):", 31, m_fonts.label, D2D1::RectF(rcForm.left + 18.0f, fy, rcForm.right - 18.0f, fy + 20.0f), m_bTextMuted);
    fy += 22.0f;
    D2D1_RECT_F rcInpAlias = D2D1::RectF(rcForm.left + 18.0f, fy, rcForm.right - 18.0f, fy + 32.0f);
    KaisekiTheme::DrawCard(m_pRenderTarget, rcInpAlias, m_bWell, m_bBorderSubtle, 4.0f);
    if (m_fonts.body) {
        m_pRenderTarget->DrawText(m_signerAlias.c_str(), (UINT32)m_signerAlias.length(), m_fonts.body,
            D2D1::RectF(rcInpAlias.left + 10.0f, fy + 6.0f, rcInpAlias.right, rcInpAlias.bottom), m_bTextPrimary);
    }
    fy += 44.0f;

    if (m_fonts.label) {
        std::wstring schemeStr = L"İmza Şemaları:  [✔] v1 (JAR)   [✔] v2 (Full APK)   [✔] v3 (Android 9+)";
        m_pRenderTarget->DrawText(schemeStr.c_str(), (UINT32)schemeStr.length(), m_fonts.label,
            D2D1::RectF(rcForm.left + 18.0f, fy, rcForm.right - 18.0f, fy + 26.0f), m_bSuccess);
    }
    fy += 36.0f;

    m_rcBtnSignApk = D2D1::RectF(rcForm.left + 18.0f, fy, rcForm.left + 180.0f, fy + 40.0f);
    KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBtnSignApk, m_bBtnCompile, nullptr, 6.0f);
    if (m_fonts.button) {
        m_pRenderTarget->DrawText(L"✍️  APK'yı İmzala", 18, m_fonts.button,
            D2D1::RectF(m_rcBtnSignApk.left, fy + 10.0f, m_rcBtnSignApk.right, fy + 35.0f), m_bTextPrimary);
    }

    m_rcBtnVerifySign = D2D1::RectF(rcForm.left + 195.0f, fy, rcForm.left + 360.0f, fy + 40.0f);
    KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBtnVerifySign, m_bCardHover, m_bAccent, 6.0f);
    if (m_fonts.button) {
        m_pRenderTarget->DrawText(L"🔍  İmzayı Doğrula", 19, m_fonts.button,
            D2D1::RectF(m_rcBtnVerifySign.left, fy + 10.0f, m_rcBtnVerifySign.right, fy + 35.0f), m_bTextPrimary);
    }
}

void UIWindow::RenderAdbPhoneView(float x, float y, float w, float h) {
    float pad = 24.0f;
    float curY = y + pad;

    if (m_fonts.hero) {
        const wchar_t* adbTitle = LocW(StringId::Adb_Title);
        m_pRenderTarget->DrawText(adbTitle, (UINT32)wcslen(adbTitle), m_fonts.hero,
            D2D1::RectF(x + pad, curY, x + w - pad, curY + 40.0f), m_bTextPrimary);
    }
    curY += 50.0f;

    float formW = KaisekiTheme::MinF(680.0f, w - pad * 2.0f);

    // Refresh Devices Bar
    m_rcBtnRefreshDevices = D2D1::RectF(x + pad + formW - 160.0f, curY, x + pad + formW, curY + 32.0f);
    KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBtnRefreshDevices, m_bCardHover, m_bAccent, 4.0f);
    if (m_fonts.button) {
        const wchar_t* refStr = L"🔄 Cihazları Yenile";
        m_pRenderTarget->DrawText(refStr, (UINT32)wcslen(refStr), m_fonts.button,
            D2D1::RectF(m_rcBtnRefreshDevices.left + 8.0f, curY + 6.0f, m_rcBtnRefreshDevices.right, curY + 30.0f), m_bTextPrimary);
    }

    if (m_fonts.subhead) {
        const wchar_t* subStr = L"Bağlı Android Cihazları & Emülatörler:";
        m_pRenderTarget->DrawText(subStr, (UINT32)wcslen(subStr), m_fonts.subhead,
            D2D1::RectF(x + pad, curY + 4.0f, m_rcBtnRefreshDevices.left - 10.0f, curY + 32.0f), m_bTextPrimary);
    }
    curY += 42.0f;

    m_rcDeviceCards.clear();

    if (m_adbDeviceList.empty()) {
        D2D1_RECT_F rcEmpty = D2D1::RectF(x + pad, curY, x + pad + formW, curY + 140.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, rcEmpty, m_bCard, m_bWarning, 8.0f);

        if (m_fonts.subhead) {
            const wchar_t* warnStr = L"⚠️ Bilgisayara bağlı Android cihaz veya emülatör algılanmadı!";
            m_pRenderTarget->DrawText(warnStr, (UINT32)wcslen(warnStr), m_fonts.subhead,
                D2D1::RectF(rcEmpty.left + 20.0f, curY + 16.0f, rcEmpty.right - 20.0f, curY + 40.0f), m_bWarning);
        }
        if (m_fonts.body) {
            std::wstring help = L"• Telefonunuzda Ayarlar -> Geliştirici Seçenekleri -> 'USB Hata Ayıklama' modunu açın.\n"
                                L"• Telefonu USB ile PC'ye takın ve ekrandaki 'Bu bilgisayara her zaman izin ver' uyarısını onaylayın.\n"
                                L"• Emülatör kullanıyorsanız (Android Studio / LDPlayer), başlatıp yukarıdaki [Cihazları Yenile]'ye basın.";
            m_pRenderTarget->DrawText(help.c_str(), (UINT32)help.length(), m_fonts.body,
                D2D1::RectF(rcEmpty.left + 20.0f, curY + 46.0f, rcEmpty.right - 20.0f, curY + 130.0f), m_bTextSecondary);
        }
        curY += 155.0f;
    } else {
        float cardH = 58.0f;
        for (size_t i = 0; i < m_adbDeviceList.size(); ++i) {
            const auto& dev = m_adbDeviceList[i];
            D2D1_RECT_F rcCard = D2D1::RectF(x + pad, curY, x + pad + formW, curY + cardH);
            m_rcDeviceCards.push_back(rcCard);

            bool isSelected = ((int)i == m_selectedDeviceIdx);
            bool isHover = (m_hoverDeviceCard == (int)i);

            KaisekiTheme::DrawCard(m_pRenderTarget, rcCard, isSelected ? m_bCardHover : (isHover ? m_bCardHover : m_bCard),
                isSelected ? m_bAccent : m_bBorderSubtle, 6.0f, isSelected ? 2.0f : 1.0f);

            VectorIcons::DrawRailPhone(m_pRenderTarget, rcCard.left + 24.0f, curY + 29.0f, 22.0f, dev.isOnline ? m_bSuccess : m_bWarning);

            std::wstring devTitle = KaisekiTheme::Utf8ToWide(dev.model.empty() ? dev.serial : dev.model);
            if (isSelected) devTitle = L"✓ " + devTitle;

            std::wstring devSub = L"Seri No: " + KaisekiTheme::Utf8ToWide(dev.serial) + L"  •  Durum: " +
                (dev.isOnline ? L"[ONLINE - Hazır]" : L"[OFFLINE / Yetki Gerekli]");

            if (m_fonts.body) {
                m_pRenderTarget->DrawText(devTitle.c_str(), (UINT32)devTitle.length(), m_fonts.body,
                    D2D1::RectF(rcCard.left + 48.0f, curY + 8.0f, rcCard.right - 100.0f, curY + 30.0f),
                    isSelected ? m_bAccent : m_bTextPrimary);
            }
            if (m_fonts.smallText) {
                m_pRenderTarget->DrawText(devSub.c_str(), (UINT32)devSub.length(), m_fonts.smallText,
                    D2D1::RectF(rcCard.left + 48.0f, curY + 32.0f, rcCard.right - 10.0f, curY + 52.0f),
                    dev.isOnline ? m_bSuccess : m_bWarning);
            }

            curY += cardH + 10.0f;
        }
        curY += 10.0f;
    }

    // Action Card
    D2D1_RECT_F rcAction = D2D1::RectF(x + pad, curY, x + pad + formW, curY + 115.0f);
    KaisekiTheme::DrawCard(m_pRenderTarget, rcAction, m_bCard, m_bBorderSubtle, 8.0f);

    float ay = curY + 16.0f;
    if (m_fonts.label) {
        std::wstring optDesc = L"Dağıtım Modu: adb install -r -d (Mevcut verileri koru & Yeniden yükle)";
        m_pRenderTarget->DrawText(optDesc.c_str(), (UINT32)optDesc.length(), m_fonts.label,
            D2D1::RectF(rcAction.left + 20.0f, ay, rcAction.right - 20.0f, ay + 22.0f), m_bTextMuted);
    }
    ay += 30.0f;

    m_rcBtnAdbInstall = D2D1::RectF(rcAction.left + 20.0f, ay, rcAction.left + 240.0f, ay + 42.0f);
    KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBtnAdbInstall, m_bSuccess, nullptr, 6.0f);
    if (m_fonts.button) {
        const wchar_t* instStr = L"📱  Seçili Telefona Yükle";
        m_pRenderTarget->DrawText(instStr, (UINT32)wcslen(instStr), m_fonts.button,
            D2D1::RectF(m_rcBtnAdbInstall.left, ay + 10.0f, m_rcBtnAdbInstall.right, ay + 36.0f), m_bTextPrimary);
    }

    m_rcBtnAdbLaunch = D2D1::RectF(rcAction.left + 255.0f, ay, rcAction.left + 450.0f, ay + 42.0f);
    KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBtnAdbLaunch, m_bCardHover, m_bAccent, 6.0f);
    if (m_fonts.button) {
        const wchar_t* openStr = L"▶  Uygulamayı Aç";
        m_pRenderTarget->DrawText(openStr, (UINT32)wcslen(openStr), m_fonts.button,
            D2D1::RectF(m_rcBtnAdbLaunch.left, ay + 10.0f, m_rcBtnAdbLaunch.right, ay + 36.0f), m_bTextPrimary);
    }
}

void UIWindow::RenderSettingsView(float x, float y, float w, float h) {
    float pad = 24.0f;
    float curY = y + pad;

    if (m_fonts.hero) {
        const wchar_t* settTitle = LocW(StringId::Settings_Title);
        m_pRenderTarget->DrawText(settTitle, (UINT32)wcslen(settTitle), m_fonts.hero,
            D2D1::RectF(x + pad, curY, x + w - pad, curY + 40.0f), m_bTextPrimary);
    }
    curY += 45.0f;

    // Sub-Tabs Header Bar (Dil, Tema, Genel, Araçlar, Dekompilasyon, Kısayollar)
    const wchar_t* subTabs[6] = {
        LocW(StringId::Settings_TabLanguage),
        LocW(StringId::Settings_TabTheme),
        LocW(StringId::Settings_TabGeneral),
        LocW(StringId::Settings_TabTools),
        LocW(StringId::Settings_TabDecompile),
        LocW(StringId::Settings_TabHotkeys)
    };
    float tabStartX = x + pad;
    float tabW = 125.0f;
    float tabH = 34.0f;

    for (int i = 0; i < 6; ++i) {
        m_rcSettingsTabs[i] = D2D1::RectF(tabStartX + i * (tabW + 6.0f), curY, tabStartX + (i + 1) * tabW + i * 6.0f, curY + tabH);
        bool isAct = (m_settingsTab == i);

        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcSettingsTabs[i], isAct ? m_bCardHover : m_bCard, isAct ? m_bAccent : m_bBorderSubtle, 6.0f);

        if (m_fonts.label) {
            m_pRenderTarget->DrawText(subTabs[i], (UINT32)wcslen(subTabs[i]), m_fonts.label,
                D2D1::RectF(m_rcSettingsTabs[i].left + 8.0f, curY + 8.0f, m_rcSettingsTabs[i].right, curY + tabH),
                isAct ? m_bAccent : m_bTextSecondary);
        }
    }
    curY += 46.0f;

    float cardW = KaisekiTheme::MinF(780.0f, w - pad * 2.0f);

    // SUB-TAB 0: DİL / LANGUAGE
    if (m_settingsTab == 0) {
        float modalContentW = cardW - 36.0f;
        m_rcLangCards.clear();
        const auto& langs = KaisekiLang::Get().GetAvailableLanguages();
        std::string currentLangCode = KaisekiLang::Get().GetLanguageCode();
        size_t totalRows = (langs.size() + 1) / 2;
        float cardH = 76.0f;
        float cardSpacingY = 12.0f;
        float totalCardBlockH = (float)totalRows * (cardH + cardSpacingY);
        float dynCardH = 100.0f + totalCardBlockH + 50.0f + 45.0f;

        D2D1_RECT_F rcLangCard = D2D1::RectF(x + pad, curY, x + pad + cardW, curY + dynCardH);
        KaisekiTheme::DrawCard(m_pRenderTarget, rcLangCard, m_bCard, m_bBorderSubtle, 8.0f);

        float ly = curY + 18.0f;
        if (m_fonts.subhead) {
            const wchar_t* lTitle = LocW(StringId::Settings_LangTitle);
            m_pRenderTarget->DrawText(lTitle, (UINT32)wcslen(lTitle), m_fonts.subhead,
                D2D1::RectF(rcLangCard.left + 18.0f, ly, rcLangCard.right, ly + 24.0f), m_bTextPrimary);
        }
        ly += 26.0f;

        if (m_fonts.label) {
            const wchar_t* lDesc = LocW(StringId::Settings_LangDesc);
            m_pRenderTarget->DrawText(lDesc, (UINT32)wcslen(lDesc), m_fonts.label,
                D2D1::RectF(rcLangCard.left + 18.0f, ly, rcLangCard.right - 18.0f, ly + 40.0f), m_bTextSecondary);
        }
        ly += 45.0f;

        float btnW = (modalContentW - 16.0f) * 0.5f;

        for (size_t i = 0; i < langs.size(); ++i) {
            float col = (float)(i % 2);
            float row = (float)(i / 2);
            float bx = rcLangCard.left + 18.0f + col * (btnW + 16.0f);
            float by = ly + row * (cardH + cardSpacingY);
            D2D1_RECT_F rcBtn = D2D1::RectF(bx, by, bx + btnW, by + cardH);
            m_rcLangCards.push_back(rcBtn);

            bool isCur = (langs[i].code == currentLangCode);
            KaisekiTheme::DrawCard(m_pRenderTarget, rcBtn, isCur ? m_bWell : m_bCardHover, isCur ? m_bAccent : m_bBorderSubtle, 8.0f, isCur ? 2.0f : 1.0f);

            if (m_fonts.title) {
                m_pRenderTarget->DrawText(langs[i].displayName.c_str(), (UINT32)langs[i].displayName.length(), m_fonts.title,
                    D2D1::RectF(rcBtn.left + 16.0f, by + 12.0f, rcBtn.right - 10.0f, by + 38.0f),
                    isCur ? m_bAccent : m_bTextPrimary);
            }
            if (m_fonts.label) {
                std::wstring subText;
                if (isCur) {
                    subText = L"✓ " + std::wstring(LocW(StringId::Settings_LangSelected));
                } else {
                    subText = std::wstring(LocW(StringId::Settings_LangSelectBtn));
                    if (!langs[i].author.empty()) {
                        subText += L" • " + langs[i].author;
                    }
                }
                m_pRenderTarget->DrawText(subText.c_str(), (UINT32)subText.length(), m_fonts.label,
                    D2D1::RectF(rcBtn.left + 16.0f, by + 42.0f, rcBtn.right - 10.0f, by + 68.0f),
                    isCur ? m_bSuccess : m_bTextMuted);
            }
        }

        ly += totalCardBlockH + 14.0f;

        // Action buttons
        float actW = 210.0f;
        float actH = 34.0f;
        m_rcBtnOpenLangFolder = D2D1::RectF(rcLangCard.left + 18.0f, ly, rcLangCard.left + 18.0f + actW, ly + actH);
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBtnOpenLangFolder, m_bWell, m_bBorderSubtle, 6.0f);
        if (m_fonts.label) {
            const wchar_t* btnOpenTxt = LocW(StringId::Settings_OpenLangFolder);
            m_pRenderTarget->DrawText(btnOpenTxt, (UINT32)wcslen(btnOpenTxt), m_fonts.label,
                D2D1::RectF(m_rcBtnOpenLangFolder.left + 10.0f, ly + 8.0f, m_rcBtnOpenLangFolder.right - 10.0f, ly + actH),
                m_bTextPrimary);
        }

        m_rcBtnRefreshLangs = D2D1::RectF(rcLangCard.left + 18.0f + actW + 12.0f, ly, rcLangCard.left + 18.0f + actW * 2.0f + 12.0f, ly + actH);
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBtnRefreshLangs, m_bWell, m_bBorderSubtle, 6.0f);
        if (m_fonts.label) {
            const wchar_t* btnRefTxt = LocW(StringId::Settings_RefreshLangs);
            m_pRenderTarget->DrawText(btnRefTxt, (UINT32)wcslen(btnRefTxt), m_fonts.label,
                D2D1::RectF(m_rcBtnRefreshLangs.left + 10.0f, ly + 8.0f, m_rcBtnRefreshLangs.right - 10.0f, ly + actH),
                m_bTextPrimary);
        }

        ly += actH + 14.0f;

        // Note hint
        D2D1_RECT_F rcLangNote = D2D1::RectF(rcLangCard.left + 18.0f, ly, rcLangCard.right - 18.0f, ly + 36.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, rcLangNote, m_bWell, m_bBorderSubtle, 6.0f);
        if (m_fonts.smallText) {
            const wchar_t* note = LocW(StringId::Settings_LangFolderHint);
            m_pRenderTarget->DrawText(note, (UINT32)wcslen(note), m_fonts.smallText,
                D2D1::RectF(rcLangNote.left + 10.0f, ly + 8.0f, rcLangNote.right, rcLangNote.bottom), m_bTextSecondary);
        }
    }
    // SUB-TAB 1: TEMA & RENKLER
    else if (m_settingsTab == 1) {
        // Color Swatches Card
        D2D1_RECT_F rcPalette = D2D1::RectF(x + pad, curY, x + pad + cardW, curY + 230.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, rcPalette, m_bCard, m_bBorderSubtle, 8.0f);

        if (m_fonts.subhead) {
            m_pRenderTarget->DrawText(L"1. Özel Renk Paleti (Varsayılan: Sakura Pastel Pembe):", 53, m_fonts.subhead,
                D2D1::RectF(rcPalette.left + 18.0f, curY + 12.0f, rcPalette.right, curY + 36.0f), m_bTextPrimary);
        }

        m_rcSwatches.clear();
        const auto& swatches = KaisekiTheme::GetPresetSwatches();

        float swW = 105.0f;
        float swH = 65.0f;
        float swStartX = rcPalette.left + 18.0f;
        float swStartY = curY + 45.0f;

        for (size_t i = 0; i < swatches.size(); ++i) {
            int row = (int)i / 6;
            int col = (int)i % 6;
            float sx = swStartX + col * (swW + 12.0f);
            float sy = swStartY + row * (swH + 12.0f);

            D2D1_RECT_F rcSw = D2D1::RectF(sx, sy, sx + swW, sy + swH);
            m_rcSwatches.push_back(rcSw);

            ID2D1SolidColorBrush* bSw = nullptr;
            m_pRenderTarget->CreateSolidColorBrush(swatches[i].color, &bSw);
            if (bSw) {
                KaisekiTheme::DrawCard(m_pRenderTarget, rcSw, m_bWell, m_bBorderSubtle, 6.0f);
                m_pRenderTarget->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(sx + 8.0f, sy + 8.0f, sx + swW - 8.0f, sy + 32.0f), 4.0f, 4.0f), bSw);
                bSw->Release();
            }

            if (m_fonts.smallText) {
                m_pRenderTarget->DrawText(swatches[i].name, (UINT32)wcslen(swatches[i].name), m_fonts.smallText,
                    D2D1::RectF(sx + 6.0f, sy + 36.0f, sx + swW, sy + 50.0f), m_bTextPrimary);
                m_pRenderTarget->DrawText(swatches[i].hexStr, (UINT32)wcslen(swatches[i].hexStr), m_fonts.smallText,
                    D2D1::RectF(sx + 6.0f, sy + 50.0f, sx + swW, sy + 64.0f), m_bTextMuted);
            }
        }

        curY += 245.0f;

        // In-App RGB Sliders Card
        D2D1_RECT_F rcSliders = D2D1::RectF(x + pad, curY, x + pad + cardW, curY + 170.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, rcSliders, m_bCard, m_bBorderSubtle, 8.0f);

        if (m_fonts.subhead) {
            m_pRenderTarget->DrawText(L"2. Dahili RGB Renk İnce Ayarı (Windows Renk Seçici Kullanılmadan):", 65, m_fonts.subhead,
                D2D1::RectF(rcSliders.left + 18.0f, curY + 12.0f, rcSliders.right, curY + 36.0f), m_bTextPrimary);
        }

        float sliderTrackW = 340.0f;
        float sy = curY + 45.0f;

        auto drawSlider = [&](const wchar_t* name, uint8_t val, D2D1_RECT_F& outRc, ID2D1SolidColorBrush* bTrack) {
            if (m_fonts.label) {
                std::wstring lStr = std::wstring(name) + L": " + std::to_wstring(val);
                m_pRenderTarget->DrawText(lStr.c_str(), (UINT32)lStr.length(), m_fonts.label,
                    D2D1::RectF(rcSliders.left + 18.0f, sy, rcSliders.left + 100.0f, sy + 20.0f), m_bTextSecondary);
            }
            outRc = D2D1::RectF(rcSliders.left + 110.0f, sy + 4.0f, rcSliders.left + 110.0f + sliderTrackW, sy + 14.0f);
            KaisekiTheme::DrawCard(m_pRenderTarget, outRc, m_bWell, m_bBorderSubtle, 4.0f);

            float thumbX = outRc.left + ((float)val / 255.0f) * sliderTrackW;
            m_pRenderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(thumbX, sy + 9.0f), 7.0f, 7.0f), bTrack);
            sy += 32.0f;
        };

        drawSlider(L"Kırmızı (R)", m_sliderR, m_rcSliderR, m_bAccent);
        drawSlider(L"Yeşil (G)", m_sliderG, m_rcSliderG, m_bSuccess);
        drawSlider(L"Mavi (B)", m_sliderB, m_rcSliderB, m_bBtnAnalyze);

        D2D1_RECT_F rcPreview = D2D1::RectF(rcSliders.left + 500.0f, curY + 45.0f, rcSliders.left + 680.0f, curY + 145.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, rcPreview, m_bAccent, m_bBorderHover, 8.0f, 2.0f);

        if (m_fonts.button) {
            std::wstring hexCode = KaisekiTheme::ColorToHexW(m_customAccentColor);
            m_pRenderTarget->DrawText(hexCode.c_str(), (UINT32)hexCode.length(), m_fonts.button,
                D2D1::RectF(rcPreview.left, curY + 80.0f, rcPreview.right, curY + 110.0f), m_bTextPrimary);
        }
    }
    // SUB-TAB 2: GENEL & ÇALIŞMA ALANI & PROJE
    else if (m_settingsTab == 2) {
        bool isTr = (KaisekiLang::Get().GetLanguage() == AppLanguage::Turkish);
        D2D1_RECT_F rcGen = D2D1::RectF(x + pad, curY, x + pad + cardW, curY + 280.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, rcGen, m_bCard, m_bBorderSubtle, 8.0f);

        float gy = curY + 18.0f;
        if (m_fonts.subhead) {
            const wchar_t* tmpLbl = LocW(StringId::Settings_TempWorkspaceTitle);
            m_pRenderTarget->DrawText(tmpLbl, (UINT32)wcslen(tmpLbl), m_fonts.subhead,
                D2D1::RectF(rcGen.left + 18.0f, gy, rcGen.right, gy + 24.0f), m_bTextPrimary);
        }
        gy += 30.0f;

        std::wstring tmpDir = SettingsManager::Get().GetDefaultTempDir();
        D2D1_RECT_F rcTmpDir = D2D1::RectF(rcGen.left + 18.0f, gy, rcGen.right - 18.0f, gy + 32.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, rcTmpDir, m_bWell, m_bBorderSubtle, 4.0f);
        if (m_fonts.body) {
            m_pRenderTarget->DrawText(tmpDir.c_str(), (UINT32)tmpDir.length(), m_fonts.body,
                D2D1::RectF(rcTmpDir.left + 10.0f, gy + 6.0f, rcTmpDir.right, rcTmpDir.bottom), m_bTextPrimary);
        }
        gy += 48.0f;

        if (m_fonts.subhead) {
            const wchar_t* cfgLbl = LocW(StringId::Settings_ConfigFileTitle);
            m_pRenderTarget->DrawText(cfgLbl, (UINT32)wcslen(cfgLbl), m_fonts.subhead,
                D2D1::RectF(rcGen.left + 18.0f, gy, rcGen.right, gy + 24.0f), m_bTextPrimary);
        }
        gy += 28.0f;

        std::wstring cfgPath = SettingsManager::Get().GetSettingsFilePath();
        if (m_fonts.code) {
            m_pRenderTarget->DrawText(cfgPath.c_str(), (UINT32)cfgPath.length(), m_fonts.code,
                D2D1::RectF(rcGen.left + 18.0f, gy, rcGen.right, gy + 22.0f), m_bAccent);
        }
        gy += 38.0f;

        if (m_fonts.subhead) {
            const wchar_t* assocLbl = LocW(StringId::Settings_FileAssocTitle);
            m_pRenderTarget->DrawText(assocLbl, (UINT32)wcslen(assocLbl), m_fonts.subhead,
                D2D1::RectF(rcGen.left + 18.0f, gy, rcGen.right, gy + 24.0f), m_bTextPrimary);
        }
        gy += 30.0f;

        m_rcBtnAssocKproj = D2D1::RectF(rcGen.left + 18.0f, gy, rcGen.left + 340.0f, gy + 38.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBtnAssocKproj, m_bCardHover, m_bAccent, 4.0f);
        if (m_fonts.button) {
            const wchar_t* btnRegLbl = LocW(StringId::Settings_BtnRegisterKproj);
            m_pRenderTarget->DrawText(btnRegLbl, (UINT32)wcslen(btnRegLbl), m_fonts.button,
                D2D1::RectF(m_rcBtnAssocKproj.left + 12.0f, gy + 8.0f, m_rcBtnAssocKproj.right, gy + 34.0f), m_bTextPrimary);
        }
    }
    // SUB-TAB 3: YEREL MOTOR & ARAÇLAR (KaisekiDex & ADB)
    else if (m_settingsTab == 3) {
        bool isTr = (KaisekiLang::Get().GetLanguage() == AppLanguage::Turkish);
        D2D1_RECT_F rcTools = D2D1::RectF(x + pad, curY, x + pad + cardW, curY + 300.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, rcTools, m_bCard, m_bBorderSubtle, 8.0f);

        float ty = curY + 18.0f;
        if (m_fonts.subhead) {
            const wchar_t* engTitle = LocW(StringId::Settings_EngineTitle);
            m_pRenderTarget->DrawText(engTitle, (UINT32)wcslen(engTitle), m_fonts.subhead,
                D2D1::RectF(rcTools.left + 18.0f, ty, rcTools.right, ty + 24.0f), m_bTextPrimary);
        }
        ty += 30.0f;

        const wchar_t* engDesc = LocW(StringId::Settings_EngineDesc);
        D2D1_RECT_F rcJadxBox = D2D1::RectF(rcTools.left + 18.0f, ty, rcTools.right - 18.0f, ty + 32.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, rcJadxBox, m_bWell, m_bBorderSubtle, 4.0f);
        if (m_fonts.code) {
            m_pRenderTarget->DrawText(engDesc, (UINT32)wcslen(engDesc), m_fonts.code,
                D2D1::RectF(rcJadxBox.left + 10.0f, ty + 6.0f, rcJadxBox.right, rcJadxBox.bottom), m_bSuccess);
        }
        ty += 46.0f;

        if (m_fonts.subhead) {
            const wchar_t* adbLbl = LocW(StringId::Settings_AdbTitle);
            m_pRenderTarget->DrawText(adbLbl, (UINT32)wcslen(adbLbl), m_fonts.subhead,
                D2D1::RectF(rcTools.left + 18.0f, ty, rcTools.right, ty + 24.0f), m_bTextPrimary);
        }
        ty += 30.0f;

        const wchar_t* adbDesc = LocW(StringId::Settings_AdbDesc);
        D2D1_RECT_F rcAdbBox = D2D1::RectF(rcTools.left + 18.0f, ty, rcTools.right - 18.0f, ty + 32.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, rcAdbBox, m_bWell, m_bBorderSubtle, 4.0f);
        if (m_fonts.code) {
            m_pRenderTarget->DrawText(adbDesc, (UINT32)wcslen(adbDesc), m_fonts.code,
                D2D1::RectF(rcAdbBox.left + 10.0f, ty + 6.0f, rcAdbBox.right, rcAdbBox.bottom), m_bSuccess);
        }
        ty += 48.0f;

        m_rcBtnExportGradleSettings = D2D1::RectF(rcTools.left + 18.0f, ty, rcTools.left + 330.0f, ty + 38.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBtnExportGradleSettings, m_bAccent, nullptr, 6.0f);
        if (m_fonts.button) {
            const wchar_t* btnGradLbl = LocW(StringId::Settings_BtnExportGradle);
            m_pRenderTarget->DrawText(btnGradLbl, (UINT32)wcslen(btnGradLbl), m_fonts.button,
                D2D1::RectF(m_rcBtnExportGradleSettings.left + 12.0f, ty + 8.0f, m_rcBtnExportGradleSettings.right, ty + 34.0f), m_bTextPrimary);
        }

        m_rcBtnToggleDeobf = D2D1::RectF(rcTools.left + 345.0f, ty, rcTools.left + 620.0f, ty + 38.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, m_rcBtnToggleDeobf, m_useDeobfuscation ? m_bSuccess : m_bCardHover, m_bBorderSubtle, 6.0f);
        if (m_fonts.button) {
            const wchar_t* deobfLbl = m_useDeobfuscation ? LocW(StringId::Settings_DeobfActive) : LocW(StringId::Settings_DeobfInactive);
            m_pRenderTarget->DrawText(deobfLbl, (UINT32)wcslen(deobfLbl), m_fonts.button,
                D2D1::RectF(m_rcBtnToggleDeobf.left + 12.0f, ty + 8.0f, m_rcBtnToggleDeobf.right, ty + 34.0f), m_bTextPrimary);
        }
    }
    // SUB-TAB 4: DEKOMPİLASYON
    else if (m_settingsTab == 4) {
        bool isTr = (KaisekiLang::Get().GetLanguage() == AppLanguage::Turkish);
        D2D1_RECT_F rcDecomp = D2D1::RectF(x + pad, curY, x + pad + cardW, curY + 240.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, rcDecomp, m_bCard, m_bBorderSubtle, 8.0f);

        float dy = curY + 18.0f;
        if (m_fonts.subhead) {
            const wchar_t* decLbl = LocW(StringId::Settings_DecompTitle);
            m_pRenderTarget->DrawText(decLbl, (UINT32)wcslen(decLbl), m_fonts.subhead,
                D2D1::RectF(rcDecomp.left + 18.0f, dy, rcDecomp.right, dy + 24.0f), m_bTextPrimary);
        }
        dy += 35.0f;

        if (m_fonts.body) {
            std::wstring d1 = isTr ? L"• Varsayılan Görünüm: Dalvik Baytkod (Smali) & Yüksek Seviyeli Java Dekompilasyonu"
                                   : L"• Default View: Dalvik Bytecode (Smali) & High-Level Java Decompilation";
            std::wstring d2 = isTr ? L"• Kod Editörü Yazı Tipi: Consolas 11.5pt (UTF-8 Mono)"
                                   : L"• Code Editor Font: Consolas 11.5pt (UTF-8 Mono)";
            std::wstring d3 = isTr ? L"• Register Formatı: Dalvik .registers (v0, v1, p0 param formatı etkin)"
                                   : L"• Register Format: Dalvik .registers (v0, v1, p0 parameter format active)";
            std::wstring d4 = isTr ? L"• AXML Çözücü: Kaiseki Binary XML Decoder v2.0 (Dahili Hızlı Kod Çözücü)"
                                   : L"• AXML Decoder: Kaiseki Binary XML Decoder v2.0 (Fast Native Decoder)";

            m_pRenderTarget->DrawText(d1.c_str(), (UINT32)d1.length(), m_fonts.body, D2D1::RectF(rcDecomp.left + 20.0f, dy, rcDecomp.right, dy + 22.0f), m_bTextSecondary); dy += 26.0f;
            m_pRenderTarget->DrawText(d2.c_str(), (UINT32)d2.length(), m_fonts.body, D2D1::RectF(rcDecomp.left + 20.0f, dy, rcDecomp.right, dy + 22.0f), m_bTextSecondary); dy += 26.0f;
            m_pRenderTarget->DrawText(d3.c_str(), (UINT32)d3.length(), m_fonts.body, D2D1::RectF(rcDecomp.left + 20.0f, dy, rcDecomp.right, dy + 22.0f), m_bTextSecondary); dy += 26.0f;
            m_pRenderTarget->DrawText(d4.c_str(), (UINT32)d4.length(), m_fonts.body, D2D1::RectF(rcDecomp.left + 20.0f, dy, rcDecomp.right, dy + 22.0f), m_bSuccess);
        }
    }
    // SUB-TAB 5: KISAYOLLAR / KEYBINDINGS
    else if (m_settingsTab == 5) {
        bool isEn = (KaisekiLang::Get().GetLanguage() == AppLanguage::English);
        D2D1_RECT_F rcHotkeys = D2D1::RectF(x + pad, curY, x + pad + cardW, curY + 360.0f);
        KaisekiTheme::DrawCard(m_pRenderTarget, rcHotkeys, m_bCard, m_bBorderSubtle, 8.0f);

        float hy = curY + 16.0f;
        if (m_fonts.subhead) {
            const wchar_t* hkTitle = isEn ? L"⌨️ Keybindings & Hotkey Configuration" : L"⌨️ Kısayollar ve Tuş Düzeni";
            m_pRenderTarget->DrawText(hkTitle, (UINT32)wcslen(hkTitle), m_fonts.subhead,
                D2D1::RectF(rcHotkeys.left + 18.0f, hy, rcHotkeys.right, hy + 24.0f), m_bTextPrimary);
        }
        hy += 30.0f;

        if (m_fonts.label) {
            const wchar_t* hkDesc = isEn ? L"Select your favorite development IDE keymap profile:" : L"Favori geliştirme ortamı kısayol profilini seçin:";
            m_pRenderTarget->DrawText(hkDesc, (UINT32)wcslen(hkDesc), m_fonts.label,
                D2D1::RectF(rcHotkeys.left + 18.0f, hy, rcHotkeys.right, hy + 18.0f), m_bTextSecondary);
        }
        hy += 26.0f;

        // Preset cards
        float innerW = cardW - 36.0f;
        float btnW = (innerW - 16.0f) / 3.0f;
        float btnH = 46.0f;
        int activePreset = SettingsManager::Get().settings.hotkeyPreset;

        m_rcHotkeyPresetVSCode = D2D1::RectF(rcHotkeys.left + 18.0f, hy, rcHotkeys.left + 18.0f + btnW, hy + btnH);
        m_rcHotkeyPresetIntelliJ = D2D1::RectF(rcHotkeys.left + 18.0f + btnW + 8.0f, hy, rcHotkeys.left + 18.0f + btnW * 2.0f + 8.0f, hy + btnH);
        m_rcHotkeyPresetVS = D2D1::RectF(rcHotkeys.left + 18.0f + (btnW + 8.0f) * 2.0f, hy, rcHotkeys.left + 18.0f + innerW, hy + btnH);

        auto drawPresetCard = [&](const D2D1_RECT_F& rc, const wchar_t* title, const wchar_t* desc, bool isActive) {
            KaisekiTheme::DrawCard(m_pRenderTarget, rc, isActive ? m_bWell : m_bCardHover, isActive ? m_bAccent : m_bBorderSubtle, 6.0f, isActive ? 2.0f : 1.0f);
            if (m_fonts.button) {
                m_pRenderTarget->DrawText(title, (UINT32)wcslen(title), m_fonts.button,
                    D2D1::RectF(rc.left + 10.0f, rc.top + 5.0f, rc.right - 8.0f, rc.top + 22.0f), isActive ? m_bAccent : m_bTextPrimary);
            }
            if (m_fonts.smallText) {
                m_pRenderTarget->DrawText(desc, (UINT32)wcslen(desc), m_fonts.smallText,
                    D2D1::RectF(rc.left + 10.0f, rc.top + 24.0f, rc.right - 8.0f, rc.bottom - 4.0f), m_bTextSecondary);
            }
        };

        drawPresetCard(m_rcHotkeyPresetVSCode, L"VS Code Default", isEn ? L"Standard IDE Keymap" : L"Standart Modern Düzen", activePreset == 0);
        drawPresetCard(m_rcHotkeyPresetIntelliJ, L"IntelliJ IDEA", isEn ? L"Android Studio Keymap" : L"Android Geliştirici Düzeni", activePreset == 1);
        drawPresetCard(m_rcHotkeyPresetVS, L"Visual Studio", isEn ? L"Classic C++ Keymap" : L"Klasik C++ Düzeni", activePreset == 2);

        hy += btnH + 16.0f;

        struct KeyRow {
            std::wstring keys;
            std::wstring desc;
        };

        std::vector<KeyRow> leftKeys = {
            { L"Ctrl + S", isEn ? L"Save Active File" : L"Aktif Dosyayı Kaydet" },
            { L"Ctrl + Shift + S", isEn ? L"Save All Open Files" : L"Tüm Açık Dosyaları Kaydet" },
            { L"Ctrl + Z", isEn ? L"Undo (History Stack)" : L"Geri Al (Undo)" },
            { L"Ctrl + Y / Shift+Z", isEn ? L"Redo Action" : L"Yinele (Redo)" },
            { L"Ctrl + X", isEn ? L"Cut Selection" : L"Seçili Metni Kes" },
            { L"Ctrl + C", isEn ? L"Copy Selection" : L"Seçili Metni Kopyala" },
            { L"Ctrl + V", isEn ? L"Paste from Clipboard" : L"Panodan Yapıştır" },
            { L"Ctrl + A", isEn ? L"Select All Text" : L"Tüm Metni Seç" },
        };

        std::vector<KeyRow> rightKeys = {
            { L"Ctrl + O", isEn ? L"Open APK File..." : L"APK Dosyası Aç..." },
            { L"Ctrl + Shift + F", isEn ? L"Global Code/Text Search" : L"Genel Kod / Metin Arama" },
            { L"Ctrl + N", isEn ? L"Class Search (JADX)" : L"Sınıf Arama (JADX)" },
            { L"Ctrl + Shift + A", isEn ? L"Go to Main Activity" : L"Ana Activity Sınıfına Git" },
            { L"Ctrl + Shift + M", isEn ? L"Open AndroidManifest" : L"AndroidManifest.xml Aç" },
            { L"Ctrl + E", isEn ? L"Export Gradle Project" : L"Gradle Projesi Dışa Aktar" },
            { L"F7 / F8", isEn ? L"DexOpt / Repackage APK" : L"DexOpt / APK Paketle & İmzala" },
            { L"Ctrl + ,", isEn ? L"Open Settings Modal" : L"Ayarlar Penceresini Aç" },
        };

        float colW = (innerW - 14.0f) * 0.5f;
        float rowH = 24.0f;
        float curYLeft = hy;
        float curYRight = hy;

        for (const auto& kr : leftKeys) {
            D2D1_RECT_F rcRow = D2D1::RectF(rcHotkeys.left + 18.0f, curYLeft, rcHotkeys.left + 18.0f + colW, curYLeft + rowH);
            KaisekiTheme::DrawCard(m_pRenderTarget, rcRow, m_bCard, m_bBorderSubtle, 4.0f);
            D2D1_RECT_F rcBadge = D2D1::RectF(rcRow.left + 5.0f, rcRow.top + 2.0f, rcRow.left + 115.0f, rcRow.bottom - 2.0f);
            KaisekiTheme::DrawCard(m_pRenderTarget, rcBadge, m_bWell, m_bAccent, 3.0f);
            if (m_fonts.smallText) {
                m_pRenderTarget->DrawText(kr.keys.c_str(), (UINT32)kr.keys.length(), m_fonts.smallText,
                    D2D1::RectF(rcBadge.left + 3.0f, rcBadge.top + 2.0f, rcBadge.right - 2.0f, rcBadge.bottom), m_bAccent);
                m_pRenderTarget->DrawText(kr.desc.c_str(), (UINT32)kr.desc.length(), m_fonts.smallText,
                    D2D1::RectF(rcRow.left + 120.0f, rcRow.top + 2.0f, rcRow.right - 4.0f, rcRow.bottom), m_bTextPrimary);
            }
            curYLeft += rowH + 4.0f;
        }

        for (const auto& kr : rightKeys) {
            D2D1_RECT_F rcRow = D2D1::RectF(rcHotkeys.left + 18.0f + colW + 14.0f, curYRight, rcHotkeys.left + 18.0f + innerW, curYRight + rowH);
            KaisekiTheme::DrawCard(m_pRenderTarget, rcRow, m_bCard, m_bBorderSubtle, 4.0f);
            D2D1_RECT_F rcBadge = D2D1::RectF(rcRow.left + 5.0f, rcRow.top + 2.0f, rcRow.left + 125.0f, rcRow.bottom - 2.0f);
            KaisekiTheme::DrawCard(m_pRenderTarget, rcBadge, m_bWell, m_bAccent, 3.0f);
            if (m_fonts.smallText) {
                m_pRenderTarget->DrawText(kr.keys.c_str(), (UINT32)kr.keys.length(), m_fonts.smallText,
                    D2D1::RectF(rcBadge.left + 3.0f, rcBadge.top + 2.0f, rcBadge.right - 2.0f, rcBadge.bottom), m_bAccent);
                m_pRenderTarget->DrawText(kr.desc.c_str(), (UINT32)kr.desc.length(), m_fonts.smallText,
                    D2D1::RectF(rcRow.left + 130.0f, rcRow.top + 2.0f, rcRow.right - 4.0f, rcRow.bottom), m_bTextPrimary);
            }
            curYRight += rowH + 4.0f;
        }
    }
}

void UIWindow::RenderToast(float w, float h) {
    // Toast popups disabled as requested
}

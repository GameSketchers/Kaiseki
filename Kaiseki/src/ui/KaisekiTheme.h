#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#ifdef DrawText
#undef DrawText
#endif

#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

enum class AppTheme {
    KaisekiDark = 0,   // Kaiseki Japanese Studio Dark
    Monochrome = 1,    // Pure Siyah-Beyaz
    CyberTorii = 2,    // Japon Torii Kırmızısı
    ZenGold = 3,       // Kintsugi Altın
    Custom = 4         // In-App Özel Renk Ayarı
};

enum class BackgroundShade {
    Charcoal = 0,      // #18191E Studio Kömür
    OledBlack = 1,     // #0B0B0D Saf Siyah OLED
    MidnightNavy = 2,  // #10141F Gece Mavisi
    ImperialPlum = 3,  // #18111A İmparatorluk Eriği
    DeepSlate = 4,     // #121316 Derin Arduvaz
    DarkObsidian = 5,  // #0E1117 Koyu Obsidyen
    NordicSnow = 6,    // #F0F2F7 Nordik Kar (Açık Beyaz / Gri)
    IvoryDay = 7,      // #F8F5EE Fildişi Gündüz (Açık Krem)
    Custom = 8         // Özel Arka Plan (In-App Seçici)
};

struct ColorSwatch {
    const wchar_t* name;
    D2D1_COLOR_F color;
    const wchar_t* hexStr;
};

namespace KaisekiTheme {

    inline float MinF(float a, float b) { return (a < b) ? a : b; }
    inline float MaxF(float a, float b) { return (a > b) ? a : b; }
    inline size_t MinSize(size_t a, size_t b) { return (a < b) ? a : b; }

    inline float ClampF(float v, float minV, float maxV) {
        if (v < minV) return minV;
        if (v > maxV) return maxV;
        return v;
    }

    inline float LerpF(float a, float b, float t) {
        return a + (b - a) * t;
    }

    inline D2D1_COLOR_F LerpColor(const D2D1_COLOR_F& a, const D2D1_COLOR_F& b, float t) {
        t = ClampF(t, 0.0f, 1.0f);
        return D2D1::ColorF(
            a.r + (b.r - a.r) * t,
            a.g + (b.g - a.g) * t,
            a.b + (b.b - a.b) * t,
            a.a + (b.a - a.a) * t
        );
    }

    inline D2D1_COLOR_F HsvToRgb(float h, float s, float v) {
        while (h < 0.0f) h += 360.0f;
        while (h >= 360.0f) h -= 360.0f;
        s = ClampF(s, 0.0f, 1.0f);
        v = ClampF(v, 0.0f, 1.0f);

        float c = v * s;
        float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
        float m = v - c;

        float r = 0, g = 0, b = 0;
        if (h < 60.0f)       { r = c; g = x; b = 0; }
        else if (h < 120.0f) { r = x; g = c; b = 0; }
        else if (h < 180.0f) { r = 0; g = c; b = x; }
        else if (h < 240.0f) { r = 0; g = x; b = c; }
        else if (h < 300.0f) { r = x; g = 0; b = c; }
        else                 { r = c; g = 0; b = x; }

        return D2D1::ColorF(r + m, g + m, b + m, 1.0f);
    }

    inline void RgbToHsv(float r, float g, float b, float& h, float& s, float& v) {
        float maxV = (std::max)({r, g, b});
        float minV = (std::min)({r, g, b});
        float delta = maxV - minV;

        v = maxV;
        s = (maxV > 0.0001f) ? (delta / maxV) : 0.0f;

        if (delta < 0.0001f) {
            h = 0.0f;
        } else {
            if (maxV == r) {
                h = 60.0f * fmodf(((g - b) / delta), 6.0f);
            } else if (maxV == g) {
                h = 60.0f * (((b - r) / delta) + 2.0f);
            } else {
                h = 60.0f * (((r - g) / delta) + 4.0f);
            }
            if (h < 0.0f) h += 360.0f;
        }
    }

    inline float EaseOutCubic(float t) {
        t = ClampF(t, 0.0f, 1.0f);
        float f = t - 1.0f;
        return f * f * f + 1.0f;
    }

    inline float EaseInOutCubic(float t) {
        t = ClampF(t, 0.0f, 1.0f);
        return (t < 0.5f) ? 4.0f * t * t * t : 1.0f - (float)pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
    }

    inline float EaseOutBack(float t) {
        t = ClampF(t, 0.0f, 1.0f);
        const float c1 = 1.70158f;
        const float c3 = c1 + 1.0f;
        float f = t - 1.0f;
        return 1.0f + c3 * f * f * f + c1 * f * f;
    }

    inline float EaseOutQuad(float t) {
        t = ClampF(t, 0.0f, 1.0f);
        return 1.0f - (1.0f - t) * (1.0f - t);
    }

    inline std::wstring Utf8ToWide(const std::string& str) {
        if (str.empty()) return L"";
        int size = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), NULL, 0);
        std::wstring wstr(size, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &wstr[0], size);
        return wstr;
    }

    inline std::string WideToUtf8(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), NULL, 0, NULL, NULL);
        std::string str(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &str[0], size, NULL, NULL);
        return str;
    }

    // JADX BinEd Style Hex Viewer Dump Formatter
    inline std::string FormatHexDump(const uint8_t* data, size_t size, size_t maxBytes = 65536) {
        if (!data || size == 0) return "// [Bos Ikili Dosya / Empty Binary]\n";
        std::ostringstream ss;
        size_t limit = (size < maxBytes) ? size : maxBytes;
        ss << "// === KAISEKI HEX VIEWER (BinEd Engine) ===\n";
        ss << "// Toplam Boyut: " << size << " bayt (" << (size / 1024) << " KB)\n";
        ss << "// Offset    00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F  |ASCII Decoded|\n";
        ss << "// -----------------------------------------------------------------------------\n";
        for (size_t i = 0; i < limit; i += 16) {
            char buf[128];
            snprintf(buf, sizeof(buf), "%08X: ", (unsigned int)i);
            ss << buf;

            for (size_t j = 0; j < 16; ++j) {
                if (i + j < limit) {
                    snprintf(buf, sizeof(buf), "%02X ", data[i + j]);
                    ss << buf;
                } else {
                    ss << "   ";
                }
                if (j == 7) ss << " ";
            }

            ss << " |";
            for (size_t j = 0; j < 16 && (i + j) < limit; ++j) {
                uint8_t c = data[i + j];
                if (c >= 32 && c <= 126) {
                    ss << (char)c;
                } else {
                    ss << '.';
                }
            }
            ss << "|\n";
        }
        if (size > maxBytes) {
            ss << "\n// ... [Geriye kalan " << (size - maxBytes) << " bayt kirpildi / Truncated for performance] ...\n";
        }
        return ss.str();
    }

    inline D2D1_COLOR_F ColorFromRgbU8(uint8_t r, uint8_t g, uint8_t b, float a = 1.0f) {
        return D2D1::ColorF(r / 255.0f, g / 255.0f, b / 255.0f, a);
    }

    inline std::wstring ColorToHexW(const D2D1_COLOR_F& c) {
        int r = (int)(c.r * 255.0f + 0.5f);
        int g = (int)(c.g * 255.0f + 0.5f);
        int b = (int)(c.b * 255.0f + 0.5f);
        if (r < 0) r = 0; if (r > 255) r = 255;
        if (g < 0) g = 0; if (g > 255) g = 255;
        if (b < 0) b = 0; if (b > 255) b = 255;
        wchar_t buf[16];
        swprintf_s(buf, L"#%02X%02X%02X", r, g, b);
        return buf;
    }

    // 12 Preset Color Swatches for the in-app color picker
    inline const std::vector<ColorSwatch>& GetPresetSwatches() {
        static const std::vector<ColorSwatch> swatches = {
            { L"Sakura Pembe", D2D1::ColorF(1.000f, 0.500f, 0.720f, 1.0f), L"#FF80B8" },
            { L"Pastel Pembe", D2D1::ColorF(1.000f, 0.894f, 1.000f, 1.0f), L"#FFE4FF" },
            { L"Torii Kırmızı", D2D1::ColorF(1.000f, 0.278f, 0.341f, 1.0f), L"#FF4757" },
            { L"Kintsugi Altın", D2D1::ColorF(0.898f, 0.702f, 0.220f, 1.0f), L"#E5B338" },
            { L"Matcha Yeşil", D2D1::ColorF(0.180f, 0.800f, 0.443f, 1.0f), L"#2ECC71" },
            { L"Kanagawa Mavi", D2D1::ColorF(0.118f, 0.565f, 1.000f, 1.0f), L"#1E90FF" },
            { L"Siyan (Cyan)", D2D1::ColorF(0.000f, 0.898f, 1.000f, 1.0f), L"#00E5FF" },
            { L"Hasu Mor",     D2D1::ColorF(0.647f, 0.369f, 0.918f, 1.0f), L"#A55EEA" },
            { L"Gümüş Gri",    D2D1::ColorF(0.620f, 0.620f, 0.620f, 1.0f), L"#9E9E9E" },
            { L"Asahi Turuncu", D2D1::ColorF(1.000f, 0.475f, 0.247f, 1.0f), L"#FF793F" },
            { L"Zümrüt Yeşil", D2D1::ColorF(0.149f, 0.871f, 0.506f, 1.0f), L"#26DE81" },
            { L"Kehribar Sarı", D2D1::ColorF(0.996f, 0.827f, 0.188f, 1.0f), L"#FED330" }
        };
        return swatches;
    }

    struct ThemeColors {
        D2D1_COLOR_F canvasBg;
        D2D1_COLOR_F panelBg;
        D2D1_COLOR_F cardBg;
        D2D1_COLOR_F cardHover;
        D2D1_COLOR_F inputWell;
        D2D1_COLOR_F borderSubtle;
        D2D1_COLOR_F borderHover;
        D2D1_COLOR_F accent;
        D2D1_COLOR_F accentLight;
        D2D1_COLOR_F textPrimary;
        D2D1_COLOR_F textSecondary;
        D2D1_COLOR_F textMuted;
        D2D1_COLOR_F badgeBg;
        D2D1_COLOR_F success;
        D2D1_COLOR_F warning;

        // Specialized Kaiseki Colors
        D2D1_COLOR_F railBg;
        D2D1_COLOR_F menuBg;
        D2D1_COLOR_F accentLine;
        D2D1_COLOR_F btnCompile;
        D2D1_COLOR_F btnAnalyze;
        D2D1_COLOR_F chartBar;
        D2D1_COLOR_F watermark;
        D2D1_COLOR_F consoleBg;
        // Antigravity Rounded Pill Tabs
        D2D1_COLOR_F tabActiveBg;
        D2D1_COLOR_F tabActiveBorder;
        D2D1_COLOR_F tabHoverBg;
        D2D1_COLOR_F tabInactiveBg;

        // Authentic IDE Syntax Highlighting (Fixed standard colors, independent of theme)
        D2D1_COLOR_F syntaxKeyword;
        D2D1_COLOR_F syntaxRegister;
        D2D1_COLOR_F syntaxString;
        D2D1_COLOR_F syntaxNumber;
        D2D1_COLOR_F syntaxComment;
        D2D1_COLOR_F syntaxAnnotation;
        D2D1_COLOR_F syntaxType;
        D2D1_COLOR_F syntaxTag;
        D2D1_COLOR_F syntaxAttr;
    };

    inline void ApplyCustomBackgroundColor(ThemeColors& c, const D2D1_COLOR_F& bg) {
        c.canvasBg  = bg;
        float lum = 0.299f * bg.r + 0.587f * bg.g + 0.114f * bg.b;
        bool isLight = (lum > 0.5f);

        if (isLight) {
            // Light Background Adaptation: If light color, all texts are black/dark charcoal!
            c.textPrimary   = D2D1::ColorF(0.08f, 0.09f, 0.12f, 1.0f); // Deep Charcoal / Black
            c.textSecondary = D2D1::ColorF(0.24f, 0.27f, 0.35f, 1.0f);
            c.textMuted     = D2D1::ColorF(0.45f, 0.48f, 0.56f, 1.0f);
            c.panelBg       = D2D1::ColorF(ClampF(bg.r * 0.96f, 0.0f, 1.0f), ClampF(bg.g * 0.96f, 0.0f, 1.0f), ClampF(bg.b * 0.96f, 0.0f, 1.0f), 1.0f);
            c.cardBg        = D2D1::ColorF(ClampF(bg.r * 1.02f + 0.02f, 0.0f, 1.0f), ClampF(bg.g * 1.02f + 0.02f, 0.0f, 1.0f), ClampF(bg.b * 1.02f + 0.02f, 0.0f, 1.0f), 1.0f);
            c.cardHover     = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
            c.inputWell     = D2D1::ColorF(ClampF(bg.r * 0.92f, 0.0f, 1.0f), ClampF(bg.g * 0.92f, 0.0f, 1.0f), ClampF(bg.b * 0.92f, 0.0f, 1.0f), 1.0f);
            c.railBg        = D2D1::ColorF(ClampF(bg.r * 0.94f, 0.0f, 1.0f), ClampF(bg.g * 0.94f, 0.0f, 1.0f), ClampF(bg.b * 0.94f, 0.0f, 1.0f), 1.0f);
            c.menuBg        = D2D1::ColorF(ClampF(bg.r * 0.97f, 0.0f, 1.0f), ClampF(bg.g * 0.97f, 0.0f, 1.0f), ClampF(bg.b * 0.97f, 0.0f, 1.0f), 1.0f);
            c.consoleBg     = D2D1::ColorF(ClampF(bg.r * 0.90f, 0.0f, 1.0f), ClampF(bg.g * 0.90f, 0.0f, 1.0f), ClampF(bg.b * 0.90f, 0.0f, 1.0f), 1.0f);
            c.borderSubtle  = D2D1::ColorF(0.78f, 0.81f, 0.86f, 1.0f);
        } else {
            // Dark Background Adaptation: If dark color, all texts are white/light gray!
            c.textPrimary   = D2D1::ColorF(0.95f, 0.95f, 0.98f, 1.0f); // White
            c.textSecondary = D2D1::ColorF(0.65f, 0.68f, 0.75f, 1.0f);
            c.textMuted     = D2D1::ColorF(0.45f, 0.48f, 0.55f, 1.0f);
            c.panelBg       = D2D1::ColorF(ClampF(bg.r * 0.88f, 0.0f, 1.0f), ClampF(bg.g * 0.88f, 0.0f, 1.0f), ClampF(bg.b * 0.88f, 0.0f, 1.0f), 1.0f);
            c.cardBg        = D2D1::ColorF(ClampF(bg.r * 1.25f + 0.02f, 0.0f, 1.0f), ClampF(bg.g * 1.25f + 0.02f, 0.0f, 1.0f), ClampF(bg.b * 1.25f + 0.02f, 0.0f, 1.0f), 1.0f);
            c.cardHover     = D2D1::ColorF(ClampF(bg.r * 1.50f + 0.04f, 0.0f, 1.0f), ClampF(bg.g * 1.50f + 0.04f, 0.0f, 1.0f), ClampF(bg.b * 1.50f + 0.04f, 0.0f, 1.0f), 1.0f);
            c.inputWell     = D2D1::ColorF(ClampF(bg.r * 0.75f, 0.0f, 1.0f), ClampF(bg.g * 0.75f, 0.0f, 1.0f), ClampF(bg.b * 0.75f, 0.0f, 1.0f), 1.0f);
            c.railBg        = D2D1::ColorF(ClampF(bg.r * 0.70f, 0.0f, 1.0f), ClampF(bg.g * 0.70f, 0.0f, 1.0f), ClampF(bg.b * 0.70f, 0.0f, 1.0f), 1.0f);
            c.menuBg        = D2D1::ColorF(ClampF(bg.r * 0.80f, 0.0f, 1.0f), ClampF(bg.g * 0.80f, 0.0f, 1.0f), ClampF(bg.b * 0.80f, 0.0f, 1.0f), 1.0f);
            c.consoleBg     = D2D1::ColorF(ClampF(bg.r * 0.65f, 0.0f, 1.0f), ClampF(bg.g * 0.65f, 0.0f, 1.0f), ClampF(bg.b * 0.65f, 0.0f, 1.0f), 1.0f);
            c.borderSubtle  = D2D1::ColorF(0.20f, 0.21f, 0.25f, 1.0f);
        }
    }

    inline void ApplyBackgroundShade(ThemeColors& c, BackgroundShade shade) {
        if (shade == BackgroundShade::OledBlack) {
            c.canvasBg  = D2D1::ColorF(0.043f, 0.043f, 0.051f, 1.0f); // #0B0B0D OLED Saf Siyah
            c.panelBg   = D2D1::ColorF(0.055f, 0.055f, 0.063f, 1.0f); // #0E0E10
            c.cardBg    = D2D1::ColorF(0.075f, 0.075f, 0.086f, 1.0f); // #131316
            c.inputWell = D2D1::ColorF(0.025f, 0.025f, 0.030f, 1.0f); // #060608
            c.railBg    = D2D1::ColorF(0.035f, 0.035f, 0.040f, 1.0f);
            c.menuBg    = D2D1::ColorF(0.043f, 0.043f, 0.051f, 1.0f);
        } else if (shade == BackgroundShade::MidnightNavy) {
            c.canvasBg  = D2D1::ColorF(0.063f, 0.078f, 0.122f, 1.0f); // #10141F Gece Mavisi
            c.panelBg   = D2D1::ColorF(0.078f, 0.098f, 0.149f, 1.0f); // #141926
            c.cardBg    = D2D1::ColorF(0.102f, 0.129f, 0.200f, 1.0f); // #1A2133
            c.inputWell = D2D1::ColorF(0.047f, 0.059f, 0.094f, 1.0f); // #0C0F18
            c.railBg    = D2D1::ColorF(0.055f, 0.067f, 0.106f, 1.0f);
            c.menuBg    = D2D1::ColorF(0.063f, 0.078f, 0.122f, 1.0f);
        } else if (shade == BackgroundShade::ImperialPlum) {
            c.canvasBg  = D2D1::ColorF(0.094f, 0.067f, 0.102f, 1.0f); // #18111A İmparatorluk Eriği
            c.panelBg   = D2D1::ColorF(0.114f, 0.082f, 0.125f, 1.0f); // #1D1520
            c.cardBg    = D2D1::ColorF(0.145f, 0.106f, 0.161f, 1.0f); // #251B29
            c.inputWell = D2D1::ColorF(0.070f, 0.047f, 0.078f, 1.0f); // #120C14
            c.railBg    = D2D1::ColorF(0.078f, 0.055f, 0.086f, 1.0f);
            c.menuBg    = D2D1::ColorF(0.094f, 0.067f, 0.102f, 1.0f);
        } else if (shade == BackgroundShade::DeepSlate) {
            c.canvasBg  = D2D1::ColorF(0.070f, 0.075f, 0.086f, 1.0f); // #121316 Derin Arduvaz
            c.panelBg   = D2D1::ColorF(0.062f, 0.066f, 0.078f, 1.0f);
            c.cardBg    = D2D1::ColorF(0.090f, 0.094f, 0.106f, 1.0f);
            c.inputWell = D2D1::ColorF(0.040f, 0.043f, 0.050f, 1.0f);
            c.railBg    = D2D1::ColorF(0.050f, 0.054f, 0.062f, 1.0f);
            c.menuBg    = D2D1::ColorF(0.060f, 0.064f, 0.072f, 1.0f);
        } else if (shade == BackgroundShade::DarkObsidian) {
            c.canvasBg  = D2D1::ColorF(0.055f, 0.067f, 0.090f, 1.0f); // #0E1117 Koyu Obsidyen
            c.panelBg   = D2D1::ColorF(0.047f, 0.055f, 0.078f, 1.0f);
            c.cardBg    = D2D1::ColorF(0.075f, 0.090f, 0.125f, 1.0f);
            c.inputWell = D2D1::ColorF(0.035f, 0.040f, 0.055f, 1.0f);
            c.railBg    = D2D1::ColorF(0.040f, 0.047f, 0.065f, 1.0f);
            c.menuBg    = D2D1::ColorF(0.048f, 0.058f, 0.078f, 1.0f);
        } else if (shade == BackgroundShade::NordicSnow) {
            c.canvasBg      = D2D1::ColorF(0.941f, 0.949f, 0.968f, 1.0f); // #F0F2F7 Nordik Kar
            c.panelBg       = D2D1::ColorF(0.900f, 0.910f, 0.935f, 1.0f);
            c.cardBg        = D2D1::ColorF(0.985f, 0.988f, 0.995f, 1.0f);
            c.cardHover     = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
            c.inputWell     = D2D1::ColorF(0.870f, 0.880f, 0.910f, 1.0f);
            c.railBg        = D2D1::ColorF(0.885f, 0.895f, 0.925f, 1.0f);
            c.menuBg        = D2D1::ColorF(0.920f, 0.930f, 0.950f, 1.0f);
            c.consoleBg     = D2D1::ColorF(0.850f, 0.860f, 0.890f, 1.0f);
            c.borderSubtle  = D2D1::ColorF(0.78f, 0.80f, 0.84f, 1.0f);
            c.textPrimary   = D2D1::ColorF(0.08f, 0.09f, 0.12f, 1.0f); // Siyah (Açık Tema Kontrastı)
            c.textSecondary = D2D1::ColorF(0.25f, 0.28f, 0.35f, 1.0f);
            c.textMuted     = D2D1::ColorF(0.45f, 0.48f, 0.55f, 1.0f);
        } else if (shade == BackgroundShade::IvoryDay) {
            c.canvasBg      = D2D1::ColorF(0.972f, 0.960f, 0.933f, 1.0f); // #F8F5EE Fildişi Gündüz
            c.panelBg       = D2D1::ColorF(0.930f, 0.915f, 0.885f, 1.0f);
            c.cardBg        = D2D1::ColorF(0.995f, 0.990f, 0.980f, 1.0f);
            c.cardHover     = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
            c.inputWell     = D2D1::ColorF(0.895f, 0.880f, 0.845f, 1.0f);
            c.railBg        = D2D1::ColorF(0.910f, 0.895f, 0.860f, 1.0f);
            c.menuBg        = D2D1::ColorF(0.945f, 0.930f, 0.900f, 1.0f);
            c.consoleBg     = D2D1::ColorF(0.880f, 0.865f, 0.830f, 1.0f);
            c.borderSubtle  = D2D1::ColorF(0.80f, 0.78f, 0.74f, 1.0f);
            c.textPrimary   = D2D1::ColorF(0.10f, 0.09f, 0.08f, 1.0f); // Siyah (Açık Tema Kontrastı)
            c.textSecondary = D2D1::ColorF(0.28f, 0.26f, 0.23f, 1.0f);
            c.textMuted     = D2D1::ColorF(0.48f, 0.45f, 0.40f, 1.0f);
        } else {
            // Charcoal (#18191E Studio Kömür) - default
            c.canvasBg      = D2D1::ColorF(0.106f, 0.110f, 0.133f, 1.0f); // #1B1C22
            c.panelBg       = D2D1::ColorF(0.094f, 0.098f, 0.118f, 1.0f); // #18191E
            c.cardBg        = D2D1::ColorF(0.125f, 0.133f, 0.160f, 1.0f); // #202229
            c.inputWell     = D2D1::ColorF(0.082f, 0.086f, 0.102f, 1.0f); // #15161A
            c.railBg        = D2D1::ColorF(0.078f, 0.078f, 0.094f, 1.0f); // #141418
            c.menuBg        = D2D1::ColorF(0.082f, 0.086f, 0.098f, 1.0f); // #151619
            c.textPrimary   = D2D1::ColorF(0.950f, 0.950f, 0.940f, 1.0f); // Beyaz
            c.textSecondary = D2D1::ColorF(0.680f, 0.710f, 0.770f, 1.0f);
            c.textMuted     = D2D1::ColorF(0.420f, 0.440f, 0.500f, 1.0f);
        }
    }

    inline ThemeColors GetTheme(AppTheme theme, const D2D1_COLOR_F* customAccent = nullptr, BackgroundShade shade = BackgroundShade::Charcoal) {
        ThemeColors c;
        if (theme == AppTheme::KaisekiDark) {
            D2D1_COLOR_F acc = customAccent ? *customAccent : D2D1::ColorF(1.000f, 0.500f, 0.720f, 1.0f); // #FF80B8 Sakura Pink
            c.canvasBg      = D2D1::ColorF(0.106f, 0.110f, 0.133f, 1.0f); // #1B1C22 Deep Ink Charcoal
            c.panelBg       = D2D1::ColorF(0.094f, 0.098f, 0.118f, 1.0f); // #18191E Sidebar
            c.cardBg        = D2D1::ColorF(0.125f, 0.133f, 0.160f, 1.0f); // #202229
            c.cardHover     = D2D1::ColorF(0.165f, 0.176f, 0.212f, 1.0f); // #2A2D36
            c.inputWell     = D2D1::ColorF(0.082f, 0.086f, 0.102f, 1.0f); // #15161A Editor
            c.borderSubtle  = D2D1::ColorF(0.200f, 0.212f, 0.250f, 1.0f); // #333640
            c.borderHover   = acc;
            c.accent        = acc;
            c.accentLight   = customAccent ? D2D1::ColorF(MinF(1.0f, acc.r * 1.25f), MinF(1.0f, acc.g * 1.25f), MinF(1.0f, acc.b * 1.25f), 1.0f) : D2D1::ColorF(1.000f, 0.894f, 1.000f, 1.0f);
            c.textPrimary   = D2D1::ColorF(0.950f, 0.950f, 0.940f, 1.0f); // #F2F2F0
            c.textSecondary = D2D1::ColorF(0.680f, 0.710f, 0.770f, 1.0f); // #ADB5C4
            c.textMuted     = D2D1::ColorF(0.420f, 0.440f, 0.500f, 1.0f); // #6B7080
            c.badgeBg       = D2D1::ColorF(0.150f, 0.160f, 0.200f, 1.0f);
            c.success       = D2D1::ColorF(0.180f, 0.800f, 0.440f, 1.0f); // #2ECC71 Green
            c.warning       = D2D1::ColorF(0.950f, 0.770f, 0.060f, 1.0f); // #F1C40F Gold

            c.railBg        = D2D1::ColorF(0.078f, 0.078f, 0.094f, 1.0f); // #141418 Leftmost icon rail
            c.menuBg        = D2D1::ColorF(0.082f, 0.086f, 0.098f, 1.0f); // #151619 Top titlebar
            c.accentLine    = acc;
            c.btnCompile    = D2D1::ColorF(1.000f, 0.400f, 0.600f, 1.0f); // Soft pink compile button
            c.btnAnalyze    = acc;
            c.chartBar      = acc;
            c.watermark     = D2D1::ColorF(acc.r, acc.g, acc.b, 0.030f);
            c.consoleBg     = D2D1::ColorF(0.067f, 0.070f, 0.086f, 1.0f); // #111216
            ApplyBackgroundShade(c, shade);
        } else if (theme == AppTheme::Monochrome) {
            c.canvasBg      = D2D1::ColorF(0.063f, 0.063f, 0.067f, 1.0f);
            c.panelBg       = D2D1::ColorF(0.094f, 0.094f, 0.102f, 1.0f);
            c.cardBg        = D2D1::ColorF(0.125f, 0.125f, 0.137f, 1.0f);
            c.cardHover     = D2D1::ColorF(0.165f, 0.165f, 0.180f, 1.0f);
            c.inputWell     = D2D1::ColorF(0.040f, 0.040f, 0.043f, 1.0f);
            c.borderSubtle  = D2D1::ColorF(0.200f, 0.200f, 0.220f, 1.0f);
            c.borderHover   = D2D1::ColorF(0.400f, 0.400f, 0.440f, 1.0f);
            c.accent        = D2D1::ColorF(0.960f, 0.960f, 0.980f, 1.0f);
            c.accentLight   = D2D1::ColorF(0.800f, 0.800f, 0.840f, 1.0f);
            c.textPrimary   = D2D1::ColorF(0.980f, 0.980f, 1.000f, 1.0f);
            c.textSecondary = D2D1::ColorF(0.650f, 0.650f, 0.700f, 1.0f);
            c.textMuted     = D2D1::ColorF(0.420f, 0.420f, 0.460f, 1.0f);
            c.badgeBg       = D2D1::ColorF(0.150f, 0.150f, 0.165f, 1.0f);
            c.success       = D2D1::ColorF(0.850f, 0.850f, 0.900f, 1.0f);
            c.warning       = D2D1::ColorF(0.750f, 0.750f, 0.800f, 1.0f);

            c.railBg        = D2D1::ColorF(0.045f, 0.045f, 0.050f, 1.0f);
            c.menuBg        = D2D1::ColorF(0.055f, 0.055f, 0.060f, 1.0f);
            c.accentLine    = D2D1::ColorF(0.300f, 0.300f, 0.320f, 1.0f);
            c.btnCompile    = D2D1::ColorF(0.250f, 0.250f, 0.270f, 1.0f);
            c.btnAnalyze    = D2D1::ColorF(0.850f, 0.850f, 0.880f, 1.0f);
            c.chartBar      = D2D1::ColorF(0.600f, 0.600f, 0.650f, 1.0f);
            c.watermark     = D2D1::ColorF(1.000f, 1.000f, 1.000f, 0.025f);
            c.consoleBg     = D2D1::ColorF(0.040f, 0.040f, 0.045f, 1.0f);
        } else if (theme == AppTheme::CyberTorii) {
            c.canvasBg      = D2D1::ColorF(0.075f, 0.075f, 0.086f, 1.0f);
            c.panelBg       = D2D1::ColorF(0.110f, 0.110f, 0.125f, 1.0f);
            c.cardBg        = D2D1::ColorF(0.145f, 0.145f, 0.165f, 1.0f);
            c.cardHover     = D2D1::ColorF(0.190f, 0.190f, 0.215f, 1.0f);
            c.inputWell     = D2D1::ColorF(0.045f, 0.045f, 0.055f, 1.0f);
            c.borderSubtle  = D2D1::ColorF(0.220f, 0.220f, 0.250f, 1.0f);
            c.borderHover   = D2D1::ColorF(1.000f, 0.278f, 0.341f, 1.0f);
            c.accent        = D2D1::ColorF(1.000f, 0.278f, 0.341f, 1.0f);
            c.accentLight   = D2D1::ColorF(1.000f, 0.450f, 0.500f, 1.0f);
            c.textPrimary   = D2D1::ColorF(0.960f, 0.960f, 0.980f, 1.0f);
            c.textSecondary = D2D1::ColorF(0.650f, 0.650f, 0.700f, 1.0f);
            c.textMuted     = D2D1::ColorF(0.420f, 0.420f, 0.460f, 1.0f);
            c.badgeBg       = D2D1::ColorF(0.180f, 0.180f, 0.205f, 1.0f);
            c.success       = D2D1::ColorF(0.100f, 0.780f, 0.520f, 1.0f);
            c.warning       = D2D1::ColorF(0.960f, 0.620f, 0.040f, 1.0f);

            c.railBg        = D2D1::ColorF(0.060f, 0.060f, 0.070f, 1.0f);
            c.menuBg        = D2D1::ColorF(0.070f, 0.070f, 0.080f, 1.0f);
            c.accentLine    = D2D1::ColorF(1.000f, 0.278f, 0.341f, 1.0f);
            c.btnCompile    = D2D1::ColorF(1.000f, 0.278f, 0.341f, 1.0f);
            c.btnAnalyze    = D2D1::ColorF(0.800f, 0.750f, 0.600f, 1.0f);
            c.chartBar      = D2D1::ColorF(0.850f, 0.350f, 0.400f, 1.0f);
            c.watermark     = D2D1::ColorF(1.000f, 0.278f, 0.341f, 0.035f);
            c.consoleBg     = D2D1::ColorF(0.050f, 0.050f, 0.060f, 1.0f);
        } else if (theme == AppTheme::ZenGold) {
            c.canvasBg      = D2D1::ColorF(0.070f, 0.075f, 0.080f, 1.0f);
            c.panelBg       = D2D1::ColorF(0.105f, 0.110f, 0.120f, 1.0f);
            c.cardBg        = D2D1::ColorF(0.140f, 0.145f, 0.155f, 1.0f);
            c.cardHover     = D2D1::ColorF(0.185f, 0.190f, 0.205f, 1.0f);
            c.inputWell     = D2D1::ColorF(0.045f, 0.048f, 0.055f, 1.0f);
            c.borderSubtle  = D2D1::ColorF(0.230f, 0.235f, 0.250f, 1.0f);
            c.borderHover   = D2D1::ColorF(0.898f, 0.702f, 0.220f, 1.0f);
            c.accent        = D2D1::ColorF(0.898f, 0.702f, 0.220f, 1.0f);
            c.accentLight   = D2D1::ColorF(0.960f, 0.800f, 0.350f, 1.0f);
            c.textPrimary   = D2D1::ColorF(0.960f, 0.960f, 0.970f, 1.0f);
            c.textSecondary = D2D1::ColorF(0.680f, 0.670f, 0.640f, 1.0f);
            c.textMuted     = D2D1::ColorF(0.440f, 0.430f, 0.410f, 1.0f);
            c.badgeBg       = D2D1::ColorF(0.170f, 0.170f, 0.180f, 1.0f);
            c.success       = D2D1::ColorF(0.100f, 0.780f, 0.520f, 1.0f);
            c.warning       = D2D1::ColorF(0.945f, 0.769f, 0.059f, 1.0f);

            c.railBg        = D2D1::ColorF(0.055f, 0.058f, 0.065f, 1.0f);
            c.menuBg        = D2D1::ColorF(0.065f, 0.068f, 0.075f, 1.0f);
            c.accentLine    = D2D1::ColorF(0.898f, 0.702f, 0.220f, 1.0f);
            c.btnCompile    = D2D1::ColorF(0.850f, 0.260f, 0.300f, 1.0f);
            c.btnAnalyze    = D2D1::ColorF(0.898f, 0.702f, 0.220f, 1.0f);
            c.chartBar      = D2D1::ColorF(0.898f, 0.702f, 0.220f, 1.0f);
            c.watermark     = D2D1::ColorF(0.898f, 0.702f, 0.220f, 0.035f);
            c.consoleBg     = D2D1::ColorF(0.045f, 0.048f, 0.055f, 1.0f);
        } else {
            // In-App Custom Palette with live Accent
            D2D1_COLOR_F acc = customAccent ? *customAccent : D2D1::ColorF(0.000f, 0.780f, 0.850f, 1.0f);
            c.canvasBg      = D2D1::ColorF(0.068f, 0.068f, 0.075f, 1.0f);
            c.panelBg       = D2D1::ColorF(0.100f, 0.100f, 0.112f, 1.0f);
            c.cardBg        = D2D1::ColorF(0.135f, 0.135f, 0.150f, 1.0f);
            c.cardHover     = D2D1::ColorF(0.180f, 0.180f, 0.200f, 1.0f);
            c.inputWell     = D2D1::ColorF(0.040f, 0.040f, 0.048f, 1.0f);
            c.borderSubtle  = D2D1::ColorF(0.220f, 0.220f, 0.240f, 1.0f);
            c.borderHover   = acc;
            c.accent        = acc;
            c.accentLight   = D2D1::ColorF(MinF(1.0f, acc.r * 1.25f), MinF(1.0f, acc.g * 1.25f), MinF(1.0f, acc.b * 1.25f), 1.0f);
            c.textPrimary   = D2D1::ColorF(0.970f, 0.970f, 0.990f, 1.0f);
            c.textSecondary = D2D1::ColorF(0.660f, 0.660f, 0.720f, 1.0f);
            c.textMuted     = D2D1::ColorF(0.430f, 0.430f, 0.470f, 1.0f);
            c.badgeBg       = D2D1::ColorF(0.160f, 0.160f, 0.180f, 1.0f);
            c.success       = D2D1::ColorF(0.100f, 0.780f, 0.520f, 1.0f);
            c.warning       = D2D1::ColorF(0.960f, 0.620f, 0.040f, 1.0f);

            c.railBg        = D2D1::ColorF(0.050f, 0.050f, 0.058f, 1.0f);
            c.menuBg        = D2D1::ColorF(0.060f, 0.060f, 0.068f, 1.0f);
            c.accentLine    = acc;
            c.btnCompile    = D2D1::ColorF(0.850f, 0.260f, 0.300f, 1.0f);
            c.btnAnalyze    = acc;
            c.chartBar      = acc;
            c.watermark     = D2D1::ColorF(acc.r, acc.g, acc.b, 0.035f);
            c.consoleBg     = D2D1::ColorF(0.040f, 0.040f, 0.048f, 1.0f);
            ApplyBackgroundShade(c, shade);
        }

        // Antigravity dark pill tabs (as seen in modern IDEs)
        c.tabActiveBg     = D2D1::ColorF(0.168f, 0.176f, 0.188f, 1.0f); // #2B2D30
        c.tabActiveBorder = D2D1::ColorF(0.243f, 0.251f, 0.267f, 1.0f); // #3E4044
        c.tabHoverBg      = D2D1::ColorF(0.141f, 0.145f, 0.157f, 1.0f); // #242528
        c.tabInactiveBg   = D2D1::ColorF(0.094f, 0.098f, 0.110f, 1.0f); // #18191C

        // Authentic IDE Syntax Highlighting (Non-theme, pure IDE standard)
        c.syntaxKeyword    = D2D1::ColorF(0.337f, 0.612f, 0.839f, 1.0f); // #569CD6 (VS Blue)
        c.syntaxRegister   = D2D1::ColorF(0.612f, 0.863f, 0.996f, 1.0f); // #9CDCFE (Cyan)
        c.syntaxString     = D2D1::ColorF(0.808f, 0.569f, 0.471f, 1.0f); // #CE9178 (Coral/Salmon)
        c.syntaxNumber     = D2D1::ColorF(0.710f, 0.808f, 0.659f, 1.0f); // #B5CEA8 (Olive/Green)
        c.syntaxComment    = D2D1::ColorF(0.416f, 0.600f, 0.333f, 1.0f); // #6A9955 (VS Code Forest Green)
        c.syntaxAnnotation = D2D1::ColorF(0.773f, 0.525f, 0.753f, 1.0f); // #C586C0 (Purple)
        c.syntaxType       = D2D1::ColorF(0.306f, 0.788f, 0.690f, 1.0f); // #4EC9B0 (Teal)
        c.syntaxTag        = D2D1::ColorF(0.337f, 0.612f, 0.839f, 1.0f); // #569CD6 (Tag Blue)
        c.syntaxAttr       = D2D1::ColorF(0.612f, 0.863f, 0.996f, 1.0f); // #9CDCFE (Attribute Cyan)

        return c;
    }

    inline void DrawCard(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, ID2D1Brush* bgBrush, ID2D1Brush* borderBrush, float radius = 5.0f, float strokeWidth = 1.0f) {
        D2D1_ROUNDED_RECT rrect = D2D1::RoundedRect(rect, radius, radius);
        if (bgBrush) rt->FillRoundedRectangle(&rrect, bgBrush);
        if (borderBrush) rt->DrawRoundedRectangle(&rrect, borderBrush, strokeWidth);
    }
}

// Backwards compatibility alias
namespace AdobeTheme = KaisekiTheme;

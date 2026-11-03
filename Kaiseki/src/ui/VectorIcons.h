#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <cmath>

namespace VectorIcons {

    // --- Seigaiha Japanese Circular Kamon Crest (Top Left Logo) ---
    inline void DrawKamon(ID2D1RenderTarget* rt, float cx, float cy, float radius, ID2D1Brush* brush, float stroke = 1.4f) {
        // Outer ring
        rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), radius, radius), brush, stroke * 1.2f);

        // Concentric wave arcs inside circle (Seigaiha traditional Japanese crest pattern)
        for (int r = 1; r <= 3; ++r) {
            float arcR = radius * (0.28f * r);
            rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy + radius * 0.35f), arcR, arcR * 0.7f), brush, stroke * 0.9f);
        }
        for (int r = 1; r <= 3; ++r) {
            float arcR = radius * (0.28f * r);
            rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx - radius * 0.45f, cy - radius * 0.1f), arcR, arcR * 0.7f), brush, stroke * 0.8f);
            rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx + radius * 0.45f, cy - radius * 0.1f), arcR, arcR * 0.7f), brush, stroke * 0.8f);
        }
    }

    // --- 5-Petal Japanese Sakura Flower ---
    inline void DrawSakura(ID2D1RenderTarget* rt, float cx, float cy, float radius, ID2D1Brush* brush, float stroke = 1.5f) {
        const float pi = 3.14159265f;
        for (int i = 0; i < 5; ++i) {
            float angle = i * (2.0f * pi / 5.0f) - pi / 2.0f;
            float px = cx + cosf(angle) * (radius * 0.65f);
            float py = cy + sinf(angle) * (radius * 0.65f);
            rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(px, py), radius * 0.45f, radius * 0.28f), brush, stroke);
        }
        rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), radius * 0.2f, radius * 0.2f), brush, stroke);
    }

    // --- Japanese Calligraphy Brush (Fude) ---
    inline void DrawBrush(ID2D1RenderTarget* rt, float x, float y, float s, ID2D1Brush* brush, float stroke = 1.4f) {
        float cx = x + s * 0.5f;
        float cy = y + s * 0.5f;
        // Diagonal brush handle
        rt->DrawLine(D2D1::Point2F(cx + s * 0.35f, cy - s * 0.38f), D2D1::Point2F(cx - s * 0.15f, cy + s * 0.12f), brush, stroke * 1.5f);
        // Brush ferrule & bristle tip
        rt->DrawLine(D2D1::Point2F(cx - s * 0.15f, cy + s * 0.12f), D2D1::Point2F(cx - s * 0.35f, cy + s * 0.40f), brush, stroke * 2.2f);
        rt->DrawLine(D2D1::Point2F(cx - s * 0.35f, cy + s * 0.40f), D2D1::Point2F(cx - s * 0.42f, cy + s * 0.44f), brush, stroke * 1.0f);
    }

    // --- Hokusai Stylized Wave Curl ---
    inline void DrawWave(ID2D1RenderTarget* rt, float x, float y, float s, ID2D1Brush* brush, float stroke = 1.5f) {
        float cx = x + s * 0.5f;
        float cy = y + s * 0.5f;
        // Base rolling wave curve
        rt->DrawLine(D2D1::Point2F(x + s * 0.15f, cy + s * 0.32f), D2D1::Point2F(cx, cy + s * 0.15f), brush, stroke);
        rt->DrawLine(D2D1::Point2F(cx, cy + s * 0.15f), D2D1::Point2F(cx + s * 0.35f, cy - s * 0.20f), brush, stroke * 1.3f);
        // Crest curling over
        rt->DrawLine(D2D1::Point2F(cx + s * 0.35f, cy - s * 0.20f), D2D1::Point2F(cx + s * 0.15f, cy - s * 0.28f), brush, stroke * 1.3f);
        rt->DrawLine(D2D1::Point2F(cx + s * 0.15f, cy - s * 0.28f), D2D1::Point2F(cx + s * 0.12f, cy - s * 0.08f), brush, stroke);
        // Spray dots
        rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx - s * 0.05f, cy - s * 0.30f), 1.2f, 1.2f), brush, stroke);
        rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx - s * 0.20f, cy - s * 0.18f), 1.0f, 1.0f), brush, stroke);
    }

    // --- Origami Crane (Tsuru) ---
    inline void DrawCrane(ID2D1RenderTarget* rt, float x, float y, float s, ID2D1Brush* brush, float stroke = 1.4f) {
        float cx = x + s * 0.5f;
        float cy = y + s * 0.5f;
        // Body diamond
        D2D1_POINT_2F pHead = D2D1::Point2F(cx - s * 0.38f, cy - s * 0.25f);
        D2D1_POINT_2F pNeck = D2D1::Point2F(cx - s * 0.15f, cy + s * 0.05f);
        D2D1_POINT_2F pTail = D2D1::Point2F(cx + s * 0.40f, cy - s * 0.15f);
        D2D1_POINT_2F pWingL = D2D1::Point2F(cx + s * 0.05f, cy - s * 0.40f);
        D2D1_POINT_2F pCenter = D2D1::Point2F(cx + s * 0.05f, cy + s * 0.20f);

        rt->DrawLine(pHead, pNeck, brush, stroke);
        rt->DrawLine(pNeck, pCenter, brush, stroke);
        rt->DrawLine(pCenter, pTail, brush, stroke);
        rt->DrawLine(pNeck, pWingL, brush, stroke);
        rt->DrawLine(pWingL, pTail, brush, stroke);
        rt->DrawLine(pWingL, pCenter, brush, stroke * 0.8f);
    }

    // --- Japanese Torii Gate ---
    inline void DrawTorii(ID2D1RenderTarget* rt, float x, float y, float w, float h, ID2D1Brush* brush, float stroke = 2.0f) {
        rt->DrawLine(D2D1::Point2F(x - w * 0.12f, y), D2D1::Point2F(x + w * 1.12f, y), brush, stroke * 1.4f);
        rt->DrawLine(D2D1::Point2F(x, y + h * 0.22f), D2D1::Point2F(x + w, y + h * 0.22f), brush, stroke);
        rt->DrawLine(D2D1::Point2F(x + w * 0.22f, y), D2D1::Point2F(x + w * 0.18f, y + h), brush, stroke * 1.3f);
        rt->DrawLine(D2D1::Point2F(x + w * 0.78f, y), D2D1::Point2F(x + w * 0.82f, y + h), brush, stroke * 1.3f);
        rt->DrawLine(D2D1::Point2F(x + w * 0.5f, y), D2D1::Point2F(x + w * 0.5f, y + h * 0.22f), brush, stroke);
    }

    // --- Collapsible Chevron (Arrow) ---
    inline void DrawChevron(ID2D1RenderTarget* rt, float x, float y, float s, bool expanded, ID2D1Brush* brush, float stroke = 1.4f) {
        float cx = x + s * 0.5f;
        float cy = y + s * 0.5f;
        if (expanded) {
            // Down pointing triangle / chevron ▼
            rt->DrawLine(D2D1::Point2F(cx - s * 0.35f, cy - s * 0.20f), D2D1::Point2F(cx, cy + s * 0.25f), brush, stroke);
            rt->DrawLine(D2D1::Point2F(cx, cy + s * 0.25f), D2D1::Point2F(cx + s * 0.35f, cy - s * 0.20f), brush, stroke);
        } else {
            // Right pointing triangle / chevron ▶
            rt->DrawLine(D2D1::Point2F(cx - s * 0.20f, cy - s * 0.35f), D2D1::Point2F(cx + s * 0.25f, cy), brush, stroke);
            rt->DrawLine(D2D1::Point2F(cx + s * 0.25f, cy), D2D1::Point2F(cx - s * 0.20f, cy + s * 0.35f), brush, stroke);
        }
    }

    // --- Clock / History Icon ---
    inline void DrawClock(ID2D1RenderTarget* rt, float cx, float cy, float radius, ID2D1Brush* brush, float stroke = 1.4f) {
        rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), radius, radius), brush, stroke);
        // Hour hand & minute hand
        rt->DrawLine(D2D1::Point2F(cx, cy), D2D1::Point2F(cx, cy - radius * 0.55f), brush, stroke);
        rt->DrawLine(D2D1::Point2F(cx, cy), D2D1::Point2F(cx + radius * 0.45f, cy), brush, stroke);
    }

    // --- Tree Folder Icon ---
    inline void DrawFolder(ID2D1RenderTarget* rt, float x, float y, float s, ID2D1Brush* brush, float stroke = 1.4f) {
        float tabW = s * 0.40f;
        float tabH = s * 0.22f;
        rt->DrawLine(D2D1::Point2F(x, y + tabH), D2D1::Point2F(x, y + s), brush, stroke);
        rt->DrawLine(D2D1::Point2F(x, y + s), D2D1::Point2F(x + s, y + s), brush, stroke);
        rt->DrawLine(D2D1::Point2F(x + s, y + s), D2D1::Point2F(x + s, y + tabH), brush, stroke);
        rt->DrawLine(D2D1::Point2F(x + s, y + tabH), D2D1::Point2F(x + tabW + s * 0.1f, y + tabH), brush, stroke);
        rt->DrawLine(D2D1::Point2F(x + tabW + s * 0.1f, y + tabH), D2D1::Point2F(x + tabW, y), brush, stroke);
        rt->DrawLine(D2D1::Point2F(x + tabW, y), D2D1::Point2F(x, y), brush, stroke);
        rt->DrawLine(D2D1::Point2F(x, y), D2D1::Point2F(x, y + tabH), brush, stroke);
    }

    // --- Document / File Icon ---
    inline void DrawDocument(ID2D1RenderTarget* rt, float x, float y, float s, ID2D1Brush* brush, float stroke = 1.4f) {
        float fold = s * 0.30f;
        rt->DrawLine(D2D1::Point2F(x, y), D2D1::Point2F(x + s - fold, y), brush, stroke);
        rt->DrawLine(D2D1::Point2F(x + s - fold, y), D2D1::Point2F(x + s, y + fold), brush, stroke);
        rt->DrawLine(D2D1::Point2F(x + s, y + fold), D2D1::Point2F(x + s, y + s), brush, stroke);
        rt->DrawLine(D2D1::Point2F(x + s, y + s), D2D1::Point2F(x, y + s), brush, stroke);
        rt->DrawLine(D2D1::Point2F(x, y + s), D2D1::Point2F(x, y), brush, stroke);

        rt->DrawLine(D2D1::Point2F(x + s - fold, y), D2D1::Point2F(x + s - fold, y + fold), brush, stroke * 0.8f);
        rt->DrawLine(D2D1::Point2F(x + s - fold, y + fold), D2D1::Point2F(x + s, y + fold), brush, stroke * 0.8f);

        rt->DrawLine(D2D1::Point2F(x + s * 0.22f, y + s * 0.45f), D2D1::Point2F(x + s * 0.78f, y + s * 0.45f), brush, stroke * 0.8f);
        rt->DrawLine(D2D1::Point2F(x + s * 0.22f, y + s * 0.65f), D2D1::Point2F(x + s * 0.78f, y + s * 0.65f), brush, stroke * 0.8f);
    }

    // --- Visual Studio / Code Icon (< / >) ---
    inline void DrawCodeIcon(ID2D1RenderTarget* rt, float x, float y, float s, ID2D1Brush* brush, float stroke = 1.5f) {
        float cx = x + s * 0.5f;
        float cy = y + s * 0.5f;
        // Left chevron <
        rt->DrawLine(D2D1::Point2F(cx - s * 0.15f, cy - s * 0.25f), D2D1::Point2F(cx - s * 0.35f, cy), brush, stroke);
        rt->DrawLine(D2D1::Point2F(cx - s * 0.35f, cy), D2D1::Point2F(cx - s * 0.15f, cy + s * 0.25f), brush, stroke);
        // Slash /
        rt->DrawLine(D2D1::Point2F(cx - s * 0.05f, cy + s * 0.30f), D2D1::Point2F(cx + s * 0.05f, cy - s * 0.30f), brush, stroke * 1.2f);
        // Right chevron >
        rt->DrawLine(D2D1::Point2F(cx + s * 0.15f, cy - s * 0.25f), D2D1::Point2F(cx + s * 0.35f, cy), brush, stroke);
        rt->DrawLine(D2D1::Point2F(cx + s * 0.35f, cy), D2D1::Point2F(cx + s * 0.15f, cy + s * 0.25f), brush, stroke);
    }

    // --- Capsule Toggle Switch ---
    inline void DrawToggleSwitch(ID2D1RenderTarget* rt, float x, float y, float w, float h, bool on, ID2D1Brush* trackBrush, ID2D1Brush* thumbBrush) {
        float r = h * 0.5f;
        D2D1_ROUNDED_RECT rcTrack = D2D1::RoundedRect(D2D1::RectF(x, y, x + w, y + h), r, r);
        rt->FillRoundedRectangle(&rcTrack, trackBrush);

        float thumbR = r - 2.5f;
        float thumbX = on ? (x + w - r) : (x + r);
        float thumbY = y + r;
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(thumbX, thumbY), thumbR, thumbR), thumbBrush);
    }

    // --- Mini Bar Visualizer for DEX Metrics (Sınıf, Metot, Değişken) ---
    inline void DrawMiniBarChart(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, int type, ID2D1Brush* barBrush, ID2D1Brush* bgBrush) {
        rt->FillRectangle(&r, bgBrush);
        float w = r.right - r.left;
        float h = r.bottom - r.top;
        float barW = (w - 12.0f) / 3.0f;

        float h1 = 0.4f, h2 = 0.7f, h3 = 0.9f;
        if (type == 0) { h1 = 0.5f; h2 = 0.85f; h3 = 0.4f; }
        else if (type == 1) { h1 = 0.3f; h2 = 0.6f; h3 = 0.95f; }
        else { h1 = 0.8f; h2 = 0.45f; h3 = 0.7f; }

        D2D1_RECT_F b1 = D2D1::RectF(r.left + 3.0f, r.bottom - h * h1, r.left + 3.0f + barW, r.bottom - 2.0f);
        D2D1_RECT_F b2 = D2D1::RectF(r.left + 6.0f + barW, r.bottom - h * h2, r.left + 6.0f + barW * 2.0f, r.bottom - 2.0f);
        D2D1_RECT_F b3 = D2D1::RectF(r.left + 9.0f + barW * 2.0f, r.bottom - h * h3, r.right - 3.0f, r.bottom - 2.0f);

        rt->FillRectangle(&b1, barBrush);
        rt->FillRectangle(&b2, barBrush);
        rt->FillRectangle(&b3, barBrush);
    }

    // --- Handcrafted Japanese Geometric Background Watermark (Asanoha Pattern) ---
    inline void DrawJapanesePattern(ID2D1RenderTarget* rt, const D2D1_RECT_F& bounds, ID2D1Brush* subtleBrush) {
        float step = 70.0f;
        float startX = bounds.left;
        float startY = bounds.top;

        for (float y = startY; y < bounds.bottom; y += step) {
            for (float x = startX; x < bounds.right; x += step) {
                // Subtle diamond and 6-star intersecting lines
                rt->DrawLine(D2D1::Point2F(x, y), D2D1::Point2F(x + step, y + step), subtleBrush, 0.6f);
                rt->DrawLine(D2D1::Point2F(x + step, y), D2D1::Point2F(x, y + step), subtleBrush, 0.6f);
                rt->DrawLine(D2D1::Point2F(x + step * 0.5f, y), D2D1::Point2F(x + step * 0.5f, y + step), subtleBrush, 0.6f);
                rt->DrawLine(D2D1::Point2F(x, y + step * 0.5f), D2D1::Point2F(x + step, y + step * 0.5f), subtleBrush, 0.6f);
            }
        }
    }

    // --- Standard Frameless Controls ---
    inline void DrawWinMinimize(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, ID2D1Brush* brush) {
        float cx = (r.left + r.right) * 0.5f;
        float cy = (r.top + r.bottom) * 0.5f;
        rt->DrawLine(D2D1::Point2F(cx - 5.0f, cy), D2D1::Point2F(cx + 5.0f, cy), brush, 1.2f);
    }

    inline void DrawWinMaximize(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, bool isMaximized, ID2D1Brush* brush) {
        float cx = (r.left + r.right) * 0.5f;
        float cy = (r.top + r.bottom) * 0.5f;
        if (isMaximized) {
            rt->DrawRectangle(D2D1::RectF(cx - 3.0f, cy - 5.0f, cx + 5.0f, cy + 3.0f), brush, 1.0f);
            rt->DrawRectangle(D2D1::RectF(cx - 5.0f, cy - 3.0f, cx + 3.0f, cy + 5.0f), brush, 1.0f);
        } else {
            rt->DrawRectangle(D2D1::RectF(cx - 5.0f, cy - 5.0f, cx + 5.0f, cy + 5.0f), brush, 1.2f);
        }
    }

    inline void DrawWinClose(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, ID2D1Brush* brush) {
        float cx = (r.left + r.right) * 0.5f;
        float cy = (r.top + r.bottom) * 0.5f;
        float d = 4.5f;
        rt->DrawLine(D2D1::Point2F(cx - d, cy - d), D2D1::Point2F(cx + d, cy + d), brush, 1.4f);
        rt->DrawLine(D2D1::Point2F(cx + d, cy - d), D2D1::Point2F(cx - d, cy + d), brush, 1.4f);
    }

    // --- Tab Close 'x' icon ---
    inline void DrawTabClose(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, ID2D1Brush* brush) {
        float cx = (r.left + r.right) * 0.5f;
        float cy = (r.top + r.bottom) * 0.5f;
        float d = 3.5f;
        rt->DrawLine(D2D1::Point2F(cx - d, cy - d), D2D1::Point2F(cx + d, cy + d), brush, 1.3f);
        rt->DrawLine(D2D1::Point2F(cx + d, cy - d), D2D1::Point2F(cx - d, cy + d), brush, 1.3f);
    }

    // --- Save Diskette Icon (Kaydet) ---
    inline void DrawSaveDisk(ID2D1RenderTarget* rt, float x, float y, float s, ID2D1Brush* brush, float stroke = 1.3f) {
        float cx = x + s * 0.5f;
        float cy = y + s * 0.5f;
        float half = s * 0.38f;
        // Outer floppy body
        rt->DrawRectangle(D2D1::RectF(cx - half, cy - half, cx + half, cy + half), brush, stroke);
        // Top shutter slider
        rt->DrawRectangle(D2D1::RectF(cx - half * 0.6f, cy - half, cx + half * 0.6f, cy - half * 0.1f), brush, stroke * 0.9f);
        // Label area
        rt->DrawRectangle(D2D1::RectF(cx - half * 0.65f, cy + half * 0.1f, cx + half * 0.65f, cy + half), brush, stroke * 0.9f);
    }

    // --- Rail Mode 0: Explorer (Dosya Gezgini) ---
    inline void DrawRailExplorer(ID2D1RenderTarget* rt, float cx, float cy, float s, ID2D1Brush* brush, float stroke = 1.4f) {
        float half = s * 0.42f;
        float topH = half * 0.35f;
        // Folder tab
        rt->DrawLine(D2D1::Point2F(cx - half, cy - half + topH), D2D1::Point2F(cx - half, cy + half), brush, stroke);
        rt->DrawLine(D2D1::Point2F(cx - half, cy + half), D2D1::Point2F(cx + half, cy + half), brush, stroke);
        rt->DrawLine(D2D1::Point2F(cx + half, cy + half), D2D1::Point2F(cx + half, cy - half + topH), brush, stroke);
        rt->DrawLine(D2D1::Point2F(cx + half, cy - half + topH), D2D1::Point2F(cx - half * 0.2f, cy - half + topH), brush, stroke);
        rt->DrawLine(D2D1::Point2F(cx - half * 0.2f, cy - half + topH), D2D1::Point2F(cx - half * 0.4f, cy - half), brush, stroke);
        rt->DrawLine(D2D1::Point2F(cx - half * 0.4f, cy - half), D2D1::Point2F(cx - half, cy - half), brush, stroke);
        rt->DrawLine(D2D1::Point2F(cx - half, cy - half), D2D1::Point2F(cx - half, cy - half + topH), brush, stroke);
    }

    // --- Rail Mode 1: APK Info (Paket Bilgisi) ---
    inline void DrawRailInfo(ID2D1RenderTarget* rt, float cx, float cy, float s, ID2D1Brush* brush, float stroke = 1.4f) {
        float r = s * 0.44f;
        rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r), brush, stroke);
        // 'i' dot
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy - r * 0.45f), 1.5f, 1.5f), brush);
        // 'i' stem
        rt->DrawLine(D2D1::Point2F(cx, cy - r * 0.15f), D2D1::Point2F(cx, cy + r * 0.45f), brush, stroke * 1.3f);
        // 'i' base serifs
        rt->DrawLine(D2D1::Point2F(cx - r * 0.25f, cy + r * 0.45f), D2D1::Point2F(cx + r * 0.25f, cy + r * 0.45f), brush, stroke);
    }

    // --- Rail Mode 2: APK Signer (İmzalama & Mühür) ---
    inline void DrawRailSigner(ID2D1RenderTarget* rt, float cx, float cy, float s, ID2D1Brush* brush, float stroke = 1.4f) {
        float half = s * 0.42f;
        // Traditional Japanese Hanko Stamp & Feather/Pen
        rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(cx - half, cy - half, cx + half * 0.35f, cy + half), 3.0f, 3.0f), brush, stroke);
        // Stamp seal mark inside
        rt->DrawLine(D2D1::Point2F(cx - half * 0.5f, cy - half * 0.4f), D2D1::Point2F(cx - half * 0.5f, cy + half * 0.4f), brush, stroke * 0.9f);
        rt->DrawLine(D2D1::Point2F(cx - half * 0.7f, cy), D2D1::Point2F(cx, cy), brush, stroke * 0.9f);
        // Pen feather tip angling out
        rt->DrawLine(D2D1::Point2F(cx + half * 0.8f, cy - half * 0.8f), D2D1::Point2F(cx + half * 0.2f, cy + half * 0.6f), brush, stroke * 1.3f);
        rt->DrawLine(D2D1::Point2F(cx + half * 0.2f, cy + half * 0.6f), D2D1::Point2F(cx + half * 0.1f, cy + half * 0.8f), brush, stroke * 1.5f);
    }

    // --- Rail Mode 3: Phone / ADB Install (Telefona Kur) ---
    inline void DrawRailPhone(ID2D1RenderTarget* rt, float cx, float cy, float s, ID2D1Brush* brush, float stroke = 1.4f) {
        float pw = s * 0.32f;
        float ph = s * 0.46f;
        // Smartphone outline
        rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(cx - pw, cy - ph, cx + pw, cy + ph), 3.0f, 3.0f), brush, stroke);
        // Speaker top notch
        rt->DrawLine(D2D1::Point2F(cx - pw * 0.35f, cy - ph + 3.0f), D2D1::Point2F(cx + pw * 0.35f, cy - ph + 3.0f), brush, stroke * 0.8f);
        // Screen download / install arrow pointing down
        rt->DrawLine(D2D1::Point2F(cx, cy - ph * 0.35f), D2D1::Point2F(cx, cy + ph * 0.30f), brush, stroke * 1.2f);
        rt->DrawLine(D2D1::Point2F(cx - 3.5f, cy + ph * 0.10f), D2D1::Point2F(cx, cy + ph * 0.30f), brush, stroke * 1.2f);
        rt->DrawLine(D2D1::Point2F(cx + 3.5f, cy + ph * 0.10f), D2D1::Point2F(cx, cy + ph * 0.30f), brush, stroke * 1.2f);
    }

    // --- Rail Mode 4: Settings (Ayarlar & Renkler) ---
    inline void DrawRailSettings(ID2D1RenderTarget* rt, float cx, float cy, float s, ID2D1Brush* brush, float stroke = 1.4f) {
        float r = s * 0.24f;
        rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r), brush, stroke * 1.2f);
        // 6 Gear teeth
        const float pi = 3.14159265f;
        for (int i = 0; i < 6; ++i) {
            float a = i * (pi / 3.0f);
            float x1 = cx + cosf(a) * (r + 1.0f);
            float y1 = cy + sinf(a) * (r + 1.0f);
            float x2 = cx + cosf(a) * (r + 4.5f);
            float y2 = cy + sinf(a) * (r + 4.5f);
            rt->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), brush, stroke * 1.8f);
        }
    }

    // =========================================================================
    // --- Fallback Android Mascot (Bugdroid) Vector Logo ---
    // =========================================================================
    inline void DrawAndroidLogo(ID2D1RenderTarget* rt, float cx, float cy, float radius, ID2D1Brush* brush) {
        float antStroke = KaisekiTheme::MaxF(1.5f, radius * 0.10f);

        // Antennae (Angled -30 and +30 deg)
        rt->DrawLine(D2D1::Point2F(cx - radius * 0.35f, cy - radius * 0.35f),
                     D2D1::Point2F(cx - radius * 0.62f, cy - radius * 0.72f), brush, antStroke);
        rt->DrawLine(D2D1::Point2F(cx + radius * 0.35f, cy - radius * 0.35f),
                     D2D1::Point2F(cx + radius * 0.62f, cy - radius * 0.72f), brush, antStroke);

        // Head Dome (Path geometry arc or rounded dome)
        ID2D1Factory* factory = nullptr;
        rt->GetFactory(&factory);
        if (factory) {
            ID2D1PathGeometry* path = nullptr;
            if (SUCCEEDED(factory->CreatePathGeometry(&path))) {
                ID2D1GeometrySink* sink = nullptr;
                if (SUCCEEDED(path->Open(&sink))) {
                    float hw = radius * 0.75f;
                    float hh = radius * 0.68f;
                    sink->BeginFigure(D2D1::Point2F(cx - hw, cy - radius * 0.05f), D2D1_FIGURE_BEGIN_FILLED);
                    D2D1_ARC_SEGMENT arc;
                    arc.point = D2D1::Point2F(cx + hw, cy - radius * 0.05f);
                    arc.size = D2D1::SizeF(hw, hh);
                    arc.rotationAngle = 0.0f;
                    arc.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
                    arc.arcSize = D2D1_ARC_SIZE_SMALL;
                    sink->AddArc(arc);
                    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                    sink->Close();
                    sink->Release();

                    rt->FillGeometry(path, brush);
                }
                path->Release();
            }
        }

        // Contrasting Eyes
        ID2D1SolidColorBrush* bEye = nullptr;
        rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.95f), &bEye);
        if (bEye) {
            float eyeR = radius * 0.09f;
            float eyeY = cy - radius * 0.28f;
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx - radius * 0.30f, eyeY), eyeR, eyeR), bEye);
            rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx + radius * 0.30f, eyeY), eyeR, eyeR), bEye);
            bEye->Release();
        }

        // Torso / Body below collar
        float torsoW = radius * 1.50f;
        float torsoH = radius * 0.72f;
        float torsoTop = cy + radius * 0.08f;
        D2D1_ROUNDED_RECT rTorso = D2D1::RoundedRect(
            D2D1::RectF(cx - torsoW * 0.5f, torsoTop, cx + torsoW * 0.5f, torsoTop + torsoH),
            radius * 0.18f, radius * 0.18f
        );
        rt->FillRoundedRectangle(&rTorso, brush);
    }

    // =========================================================================
    // --- Dedicated Vector Permission Icons (Zero Emojis, Pure Direct2D) ---
    // =========================================================================

    // Globe / Network / WiFi (INTERNET, ACCESS_NETWORK_STATE, ACCESS_WIFI_STATE)
    inline void DrawIconGlobe(ID2D1RenderTarget* rt, float cx, float cy, float r, ID2D1Brush* brush, float stroke = 1.2f) {
        rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r), brush, stroke);
        // Equator
        rt->DrawLine(D2D1::Point2F(cx - r, cy), D2D1::Point2F(cx + r, cy), brush, stroke * 0.85f);
        // Meridian ellipse
        rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r * 0.45f, r), brush, stroke * 0.85f);
    }

    // Camera (CAMERA, FOREGROUND_SERVICE_CAMERA)
    inline void DrawIconCamera(ID2D1RenderTarget* rt, float cx, float cy, float s, ID2D1Brush* brush, float stroke = 1.2f) {
        float halfW = s * 0.50f;
        float halfH = s * 0.36f;
        // Top pentaprism/flash bump
        rt->DrawRectangle(D2D1::RectF(cx - s * 0.22f, cy - halfH - s * 0.14f, cx + s * 0.06f, cy - halfH), brush, stroke * 0.9f);
        // Camera main body
        D2D1_ROUNDED_RECT rcBody = D2D1::RoundedRect(D2D1::RectF(cx - halfW, cy - halfH, cx + halfW, cy + halfH), 2.0f, 2.0f);
        rt->DrawRoundedRectangle(&rcBody, brush, stroke);
        // Center lens
        rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy + s * 0.02f), s * 0.20f, s * 0.20f), brush, stroke);
    }

    // Location Pin (ACCESS_FINE_LOCATION, ACCESS_COARSE_LOCATION, GPS)
    inline void DrawIconLocation(ID2D1RenderTarget* rt, float cx, float cy, float s, ID2D1Brush* brush, float stroke = 1.2f) {
        float r = s * 0.35f;
        float headY = cy - s * 0.12f;
        rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, headY), r, r), brush, stroke);
        // Teardrop V-point
        rt->DrawLine(D2D1::Point2F(cx - r * 0.85f, headY + r * 0.5f), D2D1::Point2F(cx, cy + s * 0.46f), brush, stroke);
        rt->DrawLine(D2D1::Point2F(cx + r * 0.85f, headY + r * 0.5f), D2D1::Point2F(cx, cy + s * 0.46f), brush, stroke);
        // Center hole dot
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, headY), 1.6f, 1.6f), brush);
    }

    // Storage / Folder (READ_EXTERNAL_STORAGE, WRITE_EXTERNAL_STORAGE, MANAGE_DOCUMENTS)
    inline void DrawIconStorage(ID2D1RenderTarget* rt, float cx, float cy, float s, ID2D1Brush* brush, float stroke = 1.2f) {
        float halfW = s * 0.46f;
        float halfH = s * 0.34f;
        float tabW = halfW * 0.70f;
        float tabH = halfH * 0.40f;
        // Tab top
        rt->DrawLine(D2D1::Point2F(cx - halfW, cy - halfH), D2D1::Point2F(cx - halfW + tabW, cy - halfH), brush, stroke);
        rt->DrawLine(D2D1::Point2F(cx - halfW + tabW, cy - halfH), D2D1::Point2F(cx - halfW + tabW + 2.0f, cy - halfH + tabH), brush, stroke);
        rt->DrawLine(D2D1::Point2F(cx - halfW + tabW + 2.0f, cy - halfH + tabH), D2D1::Point2F(cx + halfW, cy - halfH + tabH), brush, stroke);
        // Right, Bottom, Left
        rt->DrawLine(D2D1::Point2F(cx + halfW, cy - halfH + tabH), D2D1::Point2F(cx + halfW, cy + halfH), brush, stroke);
        rt->DrawLine(D2D1::Point2F(cx + halfW, cy + halfH), D2D1::Point2F(cx - halfW, cy + halfH), brush, stroke);
        rt->DrawLine(D2D1::Point2F(cx - halfW, cy + halfH), D2D1::Point2F(cx - halfW, cy - halfH), brush, stroke);
    }

    // Microphone (RECORD_AUDIO, FOREGROUND_SERVICE_MICROPHONE)
    inline void DrawIconMicrophone(ID2D1RenderTarget* rt, float cx, float cy, float s, ID2D1Brush* brush, float stroke = 1.2f) {
        float capW = s * 0.20f;
        float capH = s * 0.32f;
        // Mic capsule
        D2D1_ROUNDED_RECT rCap = D2D1::RoundedRect(D2D1::RectF(cx - capW, cy - s * 0.40f, cx + capW, cy + s * 0.06f), capW, capW);
        rt->DrawRoundedRectangle(&rCap, brush, stroke);
        // U-cradle
        rt->DrawLine(D2D1::Point2F(cx - s * 0.32f, cy - s * 0.10f), D2D1::Point2F(cx - s * 0.32f, cy + s * 0.12f), brush, stroke);
        rt->DrawLine(D2D1::Point2F(cx - s * 0.32f, cy + s * 0.12f), D2D1::Point2F(cx + s * 0.32f, cy + s * 0.12f), brush, stroke);
        rt->DrawLine(D2D1::Point2F(cx + s * 0.32f, cy + s * 0.12f), D2D1::Point2F(cx + s * 0.32f, cy - s * 0.10f), brush, stroke);
        // Stand and base
        rt->DrawLine(D2D1::Point2F(cx, cy + s * 0.12f), D2D1::Point2F(cx, cy + s * 0.36f), brush, stroke);
        rt->DrawLine(D2D1::Point2F(cx - s * 0.22f, cy + s * 0.36f), D2D1::Point2F(cx + s * 0.22f, cy + s * 0.36f), brush, stroke);
    }

    // Bluetooth (BLUETOOTH, BLUETOOTH_CONNECT, BLUETOOTH_SCAN)
    inline void DrawIconBluetooth(ID2D1RenderTarget* rt, float cx, float cy, float s, ID2D1Brush* brush, float stroke = 1.2f) {
        float topY = cy - s * 0.44f;
        float botY = cy + s * 0.44f;
        float rightX = cx + s * 0.30f;
        float leftX = cx - s * 0.22f;

        // Central vertical spine
        rt->DrawLine(D2D1::Point2F(cx, topY), D2D1::Point2F(cx, botY), brush, stroke);
        // Diagonal upper loop
        rt->DrawLine(D2D1::Point2F(leftX, cy - s * 0.20f), D2D1::Point2F(rightX, cy - s * 0.20f), brush, stroke * 0.9f);
        rt->DrawLine(D2D1::Point2F(cx, topY), D2D1::Point2F(rightX, cy - s * 0.20f), brush, stroke);
        rt->DrawLine(D2D1::Point2F(rightX, cy - s * 0.20f), D2D1::Point2F(cx, cy), brush, stroke);
        // Diagonal lower loop
        rt->DrawLine(D2D1::Point2F(cx, cy), D2D1::Point2F(rightX, cy + s * 0.20f), brush, stroke);
        rt->DrawLine(D2D1::Point2F(rightX, cy + s * 0.20f), D2D1::Point2F(cx, botY), brush, stroke);
        rt->DrawLine(D2D1::Point2F(leftX, cy + s * 0.20f), D2D1::Point2F(rightX, cy + s * 0.20f), brush, stroke * 0.9f);
    }

    // Phone / Call / Contacts (READ_PHONE_STATE, CALL_PHONE, READ_CONTACTS)
    inline void DrawIconPhone(ID2D1RenderTarget* rt, float cx, float cy, float s, ID2D1Brush* brush, float stroke = 1.2f) {
        float r = s * 0.40f;
        // Classic curved handset
        rt->DrawLine(D2D1::Point2F(cx - r * 0.6f, cy - r), D2D1::Point2F(cx, cy - r * 0.6f), brush, stroke * 1.5f);
        rt->DrawLine(D2D1::Point2F(cx, cy - r * 0.6f), D2D1::Point2F(cx + r * 0.4f, cy), brush, stroke * 1.1f);
        rt->DrawLine(D2D1::Point2F(cx + r * 0.4f, cy), D2D1::Point2F(cx + r, cy + r * 0.6f), brush, stroke * 1.5f);
        rt->DrawLine(D2D1::Point2F(cx + r * 0.6f, cy + r), D2D1::Point2F(cx + r, cy + r * 0.6f), brush, stroke * 1.5f);
    }

    // Fingerprint / Biometric (USE_FINGERPRINT, USE_BIOMETRIC)
    inline void DrawIconFingerprint(ID2D1RenderTarget* rt, float cx, float cy, float s, ID2D1Brush* brush, float stroke = 1.1f) {
        // Inner ridge
        rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy + 1.0f), s * 0.12f, s * 0.16f), brush, stroke);
        // Middle ridge
        rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), s * 0.24f, s * 0.30f), brush, stroke);
        // Outer ridge
        rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy - 1.0f), s * 0.38f, s * 0.44f), brush, stroke);
    }

    // Shield / Security / Alert / Admin (SYSTEM_ALERT_WINDOW, WAKE_LOCK, ADMIN, ACCOUNTS)
    inline void DrawIconShield(ID2D1RenderTarget* rt, float cx, float cy, float s, ID2D1Brush* brush, float stroke = 1.2f) {
        float halfW = s * 0.38f;
        float topY = cy - s * 0.40f;
        float midY = cy + s * 0.05f;
        float botY = cy + s * 0.46f;
        // Top bar
        rt->DrawLine(D2D1::Point2F(cx - halfW, topY), D2D1::Point2F(cx + halfW, topY), brush, stroke);
        // Upper sides
        rt->DrawLine(D2D1::Point2F(cx - halfW, topY), D2D1::Point2F(cx - halfW, midY), brush, stroke);
        rt->DrawLine(D2D1::Point2F(cx + halfW, topY), D2D1::Point2F(cx + halfW, midY), brush, stroke);
        // Lower V meeting point
        rt->DrawLine(D2D1::Point2F(cx - halfW, midY), D2D1::Point2F(cx, botY), brush, stroke);
        rt->DrawLine(D2D1::Point2F(cx + halfW, midY), D2D1::Point2F(cx, botY), brush, stroke);
        // Center tick
        rt->DrawLine(D2D1::Point2F(cx, topY + 2.0f), D2D1::Point2F(cx, botY - 2.0f), brush, stroke * 0.7f);
    }

    // Hardware / CPU / Vibrate / Boot / Services (VIBRATE, RECEIVE_BOOT_COMPLETED, FOREGROUND_SERVICE)
    inline void DrawIconHardware(ID2D1RenderTarget* rt, float cx, float cy, float s, ID2D1Brush* brush, float stroke = 1.2f) {
        float bodyHalf = s * 0.28f;
        // Central chip square
        D2D1_ROUNDED_RECT rChip = D2D1::RoundedRect(
            D2D1::RectF(cx - bodyHalf, cy - bodyHalf, cx + bodyHalf, cy + bodyHalf),
            1.5f, 1.5f
        );
        rt->DrawRoundedRectangle(&rChip, brush, stroke);

        // 8 Connector pins
        float pLen = s * 0.15f;
        // Top pins
        rt->DrawLine(D2D1::Point2F(cx - bodyHalf * 0.5f, cy - bodyHalf), D2D1::Point2F(cx - bodyHalf * 0.5f, cy - bodyHalf - pLen), brush, stroke);
        rt->DrawLine(D2D1::Point2F(cx + bodyHalf * 0.5f, cy - bodyHalf), D2D1::Point2F(cx + bodyHalf * 0.5f, cy - bodyHalf - pLen), brush, stroke);
        // Bottom pins
        rt->DrawLine(D2D1::Point2F(cx - bodyHalf * 0.5f, cy + bodyHalf), D2D1::Point2F(cx - bodyHalf * 0.5f, cy + bodyHalf + pLen), brush, stroke);
        rt->DrawLine(D2D1::Point2F(cx + bodyHalf * 0.5f, cy + bodyHalf), D2D1::Point2F(cx + bodyHalf * 0.5f, cy + bodyHalf + pLen), brush, stroke);
        // Left pins
        rt->DrawLine(D2D1::Point2F(cx - bodyHalf, cy - bodyHalf * 0.5f), D2D1::Point2F(cx - bodyHalf - pLen, cy - bodyHalf * 0.5f), brush, stroke);
        rt->DrawLine(D2D1::Point2F(cx - bodyHalf, cy + bodyHalf * 0.5f), D2D1::Point2F(cx - bodyHalf - pLen, cy + bodyHalf * 0.5f), brush, stroke);
        // Right pins
        rt->DrawLine(D2D1::Point2F(cx + bodyHalf, cy - bodyHalf * 0.5f), D2D1::Point2F(cx + bodyHalf + pLen, cy - bodyHalf * 0.5f), brush, stroke);
        rt->DrawLine(D2D1::Point2F(cx + bodyHalf, cy + bodyHalf * 0.5f), D2D1::Point2F(cx + bodyHalf + pLen, cy + bodyHalf * 0.5f), brush, stroke);
    }
}

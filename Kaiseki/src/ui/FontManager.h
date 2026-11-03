#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwrite.h>

class FontManager {
public:
    // Legacy & New text formats
    IDWriteTextFormat* tfTitle = nullptr;
    IDWriteTextFormat* tfSubtitle = nullptr;
    IDWriteTextFormat* tfSection = nullptr;
    IDWriteTextFormat* tfBody = nullptr;
    IDWriteTextFormat* tfBodyBold = nullptr;
    IDWriteTextFormat* tfSmall = nullptr;
    IDWriteTextFormat* tfBadge = nullptr;
    IDWriteTextFormat* tfTab = nullptr;
    IDWriteTextFormat* tfMono = nullptr;
    IDWriteTextFormat* tfMonoSmall = nullptr;
    IDWriteInlineObject* trimmingSign = nullptr;

    // Friendly names for Studio UI
    IDWriteTextFormat* hero = nullptr;
    IDWriteTextFormat* title = nullptr;
    IDWriteTextFormat* subhead = nullptr;
    IDWriteTextFormat* section = nullptr;
    IDWriteTextFormat* body = nullptr;
    IDWriteTextFormat* label = nullptr;
    IDWriteTextFormat* smallText = nullptr;
    IDWriteTextFormat* button = nullptr;
    IDWriteTextFormat* code = nullptr;
    IDWriteTextFormat* japanese = nullptr;
    IDWriteTextFormat* tabItalic = nullptr;
    float codeCharWidth = 6.325f;

    bool Initialize(IDWriteFactory* pFactory) {
        if (!pFactory) return false;

        const WCHAR* fontUi = L"Segoe UI";
        const WCHAR* fontMono = L"Consolas";

        pFactory->CreateTextFormat(fontUi, NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 24.0f, L"", &hero);
        pFactory->CreateTextFormat(fontUi, NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"", &title);
        pFactory->CreateTextFormat(fontUi, NULL, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"", &subhead);
        pFactory->CreateTextFormat(fontUi, NULL, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"", &section);
        pFactory->CreateTextFormat(fontUi, NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"", &body);
        pFactory->CreateTextFormat(fontUi, NULL, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11.0f, L"", &label);
        pFactory->CreateTextFormat(fontUi, NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"", &smallText);
        pFactory->CreateTextFormat(fontUi, NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.5f, L"", &button);
        pFactory->CreateTextFormat(fontMono, NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11.5f, L"", &code);
        pFactory->CreateTextFormat(fontUi, NULL, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"ja-JP", &japanese);
        pFactory->CreateTextFormat(fontUi, NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_ITALIC, DWRITE_FONT_STRETCH_NORMAL, 11.5f, L"", &tabItalic);

        if (hero) hero->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        if (title) title->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        if (subhead) subhead->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        if (section) section->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        if (label) label->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        if (smallText) smallText->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        if (button) button->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

        // Measure exact monospace character advance for Consolas 11.5pt
        if (pFactory && code) {
            IDWriteTextLayout* pMeasure = nullptr;
            const WCHAR* testChars = L"0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789";
            if (SUCCEEDED(pFactory->CreateTextLayout(testChars, 100, code, 10000.0f, 100.0f, &pMeasure)) && pMeasure) {
                DWRITE_TEXT_METRICS tm;
                if (SUCCEEDED(pMeasure->GetMetrics(&tm))) {
                    codeCharWidth = tm.widthIncludingTrailingWhitespace / 100.0f;
                }
                pMeasure->Release();
            }
        }

        // Assign aliases
        tfTitle = title;
        tfSubtitle = subhead;
        tfSection = section;
        tfBody = body;
        tfBodyBold = button;
        tfSmall = smallText;
        tfBadge = label;
        tfTab = label;
        tfMono = code;
        tfMonoSmall = smallText;

        // Trimming ellipsis
        pFactory->CreateEllipsisTrimmingSign(body, &trimmingSign);

        DWRITE_TRIMMING trimOptions = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
        if (trimmingSign) {
            if (body) body->SetTrimming(&trimOptions, trimmingSign);
            if (subhead) subhead->SetTrimming(&trimOptions, trimmingSign);
            if (smallText) smallText->SetTrimming(&trimOptions, trimmingSign);
        }

        return true;
    }

    void Shutdown() {
        if (trimmingSign) { trimmingSign->Release(); trimmingSign = nullptr; }
        if (hero) { hero->Release(); hero = nullptr; }
        if (title) { title->Release(); title = nullptr; }
        if (subhead) { subhead->Release(); subhead = nullptr; }
        if (section) { section->Release(); section = nullptr; }
        if (body) { body->Release(); body = nullptr; }
        if (label) { label->Release(); label = nullptr; }
        if (smallText) { smallText->Release(); smallText = nullptr; }
        if (button) { button->Release(); button = nullptr; }
        if (code) { code->Release(); code = nullptr; }
        if (japanese) { japanese->Release(); japanese = nullptr; }
        if (tabItalic) { tabItalic->Release(); tabItalic = nullptr; }
        tfTitle = nullptr;
        tfSubtitle = nullptr;
        tfSection = nullptr;
        tfBody = nullptr;
        tfBodyBold = nullptr;
        tfSmall = nullptr;
        tfBadge = nullptr;
    }
};

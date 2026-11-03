#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>
#include "KaisekiCommon.h"

#ifdef KAISEKIHEX_EXPORTS
#define KAISEKI_HEX_API __declspec(dllexport)
#else
#define KAISEKI_HEX_API __declspec(dllimport)
#endif

struct HexEntropyBlock {
    uint64_t offset = 0;
    size_t blockSize = 0;
    float entropy = 0.0f; // 0.0f - 8.0f (Shannon Entropy)
    std::string assessment; // "Düşük Entropi (Düz Metin)", "Orta Entropi (Makine Kodu / DEX)", "Yüksek Entropi (Sıkıştırılmış)", "Kritik Entropi (Şifreli / Packed)"
};

struct HexAnalysisResult {
    uint32_t structSize = sizeof(HexAnalysisResult);
    uint64_t totalSize = 0;
    float overallEntropy = 0.0f;
    std::string entropyVerdict;
    std::vector<HexEntropyBlock> entropyBlocks;
    std::string formattedDump;
    std::vector<std::string> extractedStrings;
    size_t asciiStringCount = 0;
};

extern "C" {
    KAISEKI_HEX_API bool KaisekiHex_FormatDump(const uint8_t* buffer, size_t size, uint64_t baseOffset, size_t maxBytes, std::string* outFormatted);
    KAISEKI_HEX_API void KaisekiHex_FreeDumpString(std::string* dump);
    KAISEKI_HEX_API bool KaisekiHex_AnalyzeBinary(const uint8_t* buffer, size_t size, HexAnalysisResult* outResult);
    KAISEKI_HEX_API bool KaisekiHex_CalculateEntropy(const uint8_t* buffer, size_t size, float* outOverallEntropy, std::vector<HexEntropyBlock>* outBlocks);
    KAISEKI_HEX_API bool KaisekiHex_SearchPattern(const uint8_t* buffer, size_t size, const char* hexPattern, std::vector<uint64_t>* outMatches);
    KAISEKI_HEX_API void KaisekiHex_FreeMatches(std::vector<uint64_t>* matches);
    KAISEKI_HEX_API bool KaisekiHex_ExtractStrings(const uint8_t* buffer, size_t size, size_t minLength, std::vector<std::string>* outStrings);
    KAISEKI_HEX_API void KaisekiHex_FreeStringList(std::vector<std::string>* strings);
    KAISEKI_HEX_API const char* KaisekiHex_GetVersion();
    KAISEKI_HEX_API void KaisekiHex_FreeResult(HexAnalysisResult* result);
}

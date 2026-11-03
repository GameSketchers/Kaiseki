#define _CRT_SECURE_NO_WARNINGS
#include "KaisekiHexApi.h"
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <map>

#include "KaisekiVersion.h"

extern "C" {

const char* KaisekiHex_GetVersion() {
#if defined(_DEBUG)
    return KAISEKI_VERSION_STRING "-debug-v143-mdd";
#else
    return KAISEKI_VERSION_STRING "-release-v143-md";
#endif
}

void KaisekiHex_FreeResult(HexAnalysisResult* result) {
    if (!result) return;
    std::vector<HexEntropyBlock>().swap(result->entropyBlocks);
    std::string().swap(result->formattedDump);
    std::vector<std::string>().swap(result->extractedStrings);
    std::string().swap(result->entropyVerdict);
}

void KaisekiHex_FreeDumpString(std::string* dump) {
    if (dump) std::string().swap(*dump);
}

void KaisekiHex_FreeMatches(std::vector<uint64_t>* matches) {
    if (matches) std::vector<uint64_t>().swap(*matches);
}

void KaisekiHex_FreeStringList(std::vector<std::string>* strings) {
    if (strings) std::vector<std::string>().swap(*strings);
}

static float ComputeBufferShannonEntropy(const uint8_t* data, size_t size) {
    if (!data || size == 0) return 0.0f;
    uint32_t counts[256] = { 0 };
    for (size_t i = 0; i < size; ++i) {
        counts[data[i]]++;
    }
    float entropy = 0.0f;
    float invSize = 1.0f / (float)size;
    for (int i = 0; i < 256; ++i) {
        if (counts[i] > 0) {
            float p = (float)counts[i] * invSize;
            entropy -= p * (log2f(p));
        }
    }
    return entropy;
}

bool KaisekiHex_CalculateEntropy(const uint8_t* buffer, size_t size, float* outOverallEntropy, std::vector<HexEntropyBlock>* outBlocks) {
    if (!buffer || size == 0) return false;

    if (outOverallEntropy) {
        *outOverallEntropy = ComputeBufferShannonEntropy(buffer, size);
    }

    if (outBlocks) {
        outBlocks->clear();
        size_t blockSize = 512;
        if (size < 1024) blockSize = 128;
        else if (size > 1024 * 1024) blockSize = 4096;

        size_t offset = 0;
        while (offset < size) {
            size_t curLen = (offset + blockSize <= size) ? blockSize : (size - offset);
            float blockEnt = ComputeBufferShannonEntropy(buffer + offset, curLen);

            HexEntropyBlock blk;
            blk.offset = (uint64_t)offset;
            blk.blockSize = curLen;
            blk.entropy = blockEnt;

            if (blockEnt >= 7.30f) {
                blk.assessment = "🚨 Kritik Yüksek (Şifreli / Packed / Obfuscated Payload)";
            } else if (blockEnt >= 6.00f) {
                blk.assessment = "📦 Yüksek (Sıkıştırılmış Veri / Resim / Deflate)";
            } else if (blockEnt >= 4.00f) {
                blk.assessment = "⚙️ Normal (Dalvik Bytecode / Derlenmiş Kod)";
            } else {
                blk.assessment = "📝 Düşük (Düz Metin / XML / Null Padding)";
            }

            outBlocks->push_back(blk);
            offset += curLen;
        }
    }
    return true;
}

bool KaisekiHex_FormatDump(const uint8_t* buffer, size_t size, uint64_t baseOffset, size_t maxBytes, std::string* outFormatted) {
    if (!buffer || size == 0 || !outFormatted) return false;
    outFormatted->clear();

    std::ostringstream ss;
    size_t limit = (size < maxBytes) ? size : maxBytes;

    ss << "// =============================================================================\n";
    ss << "// KAISEKI HEX VIEWER & BINARY INSPECTOR (BinEd Engine)\n";
    ss << "// Boyut: " << size << " bayt (" << (size / 1024) << " KB) | Taban Ofset: 0x" 
       << std::hex << std::uppercase << baseOffset << std::dec << "\n";
    ss << "// Ofset (HEX)  00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F  |ASCII Görüntüsü|\n";
    ss << "// -----------------------------------------------------------------------------\n";

    for (size_t i = 0; i < limit; i += 16) {
        char lineBuf[128];
        uint64_t curAddr = baseOffset + i;
        snprintf(lineBuf, sizeof(lineBuf), "%08llX: ", (unsigned long long)curAddr);
        ss << lineBuf;

        for (size_t j = 0; j < 16; ++j) {
            if (i + j < limit) {
                snprintf(lineBuf, sizeof(lineBuf), "%02X ", buffer[i + j]);
                ss << lineBuf;
            } else {
                ss << "   ";
            }
            if (j == 7) ss << " ";
        }

        ss << " |";
        for (size_t j = 0; j < 16 && (i + j) < limit; ++j) {
            uint8_t c = buffer[i + j];
            if (c >= 32 && c <= 126) {
                ss << (char)c;
            } else {
                ss << '.';
            }
        }
        ss << "|\n";
    }

    if (size > maxBytes) {
        ss << "\n// ... [Geriye kalan " << (size - maxBytes) << " bayt kırpıldı / UI Performansı için sınırlandırıldı] ...\n";
    }

    *outFormatted = ss.str();
    return true;
}

bool KaisekiHex_ExtractStrings(const uint8_t* buffer, size_t size, size_t minLength, std::vector<std::string>* outStrings) {
    if (!buffer || size == 0 || !outStrings) return false;
    outStrings->clear();

    std::string cur;
    for (size_t i = 0; i < size; ++i) {
        uint8_t c = buffer[i];
        if (c >= 32 && c <= 126) {
            cur.push_back((char)c);
        } else {
            if (cur.length() >= minLength) {
                outStrings->push_back(cur);
                if (outStrings->size() >= 2000) break; // Limit to 2000 strings
            }
            cur.clear();
        }
    }
    if (cur.length() >= minLength && outStrings->size() < 2000) {
        outStrings->push_back(cur);
    }
    return true;
}

bool KaisekiHex_SearchPattern(const uint8_t* buffer, size_t size, const char* hexPattern, std::vector<uint64_t>* outMatches) {
    if (!buffer || size == 0 || !hexPattern || !outMatches) return false;
    outMatches->clear();

    // Parse hex pattern string into bytes and mask (e.g. "50 4B 03 04" or "7F 45 ?? ??")
    std::vector<uint8_t> patBytes;
    std::vector<bool> patMask;

    std::istringstream iss(hexPattern);
    std::string token;
    while (iss >> token) {
        if (token == "??" || token == "?") {
            patBytes.push_back(0);
            patMask.push_back(false);
        } else {
            uint8_t b = (uint8_t)strtoul(token.c_str(), nullptr, 16);
            patBytes.push_back(b);
            patMask.push_back(true);
        }
    }

    if (patBytes.empty() || patBytes.size() > size) return false;

    size_t patLen = patBytes.size();
    for (size_t i = 0; i <= size - patLen; ++i) {
        bool match = true;
        for (size_t j = 0; j < patLen; ++j) {
            if (patMask[j] && buffer[i + j] != patBytes[j]) {
                match = false;
                break;
            }
        }
        if (match) {
            outMatches->push_back((uint64_t)i);
            if (outMatches->size() >= 500) break;
        }
    }
    return true;
}

bool KaisekiHex_AnalyzeBinary(const uint8_t* buffer, size_t size, HexAnalysisResult* outResult) {
    if (!buffer || size == 0 || !outResult) return false;
    if (outResult->structSize != sizeof(HexAnalysisResult)) return false;
    KaisekiHex_FreeResult(outResult);

    outResult->totalSize = size;
    KaisekiHex_CalculateEntropy(buffer, size, &outResult->overallEntropy, &outResult->entropyBlocks);
    KaisekiHex_FormatDump(buffer, size, 0, 65536, &outResult->formattedDump);
    KaisekiHex_ExtractStrings(buffer, size, 5, &outResult->extractedStrings);
    outResult->asciiStringCount = outResult->extractedStrings.size();

    // Determine verdict
    if (outResult->overallEntropy >= 7.20f) {
        outResult->entropyVerdict = "🚨 KRİTİK YÜKSEK ENTROPİ: Dosya şifrelenmiş veya packers (OLLVM, DexGuard, UPX) ile gizlenmiş olabilir.";
    } else if (outResult->overallEntropy >= 5.80f) {
        outResult->entropyVerdict = "📦 YÜKSEK ENTROPİ: Standart sıkıştırılmış arşiv (ZIP/Deflate) veya medya akışı.";
    } else if (outResult->overallEntropy >= 3.50f) {
        outResult->entropyVerdict = "⚙️ NORMAL ENTROPİ: Derlenmiş DEX veya yerel ARM/x86 makine kodu.";
    } else {
        outResult->entropyVerdict = "📝 DÜŞÜK ENTROPİ: Düz metin / Yapılandırılmış XML veya tekrarlayan veri blokları.";
    }

    // Build comprehensive formatted text report
    std::ostringstream ss;
    ss << outResult->formattedDump << "\n";
    ss << "// =============================================================================\n";
    ss << "// SHANNON ENTROPİ VE GİZLEME (OBFUSCATION / PACKER) GRAFİK ANALİZİ\n";
    ss << "// =============================================================================\n";
    ss << "// Genel Dosya Entropisi: " << std::fixed << std::setprecision(2) << outResult->overallEntropy << " / 8.00\n";
    ss << "// Değerlendirme:         " << outResult->entropyVerdict << "\n";
    ss << "// -----------------------------------------------------------------------------\n";
    ss << "// Blok Ofset      Boyut    Entropi    Görsel Dağılım Grafiği       Teşhis\n";
    ss << "// -----------------------------------------------------------------------------\n";

    for (size_t i = 0; i < outResult->entropyBlocks.size() && i < 60; ++i) {
        const auto& blk = outResult->entropyBlocks[i];
        char bBuf[64];
        snprintf(bBuf, sizeof(bBuf), "0x%08llX   %5zu B    %4.2f     ", (unsigned long long)blk.offset, blk.blockSize, blk.entropy);
        ss << "// " << bBuf;

        // Visual bar graph (10 segments)
        int filled = (int)(blk.entropy / 8.0f * 12.0f);
        if (filled > 12) filled = 12;
        ss << "[";
        for (int k = 0; k < filled; ++k) ss << "=";
        for (int k = filled; k < 12; ++k) ss << " ";
        ss << "]  " << blk.assessment << "\n";
    }

    if (outResult->entropyBlocks.size() > 60) {
        ss << "// ... [+" << (outResult->entropyBlocks.size() - 60) << " ek blok listelendi] ...\n";
    }

    ss << "// -----------------------------------------------------------------------------\n";
    ss << "// AYIKLANAN DİKKAT ÇEKİCİ STRINGS (İlk " << (outResult->extractedStrings.size() < 40 ? outResult->extractedStrings.size() : 40)
       << " / Toplam " << outResult->asciiStringCount << " metin):\n";
    ss << "// -----------------------------------------------------------------------------\n";
    for (size_t i = 0; i < outResult->extractedStrings.size() && i < 40; ++i) {
        ss << "//   [" << (i + 1) << "] " << outResult->extractedStrings[i] << "\n";
    }

    outResult->formattedDump = ss.str();
    return true;
}

}

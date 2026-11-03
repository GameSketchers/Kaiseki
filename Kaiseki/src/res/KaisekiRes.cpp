#define _CRT_SECURE_NO_WARNINGS
#include "KaisekiResApi.h"
#include <sstream>
#include <iomanip>
#include <map>
#include <algorithm>

#include "KaisekiVersion.h"

extern "C" {

const char* KaisekiRes_GetVersion() {
#if defined(_DEBUG)
    return KAISEKI_VERSION_STRING "-debug-v143-mdd";
#else
    return KAISEKI_VERSION_STRING "-release-v143-md";
#endif
}

void KaisekiRes_FreeOutput(ArscParseOutput* output) {
    if (!output) return;
    std::vector<std::string>().swap(output->resourceTypes);
    std::vector<ArscStringEntry>().swap(output->entries);
    std::string().swap(output->formattedStringsXml);
    std::string().swap(output->formattedReport);
    std::string().swap(output->packageName);
}

void KaisekiRes_FreeXmlOutputs(std::string* stringsXml, std::string* colorsXml, std::string* publicXml) {
    if (stringsXml) std::string().swap(*stringsXml);
    if (colorsXml) std::string().swap(*colorsXml);
    if (publicXml) std::string().swap(*publicXml);
}

static bool ReadStringPool(const uint8_t* data, size_t size, size_t offset, std::vector<std::string>& outStrings, size_t* outChunkSize) {
    return SafeReadStringPool(data, size, offset, outStrings, outChunkSize);
}

KaisekiResult KaisekiRes_ParseArsc(const uint8_t* arscData, size_t arscSize, ArscParseOutput* outOutput) {
    if (!arscData || arscSize < 12 || !outOutput) {
        return KaisekiResult::Fail(KaisekiStatus::InvalidFormat, "ARSC verisi gecersiz veya eksik");
    }
    if (outOutput->structSize != sizeof(ArscParseOutput)) {
        return KaisekiResult::Fail(KaisekiStatus::AbiMismatch, "ArscParseOutput structSize header ile uyusmuyor");
    }
    KaisekiRes_FreeOutput(outOutput);

    uint16_t rootType = *(const uint16_t*)arscData;
    if (rootType != 0x0002) { // RES_TABLE_TYPE
        return KaisekiResult::Fail(KaisekiStatus::InvalidFormat, "ARSC baslik tipi RES_TABLE_TYPE (0x0002) degil");
    }

    uint32_t rootChunkSize = *(const uint32_t*)(arscData + 4);
    uint32_t pkgCount = *(const uint32_t*)(arscData + 8);

    size_t offset = 12;
    std::vector<std::string> globalStringPool;
    std::vector<std::string> typeStringPool;
    std::vector<std::string> keyStringPool;

    // 1. Read Global String Pool
    size_t globalPoolSize = 0;
    if (ReadStringPool(arscData, arscSize, offset, globalStringPool, &globalPoolSize)) {
        offset += globalPoolSize;
    }

    outOutput->totalStringCount = globalStringPool.size();

    // 2. Iterate through Packages and Chunks
    while (offset + 8 <= arscSize) {
        uint16_t chunkType = *(const uint16_t*)(arscData + offset);
        uint16_t headerSize = *(const uint16_t*)(arscData + offset + 2);
        uint32_t chunkSize = *(const uint32_t*)(arscData + offset + 4);

        if (chunkSize < 8 || offset + chunkSize > arscSize) break;

        if (chunkType == 0x0200) { // RES_TABLE_PACKAGE_TYPE
            if (offset + 288 <= arscSize) {
                outOutput->packageId = *(const uint32_t*)(arscData + offset + 8);
                
                // Package Name: UTF-16 array of 128 chars at offset + 12
                const wchar_t* wPkg = (const wchar_t*)(arscData + offset + 12);
                std::wstring wsPkg(wPkg, wcsnlen(wPkg, 128));
                std::string sPkg;
                for (wchar_t wc : wsPkg) sPkg.push_back((char)(wc & 0xFF));
                outOutput->packageName = sPkg;

                uint32_t typeStringsOffset = *(const uint32_t*)(arscData + offset + 268);
                uint32_t keyStringsOffset = *(const uint32_t*)(arscData + offset + 276);

                if (typeStringsOffset > 0 && offset + typeStringsOffset < arscSize) {
                    size_t tsLen = 0;
                    ReadStringPool(arscData, arscSize, offset + typeStringsOffset, typeStringPool, &tsLen);
                }
                if (keyStringsOffset > 0 && offset + keyStringsOffset < arscSize) {
                    size_t ksLen = 0;
                    ReadStringPool(arscData, arscSize, offset + keyStringsOffset, keyStringPool, &ksLen);
                }
            }
        }
        offset += chunkSize;
    }

    outOutput->resourceTypes = typeStringPool;

    // 3. Synthesize human-readable entries from Key String Pool and Global String Pool
    std::ostringstream xmlStream;
    xmlStream << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    xmlStream << "<!-- Kaiseki - Android resources.arsc Decompiled Strings -->\n";
    xmlStream << "<resources>\n";

    size_t count = (keyStringPool.size() < globalStringPool.size()) ? keyStringPool.size() : globalStringPool.size();
    if (count > 5000) count = 5000;

    for (size_t i = 0; i < count; ++i) {
        ArscStringEntry entry;
        entry.keyName = !keyStringPool.empty() && i < keyStringPool.size() ? keyStringPool[i] : ("string_" + std::to_string(i));
        entry.value = !globalStringPool.empty() && i < globalStringPool.size() ? globalStringPool[i] : "";
        entry.type = "string";
        entry.resId = 0x7F000000 | (uint32_t)i;

        // Escape XML entities
        std::string escVal = entry.value;
        size_t pos = 0;
        while ((pos = escVal.find('&', pos)) != std::string::npos) { escVal.replace(pos, 1, "&amp;"); pos += 5; }
        pos = 0;
        while ((pos = escVal.find('<', pos)) != std::string::npos) { escVal.replace(pos, 1, "&lt;"); pos += 4; }
        pos = 0;
        while ((pos = escVal.find('>', pos)) != std::string::npos) { escVal.replace(pos, 1, "&gt;"); pos += 4; }

        if (!entry.keyName.empty() && !escVal.empty()) {
            xmlStream << "    <string name=\"" << entry.keyName << "\">" << escVal << "</string>\n";
            outOutput->entries.push_back(entry);
        }
    }
    xmlStream << "</resources>\n";
    outOutput->formattedStringsXml = xmlStream.str();
    outOutput->totalEntryCount = outOutput->entries.size();

    // 4. Formatted Overview Report
    std::ostringstream rep;
    rep << "================================================================================\n";
    rep << " KAISEKI - ANDROID ARSC KAYNAK TABLOSU (KaisekiRes Engine)\n";
    rep << "================================================================================\n";
    rep << "Hedef Paket:            " << (outOutput->packageName.empty() ? "com.android.app" : outOutput->packageName) << "\n";
    rep << "Paket ID (Header):      0x" << std::hex << std::uppercase << (outOutput->packageId ? outOutput->packageId : 0x7F) << std::dec << "\n";
    rep << "Toplam Havuz Metinleri: " << outOutput->totalStringCount << " string\n";
    rep << "Tanımlı Kaynak Tipleri: " << typeStringPool.size() << " tip (";
    for (size_t i = 0; i < typeStringPool.size(); ++i) {
        rep << typeStringPool[i] << (i + 1 < typeStringPool.size() ? ", " : "");
    }
    rep << ")\n";
    rep << "Anahtar (Key) Sayısı:   " << keyStringPool.size() << " sembol\n";
    rep << "Çözümlenen Değerler:    " << outOutput->totalEntryCount << " girdi\n";
    rep << "--------------------------------------------------------------------------------\n\n";
    rep << outOutput->formattedStringsXml;

    outOutput->formattedReport = rep.str();
    return KaisekiResult::Success();
}

bool KaisekiRes_DecompileXmlValues(const uint8_t* arscData, size_t arscSize, std::string* outStringsXml, std::string* outColorsXml, std::string* outPublicXml) {
    ArscParseOutput out;
    if (!KaisekiRes_ParseArsc(arscData, arscSize, &out)) return false;

    if (outStringsXml) *outStringsXml = out.formattedStringsXml;
    if (outColorsXml) {
        std::ostringstream cs;
        cs << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<resources>\n";
        for (const auto& e : out.entries) {
            if (e.keyName.find("color") != std::string::npos || e.value.find("#") != std::string::npos) {
                cs << "    <color name=\"" << e.keyName << "\">" << e.value << "</color>\n";
            }
        }
        cs << "</resources>\n";
        *outColorsXml = cs.str();
    }
    if (outPublicXml) {
        std::ostringstream ps;
        ps << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<resources>\n";
        for (const auto& e : out.entries) {
            char idBuf[32];
            snprintf(idBuf, sizeof(idBuf), "0x%08X", e.resId);
            ps << "    <public type=\"" << e.type << "\" name=\"" << e.keyName << "\" id=\"" << idBuf << "\" />\n";
        }
        ps << "</resources>\n";
        *outPublicXml = ps.str();
    }
    KaisekiRes_FreeOutput(&out);
    return true;
}

}

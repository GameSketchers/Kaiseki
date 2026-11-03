#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>
#include "KaisekiCommon.h"

#ifdef KAISEKIRES_EXPORTS
#define KAISEKI_RES_API __declspec(dllexport)
#else
#define KAISEKI_RES_API __declspec(dllimport)
#endif

struct ArscStringEntry {
    std::string keyName;
    std::string value;
    std::string type; // "string", "color", "drawable", "dimen", "id", "bool", "integer", "layout"
    uint32_t resId = 0;
};

struct ArscParseOutput {
    uint32_t structSize = sizeof(ArscParseOutput);
    std::string packageName;
    uint32_t packageId = 0;
    size_t totalStringCount = 0;
    size_t totalEntryCount = 0;
    std::vector<std::string> resourceTypes;
    std::vector<ArscStringEntry> entries;
    std::string formattedStringsXml;
    std::string formattedReport;
};

extern "C" {
    KAISEKI_RES_API KaisekiResult KaisekiRes_ParseArsc(const uint8_t* arscData, size_t arscSize, ArscParseOutput* outOutput);
    KAISEKI_RES_API bool KaisekiRes_DecompileXmlValues(const uint8_t* arscData, size_t arscSize, std::string* outStringsXml, std::string* outColorsXml, std::string* outPublicXml);
    KAISEKI_RES_API void KaisekiRes_FreeXmlOutputs(std::string* stringsXml, std::string* colorsXml, std::string* publicXml);
    KAISEKI_RES_API const char* KaisekiRes_GetVersion();
    KAISEKI_RES_API void KaisekiRes_FreeOutput(ArscParseOutput* output);
}

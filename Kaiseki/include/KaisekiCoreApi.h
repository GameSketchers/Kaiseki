#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>
#include "KaisekiCommon.h"

#if defined(KAISEKICORE_EXPORTS) || defined(APKCORE_EXPORTS)
#define KAISEKI_CORE_API __declspec(dllexport)
#else
#define KAISEKI_CORE_API __declspec(dllimport)
#endif

#ifndef APK_CORE_API
#define APK_CORE_API KAISEKI_CORE_API
#endif

struct DexDataInfo {
    std::string filename;
    std::string version;
    uint32_t fileSize = 0;
    uint32_t checksum = 0;
    std::string sha1Hex;
    uint32_t classCount = 0;
    uint32_t methodCount = 0;
    uint32_t fieldCount = 0;
    uint32_t stringCount = 0;
    uint32_t protoCount = 0;
    uint32_t typeCount = 0;
};

struct ResourceItemInfo {
    std::string path;
    std::string category;
    uint32_t uncompressedSize = 0;
    uint32_t compressedSize = 0;
    uint16_t method = 0;
};

struct CategorySummary {
    std::string category;
    int count = 0;
    uint64_t totalSize = 0;
};

struct PackageClassItem {
    std::string className;      // e.g. "MainActivity"
    std::string fullClassName;  // e.g. "com.example.app.MainActivity"
    std::string superClassName;
    int methodCount = 0;
    int fieldCount = 0;
};

struct PackageGroup {
    std::string packageName;    // e.g. "com.google.android.youtube" or "defpackage"
    std::vector<PackageClassItem> classes;
    bool isExpanded = false;
};

struct ApkSignatureInfo {
    bool hasV1 = false;
    bool hasV2 = false;
    bool hasV3 = false;
    bool hasV4 = false;
    std::string signerSubject;
    std::string signerIssuer;
    std::string certSha256;
    std::string certSha1;
    std::string formattedReport;
};

struct ApkDataModel {
    uint32_t structSize = sizeof(ApkDataModel);
    std::wstring fullPath;
    std::wstring fileName;
    uint64_t fileSize = 0;
    std::string sha256;
    std::string md5;

    // Manifest Details
    std::string packageName;
    std::string versionCode;
    std::string versionName;
    std::string minSdk;
    std::string targetSdk;
    std::string appLabel;
    std::string appIconPath;
    std::vector<std::string> permissions;
    std::vector<std::string> activities;
    std::vector<std::string> services;
    std::vector<std::string> receivers;
    std::string formattedXml;

    // Extracted Icon Bytes (Ham PNG/WebP/JPEG bayt dizisi; çizim için WIC / Direct2D ile decode edilir)
    std::vector<uint8_t> iconBytes;

    // DEX and Resources
    std::vector<DexDataInfo> dexFiles;
    uint32_t totalClasses = 0;
    uint32_t totalMethods = 0;

    std::vector<ResourceItemInfo> resources;
    std::vector<CategorySummary> categories;
    size_t totalResourceCount = 0;

    // JADX-Style Navigation & Signatures
    std::vector<PackageGroup> packages;
    ApkSignatureInfo signature;
    std::string summaryReport;

    // Native ABIs & Total Entries
    std::vector<std::string> abis;
    size_t totalZipEntries = 0;
};

struct PeImportItem {
    std::string dllName;
    std::vector<std::string> functions;
};

struct PeSectionItem {
    std::string name;
    uint32_t virtualAddress = 0;
    uint32_t virtualSize = 0;
    uint32_t rawDataSize = 0;
    uint32_t characteristics = 0;
};

struct PeAnalysisReport {
    uint32_t structSize = sizeof(PeAnalysisReport);
    bool isPe = false;
    bool is64Bit = false;
    uint16_t machine = 0;
    uint32_t entryPointRva = 0;
    uint64_t imageBase = 0;
    uint16_t subsystem = 0;
    std::vector<PeSectionItem> sections;
    std::vector<PeImportItem> imports;
    std::vector<std::string> criticalStrings;
    std::string formattedReport;
};

struct ApkRepackEntry {
    const char* entryPath;
    const uint8_t* data;
    size_t size;
};

extern "C" {
    KAISEKI_CORE_API KaisekiResult KaisekiCore_InspectFile(const wchar_t* apkPath, ApkDataModel** outModel);
    KAISEKI_CORE_API void KaisekiCore_FreeModel(ApkDataModel* model);
    KAISEKI_CORE_API bool KaisekiCore_ExtractFile(const wchar_t* apkPath, const char* entryName, uint8_t** outData, size_t* outSize);
    KAISEKI_CORE_API void KaisekiCore_FreeBuffer(uint8_t* data);
    KAISEKI_CORE_API const char* KaisekiCore_GetVersion();
    KAISEKI_CORE_API bool KaisekiCore_DecompileClass(const wchar_t* apkPath, const char* className, char** outJavaCode, char** outSmaliCode, char** outFridaScript);
    KAISEKI_CORE_API void KaisekiCore_FreeDecompiledCode(char* javaCode, char* smaliCode, char* fridaScript);
    KAISEKI_CORE_API bool KaisekiCore_ExportGradleProject(const wchar_t* apkPath, const wchar_t* outDir);
    KAISEKI_CORE_API bool KaisekiCore_IsJadxAvailable();
    KAISEKI_CORE_API bool KaisekiCore_DecompileClassWithJadx(const wchar_t* apkPath, const char* className, char** outJavaCode);
    KAISEKI_CORE_API KaisekiResult KaisekiCore_InspectPE(const wchar_t* filePath, PeAnalysisReport* outReport);
    KAISEKI_CORE_API bool KaisekiCore_LoadMappingFile(const wchar_t* mappingPath, int* outMappedCount);
    KAISEKI_CORE_API bool KaisekiCore_ApplyMappingToModel(ApkDataModel* model);
    KAISEKI_CORE_API bool KaisekiCore_RepackageApk(const wchar_t* srcApkPath, const wchar_t* outApkPath, const ApkRepackEntry* modifiedEntries, size_t modifiedCount, std::string* outLog);
    KAISEKI_CORE_API bool KaisekiCore_DecompileXml(const uint8_t* data, size_t size, std::string* outXml);
    KAISEKI_CORE_API void KaisekiCore_FreeXmlString(std::string* xml);

    // Compatibility aliases with ApkCore_*
    KAISEKI_CORE_API KaisekiResult ApkCore_InspectFile(const wchar_t* apkPath, ApkDataModel** outModel);
    KAISEKI_CORE_API void ApkCore_FreeModel(ApkDataModel* model);
    KAISEKI_CORE_API bool ApkCore_ExtractFile(const wchar_t* apkPath, const char* entryName, uint8_t** outData, size_t* outSize);
    KAISEKI_CORE_API void ApkCore_FreeBuffer(uint8_t* data);
    KAISEKI_CORE_API const char* ApkCore_GetVersion();
    KAISEKI_CORE_API bool ApkCore_DecompileClass(const wchar_t* apkPath, const char* className, char** outJavaCode, char** outSmaliCode, char** outFridaScript);
    KAISEKI_CORE_API void ApkCore_FreeDecompiledCode(char* javaCode, char* smaliCode, char* fridaScript);
    KAISEKI_CORE_API bool ApkCore_ExportGradleProject(const wchar_t* apkPath, const wchar_t* outDir);
    KAISEKI_CORE_API bool ApkCore_IsJadxAvailable();
    KAISEKI_CORE_API bool ApkCore_DecompileClassWithJadx(const wchar_t* apkPath, const char* className, char** outJavaCode);
    KAISEKI_CORE_API KaisekiResult ApkCore_InspectPE(const wchar_t* filePath, PeAnalysisReport* outReport);
    KAISEKI_CORE_API bool ApkCore_LoadMappingFile(const wchar_t* mappingPath, int* outMappedCount);
    KAISEKI_CORE_API bool ApkCore_ApplyMappingToModel(ApkDataModel* model);
    KAISEKI_CORE_API bool ApkCore_RepackageApk(const wchar_t* srcApkPath, const wchar_t* outApkPath, const ApkRepackEntry* modifiedEntries, size_t modifiedCount, std::string* outLog);
    KAISEKI_CORE_API bool ApkCore_DecompileXml(const uint8_t* data, size_t size, std::string* outXml);
    KAISEKI_CORE_API void ApkCore_FreeXmlString(std::string* xml);
}

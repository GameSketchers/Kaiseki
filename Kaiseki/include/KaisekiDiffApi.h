#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>
#include "KaisekiCommon.h"
#include "KaisekiCoreApi.h"

#ifdef KAISEKIDIFF_EXPORTS
#define KAISEKI_DIFF_API __declspec(dllexport)
#else
#define KAISEKI_DIFF_API __declspec(dllimport)
#endif

enum class DiffItemStatus {
    Added,
    Removed,
    Modified,
    Unchanged
};

struct DiffFileEntry {
    std::string path;
    DiffItemStatus status;
    uint32_t sizeA = 0;
    uint32_t sizeB = 0;
};

struct DiffClassEntry {
    std::string className;
    DiffItemStatus status;
    int methodsA = 0;
    int methodsB = 0;
};

struct ApkDiffSummary {
    uint32_t structSize = sizeof(ApkDiffSummary);
    std::wstring pathA;
    std::wstring pathB;
    std::wstring nameA;
    std::wstring nameB;

    // File Metrics
    uint64_t sizeA = 0;
    uint64_t sizeB = 0;
    int totalFilesA = 0;
    int totalFilesB = 0;
    int filesAdded = 0;
    int filesRemoved = 0;
    int filesModified = 0;

    // Manifest Differences
    std::string versionA;
    std::string versionB;
    std::string sdkA;
    std::string sdkB;
    std::vector<std::string> permsAdded;
    std::vector<std::string> permsRemoved;
    std::vector<std::string> activitiesAdded;
    std::vector<std::string> activitiesRemoved;
    std::vector<std::string> servicesAdded;
    std::vector<std::string> servicesRemoved;

    // DEX Code Differences
    int classesAdded = 0;
    int classesRemoved = 0;
    int classesModified = 0;
    std::vector<DiffClassEntry> changedClasses;

    // Signature Tampering Analysis
    bool certMatches = true;
    std::string signerA;
    std::string signerB;
    std::string sha256A;
    std::string sha256B;

    // Overall Tampering Risk Score (0 = Clean/Identical, 100 = Malicious Tampering)
    int tamperingScore = 0;
    std::string tamperingVerdict; // e.g. "GÜVENLİ (Resmi Güncelleme)" or "🚨 KRİTİK RİSK (İmza Değiştirilmiş / Modlu APK)"

    // Full Formatted Diff Report
    std::string formattedReport;
    std::vector<DiffFileEntry> fileDiffs;
};

extern "C" {
    KAISEKI_DIFF_API bool KaisekiDiff_CompareApks(const wchar_t* apkPathA, const wchar_t* apkPathB, ApkDiffSummary* outSummary);
    KAISEKI_DIFF_API bool KaisekiDiff_GenerateUnifiedDiff(const char* textA, const char* textB, std::string* outDiff);
    KAISEKI_DIFF_API void KaisekiDiff_FreeDiffString(std::string* diff);
    KAISEKI_DIFF_API const char* KaisekiDiff_GetVersion();
    KAISEKI_DIFF_API void KaisekiDiff_FreeSummary(ApkDiffSummary* summary);
}

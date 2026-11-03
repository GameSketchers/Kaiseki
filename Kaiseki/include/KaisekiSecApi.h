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

#if defined(KAISEKISEC_EXPORTS) || defined(KAISEKI_SEC_EXPORTS)
#define KAISEKI_SEC_API __declspec(dllexport)
#else
#define KAISEKI_SEC_API __declspec(dllimport)
#endif

#ifndef KAISEKISEC_API
#define KAISEKISEC_API KAISEKI_SEC_API
#endif

enum class SecSeverity {
    Critical = 0,  // Crimson Red (#FF4757)
    High = 1,      // Asahi Orange (#FF793F)
    Medium = 2,    // Kohaku Yellow (#FED330)
    Low = 3,       // Kanagawa Blue (#1E90FF)
    Info = 4       // Hasu Purple (#A55EEA)
};

struct SecFinding {
    SecSeverity severity;
    std::string title;
    std::string description;
    std::string recommendation;
    std::string location;
    std::string cwe;
};

struct PackerInfo {
    bool isPacked = false;
    std::string name;          // e.g. "360 Jiagu (360加固)", "Tencent Legu (腾讯乐固)", "Yok (Korumasız APK)"
    std::string signature;     // e.g. "libjiagu.so", "com.qihoo.util.StubApp"
    std::string encryptionAlg; // e.g. "AES-256-CBC + Native Stub", "Yok (Standart DEX)"
    std::string description;   // Detailed report string
};

struct SecReport {
    uint32_t structSize = sizeof(SecReport);
    int riskScore = 100; // 0 - 100
    std::string riskGrade = "A+"; // A+, A, B, C, D, F
    int criticalCount = 0;
    int highCount = 0;
    int mediumCount = 0;
    int lowCount = 0;
    std::vector<SecFinding> findings;
    std::string universalFridaScript;
    std::string nativeElfReport;
    PackerInfo packer;
};

extern "C" {
    KAISEKI_SEC_API bool KaisekiSec_AnalyzeApk(const ApkDataModel* model, const wchar_t* apkPath, SecReport* outReport);
    KAISEKI_SEC_API bool KaisekiSec_DetectPacker(const ApkDataModel* model, const wchar_t* apkPath, PackerInfo* outPacker);
    KAISEKI_SEC_API void KaisekiSec_FreePackerInfo(PackerInfo* packer);
    KAISEKI_SEC_API bool KaisekiSec_GenerateFridaBypass(const char* packageName, std::string* outScript);
    KAISEKI_SEC_API void KaisekiSec_FreeScript(std::string* script);
    KAISEKI_SEC_API bool KaisekiSec_InspectNativeElf(const uint8_t* elfData, size_t size, std::string* outReport);
    KAISEKI_SEC_API void KaisekiSec_FreeElfReport(std::string* report);
    KAISEKI_SEC_API const char* KaisekiSec_GetVersion();
    KAISEKI_SEC_API void KaisekiSec_FreeReport(SecReport* report);
}

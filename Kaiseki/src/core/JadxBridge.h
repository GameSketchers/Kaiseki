#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <cstdint>

struct JadxSignatureDetails {
    bool hasV1 = false;
    bool hasV2 = false;
    bool hasV3 = false;
    bool hasV4 = false;
    std::string signerSubject;
    std::string signerIssuer;
    std::string certSha256;
    std::string certSha1;
    std::string certMd5;
    std::string serialNumber;
    std::string validFrom;
    std::string validTo;
    std::string formattedReport;
};

class JadxBridge {
public:
    static JadxBridge& Get();

    // Configuration (Native Engine)
    void SetJadxDirectory(const std::wstring& dir);
    std::wstring GetJadxDirectory() const;
    std::wstring GetJavaExePath() const;
    std::wstring GetJadxJarPath() const;
    bool IsAvailable() const;

    // Native Decompiler Actions (Zero External Dependency)
    bool DecompileSingleClass(const std::wstring& apkPath, const std::string& className, std::string& outJavaCode, bool useDeobf = false);
    bool ExportGradleProject(const std::wstring& apkPath, const std::wstring& outDir, std::string* outLog = nullptr);
    bool DecompileAllClasses(const std::wstring& apkPath, const std::wstring& outDir, std::string* outLog = nullptr);

    // Native Signature Extraction (Win32 Crypt32)
    bool ExtractSignatures(const std::wstring& apkPath, JadxSignatureDetails& outSig);

    // Cache management
    void ClearCache();
    bool GetCachedClass(const std::string& className, std::string& outJavaCode) const;
    void SetCachedClass(const std::string& className, const std::string& javaCode);

private:
    JadxBridge();
    std::wstring m_jadxDir;
    std::map<std::string, std::string> m_classCache;
    std::wstring AutoDetectJadx() const;
};

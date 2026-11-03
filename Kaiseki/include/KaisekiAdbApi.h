#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <vector>
#include "KaisekiCommon.h"

#ifdef KAISEKIADB_EXPORTS
#define KAISEKI_ADB_API __declspec(dllexport)
#else
#define KAISEKI_ADB_API __declspec(dllimport)
#endif

struct AdbDeviceInfo {
    std::string serial;
    std::string model;
    std::string product;
    std::string state;      // "device", "unauthorized", "offline"
    bool isOnline = false;
};

struct AdbLogcatEntry {
    std::string timestamp;
    std::string pid;
    std::string tid;
    char level = 'I';      // 'V', 'D', 'I', 'W', 'E', 'F'
    std::string tag;
    std::string message;
};

typedef void (*AdbLogcatStreamCallback)(const AdbLogcatEntry* entry, void* userData);

extern "C" {
    KAISEKI_ADB_API bool KaisekiADB_DetectAdb(wchar_t* outPath, size_t maxLen);
    KAISEKI_ADB_API bool KaisekiADB_GetDevices(std::vector<AdbDeviceInfo>& outDevices);
    KAISEKI_ADB_API void KaisekiADB_FreeDeviceList(std::vector<AdbDeviceInfo>* devices);
    KAISEKI_ADB_API bool KaisekiADB_InstallApk(const char* serial, const wchar_t* apkPath, std::string* outLog);
    KAISEKI_ADB_API bool KaisekiADB_LaunchApp(const char* serial, const char* packageName, const char* activityName, std::string* outLog);
    KAISEKI_ADB_API bool KaisekiADB_PullApk(const char* serial, const char* packageName, const wchar_t* localDest, std::string* outLog);
    KAISEKI_ADB_API bool KaisekiADB_GetRecentLogcat(const char* serial, int maxLines, std::vector<AdbLogcatEntry>& outEntries);
    KAISEKI_ADB_API bool KaisekiADB_GetPackagePid(const char* serial, const char* packageName, std::string* outPid);
    KAISEKI_ADB_API bool KaisekiADB_ClearDeviceLogcat(const char* serial);
    KAISEKI_ADB_API void KaisekiADB_FreeLogcatEntries(std::vector<AdbLogcatEntry>* entries);
    KAISEKI_ADB_API bool KaisekiADB_StartLogcatStream(const char* serial, AdbLogcatStreamCallback callback, void* userData, void** outStreamHandle);
    KAISEKI_ADB_API void KaisekiADB_StopLogcatStream(void* streamHandle);
    KAISEKI_ADB_API bool KaisekiADB_UninstallPackage(const char* serial, const char* packageName, std::string* outLog);
    KAISEKI_ADB_API bool KaisekiADB_RestartServer(std::string* outLog);
    KAISEKI_ADB_API bool KaisekiADB_ConnectWireless(const char* ipPort, std::string* outLog);
    KAISEKI_ADB_API const char* KaisekiADB_GetVersion();
}

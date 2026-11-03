#ifndef KAISEKIADB_EXPORTS
#define KAISEKIADB_EXPORTS
#endif
#include "KaisekiAdbApi.h"
#include <sstream>
#include <fstream>
#include <algorithm>
#include <mutex>
#include <atomic>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

static std::wstring g_cachedAdbPath = L"";
static std::mutex g_adbPathMutex;

static std::wstring Utf8ToWideNoNull(const std::string& str) {
    if (str.empty()) return L"";
    int wlen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), NULL, 0);
    if (wlen <= 0) return L"";
    std::wstring result(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), &result[0], wlen);
    return result;
}

static std::wstring QuoteArg(const std::wstring& s) {
    if (s.empty()) return L"\"\"";
    if (s.find_first_of(L" \t\"") == std::wstring::npos) return s;
    std::wstring out = L"\"";
    for (wchar_t c : s) {
        if (c == L'"') out += L'\\';
        out += c;
    }
    out += L"\"";
    return out;
}

static bool ExecuteCommandPiped(const std::wstring& cmdLine, std::string& outOutput, DWORD timeoutMs = 8000) {
    outOutput.clear();

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE hRead = NULL;
    HANDLE hWrite = NULL;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        return false;
    }
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = { 0 };
    std::wstring mutableCmd = cmdLine;

    BOOL ok = CreateProcessW(
        NULL, &mutableCmd[0], NULL, NULL, TRUE,
        CREATE_NO_WINDOW, NULL, NULL, &si, &pi
    );

    CloseHandle(hWrite);

    if (!ok) {
        CloseHandle(hRead);
        return false;
    }

    char buffer[4096];
    DWORD bytesRead = 0;
    DWORD startTime = GetTickCount();

    while (true) {
        DWORD bytesAvail = 0;
        if (PeekNamedPipe(hRead, NULL, 0, NULL, &bytesAvail, NULL) && bytesAvail > 0) {
            DWORD toRead = (bytesAvail < sizeof(buffer) - 1) ? bytesAvail : sizeof(buffer) - 1;
            if (ReadFile(hRead, buffer, toRead, &bytesRead, NULL) && bytesRead > 0) {
                outOutput.append(buffer, bytesRead);
            }
        }

        DWORD waitRes = WaitForSingleObject(pi.hProcess, 50);
        if (waitRes != WAIT_TIMEOUT) {
            while (PeekNamedPipe(hRead, NULL, 0, NULL, &bytesAvail, NULL) && bytesAvail > 0) {
                DWORD toRead = (bytesAvail < sizeof(buffer) - 1) ? bytesAvail : sizeof(buffer) - 1;
                if (ReadFile(hRead, buffer, toRead, &bytesRead, NULL) && bytesRead > 0) {
                    outOutput.append(buffer, bytesRead);
                } else {
                    break;
                }
            }
            break;
        }

        if (GetTickCount() - startTime >= timeoutMs) {
            TerminateProcess(pi.hProcess, 1);
            break;
        }
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hRead);

    return true;
}

#include "KaisekiVersion.h"

const char* KaisekiADB_GetVersion() {
#if defined(_DEBUG)
    return KAISEKI_VERSION_STRING "-debug-v143-mdd";
#else
    return KAISEKI_VERSION_STRING "-release-v143-md";
#endif
}

void KaisekiADB_FreeDeviceList(std::vector<AdbDeviceInfo>* devices) {
    if (!devices) return;
    std::vector<AdbDeviceInfo>().swap(*devices);
}

void KaisekiADB_FreeLogcatEntries(std::vector<AdbLogcatEntry>* entries) {
    if (!entries) return;
    std::vector<AdbLogcatEntry>().swap(*entries);
}

bool KaisekiADB_DetectAdb(wchar_t* outPath, size_t maxLen) {
    std::lock_guard<std::mutex> lock(g_adbPathMutex);
    if (!g_cachedAdbPath.empty()) {
        if (outPath && maxLen > 0) {
            wcsncpy_s(outPath, maxLen, g_cachedAdbPath.c_str(), _TRUNCATE);
        }
        return true;
    }

    // 1. Check LOCALAPPDATA / Android / Sdk / platform-tools
    wchar_t localApp[MAX_PATH];
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", localApp, MAX_PATH) > 0) {
        std::wstring candidate = std::wstring(localApp) + L"\\Android\\Sdk\\platform-tools\\adb.exe";
        if (PathFileExistsW(candidate.c_str())) {
            g_cachedAdbPath = candidate;
            if (outPath && maxLen > 0) wcsncpy_s(outPath, maxLen, candidate.c_str(), _TRUNCATE);
            return true;
        }
    }

    // 2. Check ANDROID_HOME / platform-tools
    wchar_t androidHome[MAX_PATH];
    if (GetEnvironmentVariableW(L"ANDROID_HOME", androidHome, MAX_PATH) > 0) {
        std::wstring candidate = std::wstring(androidHome) + L"\\platform-tools\\adb.exe";
        if (PathFileExistsW(candidate.c_str())) {
            g_cachedAdbPath = candidate;
            if (outPath && maxLen > 0) wcsncpy_s(outPath, maxLen, candidate.c_str(), _TRUNCATE);
            return true;
        }
    }

    // 3. Check ANDROID_SDK_ROOT / platform-tools
    wchar_t androidSdkRoot[MAX_PATH];
    if (GetEnvironmentVariableW(L"ANDROID_SDK_ROOT", androidSdkRoot, MAX_PATH) > 0) {
        std::wstring candidate = std::wstring(androidSdkRoot) + L"\\platform-tools\\adb.exe";
        if (PathFileExistsW(candidate.c_str())) {
            g_cachedAdbPath = candidate;
            if (outPath && maxLen > 0) wcsncpy_s(outPath, maxLen, candidate.c_str(), _TRUNCATE);
            return true;
        }
    }

    // 4. Check system PATH
    wchar_t found[MAX_PATH];
    if (SearchPathW(NULL, L"adb.exe", NULL, MAX_PATH, found, NULL) > 0) {
        g_cachedAdbPath = found;
        if (outPath && maxLen > 0) wcsncpy_s(outPath, maxLen, found, _TRUNCATE);
        return true;
    }

    // 5. Check Kaiseki tools/platform-tools folder (bundled)
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    PathRemoveFileSpecW(exePath);
    std::wstring bundled = std::wstring(exePath) + L"\\tools\\platform-tools\\adb.exe";
    if (PathFileExistsW(bundled.c_str())) {
        g_cachedAdbPath = bundled;
        if (outPath && maxLen > 0) wcsncpy_s(outPath, maxLen, bundled.c_str(), _TRUNCATE);
        return true;
    }

    return false;
}

bool KaisekiADB_GetDevices(std::vector<AdbDeviceInfo>& outDevices) {
    outDevices.clear();

    wchar_t adbPath[MAX_PATH];
    if (!KaisekiADB_DetectAdb(adbPath, MAX_PATH)) {
        return false;
    }

    std::wstring cmd = QuoteArg(std::wstring(adbPath)) + L" devices -l";
    std::string output;
    if (!ExecuteCommandPiped(cmd, output, 5000)) {
        return false;
    }

    std::istringstream stream(output);
    std::string line;
    bool pastHeader = false;

    while (std::getline(stream, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
            line.pop_back();
        }
        if (line.empty()) continue;

        if (!pastHeader) {
            if (line.find("List of devices") != std::string::npos) {
                pastHeader = true;
            }
            continue;
        }

        std::istringstream lineStream(line);
        std::string serial, state;
        if (!(lineStream >> serial >> state)) continue;

        AdbDeviceInfo dev;
        dev.serial = serial;
        dev.state = state;
        dev.model = "Android Device";
        dev.product = "";

        std::string token;
        while (lineStream >> token) {
            if (token.rfind("model:", 0) == 0) {
                dev.model = token.substr(6);
                std::replace(dev.model.begin(), dev.model.end(), '_', ' ');
                if (state == "unauthorized") {
                    dev.model += " (Yetkisiz)";
                }
            } else if (token.rfind("product:", 0) == 0) {
                dev.product = token.substr(8);
            }
        }

        outDevices.push_back(dev);
    }

    return true;
}

bool KaisekiADB_InstallApk(const char* serial, const wchar_t* apkPath, std::string* outLog) {
    if (!apkPath) return false;

    wchar_t adbPath[MAX_PATH];
    KaisekiADB_DetectAdb(adbPath, MAX_PATH);

    std::wstring baseCmd = QuoteArg(std::wstring(adbPath));
    if (serial && strlen(serial) > 0) {
        baseCmd += L" -s " + QuoteArg(Utf8ToWideNoNull(serial));
    }

    // 1. First attempt: standard universal install parameters supported across all Android versions
    std::wstring cmd1 = baseCmd + L" install -r -d -t " + QuoteArg(std::wstring(apkPath));
    std::string output1;
    bool ok1 = ExecuteCommandPiped(cmd1, output1, 120000);
    if (ok1 && output1.find("Success") != std::string::npos) {
        if (outLog) *outLog = output1;
        return true;
    }

    // 2. If failed due to Android 14+ low-target-sdk block or permissions, retry with bypass flag
    if (output1.find("INSTALL_FAILED_DEPRECATED_SDK_VERSION") != std::string::npos ||
        output1.find("bypass-low-target-sdk-block") != std::string::npos ||
        output1.find("INSTALL_FAILED_USER_RESTRICTED") != std::string::npos) {
        std::wstring cmd2 = baseCmd + L" install -r -d -t -g --bypass-low-target-sdk-block " + QuoteArg(std::wstring(apkPath));
        std::string output2;
        bool ok2 = ExecuteCommandPiped(cmd2, output2, 120000);
        if (outLog) *outLog = output1 + "\n" + output2;
        return ok2 && (output2.find("Success") != std::string::npos);
    }

    if (outLog) *outLog = output1;
    return false;
}

bool KaisekiADB_LaunchApp(const char* serial, const char* packageName, const char* activityName, std::string* outLog) {
    if (!packageName) return false;

    wchar_t adbPath[MAX_PATH];
    KaisekiADB_DetectAdb(adbPath, MAX_PATH);

    std::wstring cmd = QuoteArg(std::wstring(adbPath));
    if (serial && strlen(serial) > 0) {
        cmd += L" -s " + QuoteArg(Utf8ToWideNoNull(serial));
    }

    if (activityName && strlen(activityName) > 0) {
        cmd += L" shell am start -n " + QuoteArg(Utf8ToWideNoNull(packageName) + L"/" + Utf8ToWideNoNull(activityName));
    } else {
        cmd += L" shell monkey -p " + QuoteArg(Utf8ToWideNoNull(packageName)) + L" -c android.intent.category.LAUNCHER 1";
    }

    std::string output;
    bool ok = ExecuteCommandPiped(cmd, output, 8000);
    if (outLog) *outLog = output;
    return ok;
}

bool KaisekiADB_PullApk(const char* serial, const char* packageName, const wchar_t* localDest, std::string* outLog) {
    if (!packageName || !localDest) return false;

    wchar_t adbPath[MAX_PATH];
    KaisekiADB_DetectAdb(adbPath, MAX_PATH);

    // 1. Get remote APK path via pm path <package>
    std::wstring cmdPath = QuoteArg(std::wstring(adbPath));
    if (serial && strlen(serial) > 0) {
        cmdPath += L" -s " + QuoteArg(Utf8ToWideNoNull(serial));
    }
    cmdPath += L" shell pm path " + QuoteArg(Utf8ToWideNoNull(packageName));

    std::string pathOutput;
    if (!ExecuteCommandPiped(cmdPath, pathOutput, 6000) || pathOutput.empty()) {
        if (outLog) *outLog = "Paket konumu alinamadi!";
        return false;
    }

    size_t colon = pathOutput.find("package:");
    if (colon == std::string::npos) return false;
    std::string remoteApk = pathOutput.substr(colon + 8);
    while (!remoteApk.empty() && (remoteApk.back() == '\r' || remoteApk.back() == '\n' || isspace((unsigned char)remoteApk.back()))) {
        remoteApk.pop_back();
    }

    // 2. Pull remote APK
    std::wstring cmdPull = QuoteArg(std::wstring(adbPath));
    if (serial && strlen(serial) > 0) {
        cmdPull += L" -s " + QuoteArg(Utf8ToWideNoNull(serial));
    }
    cmdPull += L" pull " + QuoteArg(Utf8ToWideNoNull(remoteApk)) + L" " + QuoteArg(std::wstring(localDest));

    std::string pullOutput;
    bool ok = ExecuteCommandPiped(cmdPull, pullOutput, 25000);
    if (outLog) *outLog = pullOutput;
    return ok;
}

bool KaisekiADB_GetPackagePid(const char* serial, const char* packageName, std::string* outPid) {
    if (!packageName || !outPid) return false;
    outPid->clear();

    wchar_t adbPath[MAX_PATH];
    if (!KaisekiADB_DetectAdb(adbPath, MAX_PATH)) return false;

    std::wstring cmd = QuoteArg(std::wstring(adbPath));
    if (serial && strlen(serial) > 0) {
        cmd += L" -s " + QuoteArg(Utf8ToWideNoNull(serial));
    }
    cmd += L" shell pidof -s " + QuoteArg(Utf8ToWideNoNull(packageName));

    std::string output;
    if (ExecuteCommandPiped(cmd, output, 3000)) {
        while (!output.empty() && (output.back() == '\r' || output.back() == '\n' || isspace((unsigned char)output.back()))) {
            output.pop_back();
        }
        if (!output.empty()) {
            *outPid = output;
            return true;
        }
    }

    // Secondary fallback: cmd activity get-package-pids
    std::wstring cmd2 = QuoteArg(std::wstring(adbPath));
    if (serial && strlen(serial) > 0) {
        cmd2 += L" -s " + QuoteArg(Utf8ToWideNoNull(serial));
    }
    cmd2 += L" shell cmd activity get-package-pids " + QuoteArg(Utf8ToWideNoNull(packageName));
    std::string output2;
    if (ExecuteCommandPiped(cmd2, output2, 3000)) {
        std::istringstream ss(output2);
        std::string p;
        if (ss >> p && !p.empty()) {
            *outPid = p;
            return true;
        }
    }
    return false;
}

bool KaisekiADB_ClearDeviceLogcat(const char* serial) {
    wchar_t adbPath[MAX_PATH];
    if (!KaisekiADB_DetectAdb(adbPath, MAX_PATH)) return false;

    std::wstring cmd = QuoteArg(std::wstring(adbPath));
    if (serial && strlen(serial) > 0) {
        cmd += L" -s " + QuoteArg(Utf8ToWideNoNull(serial));
    }
    cmd += L" logcat -c";

    std::string output;
    return ExecuteCommandPiped(cmd, output, 4000);
}

static bool ParseThreadtimeLogLine(const std::string& line, AdbLogcatEntry& out) {
    out = AdbLogcatEntry();
    out.level = 'I';

    // Threadtime format: "10-29 11:34:12.123  1234  5678 D TagName: message"
    if (line.size() > 25 && (line[2] == '-' || line[5] == ' ')) {
        out.timestamp = line.substr(0, 18);
        std::string rest = line.substr(18);
        std::istringstream restStream(rest);
        std::string pidStr, tidStr;
        char lvl = 'I';
        if (restStream >> pidStr >> tidStr >> lvl) {
            out.pid = pidStr;
            out.tid = tidStr;
            out.level = lvl;
            std::string remainder;
            std::getline(restStream, remainder);
            size_t colon = remainder.find(':');
            if (colon != std::string::npos) {
                out.tag = remainder.substr(0, colon);
                size_t start = out.tag.find_first_not_of(" \t");
                if (start != std::string::npos) out.tag = out.tag.substr(start);
                size_t end = out.tag.find_last_not_of(" \t");
                if (end != std::string::npos) out.tag = out.tag.substr(0, end + 1);

                out.message = (colon + 1 < remainder.size()) ? remainder.substr(colon + 1) : "";
                if (!out.message.empty() && out.message[0] == ' ') out.message.erase(0, 1);
            } else {
                out.message = remainder;
            }
            return true;
        }
    }

    out.message = line;
    return true;
}

bool KaisekiADB_GetRecentLogcat(const char* serial, int maxLines, std::vector<AdbLogcatEntry>& outEntries) {
    outEntries.clear();

    wchar_t adbPath[MAX_PATH];
    if (!KaisekiADB_DetectAdb(adbPath, MAX_PATH)) return false;

    int count = (maxLines > 0 && maxLines <= 2000) ? maxLines : 500;
    std::wstring cmd = QuoteArg(std::wstring(adbPath));
    if (serial && strlen(serial) > 0) {
        cmd += L" -s " + QuoteArg(Utf8ToWideNoNull(serial));
    }
    cmd += L" logcat -v threadtime -d -t " + std::to_wstring(count);

    std::string output;
    if (!ExecuteCommandPiped(cmd, output, 8000)) {
        return false;
    }

    std::istringstream ss(output);
    std::string line;
    while (std::getline(ss, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        if (line.empty()) continue;

        AdbLogcatEntry entry;
        ParseThreadtimeLogLine(line, entry);
        outEntries.push_back(entry);
    }

    return true;
}

struct LogcatStreamContext {
    std::string serial;
    AdbLogcatStreamCallback callback = nullptr;
    void* userData = nullptr;
    HANDLE hThread = NULL;
    HANDLE hExitedEvent = NULL;
    std::atomic<bool> stopRequested{false};
    std::atomic<HANDLE> hProcess{NULL};
};

static DWORD WINAPI LogcatStreamThreadProc(LPVOID lpParam) {
    LogcatStreamContext* ctx = (LogcatStreamContext*)lpParam;
    if (!ctx) return 0;

    wchar_t adb[MAX_PATH];
    if (!KaisekiADB_DetectAdb(adb, MAX_PATH)) {
        SetEvent(ctx->hExitedEvent);
        return 0;
    }

    std::wstring cmd = QuoteArg(std::wstring(adb));
    if (!ctx->serial.empty()) {
        cmd += L" -s " + QuoteArg(Utf8ToWideNoNull(ctx->serial));
    }
    cmd += L" logcat -v threadtime";

    HANDLE hRead = NULL, hWrite = NULL;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        SetEvent(ctx->hExitedEvent);
        return 0;
    }
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;

    PROCESS_INFORMATION pi = { 0 };
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(0);

    if (CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hWrite);
        hWrite = NULL;
        ctx->hProcess.store(pi.hProcess);

        char buffer[4096];
        DWORD bytesRead = 0;
        std::string lineAccum;

        while (!ctx->stopRequested.load() && ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = 0;
            lineAccum.append(buffer, bytesRead);

            size_t nl;
            while ((nl = lineAccum.find('\n')) != std::string::npos) {
                std::string line = lineAccum.substr(0, nl);
                lineAccum.erase(0, nl + 1);
                while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();

                if (!line.empty() && ctx->callback) {
                    AdbLogcatEntry entry;
                    ParseThreadtimeLogLine(line, entry);
                    ctx->callback(&entry, ctx->userData);
                }
            }
        }

        HANDLE proc = ctx->hProcess.exchange(NULL);
        if (proc) {
            TerminateProcess(proc, 0);
            CloseHandle(proc);
        }
        CloseHandle(pi.hThread);
    }

    if (hWrite) CloseHandle(hWrite);
    if (hRead) CloseHandle(hRead);
    SetEvent(ctx->hExitedEvent);
    return 0;
}

bool KaisekiADB_StartLogcatStream(const char* serial, AdbLogcatStreamCallback callback, void* userData, void** outStreamHandle) {
    if (!callback || !outStreamHandle) return false;
    LogcatStreamContext* ctx = new LogcatStreamContext();
    ctx->hExitedEvent = CreateEventW(NULL, TRUE, FALSE, NULL); // manual-reset
    ctx->serial = serial ? serial : "";
    ctx->callback = callback;
    ctx->userData = userData;
    ctx->hThread = CreateThread(NULL, 0, LogcatStreamThreadProc, ctx, 0, NULL);
    if (!ctx->hThread) {
        if (ctx->hExitedEvent) CloseHandle(ctx->hExitedEvent);
        delete ctx;
        return false;
    }
    *outStreamHandle = ctx;
    return true;
}

void KaisekiADB_StopLogcatStream(void* streamHandle) {
    if (!streamHandle) return;
    LogcatStreamContext* ctx = (LogcatStreamContext*)streamHandle;
    ctx->stopRequested.store(true);
    HANDLE proc = ctx->hProcess.load();
    if (proc) {
        TerminateProcess(proc, 0);
    }
    DWORD waitRes = WaitForSingleObject(ctx->hExitedEvent, 5000);
    if (ctx->hThread) CloseHandle(ctx->hThread);
    CloseHandle(ctx->hExitedEvent);

    if (waitRes != WAIT_OBJECT_0) {
        // Thread did not terminate in 5000ms: do NOT delete ctx to avoid UAF
        return;
    }
    delete ctx;
}

bool KaisekiADB_UninstallPackage(const char* serial, const char* packageName, std::string* outLog) {
    if (!packageName) return false;
    wchar_t adbPath[MAX_PATH];
    KaisekiADB_DetectAdb(adbPath, MAX_PATH);
    std::wstring cmd = QuoteArg(std::wstring(adbPath));
    if (serial && strlen(serial) > 0) {
        cmd += L" -s " + QuoteArg(Utf8ToWideNoNull(serial));
    }
    cmd += L" uninstall " + QuoteArg(Utf8ToWideNoNull(packageName));
    std::string output;
    bool ok = ExecuteCommandPiped(cmd, output, 30000);
    if (outLog) *outLog = output;
    return ok && (output.find("Success") != std::string::npos);
}

bool KaisekiADB_RestartServer(std::string* outLog) {
    wchar_t adbPath[MAX_PATH];
    KaisekiADB_DetectAdb(adbPath, MAX_PATH);
    std::wstring cmdKill = QuoteArg(std::wstring(adbPath)) + L" kill-server";
    std::wstring cmdStart = QuoteArg(std::wstring(adbPath)) + L" start-server";
    std::string dummy;
    ExecuteCommandPiped(cmdKill, dummy, 5000);
    bool ok = ExecuteCommandPiped(cmdStart, dummy, 10000);
    if (outLog) *outLog = dummy;
    return ok;
}

bool KaisekiADB_ConnectWireless(const char* ipPort, std::string* outLog) {
    if (!ipPort || strlen(ipPort) == 0) return false;
    wchar_t adbPath[MAX_PATH];
    KaisekiADB_DetectAdb(adbPath, MAX_PATH);
    std::wstring cmd = QuoteArg(std::wstring(adbPath)) + L" connect " + QuoteArg(Utf8ToWideNoNull(ipPort));
    std::string output;
    bool ok = ExecuteCommandPiped(cmd, output, 10000);
    if (outLog) *outLog = output;
    return ok && (output.find("connected to") != std::string::npos);
}

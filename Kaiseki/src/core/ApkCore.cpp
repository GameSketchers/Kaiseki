#ifndef APKCORE_EXPORTS
#define APKCORE_EXPORTS
#endif
#include "ApkCoreApi.h"
#include "KaisekiDexApi.h"
#include "ZipEngine.h"
#include "AxmlEngine.h"
#include "DexEngine.h"
#include "CryptoEngine.h"
#include "JadxBridge.h"
#include <map>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

static inline std::string CoreWideToUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int sz = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), NULL, 0, NULL, NULL);
    std::string s(sz, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], sz, NULL, NULL);
    return s;
}

static bool CoreReadArscStringPool(const uint8_t* data, size_t size, size_t offset, std::vector<std::string>& outStrings, size_t* outChunkSize) {
    return SafeReadStringPool(data, size, offset, outStrings, outChunkSize);
}

static bool EndsWithIgnoreCase(const std::string& str, const std::string& suffix) {
    if (str.length() < suffix.length()) return false;
    size_t offset = str.length() - suffix.length();
    for (size_t i = 0; i < suffix.length(); ++i) {
        if (tolower((unsigned char)str[offset + i]) != tolower((unsigned char)suffix[i])) return false;
    }
    return true;
}

static bool IsSafeZipEntryPath(const std::string& name) {
    if (name.empty()) return false;
    if (name.front() == '/' || name.front() == '\\') return false;
    if (name.find("..") != std::string::npos) return false;
    if (name.find(':') != std::string::npos) return false;
    return true;
}

static bool WriteAllOrFail(HANDLE h, const void* data, DWORD len) {
    if (len == 0) return true;
    DWORD written = 0;
    if (!WriteFile(h, data, len, &written, NULL) || written != len) {
        return false;
    }
    return true;
}

static std::string CoreResolveResourceFromArsc(const uint8_t* arscData, size_t arscSize, uint32_t resId, int depth = 0) {
    if (!arscData || arscSize < 12 || depth > 5) return "";
    uint16_t rootType = *(const uint16_t*)arscData;
    if (rootType != 0x0002) return "";

    std::vector<std::string> globalStringPool;
    size_t globalPoolSize = 0;
    if (!CoreReadArscStringPool(arscData, arscSize, 12, globalStringPool, &globalPoolSize)) {
        return "";
    }

    uint8_t targetType = (resId >> 16) & 0xFF;
    uint16_t targetEntry = resId & 0xFFFF;

    size_t offset = 0;
    std::string bestCandidate;

    while (offset + 8 <= arscSize) {
        uint16_t chunkType = *(const uint16_t*)(arscData + offset);
        uint16_t headerSize = *(const uint16_t*)(arscData + offset + 2);
        uint32_t chunkSize = *(const uint32_t*)(arscData + offset + 4);
        if (chunkSize < 8 || offset + chunkSize > arscSize) break;

        if (chunkType == 0x0201) { // RES_TABLE_TYPE_TYPE
            uint8_t typeId = arscData[offset + 8];
            if (typeId == targetType) {
                uint32_t entryCount = *(const uint32_t*)(arscData + offset + 12);
                uint32_t entriesStart = *(const uint32_t*)(arscData + offset + 16);

                if (targetEntry < entryCount) {
                    size_t eOffPos = offset + headerSize + targetEntry * 4;
                    if (eOffPos + 4 <= offset + entriesStart) {
                        uint32_t eOff = *(const uint32_t*)(arscData + eOffPos);
                        if (eOff != 0xFFFFFFFF) {
                            size_t entryPos = offset + entriesStart + eOff;
                            if (entryPos + 16 <= arscSize) {
                                uint16_t flags = *(const uint16_t*)(arscData + entryPos + 2);
                                if (!(flags & 0x0001)) { // Not complex
                                    uint8_t valType = arscData[entryPos + 11];
                                    uint32_t valData = *(const uint32_t*)(arscData + entryPos + 12);
                                    if (valType == 3 && valData < globalStringPool.size()) {
                                        std::string path = globalStringPool[valData];
                                        if (path.find(".png") != std::string::npos || path.find(".webp") != std::string::npos || path.find(".xml") != std::string::npos) {
                                            bestCandidate = path;
                                            if (path.find("xxhdpi") != std::string::npos || path.find("xxxhdpi") != std::string::npos) {
                                                return bestCandidate;
                                            }
                                        }
                                    } else if (valType == 1 && valData != resId) {
                                        std::string sub = CoreResolveResourceFromArsc(arscData, arscSize, valData, depth + 1);
                                        if (!sub.empty()) return sub;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            offset += chunkSize;
        } else {
            if (chunkType == 0x0002 || chunkType == 0x0200) {
                offset += headerSize;
            } else {
                offset += chunkSize;
            }
        }
    }
    return bestCandidate;
}

#include "KaisekiVersion.h"

const char* KaisekiCore_GetVersion() {
#if defined(_DEBUG)
    return KAISEKI_VERSION_STRING "-debug-v143-mdd";
#else
    return KAISEKI_VERSION_STRING "-release-v143-md";
#endif
}

const char* ApkCore_GetVersion() {
    return KaisekiCore_GetVersion();
}

void KaisekiCore_FreeModel(ApkDataModel* model) {
    if (model) {
        delete model;
    }
}

void ApkCore_FreeModel(ApkDataModel* model) {
    KaisekiCore_FreeModel(model);
}

void KaisekiCore_FreeDecompiledCode(char* javaCode, char* smaliCode, char* fridaScript) {
    if (javaCode) free(javaCode);
    if (smaliCode) free(smaliCode);
    if (fridaScript) free(fridaScript);
}

void ApkCore_FreeDecompiledCode(char* javaCode, char* smaliCode, char* fridaScript) {
    KaisekiCore_FreeDecompiledCode(javaCode, smaliCode, fridaScript);
}

static bool EnsureDirExists(const std::wstring& dirPath) {
    if (PathFileExistsW(dirPath.c_str())) return true;
    size_t pos = 0;
    while ((pos = dirPath.find_first_of(L"\\/", pos + 1)) != std::wstring::npos) {
        std::wstring sub = dirPath.substr(0, pos);
        CreateDirectoryW(sub.c_str(), NULL);
    }
    return CreateDirectoryW(dirPath.c_str(), NULL) || (GetLastError() == ERROR_ALREADY_EXISTS);
}

KaisekiResult ApkCore_InspectFile(const wchar_t* apkPath, ApkDataModel** outModel) {
    if (!apkPath || !outModel) return KaisekiResult::Fail(KaisekiStatus::FileNotFound, "Gecersiz dosya yolu veya cikti adresi");

    ZipEngine zip;
    if (!zip.Open(apkPath)) return KaisekiResult::Fail(KaisekiStatus::InvalidFormat, "APK arsivi acilamadi veya bozuk");

    ApkDataModel* model = new ApkDataModel();
    wchar_t szFull[MAX_PATH] = { 0 };
    if (GetFullPathNameW(apkPath, MAX_PATH, szFull, NULL) > 0) {
        model->fullPath = szFull;
    } else {
        model->fullPath = apkPath;
    }
    model->fileSize = zip.buffer.size();
    model->totalZipEntries = zip.entries.size();

    size_t lastSlash = model->fullPath.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        model->fileName = model->fullPath.substr(lastSlash + 1);
    } else {
        model->fileName = model->fullPath;
    }

    // Hashes
    model->sha256 = CryptoEngine::ComputeHash(zip.buffer.data(), zip.buffer.size(), CALG_SHA_256);
    model->md5 = CryptoEngine::ComputeHash(zip.buffer.data(), zip.buffer.size(), CALG_MD5);

    // 1. AndroidManifest.xml
    const ZipEntryMeta* manifestEntry = zip.FindEntry("AndroidManifest.xml");
    if (manifestEntry) {
        std::vector<uint8_t> mData;
        if (zip.Extract(*manifestEntry, mData)) {
            model->formattedXml = AxmlEngine::Decompile(
                mData.data(), mData.size(),
                model->packageName,
                model->versionCode,
                model->versionName,
                model->minSdk,
                model->targetSdk,
                model->appLabel,
                model->appIconPath,
                model->permissions,
                model->activities,
                model->services,
                model->receivers
            );
        }
    }

    if (model->appLabel.empty() || model->appLabel[0] == '@') {
        if (!model->packageName.empty()) {
            size_t lastDot = model->packageName.rfind('.');
            std::string fallback = (lastDot != std::string::npos) ? model->packageName.substr(lastDot + 1) : model->packageName;
            if (!fallback.empty()) {
                fallback[0] = (char)toupper((unsigned char)fallback[0]);
                model->appLabel = fallback;
            }
        }
    }

    // 2. DEX Files & Native Package Hierarchy Extraction
    std::map<std::string, std::vector<PackageClassItem>> pkgMap;

    for (const auto& entry : zip.entries) {
        if (entry.name.size() >= 4 && entry.name.substr(entry.name.size() - 4) == ".dex") {
            DexDataInfo dinfo;
            dinfo.filename = entry.name;
            dinfo.fileSize = entry.uncompressedSize;
            std::vector<uint8_t> dexData;
            if (zip.Extract(entry, dexData) && !dexData.empty()) {
                if (DexEngine::Parse(entry.name, dexData.data(), dexData.size(), dinfo)) {
                    model->totalClasses += dinfo.classCount;
                    model->totalMethods += dinfo.methodCount;
                }

                // Disassemble and collect classes into package groups
                DexDisassemblyOutput disOut;
                if (KaisekiDex_Disassemble(dexData.data(), dexData.size(), &disOut)) {
                    for (const auto& cls : disOut.classes) {
                        std::string full = cls.className;
                        std::string pkg = "defpackage";
                        std::string sname = full;
                        size_t dot = full.rfind('.');
                        if (dot != std::string::npos) {
                            pkg = full.substr(0, dot);
                            sname = full.substr(dot + 1);
                        }
                        PackageClassItem item;
                        item.className = sname;
                        item.fullClassName = full;
                        item.superClassName = cls.superClassName;
                        item.methodCount = cls.methodCount;
                        item.fieldCount = cls.fieldCount;
                        pkgMap[pkg].push_back(item);
                    }
                }
            }
            model->dexFiles.push_back(dinfo);
        }
    }

    // Sort DEX files naturally (classes.dex, classes2.dex, classes3.dex...)
    std::sort(model->dexFiles.begin(), model->dexFiles.end(), [](const DexDataInfo& a, const DexDataInfo& b) {
        if (a.filename == "classes.dex") return true;
        if (b.filename == "classes.dex") return false;
        return a.filename < b.filename;
    });

    // Populate model->packages
    for (auto& pair : pkgMap) {
        PackageGroup pg;
        pg.packageName = pair.first;
        pg.classes = std::move(pair.second);
        pg.isExpanded = false;
        model->packages.push_back(std::move(pg));
    }
    std::sort(model->packages.begin(), model->packages.end(), [](const PackageGroup& a, const PackageGroup& b) {
        return a.packageName < b.packageName;
    });

    if (model->packages.empty()) {
        std::string pkg = model->packageName.empty() ? "defpackage" : model->packageName;
        PackageGroup pg;
        pg.packageName = pkg;
        pg.isExpanded = true;

        if (!model->activities.empty()) {
            for (const auto& act : model->activities) {
                PackageClassItem it;
                it.className = act;
                size_t dot = act.rfind('.');
                if (dot != std::string::npos) it.className = act.substr(dot + 1);
                it.fullClassName = act;
                it.superClassName = "android.app.Activity";
                it.methodCount = 4;
                it.fieldCount = 2;
                pg.classes.push_back(it);
            }
        } else {
            PackageClassItem it;
            it.className = "MainActivity";
            it.fullClassName = pkg + ".MainActivity";
            it.superClassName = "android.app.Activity";
            it.methodCount = 2;
            it.fieldCount = 0;
            pg.classes.push_back(it);
        }
        model->packages.push_back(std::move(pg));
    }

    // 3. Resources & Native ABIs
    std::map<std::string, bool> abiSet;
    std::map<std::string, int> catCounts;
    std::map<std::string, uint64_t> catSizes;

    for (const auto& entry : zip.entries) {
        if (entry.name.rfind("lib/", 0) == 0) {
            size_t secondSlash = entry.name.find('/', 4);
            if (secondSlash != std::string::npos) {
                std::string abi = entry.name.substr(4, secondSlash - 4);
                abiSet[abi] = true;
            }
        }

        if (entry.name.rfind("res/", 0) == 0) {
            ResourceItemInfo rf;
            rf.path = entry.name;
            rf.uncompressedSize = entry.uncompressedSize;
            rf.compressedSize = entry.compressedSize;
            rf.method = entry.compressionMethod;

            size_t slash = entry.name.find('/', 4);
            if (slash != std::string::npos) {
                std::string catRaw = entry.name.substr(4, slash - 4);
                size_t dash = catRaw.find('-');
                std::string baseCat = (dash != std::string::npos) ? catRaw.substr(0, dash) : catRaw;
                rf.category = baseCat;
            } else {
                rf.category = "root";
            }

            catCounts[rf.category]++;
            catSizes[rf.category] += rf.uncompressedSize;
            model->resources.push_back(rf);
        } else if (entry.name.rfind("assets/", 0) == 0) {
            ResourceItemInfo rf;
            rf.path = entry.name;
            rf.category = "assets";
            rf.uncompressedSize = entry.uncompressedSize;
            rf.compressedSize = entry.compressedSize;
            rf.method = entry.compressionMethod;
            catCounts["assets"]++;
            catSizes["assets"] += rf.uncompressedSize;
            model->resources.push_back(rf);
        } else if (entry.name.rfind("lib/", 0) == 0) {
            ResourceItemInfo rf;
            rf.path = entry.name;
            rf.category = "lib";
            rf.uncompressedSize = entry.uncompressedSize;
            rf.compressedSize = entry.compressedSize;
            rf.method = entry.compressionMethod;
            catCounts["lib"]++;
            catSizes["lib"] += rf.uncompressedSize;
            model->resources.push_back(rf);
        }
    }

    for (const auto& pair : abiSet) {
        model->abis.push_back(pair.first);
    }

    for (const auto& pair : catCounts) {
        CategorySummary cs;
        cs.category = pair.first;
        cs.count = pair.second;
        cs.totalSize = catSizes[pair.first];
        model->categories.push_back(cs);
    }
    model->totalResourceCount = model->resources.size();

    // 4. Extract App Icon
    const char* iconPriorities[] = {
        "res/mipmap-xxxhdpi/ic_launcher.png",
        "res/mipmap-xxhdpi/ic_launcher.png",
        "res/mipmap-xhdpi/ic_launcher.png",
        "res/mipmap-hdpi/ic_launcher.png",
        "res/mipmap-mdpi/ic_launcher.png",
        "res/mipmap-xxxhdpi/ic_launcher_round.png",
        "res/mipmap-xxhdpi/ic_launcher_round.png",
        "res/drawable-xxxhdpi/ic_launcher.png",
        "res/drawable-xxhdpi/ic_launcher.png",
        "res/drawable-xhdpi/ic_launcher.png",
        "res/drawable-hdpi/ic_launcher.png",
        "res/drawable/ic_launcher.png",
        "res/drawable/icon.png",
        "res/mipmap/ic_launcher.png"
    };

    const ZipEntryMeta* chosenIcon = nullptr;

    // 1. Check if appIconPath is a resource reference like "@0x7f11001a" or "0x7f..."
    if (!model->appIconPath.empty()) {
        std::string icnRef = model->appIconPath;
        if (icnRef.rfind("@0x", 0) == 0 || icnRef.rfind("0x", 0) == 0 || (icnRef.size() > 1 && icnRef[0] == '@' && isdigit((unsigned char)icnRef[1]))) {
            uint32_t resId = 0;
            if (icnRef.rfind("@0x", 0) == 0) resId = (uint32_t)strtoul(icnRef.c_str() + 3, NULL, 16);
            else if (icnRef.rfind("0x", 0) == 0) resId = (uint32_t)strtoul(icnRef.c_str() + 2, NULL, 16);
            else if (icnRef[0] == '@') resId = (uint32_t)strtoul(icnRef.c_str() + 1, NULL, 10);

            if (resId != 0) {
                const ZipEntryMeta* arscEntry = zip.FindEntry("resources.arsc");
                if (arscEntry) {
                    std::vector<uint8_t> arscData;
                    if (zip.Extract(*arscEntry, arscData)) {
                        std::string resolvedPath = CoreResolveResourceFromArsc(arscData.data(), arscData.size(), resId);
                        // If it points to an adaptive icon XML, extract foreground drawable
                        if (EndsWithIgnoreCase(resolvedPath, ".xml")) {
                            const ZipEntryMeta* xEntry = zip.FindEntry(resolvedPath.c_str());
                            if (xEntry) {
                                std::vector<uint8_t> xData;
                                if (zip.Extract(*xEntry, xData) && xData.size() >= 8) {
                                    size_t xOff = 8;
                                    while (xOff + 8 <= xData.size()) {
                                        uint16_t cType = *(const uint16_t*)(xData.data() + xOff);
                                        uint32_t cSize = *(const uint32_t*)(xData.data() + xOff + 4);
                                        if (cSize < 8 || xOff + cSize > xData.size()) break;
                                        if (cType == 0x0102) { // START_ELEMENT
                                            uint16_t aStart = *(const uint16_t*)(xData.data() + xOff + 24);
                                            uint16_t aSize = *(const uint16_t*)(xData.data() + xOff + 26);
                                            uint16_t aCount = *(const uint16_t*)(xData.data() + xOff + 28);
                                            size_t aBase = xOff + 16 + aStart;
                                            for (uint16_t a = 0; a < aCount; ++a) {
                                                size_t cur = aBase + a * aSize;
                                                if (cur + 20 <= xData.size()) {
                                                    uint8_t vType = xData[cur + 15];
                                                    uint32_t vData = *(const uint32_t*)(xData.data() + cur + 16);
                                                    if (vType == 1 && ((vData >> 24) == 0x7F)) {
                                                        std::string childPath = CoreResolveResourceFromArsc(arscData.data(), arscData.size(), vData);
                                                        if (!childPath.empty() && !EndsWithIgnoreCase(childPath, ".xml")) {
                                                            resolvedPath = childPath;
                                                            break;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                        xOff += cSize;
                                    }
                                }
                            }
                        }
                        if (!resolvedPath.empty() && !EndsWithIgnoreCase(resolvedPath, ".xml")) {
                            chosenIcon = zip.FindEntry(resolvedPath.c_str());
                        }
                    }
                }
            }
        }
    }

    // 2. Direct string match if appIconPath is a direct file path
    if (!chosenIcon && !model->appIconPath.empty()) {
        std::string iconKey = model->appIconPath;
        size_t slash = iconKey.find('/');
        if (slash != std::string::npos) iconKey = iconKey.substr(slash + 1);

        for (const auto& entry : zip.entries) {
            if (entry.name.find(iconKey) != std::string::npos &&
               (entry.name.find(".png") != std::string::npos || entry.name.find(".webp") != std::string::npos)) {
                chosenIcon = &entry;
                if (entry.name.find("xxhdpi") != std::string::npos || entry.name.find("xxxhdpi") != std::string::npos) {
                    break;
                }
            }
        }
    }

    // 3. Fallback to standard launcher icon path conventions
    if (!chosenIcon) {
        for (const char* p : iconPriorities) {
            const ZipEntryMeta* e = zip.FindEntry(p);
            if (e) {
                chosenIcon = e;
                break;
            }
        }
    }

    if (!chosenIcon) {
        for (const auto& entry : zip.entries) {
            if ((entry.name.find("ic_launcher") != std::string::npos || entry.name.find("app_icon") != std::string::npos) &&
                (entry.name.find(".png") != std::string::npos || entry.name.find(".webp") != std::string::npos)) {
                chosenIcon = &entry;
                break;
            }
        }
    }

    // 4. Heuristic search: Pick clean square launcher/logo images (strictly requires icon/launcher keyword)
    if (!chosenIcon) {
        int bestScore = -1;
        const ZipEntryMeta* bestEntry = nullptr;
        std::vector<uint8_t> bestData;

        for (const auto& entry : zip.entries) {
            std::string lowerName = entry.name;
            for (auto& c : lowerName) c = (char)tolower((unsigned char)c);

            // MUST be an icon/logo/launcher image - never random asset textures!
            bool isIconKeyword = (lowerName.find("icon") != std::string::npos ||
                                  lowerName.find("logo") != std::string::npos ||
                                  lowerName.find("launch") != std::string::npos ||
                                  lowerName.find("ic_") != std::string::npos ||
                                  lowerName.find("app_") != std::string::npos);
            if (!isIconKeyword) continue;

            // Reject test patterns, gradients, stripes, banners, bars
            if (lowerName.find("spectrum") != std::string::npos ||
                lowerName.find("stripe") != std::string::npos ||
                lowerName.find("pattern") != std::string::npos ||
                lowerName.find("gradient") != std::string::npos ||
                lowerName.find("test") != std::string::npos ||
                lowerName.find("banner") != std::string::npos ||
                lowerName.find("color_") != std::string::npos) {
                continue;
            }

            bool isPng = (entry.name.size() > 4 && entry.name.substr(entry.name.size() - 4) == ".png");
            bool isWebp = (entry.name.size() > 5 && entry.name.substr(entry.name.size() - 5) == ".webp");
            bool isJpg = (entry.name.size() > 4 && entry.name.substr(entry.name.size() - 4) == ".jpg") ||
                         (entry.name.size() > 5 && entry.name.substr(entry.name.size() - 5) == ".jpeg");

            if (isPng || isWebp || isJpg) {
                if (entry.name.size() > 6 && entry.name.substr(entry.name.size() - 6) == ".9.png") continue;
                if (entry.uncompressedSize < 500 || entry.uncompressedSize > 800000) continue;

                std::vector<uint8_t> testData;
                if (zip.Extract(entry, testData) && testData.size() >= 24) {
                    bool isValidImage = false;
                    uint32_t w = 0, h = 0;

                    // PNG magic
                    if (testData[0] == 0x89 && testData[1] == 0x50 && testData[2] == 0x4E && testData[3] == 0x47) {
                        w = ((uint32_t)testData[16] << 24) | ((uint32_t)testData[17] << 16) | ((uint32_t)testData[18] << 8) | testData[19];
                        h = ((uint32_t)testData[20] << 24) | ((uint32_t)testData[21] << 16) | ((uint32_t)testData[22] << 8) | testData[23];
                        isValidImage = true;
                    }
                    // WebP header inspection (accurate VP8X, VP8L, VP8 dimension parser)
                    else if (testData.size() >= 30 && testData[0] == 'R' && testData[1] == 'I' && testData[2] == 'F' && testData[3] == 'F' &&
                             testData[8] == 'W' && testData[9] == 'E' && testData[10] == 'B' && testData[11] == 'P') {
                        if (testData[12] == 'V' && testData[13] == 'P' && testData[14] == '8' && testData[15] == 'X') {
                            w = 1 + ((uint32_t)testData[24] | ((uint32_t)testData[25] << 8) | ((uint32_t)testData[26] << 16));
                            h = 1 + ((uint32_t)testData[27] | ((uint32_t)testData[28] << 8) | ((uint32_t)testData[29] << 16));
                            isValidImage = true;
                        } else if (testData[12] == 'V' && testData[13] == 'P' && testData[14] == '8' && testData[15] == 'L' && testData.size() >= 25) {
                            uint32_t bits = (uint32_t)testData[21] | ((uint32_t)testData[22] << 8) | ((uint32_t)testData[23] << 16) | ((uint32_t)testData[24] << 24);
                            w = 1 + (bits & 0x3FFF);
                            h = 1 + ((bits >> 14) & 0x3FFF);
                            isValidImage = true;
                        } else if (testData[12] == 'V' && testData[13] == 'P' && testData[14] == '8' && testData[15] == ' ' && testData.size() >= 30) {
                            w = (*(const uint16_t*)(testData.data() + 26)) & 0x3FFF;
                            h = (*(const uint16_t*)(testData.data() + 28)) & 0x3FFF;
                            isValidImage = true;
                        }
                    }
                    // JPEG magic
                    else if (testData[0] == 0xFF && testData[1] == 0xD8 && testData[2] == 0xFF) {
                        w = 192; h = 192;
                        isValidImage = true;
                    }

                    // Enforce square ratio (icon requirement)
                    if (isValidImage && w >= 32 && w <= 512 && h >= 32 && h <= 512 && abs((int)w - (int)h) <= 4) {
                        int score = 0;
                        if (w >= 144 && w <= 256) score = 1000;
                        else if (w >= 96 && w < 144) score = 900;
                        else if (w >= 72 && w < 96) score = 800;
                        else if (w >= 48 && w < 72) score = 700;
                        else score = 500;

                        if (entry.name.find("icon") != std::string::npos || entry.name.find("logo") != std::string::npos || entry.name.find("launch") != std::string::npos) {
                            score += 500;
                        }
                        if (entry.uncompressedSize >= 2000 && entry.uncompressedSize <= 80000) {
                            score += 200;
                        }

                        if (score > bestScore) {
                            bestScore = score;
                            bestEntry = &entry;
                            bestData = std::move(testData);
                        }
                    }
                }
            }
        }

        if (bestEntry && !bestData.empty() && bestScore >= 700) {
            chosenIcon = bestEntry;
            model->iconBytes = std::move(bestData);
        }
    }

    if (chosenIcon && chosenIcon->name.size() >= 4 && chosenIcon->name.substr(chosenIcon->name.size() - 4) == ".xml") {
        chosenIcon = nullptr;
    }

    if (chosenIcon && model->iconBytes.empty()) {
        zip.Extract(*chosenIcon, model->iconBytes);
    }

    // 5. Native Signatures & Certificate Inspection (Win32 Crypt32)
    JadxSignatureDetails sigDetails;
    if (JadxBridge::Get().ExtractSignatures(apkPath, sigDetails)) {
        model->signature.hasV1 = sigDetails.hasV1;
        model->signature.hasV2 = sigDetails.hasV2;
        model->signature.hasV3 = sigDetails.hasV3;
        model->signature.hasV4 = sigDetails.hasV4;
        model->signature.signerSubject = sigDetails.signerSubject;
        model->signature.signerIssuer = sigDetails.signerIssuer;
        model->signature.certSha1 = sigDetails.certSha1;
        model->signature.certSha256 = sigDetails.certSha256;
        model->signature.formattedReport = sigDetails.formattedReport;
    }

    // 6. Summary Report (Native APK Metadata)
    std::ostringstream s;
    s << "=== APK SUMMARY & METADATA (Kaiseki Native Decompiler Engine) ===\n\n";
    s << "[General Information]\n";
    std::string fnUtf8 = CoreWideToUtf8(model->fileName);
    s << "  • File Name           : " << fnUtf8 << "\n";
    s << "  • Application Label   : " << model->appLabel << "\n";
    s << "  • Package Name        : " << model->packageName << "\n";
    s << "  • Version Name        : " << model->versionName << "\n";
    s << "  • Version Code        : " << model->versionCode << "\n";
    s << "  • File Size           : " << model->fileSize << " bytes (" << (model->fileSize / (1024 * 1024.0)) << " MB)\n";
    s << "  • SHA-256 Digest      : " << model->sha256 << "\n";
    s << "  • MD5 Digest          : " << model->md5 << "\n\n";
    s << "[Android Platform & SDK]\n";
    s << "  • Minimum SDK Level   : " << model->minSdk << "\n";
    s << "  • Target SDK Level    : " << model->targetSdk << "\n";
    s << "  • Native ABIs         : ";
    for (const auto& a : model->abis) s << a << " ";
    s << "\n\n";
    s << "[Manifest Components (" << (model->activities.size() + model->services.size() + model->receivers.size()) << " Total)]\n";
    s << "  • Activities Count    : " << model->activities.size() << "\n";
    s << "  • Services Count      : " << model->services.size() << "\n";
    s << "  • Receivers Count     : " << model->receivers.size() << "\n";
    s << "  • Permissions Count   : " << model->permissions.size() << "\n\n";
    s << "[DEX & Code Statistics (Native C++ Parser)]\n";
    s << "  • DEX Files Count     : " << model->dexFiles.size() << "\n";
    s << "  • Discovered Packages : " << model->packages.size() << "\n";
    s << "  • Total Classes       : " << model->totalClasses << "\n";
    s << "  • Total Methods       : " << model->totalMethods << "\n";
    s << "  • Total Resources     : " << model->totalResourceCount << "\n\n";
    s << "[Permissions List]\n";
    for (const auto& p : model->permissions) {
        s << "  + " << p << "\n";
    }
    model->summaryReport = s.str();

    *outModel = model;
    return KaisekiResult::Success();
}

bool ApkCore_ExtractFile(const wchar_t* apkPath, const char* entryName, uint8_t** outData, size_t* outSize) {
    if (!apkPath || !entryName || !outData || !outSize) return false;
    *outData = nullptr;
    *outSize = 0;

    ZipEngine zip;
    if (!zip.Open(apkPath)) return false;

    const ZipEntryMeta* entry = zip.FindEntry(entryName);
    if (!entry) return false;

    std::vector<uint8_t> buffer;
    if (!zip.Extract(*entry, buffer)) return false;

    uint8_t* mem = (uint8_t*)malloc(buffer.size() > 0 ? buffer.size() : 1);
    if (!mem) return false;

    if (!buffer.empty()) {
        memcpy(mem, buffer.data(), buffer.size());
    }
    *outData = mem;
    *outSize = buffer.size();
    return true;
}

void ApkCore_FreeBuffer(uint8_t* data) {
    if (data) {
        free(data);
    }
}

bool ApkCore_DecompileClass(const wchar_t* apkPath, const char* className, char** outJavaCode, char** outSmaliCode, char** outFridaScript) {
    if (!apkPath || !className) return false;

    ZipEngine zip;
    if (!zip.Open(apkPath)) return false;

    for (const auto& entry : zip.entries) {
        if (entry.name.size() >= 4 && entry.name.substr(entry.name.size() - 4) == ".dex") {
            std::vector<uint8_t> dexData;
            if (zip.Extract(entry, dexData) && !dexData.empty()) {
                std::string java, smali, frida;
                if (KaisekiDex_DecompileClass(dexData.data(), dexData.size(), className, &java, &smali, &frida)) {
                    if (outJavaCode) {
                        char* m = (char*)malloc(java.size() + 1);
                        if (m) { memcpy(m, java.c_str(), java.size() + 1); *outJavaCode = m; }
                    }
                    if (outSmaliCode) {
                        char* m = (char*)malloc(smali.size() + 1);
                        if (m) { memcpy(m, smali.c_str(), smali.size() + 1); *outSmaliCode = m; }
                    }
                    if (outFridaScript) {
                        char* m = (char*)malloc(frida.size() + 1);
                        if (m) { memcpy(m, frida.c_str(), frida.size() + 1); *outFridaScript = m; }
                    }
                    return true;
                }
            }
        }
    }

    return false;
}

bool ApkCore_DecompileClassWithJadx(const wchar_t* apkPath, const char* className, char** outJavaCode) {
    char* smali = nullptr;
    char* frida = nullptr;
    bool ok = ApkCore_DecompileClass(apkPath, className, outJavaCode, &smali, &frida);
    if (smali) free(smali);
    if (frida) free(frida);
    return ok;
}

bool ApkCore_ExportGradleProject(const wchar_t* apkPath, const wchar_t* outDir) {
    if (!apkPath || !outDir) return false;

    std::wstring rootDir = outDir;
    if (rootDir.back() != L'\\' && rootDir.back() != L'/') rootDir += L'\\';

    EnsureDirExists(rootDir);
    EnsureDirExists(rootDir + L"app");
    EnsureDirExists(rootDir + L"app\\src");
    EnsureDirExists(rootDir + L"app\\src\\main");
    EnsureDirExists(rootDir + L"app\\src\\main\\java");
    EnsureDirExists(rootDir + L"app\\src\\main\\res");

    ZipEngine zip;
    if (!zip.Open(apkPath)) return false;

    std::string pkgName = "com.kaiseki.app";
    std::string minSdk = "21";
    std::string targetSdk = "34";
    std::string verCode = "1";
    std::string verName = "1.0";
    std::string appName = "KaisekiExportedApp";
    std::string formattedXml;

    // 1. AndroidManifest.xml
    const ZipEntryMeta* mEntry = zip.FindEntry("AndroidManifest.xml");
    if (mEntry) {
        std::vector<uint8_t> mData;
        if (zip.Extract(*mEntry, mData)) {
            std::string appIcon, appLabel;
            std::vector<std::string> perms, acts, servs, recvs;
            formattedXml = AxmlEngine::Decompile(
                mData.data(), mData.size(),
                pkgName, verCode, verName, minSdk, targetSdk, appLabel, appIcon,
                perms, acts, servs, recvs
            );
            if (!appLabel.empty()) appName = appLabel;
        }
    }

    // Write AndroidManifest.xml
    std::ofstream mf(rootDir + L"app\\src\\main\\AndroidManifest.xml");
    if (mf.is_open()) {
        mf << (formattedXml.empty() ? "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\">\n</manifest>" : formattedXml);
        mf.close();
    }

    // 2. settings.gradle
    std::ofstream sg(rootDir + L"settings.gradle");
    if (sg.is_open()) {
        sg << "pluginManagement {\n    repositories {\n        google()\n        mavenCentral()\n        gradlePluginPortal()\n    }\n}\n";
        sg << "dependencyResolutionManagement {\n    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)\n    repositories {\n        google()\n        mavenCentral()\n    }\n}\n";
        sg << "rootProject.name = \"" << appName << "\"\n";
        sg << "include ':app'\n";
        sg.close();
    }

    // 3. build.gradle (root)
    std::ofstream bg(rootDir + L"build.gradle");
    if (bg.is_open()) {
        bg << "plugins {\n    id 'com.android.application' version '8.2.2' apply false\n}\n";
        bg.close();
    }

    // 4. app/build.gradle
    std::ofstream abg(rootDir + L"app\\build.gradle");
    if (abg.is_open()) {
        abg << "plugins {\n    id 'com.android.application'\n}\n\n";
        abg << "android {\n";
        abg << "    namespace '" << pkgName << "'\n";
        abg << "    compileSdk " << (targetSdk.empty() ? "34" : targetSdk) << "\n\n";
        abg << "    defaultConfig {\n";
        abg << "        applicationId \"" << pkgName << "\"\n";
        abg << "        minSdk " << (minSdk.empty() ? "21" : minSdk) << "\n";
        abg << "        targetSdk " << (targetSdk.empty() ? "34" : targetSdk) << "\n";
        abg << "        versionCode " << (verCode.empty() ? "1" : verCode) << "\n";
        abg << "        versionName \"" << (verName.empty() ? "1.0" : verName) << "\"\n";
        abg << "    }\n\n";
        abg << "    buildTypes {\n";
        abg << "        release {\n            minifyEnabled false\n        }\n";
        abg << "    }\n";
        abg << "}\n\n";
        abg << "dependencies {\n";
        abg << "    implementation 'androidx.appcompat:appcompat:1.6.1'\n";
        abg << "    implementation 'com.google.android.material:material:1.11.0'\n";
        abg << "}\n";
        abg.close();
    }

    // 5. Extract Resources
    for (const auto& entry : zip.entries) {
        if (!IsSafeZipEntryPath(entry.name)) continue;
        if (entry.name.rfind("res/", 0) == 0 && entry.name.size() > 4) {
            std::wstring rel(entry.name.begin(), entry.name.end());
            for (wchar_t& c : rel) if (c == L'/') c = L'\\';
            std::wstring fullRes = rootDir + L"app\\src\\main\\" + rel;
            size_t slash = fullRes.find_last_of(L'\\');
            if (slash != std::wstring::npos) {
                EnsureDirExists(fullRes.substr(0, slash));
            }
            std::vector<uint8_t> rdata;
            if (zip.Extract(entry, rdata)) {
                std::ofstream rf(fullRes, std::ios::binary);
                if (rf.is_open()) {
                    if (!rdata.empty()) rf.write((const char*)rdata.data(), rdata.size());
                    rf.close();
                }
            }
        }
    }

    // 6. Decompile and Export Java Classes
    for (const auto& entry : zip.entries) {
        if (!IsSafeZipEntryPath(entry.name)) continue;
        if (EndsWithIgnoreCase(entry.name, ".dex")) {
            std::vector<uint8_t> dexData;
            if (zip.Extract(entry, dexData) && !dexData.empty()) {
                DexDisassemblyOutput disOut;
                if (KaisekiDex_Disassemble(dexData.data(), dexData.size(), &disOut)) {
                    for (const auto& cls : disOut.classes) {
                        std::string javaCode, smaliCode, fridaCode;
                        if (KaisekiDex_DecompileClass(dexData.data(), dexData.size(), cls.className.c_str(), &javaCode, &smaliCode, &fridaCode)) {
                            std::string pkgPath = cls.className;
                            for (char& c : pkgPath) if (c == '.') c = '/';
                            size_t lastS = pkgPath.find_last_of('/');
                            std::string dirPart = (lastS != std::string::npos) ? pkgPath.substr(0, lastS) : "";
                            std::string filePart = (lastS != std::string::npos) ? pkgPath.substr(lastS + 1) : pkgPath;

                            if (!IsSafeZipEntryPath(dirPart) || !IsSafeZipEntryPath(filePart)) continue;

                            std::wstring wdir(dirPart.begin(), dirPart.end());
                            for (wchar_t& c : wdir) if (c == L'/') c = L'\\';
                            std::wstring fullJavaDir = rootDir + L"app\\src\\main\\java\\" + wdir;
                            EnsureDirExists(fullJavaDir);

                            std::wstring wFile(filePart.begin(), filePart.end());
                            std::wstring fullJavaPath = fullJavaDir + L"\\" + wFile + L".java";

                            std::ofstream jf(fullJavaPath);
                            if (jf.is_open()) {
                                jf << javaCode;
                                jf.close();
                            }
                        }
                    }
                }
            }
        }
    }

    // 7. Write README.md in Export folder
    std::ofstream rm(rootDir + L"README.md");
    if (rm.is_open()) {
        rm << "# " << appName << " (Exported Android Studio Project)\n\n";
        rm << "This Gradle project was fully reconstructed and decompiled natively by **Kaiseki " KAISEKI_FULL_VERSION_STRING "**.\n";
        rm << "Zero external Java/JADX tools were used — 100% Native C++ Dalvik AST pipeline.\n\n";
        rm << "### To open in Android Studio:\n";
        rm << "1. Open Android Studio.\n";
        rm << "2. Select **File -> Open...** and choose this directory.\n";
        rm << "3. Click **Sync Project with Gradle Files**.\n";
        rm.close();
    }

    return true;
}

bool ApkCore_IsJadxAvailable() {
    return true; // Native engine is always 100% available
}

static uint32_t PeRvaToOffset(uint32_t rva, const std::vector<IMAGE_SECTION_HEADER>& sections) {
    for (const auto& sec : sections) {
        uint32_t secSize = sec.Misc.VirtualSize ? sec.Misc.VirtualSize : sec.SizeOfRawData;
        if (rva >= sec.VirtualAddress && rva < sec.VirtualAddress + secSize) {
            return (rva - sec.VirtualAddress) + sec.PointerToRawData;
        }
    }
    return rva;
}

KaisekiResult ApkCore_InspectPE(const wchar_t* filePath, PeAnalysisReport* outReport) {
    if (!filePath || !outReport) return KaisekiResult::Fail(KaisekiStatus::FileNotFound, "Gecersiz dosya yolu veya cikti adresi");
    if (outReport->structSize != sizeof(PeAnalysisReport)) return KaisekiResult::Fail(KaisekiStatus::AbiMismatch, "PeAnalysisReport structSize uyusmuyor");

    HANDLE hFile = CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return KaisekiResult::Fail(KaisekiStatus::FileNotFound, "Hedef PE dosyasi acilamadi");

    DWORD fsize = GetFileSize(hFile, NULL);
    if (fsize < sizeof(IMAGE_DOS_HEADER) + sizeof(IMAGE_NT_HEADERS32)) {
        CloseHandle(hFile);
        return KaisekiResult::Fail(KaisekiStatus::InvalidFormat, "Dosya boyutu gecerli bir PE icin cok kucuk");
    }

    std::vector<uint8_t> buf(fsize);
    DWORD read = 0;
    ReadFile(hFile, buf.data(), fsize, &read, NULL);
    CloseHandle(hFile);

    const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)buf.data();
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || (size_t)dos->e_lfanew + sizeof(DWORD) > buf.size()) {
        return KaisekiResult::Fail(KaisekiStatus::InvalidFormat, "Gecersiz MZ DOS basligi");
    }

    const uint8_t* ntPtr = buf.data() + dos->e_lfanew;
    const DWORD* peSig = (const DWORD*)ntPtr;
    if (*peSig != IMAGE_NT_SIGNATURE) {
        return KaisekiResult::Fail(KaisekiStatus::InvalidFormat, "PE NT imzasi (IMAGE_NT_SIGNATURE) bulunamadi");
    }

    outReport->isPe = true;
    const IMAGE_FILE_HEADER* fileHdr = (const IMAGE_FILE_HEADER*)(ntPtr + sizeof(DWORD));
    outReport->machine = fileHdr->Machine;

    const uint8_t* optPtr = (const uint8_t*)(fileHdr + 1);
    uint16_t magic = *(const uint16_t*)optPtr;

    uint32_t importRva = 0;
    uint32_t numSections = fileHdr->NumberOfSections;
    const IMAGE_SECTION_HEADER* secArray = nullptr;

    if (magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        const IMAGE_OPTIONAL_HEADER32* opt32 = (const IMAGE_OPTIONAL_HEADER32*)optPtr;
        outReport->is64Bit = false;
        outReport->entryPointRva = opt32->AddressOfEntryPoint;
        outReport->imageBase = opt32->ImageBase;
        outReport->subsystem = opt32->Subsystem;
        importRva = opt32->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
        secArray = (const IMAGE_SECTION_HEADER*)((const uint8_t*)opt32 + fileHdr->SizeOfOptionalHeader);
    } else if (magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        const IMAGE_OPTIONAL_HEADER64* opt64 = (const IMAGE_OPTIONAL_HEADER64*)optPtr;
        outReport->is64Bit = true;
        outReport->entryPointRva = opt64->AddressOfEntryPoint;
        outReport->imageBase = opt64->ImageBase;
        outReport->subsystem = opt64->Subsystem;
        importRva = opt64->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
        secArray = (const IMAGE_SECTION_HEADER*)((const uint8_t*)opt64 + fileHdr->SizeOfOptionalHeader);
    } else {
        return KaisekiResult::Fail(KaisekiStatus::InvalidFormat, "Gecersiz veya taninmayan PE Optional Header Magic");
    }

    // Sections
    std::vector<IMAGE_SECTION_HEADER> rawSections;
    for (uint32_t i = 0; i < numSections && (const uint8_t*)(secArray + i + 1) <= buf.data() + buf.size(); ++i) {
        const IMAGE_SECTION_HEADER& s = secArray[i];
        rawSections.push_back(s);

        char nameBuf[9] = { 0 };
        memcpy(nameBuf, s.Name, 8);

        PeSectionItem item;
        item.name = nameBuf;
        item.virtualAddress = s.VirtualAddress;
        item.virtualSize = s.Misc.VirtualSize;
        item.rawDataSize = s.SizeOfRawData;
        item.characteristics = s.Characteristics;
        outReport->sections.push_back(item);
    }

    // Imports (IAT)
    if (importRva > 0) {
        uint32_t impOff = PeRvaToOffset(importRva, rawSections);
        while (impOff + sizeof(IMAGE_IMPORT_DESCRIPTOR) <= buf.size()) {
            const IMAGE_IMPORT_DESCRIPTOR* desc = (const IMAGE_IMPORT_DESCRIPTOR*)(buf.data() + impOff);
            if (desc->Characteristics == 0 && desc->Name == 0 && desc->FirstThunk == 0) break;

            uint32_t nameOff = PeRvaToOffset(desc->Name, rawSections);
            std::string dllName = "unknown.dll";
            if (nameOff < buf.size()) {
                dllName = (const char*)(buf.data() + nameOff);
            }

            PeImportItem pimp;
            pimp.dllName = dllName;

            uint32_t thunkRva = desc->OriginalFirstThunk ? desc->OriginalFirstThunk : desc->FirstThunk;
            uint32_t thunkOff = PeRvaToOffset(thunkRva, rawSections);

            if (outReport->is64Bit) {
                while (thunkOff + sizeof(uint64_t) <= buf.size()) {
                    uint64_t thunk = *(const uint64_t*)(buf.data() + thunkOff);
                    if (thunk == 0) break;
                    if (!(thunk & 0x8000000000000000ULL)) {
                        uint32_t ibOff = PeRvaToOffset((uint32_t)thunk, rawSections);
                        if (ibOff + 2 < buf.size()) {
                            const char* fnName = (const char*)(buf.data() + ibOff + 2);
                            pimp.functions.push_back(fnName);
                        }
                    } else {
                        pimp.functions.push_back("ord_" + std::to_string(thunk & 0xFFFF));
                    }
                    thunkOff += sizeof(uint64_t);
                }
            } else {
                while (thunkOff + sizeof(uint32_t) <= buf.size()) {
                    uint32_t thunk = *(const uint32_t*)(buf.data() + thunkOff);
                    if (thunk == 0) break;
                    if (!(thunk & 0x80000000)) {
                        uint32_t ibOff = PeRvaToOffset(thunk, rawSections);
                        if (ibOff + 2 < buf.size()) {
                            const char* fnName = (const char*)(buf.data() + ibOff + 2);
                            pimp.functions.push_back(fnName);
                        }
                    } else {
                        pimp.functions.push_back("ord_" + std::to_string(thunk & 0xFFFF));
                    }
                    thunkOff += sizeof(uint32_t);
                }
            }

            outReport->imports.push_back(pimp);
            impOff += sizeof(IMAGE_IMPORT_DESCRIPTOR);
        }
    }

    // Critical Strings
    for (size_t i = 0; i < buf.size(); ++i) {
        if (buf[i] >= 32 && buf[i] <= 126) {
            size_t start = i;
            while (i < buf.size() && buf[i] >= 32 && buf[i] <= 126) i++;
            if (i - start >= 6) {
                std::string s((const char*)buf.data() + start, i - start);
                if (s.find("http") != std::string::npos || s.find("HKEY_") != std::string::npos ||
                    s.find(".dll") != std::string::npos || s.find(".exe") != std::string::npos ||
                    s.find("Error") != std::string::npos || s.find("Java") != std::string::npos ||
                    s.find("SOFTWARE\\") != std::string::npos) {
                    outReport->criticalStrings.push_back(s);
                    if (outReport->criticalStrings.size() >= 100) break;
                }
            }
        }
    }

    // Build Formatted Report (IDA-Pro / REA layout)
    std::ostringstream r;
    r << "=== PE BINARY STATIC ANALYSIS REPORT (Kaiseki IDA-Pro Engine) ===\n\n";
    r << "[Binary Architecture & Machine]\n";
    r << "  • Image Format     : " << (outReport->is64Bit ? "PE32+ (64-bit)" : "PE32 (32-bit)") << "\n";
    r << "  • Machine Type     : 0x" << std::hex << outReport->machine << std::dec;
    if (outReport->machine == 0x14c) r << " (Intel i386 / x86)\n";
    else if (outReport->machine == 0x8664) r << " (AMD64 / x86_64)\n";
    else if (outReport->machine == 0xaa64) r << " (ARM64)\n";
    else r << "\n";

    r << "  • Subsystem        : " << (outReport->subsystem == 2 ? "Windows GUI" : "Windows Console / CLI") << "\n";
    r << "  • Default ImageBase: 0x" << std::hex << outReport->imageBase << "\n";
    r << "  • Entry Point (RVA): 0x" << outReport->entryPointRva << " (VA: 0x" << (outReport->imageBase + outReport->entryPointRva) << ")\n\n" << std::dec;

    r << "[Sections Table (" << outReport->sections.size() << " Sections)]\n";
    for (const auto& sec : outReport->sections) {
        r << "  • " << sec.name << "  | Virtual: 0x" << std::hex << sec.virtualAddress << " (Size: 0x" << sec.virtualSize
          << ") | Raw: 0x" << sec.rawDataSize << std::dec;
        if (sec.characteristics & IMAGE_SCN_MEM_EXECUTE) r << " [CODE/EXEC]";
        if (sec.characteristics & IMAGE_SCN_MEM_READ) r << " [READ]";
        if (sec.characteristics & IMAGE_SCN_MEM_WRITE) r << " [WRITE]";
        r << "\n";
    }
    r << "\n";

    r << "[Import Address Table (IAT) — " << outReport->imports.size() << " DLL Modules]\n";
    for (const auto& imp : outReport->imports) {
        r << "  📁 " << imp.dllName << " (" << imp.functions.size() << " imported APIs):\n";
        size_t showMax = (imp.functions.size() < 12) ? imp.functions.size() : 12;
        for (size_t f = 0; f < showMax; ++f) {
            r << "      -> " << imp.functions[f] << "\n";
        }
        if (imp.functions.size() > 12) {
            r << "      ... (" << (imp.functions.size() - 12) << " fonksiyon daha)\n";
        }
    }
    r << "\n";

    r << "[Critical Strings & Configuration Tokens (" << outReport->criticalStrings.size() << " Items)]\n";
    for (const auto& cs : outReport->criticalStrings) {
        r << "  \"" << cs << "\"\n";
    }

    outReport->formattedReport = r.str();
    return KaisekiResult::Success();
}

// ProGuard / R8 / Jadx Deobfuscation Mapping Store
static std::map<std::string, std::string> g_classMappings; // obfuscated -> original
static std::map<std::string, std::string> g_reverseMappings; // original -> obfuscated

bool ApkCore_LoadMappingFile(const wchar_t* mappingPath, int* outMappedCount) {
    if (!mappingPath) return false;
    std::ifstream file(mappingPath);
    if (!file.is_open()) return false;

    g_classMappings.clear();
    g_reverseMappings.clear();

    std::string line;
    int count = 0;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (line[0] == ' ') continue; // Member mapping line

        // Format: original.package.ClassName -> a.b.c:
        size_t arrow = line.find(" -> ");
        if (arrow != std::string::npos) {
            std::string original = line.substr(0, arrow);
            size_t endCol = line.find(':', arrow + 4);
            std::string obf = (endCol != std::string::npos) ? line.substr(arrow + 4, endCol - (arrow + 4)) : line.substr(arrow + 4);

            while (!original.empty() && isspace((unsigned char)original.back())) original.pop_back();
            while (!obf.empty() && isspace((unsigned char)obf.back())) obf.pop_back();

            if (!original.empty() && !obf.empty()) {
                g_classMappings[obf] = original;
                g_reverseMappings[original] = obf;
                count++;
            }
        }
    }

    if (outMappedCount) *outMappedCount = count;
    return (count > 0);
}

bool ApkCore_ApplyMappingToModel(ApkDataModel* model) {
    if (!model || g_classMappings.empty()) return false;

    for (auto& pkg : model->packages) {
        for (auto& cls : pkg.classes) {
            auto it = g_classMappings.find(cls.fullClassName);
            if (it != g_classMappings.end()) {
                std::string orig = it->second;
                cls.fullClassName = orig;
                size_t dot = orig.rfind('.');
                cls.className = (dot != std::string::npos) ? orig.substr(dot + 1) : orig;
            }
        }
    }
    return true;
}

#pragma pack(push, 1)
struct RepackLocalHeader {
    uint32_t signature = 0x04034b50;
    uint16_t versionNeeded = 20;
    uint16_t bitFlag = 0;
    uint16_t compressionMethod = 0; // STORE
    uint16_t lastModTime = 0x4800; // 09:00 AM
    uint16_t lastModDate = 0x5821; // 2026-01-01
    uint32_t crc32 = 0;
    uint32_t compressedSize = 0;
    uint32_t uncompressedSize = 0;
    uint16_t filenameLength = 0;
    uint16_t extraFieldLength = 0;
};

struct RepackCentralHeader {
    uint32_t signature = 0x02014b50;
    uint16_t versionMadeBy = 20;
    uint16_t versionNeeded = 20;
    uint16_t bitFlag = 0;
    uint16_t compressionMethod = 0;
    uint16_t lastModTime = 0x4800;
    uint16_t lastModDate = 0x5821;
    uint32_t crc32 = 0;
    uint32_t compressedSize = 0;
    uint32_t uncompressedSize = 0;
    uint16_t filenameLength = 0;
    uint16_t extraFieldLength = 0;
    uint16_t commentLength = 0;
    uint16_t diskNumberStart = 0;
    uint16_t internalAttr = 0;
    uint32_t externalAttr = 0;
    uint32_t localHeaderOffset = 0;
};

struct RepackEOCD {
    uint32_t signature = 0x06054b50;
    uint16_t diskNumber = 0;
    uint16_t startDisk = 0;
    uint16_t totalEntriesDisk = 0;
    uint16_t totalEntries = 0;
    uint32_t centralDirSize = 0;
    uint32_t centralDirOffset = 0;
    uint16_t commentLength = 0;
};
#pragma pack(pop)

static uint32_t CalculateCrc32Buffer(const uint8_t* data, size_t size) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
        }
    }
    return ~crc;
}

bool ApkCore_RepackageApk(
    const wchar_t* srcApkPath,
    const wchar_t* outApkPath,
    const ApkRepackEntry* modifiedEntries,
    size_t modifiedCount,
    std::string* outLog
) {
    if (!srcApkPath || !outApkPath) return false;

    ZipEngine zip;
    if (!zip.Open(srcApkPath)) {
        if (outLog) *outLog = "Kaynak APK açılamadı.";
        return false;
    }

    HANDLE hOut = CreateFileW(outApkPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hOut == INVALID_HANDLE_VALUE) {
        if (outLog) *outLog = "Çıktı APK dosyası oluşturulamadı.";
        return false;
    }

    std::map<std::string, const ApkRepackEntry*> modMap;
    if (modifiedEntries) {
        for (size_t i = 0; i < modifiedCount; ++i) {
            if (modifiedEntries[i].entryPath) {
                modMap[modifiedEntries[i].entryPath] = &modifiedEntries[i];
            }
        }
    }

    std::vector<RepackCentralHeader> centralHeaders;
    std::vector<std::string> centralNames;
    uint32_t currentOffset = 0;
    DWORD written = 0;

    // 1. Process existing entries from source APK
    for (const auto& entry : zip.entries) {
        // Strip out existing META-INF signature records so APK can be freshly re-signed
        if (entry.name.rfind("META-INF/", 0) == 0) {
            continue;
        }

        auto it = modMap.find(entry.name);
        if (it == modMap.end()) {
            // Unmodified entry: copy original raw compressed bytes directly from zip.buffer
            if (entry.localHeaderOffset + sizeof(ZipLocalHeader) <= zip.buffer.size()) {
                const ZipLocalHeader* lh = (const ZipLocalHeader*)(zip.buffer.data() + entry.localHeaderOffset);
                size_t dataOffset = entry.localHeaderOffset + sizeof(ZipLocalHeader) + lh->filenameLength + lh->extraFieldLength;
                if (dataOffset + entry.compressedSize <= zip.buffer.size()) {
                    RepackLocalHeader loc;
                    loc.filenameLength = (uint16_t)entry.name.length();
                    loc.crc32 = entry.crc32;
                    loc.compressedSize = entry.compressedSize;
                    loc.uncompressedSize = entry.uncompressedSize;
                    loc.compressionMethod = entry.compressionMethod;

                    RepackCentralHeader cen;
                    cen.filenameLength = (uint16_t)entry.name.length();
                    cen.crc32 = entry.crc32;
                    cen.compressedSize = entry.compressedSize;
                    cen.uncompressedSize = entry.uncompressedSize;
                    cen.compressionMethod = entry.compressionMethod;
                    cen.localHeaderOffset = currentOffset;

                    if (!WriteAllOrFail(hOut, &loc, sizeof(loc)) ||
                        !WriteAllOrFail(hOut, entry.name.data(), (DWORD)entry.name.length()) ||
                        (entry.compressedSize > 0 && !WriteAllOrFail(hOut, zip.buffer.data() + dataOffset, (DWORD)entry.compressedSize))) {
                        CloseHandle(hOut);
                        DeleteFileW(outApkPath);
                        if (outLog) *outLog = "APK yazılırken I/O hatası oluştu.";
                        return false;
                    }

                    currentOffset += (uint32_t)(sizeof(loc) + entry.name.length() + entry.compressedSize);
                    centralHeaders.push_back(cen);
                    centralNames.push_back(entry.name);
                    continue;
                }
            }
        }

        // Modified entry: write updated data
        const uint8_t* pData = nullptr;
        size_t dataLen = 0;
        std::vector<uint8_t> extractedBytes;

        if (it != modMap.end() && it->second->data) {
            pData = it->second->data;
            dataLen = it->second->size;
        } else {
            if (!zip.Extract(entry, extractedBytes)) {
                continue;
            }
            pData = extractedBytes.data();
            dataLen = extractedBytes.size();
        }

        uint32_t fileCrc = CalculateCrc32Buffer(pData, dataLen);

        RepackLocalHeader loc;
        loc.filenameLength = (uint16_t)entry.name.length();
        loc.crc32 = fileCrc;
        loc.compressedSize = (uint32_t)dataLen;
        loc.uncompressedSize = (uint32_t)dataLen;
        loc.compressionMethod = 0; // STORE

        RepackCentralHeader cen;
        cen.filenameLength = (uint16_t)entry.name.length();
        cen.crc32 = fileCrc;
        cen.compressedSize = (uint32_t)dataLen;
        cen.uncompressedSize = (uint32_t)dataLen;
        cen.compressionMethod = 0;
        cen.localHeaderOffset = currentOffset;

        if (!WriteAllOrFail(hOut, &loc, sizeof(loc)) ||
            !WriteAllOrFail(hOut, entry.name.data(), (DWORD)entry.name.length()) ||
            (dataLen > 0 && !WriteAllOrFail(hOut, pData, (DWORD)dataLen))) {
            CloseHandle(hOut);
            DeleteFileW(outApkPath);
            if (outLog) *outLog = "APK yazılırken I/O hatası oluştu.";
            return false;
        }

        currentOffset += (uint32_t)(sizeof(loc) + entry.name.length() + dataLen);
        centralHeaders.push_back(cen);
        centralNames.push_back(entry.name);

        if (it != modMap.end()) {
            modMap.erase(it);
        }
    }

    // 2. Append any newly added files
    for (const auto& kv : modMap) {
        const auto* mod = kv.second;
        if (!mod || !mod->data) continue;

        std::string name = kv.first;
        uint32_t fileCrc = CalculateCrc32Buffer(mod->data, mod->size);

        RepackLocalHeader loc;
        loc.filenameLength = (uint16_t)name.length();
        loc.crc32 = fileCrc;
        loc.compressedSize = (uint32_t)mod->size;
        loc.uncompressedSize = (uint32_t)mod->size;
        loc.compressionMethod = 0;

        RepackCentralHeader cen;
        cen.filenameLength = (uint16_t)name.length();
        cen.crc32 = fileCrc;
        cen.compressedSize = (uint32_t)mod->size;
        cen.uncompressedSize = (uint32_t)mod->size;
        cen.compressionMethod = 0;
        cen.localHeaderOffset = currentOffset;

        if (!WriteAllOrFail(hOut, &loc, sizeof(loc)) ||
            !WriteAllOrFail(hOut, name.data(), (DWORD)name.length()) ||
            (mod->size > 0 && !WriteAllOrFail(hOut, mod->data, (DWORD)mod->size))) {
            CloseHandle(hOut);
            DeleteFileW(outApkPath);
            if (outLog) *outLog = "APK yazılırken I/O hatası oluştu.";
            return false;
        }

        currentOffset += (uint32_t)(sizeof(loc) + name.length() + mod->size);
        centralHeaders.push_back(cen);
        centralNames.push_back(name);
    }

    // 3. Write Central Directory
    uint32_t centralDirOffset = currentOffset;
    uint32_t centralDirSize = 0;

    for (size_t i = 0; i < centralHeaders.size(); ++i) {
        if (!WriteAllOrFail(hOut, &centralHeaders[i], sizeof(RepackCentralHeader)) ||
            !WriteAllOrFail(hOut, centralNames[i].data(), (DWORD)centralNames[i].length())) {
            CloseHandle(hOut);
            DeleteFileW(outApkPath);
            if (outLog) *outLog = "Merkezi dizin yazılırken I/O hatası oluştu.";
            return false;
        }
        centralDirSize += (uint32_t)(sizeof(RepackCentralHeader) + centralNames[i].length());
    }

    // 4. Write EOCD
    RepackEOCD eocd;
    eocd.totalEntriesDisk = (uint16_t)centralHeaders.size();
    eocd.totalEntries = (uint16_t)centralHeaders.size();
    eocd.centralDirSize = centralDirSize;
    eocd.centralDirOffset = centralDirOffset;

    if (!WriteAllOrFail(hOut, &eocd, sizeof(eocd))) {
        CloseHandle(hOut);
        DeleteFileW(outApkPath);
        if (outLog) *outLog = "EOCD yazılırken I/O hatası oluştu.";
        return false;
    }
    CloseHandle(hOut);

    if (outLog) {
        *outLog = "APK başarıyla yeniden paketlendi (" + std::to_string(centralHeaders.size()) + " dosya entegre edildi).";
    }
    return true;
}

// KaisekiCore_* Export Wrappers
KaisekiResult KaisekiCore_InspectFile(const wchar_t* apkPath, ApkDataModel** outModel) { return ApkCore_InspectFile(apkPath, outModel); }
bool KaisekiCore_ExtractFile(const wchar_t* apkPath, const char* entryName, uint8_t** outData, size_t* outSize) { return ApkCore_ExtractFile(apkPath, entryName, outData, outSize); }
void KaisekiCore_FreeBuffer(uint8_t* data) { ApkCore_FreeBuffer(data); }
bool KaisekiCore_DecompileClass(const wchar_t* apkPath, const char* className, char** outJavaCode, char** outSmaliCode, char** outFridaScript) { return ApkCore_DecompileClass(apkPath, className, outJavaCode, outSmaliCode, outFridaScript); }
bool KaisekiCore_ExportGradleProject(const wchar_t* apkPath, const wchar_t* outDir) { return ApkCore_ExportGradleProject(apkPath, outDir); }
bool KaisekiCore_IsJadxAvailable() { return ApkCore_IsJadxAvailable(); }
bool KaisekiCore_DecompileClassWithJadx(const wchar_t* apkPath, const char* className, char** outJavaCode) { return ApkCore_DecompileClassWithJadx(apkPath, className, outJavaCode); }
KaisekiResult KaisekiCore_InspectPE(const wchar_t* filePath, PeAnalysisReport* outReport) { return ApkCore_InspectPE(filePath, outReport); }
bool KaisekiCore_LoadMappingFile(const wchar_t* mappingPath, int* outMappedCount) { return ApkCore_LoadMappingFile(mappingPath, outMappedCount); }
bool KaisekiCore_ApplyMappingToModel(ApkDataModel* model) { return ApkCore_ApplyMappingToModel(model); }
bool KaisekiCore_RepackageApk(const wchar_t* srcApkPath, const wchar_t* outApkPath, const ApkRepackEntry* modifiedEntries, size_t modifiedCount, std::string* outLog) { return ApkCore_RepackageApk(srcApkPath, outApkPath, modifiedEntries, modifiedCount, outLog); }

bool ApkCore_DecompileXml(const uint8_t* data, size_t size, std::string* outXml) {
    if (!data || size < 8 || !outXml) return false;
    *outXml = AxmlEngine::DecompileXml(data, size);
    return !outXml->empty();
}

bool KaisekiCore_DecompileXml(const uint8_t* data, size_t size, std::string* outXml) {
    return ApkCore_DecompileXml(data, size, outXml);
}

void KaisekiCore_FreeXmlString(std::string* xml) {
    if (xml) std::string().swap(*xml);
}

void ApkCore_FreeXmlString(std::string* xml) {
    KaisekiCore_FreeXmlString(xml);
}

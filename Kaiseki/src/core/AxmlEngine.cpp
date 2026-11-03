#include "AxmlEngine.h"
#include "KaisekiCommon.h"
#include <windows.h>
#include <sstream>
#include <iomanip>
#include <map>
#include <vector>

void AxmlEngine::ParsePlainText(const std::string& xml,
                                std::string& outPackageName,
                                std::string& outVersionCode,
                                std::string& outVersionName,
                                std::string& outMinSdk,
                                std::string& outTargetSdk,
                                std::string& outAppLabel,
                                std::string& outAppIcon,
                                std::vector<std::string>& outPermissions,
                                std::vector<std::string>& outActivities,
                                std::vector<std::string>& outServices,
                                std::vector<std::string>& outReceivers)
{
    auto findAttr = [&](const std::string& tag, const std::string& attr) -> std::string {
        size_t tagPos = xml.find("<" + tag);
        if (tagPos == std::string::npos) return "";
        size_t tagEnd = xml.find('>', tagPos);
        if (tagEnd == std::string::npos) return "";
        std::string sub = xml.substr(tagPos, tagEnd - tagPos);
        size_t aPos = sub.find(attr + "=\"");
        if (aPos == std::string::npos) return "";
        size_t vStart = aPos + attr.length() + 2;
        size_t vEnd = sub.find('\"', vStart);
        if (vEnd == std::string::npos) return "";
        return sub.substr(vStart, vEnd - vStart);
    };

    std::string pkg = findAttr("manifest", "package");
    if (!pkg.empty()) outPackageName = pkg;
    std::string vc = findAttr("manifest", "android:versionCode");
    if (vc.empty()) vc = findAttr("manifest", "versionCode");
    if (!vc.empty()) outVersionCode = vc;
    std::string vn = findAttr("manifest", "android:versionName");
    if (vn.empty()) vn = findAttr("manifest", "versionName");
    if (!vn.empty()) outVersionName = vn;

    std::string minS = findAttr("uses-sdk", "android:minSdkVersion");
    if (!minS.empty()) outMinSdk = minS;
    std::string tgtS = findAttr("uses-sdk", "android:targetSdkVersion");
    if (!tgtS.empty()) outTargetSdk = tgtS;

    std::string lbl = findAttr("application", "android:label");
    if (!lbl.empty()) outAppLabel = lbl;
    std::string icn = findAttr("application", "android:icon");
    if (!icn.empty()) outAppIcon = icn;

    size_t p = 0;
    while ((p = xml.find("<uses-permission", p)) != std::string::npos) {
        size_t end = xml.find('>', p);
        if (end == std::string::npos) break;
        std::string sub = xml.substr(p, end - p);
        size_t nPos = sub.find("android:name=\"");
        if (nPos != std::string::npos) {
            size_t vStart = nPos + 14;
            size_t vEnd = sub.find('\"', vStart);
            if (vEnd != std::string::npos) {
                outPermissions.push_back(sub.substr(vStart, vEnd - vStart));
            }
        }
        p = end + 1;
    }

    p = 0;
    while ((p = xml.find("<activity", p)) != std::string::npos) {
        size_t end = xml.find('>', p);
        if (end == std::string::npos) break;
        std::string sub = xml.substr(p, end - p);
        size_t nPos = sub.find("android:name=\"");
        if (nPos != std::string::npos) {
            size_t vStart = nPos + 14;
            size_t vEnd = sub.find('\"', vStart);
            if (vEnd != std::string::npos) {
                outActivities.push_back(sub.substr(vStart, vEnd - vStart));
            }
        }
        p = end + 1;
    }

    p = 0;
    while ((p = xml.find("<service", p)) != std::string::npos) {
        size_t end = xml.find('>', p);
        if (end == std::string::npos) break;
        std::string sub = xml.substr(p, end - p);
        size_t nPos = sub.find("android:name=\"");
        if (nPos != std::string::npos) {
            size_t vStart = nPos + 14;
            size_t vEnd = sub.find('\"', vStart);
            if (vEnd != std::string::npos) {
                outServices.push_back(sub.substr(vStart, vEnd - vStart));
            }
        }
        p = end + 1;
    }

    p = 0;
    while ((p = xml.find("<receiver", p)) != std::string::npos) {
        size_t end = xml.find('>', p);
        if (end == std::string::npos) break;
        std::string sub = xml.substr(p, end - p);
        size_t nPos = sub.find("android:name=\"");
        if (nPos != std::string::npos) {
            size_t vStart = nPos + 14;
            size_t vEnd = sub.find('\"', vStart);
            if (vEnd != std::string::npos) {
                outReceivers.push_back(sub.substr(vStart, vEnd - vStart));
            }
        }
        p = end + 1;
    }
}

std::string AxmlEngine::Decompile(const uint8_t* data, size_t size,
                                  std::string& outPackageName,
                                  std::string& outVersionCode,
                                  std::string& outVersionName,
                                  std::string& outMinSdk,
                                  std::string& outTargetSdk,
                                  std::string& outAppLabel,
                                  std::string& outAppIcon,
                                  std::vector<std::string>& outPermissions,
                                  std::vector<std::string>& outActivities,
                                  std::vector<std::string>& outServices,
                                  std::vector<std::string>& outReceivers)
{
    if (!data || size < 4) return "";

    // 0. Handle text XML with Byte Order Marks (BOM)
    // UTF-8 BOM: 0xEF, 0xBB, 0xBF
    if (size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
        std::string plain((const char*)(data + 3), size - 3);
        ParsePlainText(plain, outPackageName, outVersionCode, outVersionName,
                       outMinSdk, outTargetSdk, outAppLabel, outAppIcon,
                       outPermissions, outActivities, outServices, outReceivers);
        return plain;
    }

    // UTF-16 LE BOM: 0xFF, 0xFE
    if (size >= 2 && data[0] == 0xFF && data[1] == 0xFE) {
        const wchar_t* wData = (const wchar_t*)(data + 2);
        int wLen = (int)((size - 2) / 2);
        int u8Len = WideCharToMultiByte(CP_UTF8, 0, wData, wLen, NULL, 0, NULL, NULL);
        if (u8Len > 0) {
            std::string plain(u8Len, 0);
            WideCharToMultiByte(CP_UTF8, 0, wData, wLen, &plain[0], u8Len, NULL, NULL);
            ParsePlainText(plain, outPackageName, outVersionCode, outVersionName,
                           outMinSdk, outTargetSdk, outAppLabel, outAppIcon,
                           outPermissions, outActivities, outServices, outReceivers);
            return plain;
        }
    }

    // UTF-16 BE BOM: 0xFE, 0xFF
    if (size >= 2 && data[0] == 0xFE && data[1] == 0xFF) {
        int wLen = (int)((size - 2) / 2);
        std::vector<wchar_t> wSwapped(wLen);
        for (int i = 0; i < wLen; ++i) {
            wSwapped[i] = (wchar_t)((data[2 + i * 2] << 8) | data[2 + i * 2 + 1]);
        }
        int u8Len = WideCharToMultiByte(CP_UTF8, 0, wSwapped.data(), wLen, NULL, 0, NULL, NULL);
        if (u8Len > 0) {
            std::string plain(u8Len, 0);
            WideCharToMultiByte(CP_UTF8, 0, wSwapped.data(), wLen, &plain[0], u8Len, NULL, NULL);
            ParsePlainText(plain, outPackageName, outVersionCode, outVersionName,
                           outMinSdk, outTargetSdk, outAppLabel, outAppIcon,
                           outPermissions, outActivities, outServices, outReceivers);
            return plain;
        }
    }

    uint16_t rootType = *(const uint16_t*)data;
    if (rootType != 0x0003) {
        std::string plain((const char*)data, size);
        ParsePlainText(plain, outPackageName, outVersionCode, outVersionName,
                       outMinSdk, outTargetSdk, outAppLabel, outAppIcon,
                       outPermissions, outActivities, outServices, outReceivers);
        return plain;
    }

    uint16_t rootHeaderSize = (size >= 4) ? *(const uint16_t*)(data + 2) : 8;
    size_t offset = (rootHeaderSize >= 8 && rootHeaderSize <= size) ? rootHeaderSize : 8;
    std::vector<std::string> stringPool;
    std::ostringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";

    int indentLevel = 0;
    auto getIndent = [&]() {
        return std::string(indentLevel * 4, ' ');
    };

    std::map<std::string, std::string> namespaces;

    while (offset + 8 <= size) {
        uint16_t chunkType = *(const uint16_t*)(data + offset);
        uint16_t headerSize = *(const uint16_t*)(data + offset + 2);
        uint32_t chunkSize = *(const uint32_t*)(data + offset + 4);

        if (chunkSize < 8 || offset + chunkSize > size) break;

        if (chunkType == 0x0001) { // RES_STRING_POOL_TYPE
            SafeReadStringPool(data, size, offset, stringPool);
        } else if (chunkType == 0x0100) { // RES_XML_START_NAMESPACE_TYPE
            if (offset + 24 <= size) {
                uint32_t prefixIdx = *(const uint32_t*)(data + offset + 16);
                uint32_t uriIdx = *(const uint32_t*)(data + offset + 20);
                if (prefixIdx < stringPool.size() && uriIdx < stringPool.size()) {
                    namespaces[stringPool[uriIdx]] = stringPool[prefixIdx];
                }
            }
        } else if (chunkType == 0x0102) { // RES_XML_START_ELEMENT_TYPE
            if (offset + 36 <= size) {
                uint32_t nsIdx = *(const uint32_t*)(data + offset + 16);
                uint32_t nameIdx = *(const uint32_t*)(data + offset + 20);
                uint16_t attrStart = *(const uint16_t*)(data + offset + 24);
                uint16_t attrSize = *(const uint16_t*)(data + offset + 26);
                uint16_t attrCount = *(const uint16_t*)(data + offset + 28);

                std::string elemName = (nameIdx < stringPool.size()) ? stringPool[nameIdx] : "element";
                xml << getIndent() << "<" << elemName;

                // attrStart is offset from after the 16-byte ResXMLTree_node header
                size_t attrOffset = offset + 16 + attrStart;

                for (uint16_t a = 0; a < attrCount; ++a) {
                    size_t aOff = attrOffset + a * attrSize;
                    if (aOff + 20 > size) break;

                    uint32_t aNsIdx = *(const uint32_t*)(data + aOff);
                    uint32_t aNameIdx = *(const uint32_t*)(data + aOff + 4);
                    uint32_t aRawIdx = *(const uint32_t*)(data + aOff + 8);
                    uint8_t valType = *(const uint8_t*)(data + aOff + 15);
                    uint32_t valData = *(const uint32_t*)(data + aOff + 16);

                    std::string attrName = (aNameIdx < stringPool.size()) ? stringPool[aNameIdx] : "attr";
                    std::string attrNs = "";
                    if (aNsIdx < stringPool.size()) {
                        std::string uri = stringPool[aNsIdx];
                        if (namespaces.find(uri) != namespaces.end()) {
                            attrNs = namespaces[uri] + ":";
                        } else if (uri.find("res/android") != std::string::npos) {
                            attrNs = "android:";
                        }
                    }

                    std::string attrVal = "";
                    if (aRawIdx < stringPool.size() && !stringPool[aRawIdx].empty()) {
                        attrVal = stringPool[aRawIdx];
                    } else {
                        if (valType == 3 && valData < stringPool.size()) {
                            attrVal = stringPool[valData];
                        } else if (valType == 18) {
                            attrVal = (valData != 0) ? "true" : "false";
                        } else if (valType == 16) {
                            attrVal = std::to_string((int)valData);
                        } else if (valType == 17) {
                            std::ostringstream ss;
                            ss << "0x" << std::hex << valData;
                            attrVal = ss.str();
                        } else if (valType == 1) {
                            std::ostringstream ss;
                            ss << "@0x" << std::hex << valData;
                            attrVal = ss.str();
                        } else if (valType == 2) {
                            std::ostringstream ss;
                            ss << "?0x" << std::hex << valData;
                            attrVal = ss.str();
                        } else if (valType == 4) {
                            float fv = *(const float*)&valData;
                            attrVal = std::to_string(fv);
                        } else if (valType == 5) { // TYPE_DIMENSION
                            const char* DIM_UNITS[] = { "px", "dp", "sp", "pt", "in", "mm" };
                            int unit = valData & 0x0F;
                            int radix = (valData >> 4) & 0x03;
                            const float MANTISSA_MULT[] = {
                                1.0f / (1 << 8),
                                1.0f / (1 << 15),
                                1.0f / (1 << 23),
                                1.0f / (1 << 31)
                            };
                            float dimVal = (float)(valData >> 8) * MANTISSA_MULT[radix];
                            char buf[32];
                            snprintf(buf, sizeof(buf), "%.1f%s", dimVal, (unit < 6 ? DIM_UNITS[unit] : ""));
                            attrVal = buf;
                        } else if (valType >= 0x1c && valType <= 0x1f) { // TYPE_INT_COLOR_*
                            char buf[16];
                            snprintf(buf, sizeof(buf), "#%08X", valData);
                            attrVal = buf;
                        } else {
                            attrVal = std::to_string(valData);
                        }
                    }

                    if (elemName == "manifest") {
                        if (attrName == "package") outPackageName = attrVal;
                        else if (attrName == "versionCode") outVersionCode = attrVal;
                        else if (attrName == "versionName") outVersionName = attrVal;
                    } else if (elemName == "uses-sdk") {
                        if (attrName == "minSdkVersion") outMinSdk = attrVal;
                        else if (attrName == "targetSdkVersion") outTargetSdk = attrVal;
                    } else if (elemName == "application") {
                        if (attrName == "label") outAppLabel = attrVal;
                        else if (attrName == "icon") outAppIcon = attrVal;
                    } else if (elemName == "uses-permission") {
                        if (attrName == "name") outPermissions.push_back(attrVal);
                    } else if (elemName == "activity") {
                        if (attrName == "name") outActivities.push_back(attrVal);
                    } else if (elemName == "service") {
                        if (attrName == "name") outServices.push_back(attrVal);
                    } else if (elemName == "receiver") {
                        if (attrName == "name") outReceivers.push_back(attrVal);
                    }

                    xml << "\n" << getIndent() << "    " << attrNs << attrName << "=\"" << attrVal << "\"";
                }

                xml << ">\n";
                indentLevel++;
            }
        } else if (chunkType == 0x0103) { // RES_XML_END_ELEMENT_TYPE
            if (offset + 24 <= size) {
                uint32_t nameIdx = *(const uint32_t*)(data + offset + 20);
                std::string elemName = (nameIdx < stringPool.size()) ? stringPool[nameIdx] : "element";
                if (indentLevel > 0) indentLevel--;
                xml << getIndent() << "</" << elemName << ">\n";
            }
        } else if (chunkType == 0x0104) { // RES_XML_CDATA_TYPE
            if (offset + 28 <= size) {
                uint32_t dataIdx = *(const uint32_t*)(data + offset + 16);
                if (dataIdx < stringPool.size() && !stringPool[dataIdx].empty()) {
                    xml << getIndent() << "<![CDATA[" << stringPool[dataIdx] << "]]>\n";
                }
            }
        }

        offset += chunkSize;
    }

    return xml.str();
}

std::string AxmlEngine::DecompileXml(const uint8_t* data, size_t size) {
    std::string dummyPkg, dummyVer, dummyVerName, dummyMin, dummyTgt, dummyLbl, dummyIcn;
    std::vector<std::string> dummyPerms, dummyActs, dummyServs, dummyRecvs;
    return Decompile(data, size, dummyPkg, dummyVer, dummyVerName, dummyMin, dummyTgt, dummyLbl, dummyIcn, dummyPerms, dummyActs, dummyServs, dummyRecvs);
}

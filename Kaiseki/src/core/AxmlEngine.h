#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <vector>
#include <string>
#include <cstdint>

class AxmlEngine {
public:
    static std::string Decompile(const uint8_t* data, size_t size,
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
                                 std::vector<std::string>& outReceivers);

    static std::string DecompileXml(const uint8_t* data, size_t size);

private:
    static void ParsePlainText(const std::string& xml,
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
                               std::vector<std::string>& outReceivers);
};

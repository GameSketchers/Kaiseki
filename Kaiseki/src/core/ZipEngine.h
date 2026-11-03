#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <vector>
#include <string>
#include <cstdint>

#pragma pack(push, 1)
struct ZipLocalHeader {
    uint32_t signature;
    uint16_t versionNeeded;
    uint16_t bitFlag;
    uint16_t compressionMethod;
    uint16_t lastModTime;
    uint16_t lastModDate;
    uint32_t crc32;
    uint32_t compressedSize;
    uint32_t uncompressedSize;
    uint16_t filenameLength;
    uint16_t extraFieldLength;
};

struct ZipCentralHeader {
    uint32_t signature;
    uint16_t versionMadeBy;
    uint16_t versionNeeded;
    uint16_t bitFlag;
    uint16_t compressionMethod;
    uint16_t lastModTime;
    uint16_t lastModDate;
    uint32_t crc32;
    uint32_t compressedSize;
    uint32_t uncompressedSize;
    uint16_t filenameLength;
    uint16_t extraFieldLength;
    uint16_t commentLength;
    uint16_t diskNumberStart;
    uint16_t internalFileAttr;
    uint32_t externalFileAttr;
    uint32_t localHeaderOffset;
};

struct ZipEOCD {
    uint32_t signature;
    uint16_t diskNumber;
    uint16_t centralDirDisk;
    uint16_t numEntriesThisDisk;
    uint16_t totalEntries;
    uint32_t centralDirSize;
    uint32_t centralDirOffset;
    uint16_t commentLength;
};
#pragma pack(pop)

struct ZipEntryMeta {
    std::string name;
    uint16_t compressionMethod = 0;
    uint32_t compressedSize = 0;
    uint32_t uncompressedSize = 0;
    uint32_t localHeaderOffset = 0;
    uint32_t crc32 = 0;
};

class ZipEngine {
public:
    ZipEngine();
    ~ZipEngine();

    std::vector<uint8_t> buffer;
    std::vector<ZipEntryMeta> entries;

    bool Open(const std::wstring& path);
    bool LoadFromFile(const std::wstring& path);
    bool LoadFromMemory(const uint8_t* data, size_t size);
    bool Extract(const ZipEntryMeta& entry, std::vector<uint8_t>& out) const;
    const ZipEntryMeta* FindEntry(const std::string& name) const;

private:
    bool ParseCentralDirectory();
};

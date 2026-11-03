#include "ZipEngine.h"
extern "C" {
#include "puff.h"
}
#include <fstream>
#include <algorithm>
#include <compressapi.h>

#pragma comment(lib, "cabinet.lib")

ZipEngine::ZipEngine() {}
ZipEngine::~ZipEngine() {}

bool ZipEngine::Open(const std::wstring& path) {
    return LoadFromFile(path);
}

bool ZipEngine::LoadFromFile(const std::wstring& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size <= 0 || size > 2147483648LL) { // 2GB limit
        return false;
    }

    buffer.resize((size_t)size);
    if (!file.read((char*)buffer.data(), size)) {
        buffer.clear();
        return false;
    }

    return ParseCentralDirectory();
}

bool ZipEngine::LoadFromMemory(const uint8_t* data, size_t size) {
    if (!data || size == 0 || size > 2147483648LL) return false;
    buffer.assign(data, data + size);
    return ParseCentralDirectory();
}

bool ZipEngine::ParseCentralDirectory() {
    if (buffer.size() < sizeof(ZipEOCD)) return false;

    const uint32_t eocdSig = 0x06054b50;
    size_t limit = 65535 + sizeof(ZipEOCD) + 4096; // Allow comment + APK signing block cushion
    size_t maxSearch = (buffer.size() < limit) ? buffer.size() : limit;
    size_t searchStart = buffer.size() - sizeof(ZipEOCD);
    size_t searchEnd = (buffer.size() > maxSearch) ? (buffer.size() - maxSearch) : 0;

    const ZipEOCD* eocd = nullptr;
    for (size_t i = searchStart; i >= searchEnd; --i) {
        if (*(const uint32_t*)(buffer.data() + i) == eocdSig) {
            eocd = (const ZipEOCD*)(buffer.data() + i);
            break;
        }
        if (i == 0) break;
    }

    if (!eocd) return false;

    size_t offset = eocd->centralDirOffset;
    uint32_t total = eocd->totalEntries;
    entries.clear();

    // Iterate through all Central Directory entries using subtractive bounds checking
    uint32_t count = 0;
    while (buffer.size() >= sizeof(ZipCentralHeader) && offset <= buffer.size() - sizeof(ZipCentralHeader)) {
        const ZipCentralHeader* cd = (const ZipCentralHeader*)(buffer.data() + offset);
        if (cd->signature != 0x02014b50) break;

        size_t nameOffset = offset + sizeof(ZipCentralHeader);
        if (cd->filenameLength > buffer.size() || nameOffset > buffer.size() - cd->filenameLength) break;

        std::string name((const char*)(buffer.data() + nameOffset), cd->filenameLength);

        ZipEntryMeta ze;
        ze.name = name;
        ze.compressionMethod = cd->compressionMethod;
        ze.compressedSize = cd->compressedSize;
        ze.uncompressedSize = cd->uncompressedSize;
        ze.localHeaderOffset = cd->localHeaderOffset;
        ze.crc32 = cd->crc32;
        entries.push_back(ze);

        size_t entryTotalLen = (size_t)sizeof(ZipCentralHeader) + cd->filenameLength + cd->extraFieldLength + cd->commentLength;
        if (entryTotalLen > buffer.size() || offset > buffer.size() - entryTotalLen) break;

        offset += entryTotalLen;
        count++;
        if (total != 0xFFFF && count >= total) break;
    }

    return !entries.empty();
}

bool ZipEngine::Extract(const ZipEntryMeta& entry, std::vector<uint8_t>& out) const {
    if (buffer.size() < sizeof(ZipLocalHeader) || entry.localHeaderOffset > buffer.size() - sizeof(ZipLocalHeader)) return false;
    const ZipLocalHeader* lh = (const ZipLocalHeader*)(buffer.data() + entry.localHeaderOffset);
    if (lh->signature != 0x04034b50) return false;

    size_t headerSize = (size_t)sizeof(ZipLocalHeader) + lh->filenameLength + lh->extraFieldLength;
    if (entry.localHeaderOffset > buffer.size() || headerSize > buffer.size() - entry.localHeaderOffset) return false;
    size_t dataOffset = entry.localHeaderOffset + headerSize;

    if (entry.compressedSize > buffer.size() || dataOffset > buffer.size() - entry.compressedSize) return false;

    out.resize(entry.uncompressedSize);
    if (entry.uncompressedSize == 0) return true;

    if (entry.compressionMethod == 0) { // STORE
        // Verify compressedSize matches uncompressedSize and bounds are strictly validated
        if (entry.compressedSize != entry.uncompressedSize) return false;
        if (entry.uncompressedSize > buffer.size() || dataOffset > buffer.size() - entry.uncompressedSize) return false;
        memcpy(out.data(), buffer.data() + dataOffset, entry.uncompressedSize);
        return true;
    } else if (entry.compressionMethod == 8) { // DEFLATE
        // 1. Try Windows native hardware-accelerated CompressAPI (Cabinet.dll)
        DECOMPRESSOR_HANDLE hDecomp = NULL;
        if (CreateDecompressor(COMPRESS_ALGORITHM_MSZIP | COMPRESS_RAW, NULL, &hDecomp)) {
            SIZE_T actualDecompressed = 0;
            BOOL ok = Decompress(hDecomp, buffer.data() + dataOffset, entry.compressedSize,
                                 out.data(), entry.uncompressedSize, &actualDecompressed);
            CloseDecompressor(hDecomp);
            if (ok && actualDecompressed == entry.uncompressedSize) {
                return true;
            }
        }

        // 2. Fallback to puff for legacy/custom streams
        unsigned long destLen = entry.uncompressedSize;
        unsigned long srcLen = entry.compressedSize;
        int ret = puff(out.data(), &destLen, buffer.data() + dataOffset, &srcLen);
        return ret == 0;
    }
    return false;
}

const ZipEntryMeta* ZipEngine::FindEntry(const std::string& name) const {
    for (const auto& e : entries) {
        if (e.name == name) return &e;
    }
    return nullptr;
}

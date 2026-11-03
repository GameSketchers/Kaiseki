#include "DexEngine.h"
#include <iomanip>
#include <sstream>

#pragma pack(push, 1)
struct DexHeaderRaw {
    uint8_t  magic[8];
    uint32_t checksum;
    uint8_t  signature[20];
    uint32_t file_size;
    uint32_t header_size;
    uint32_t endian_tag;
    uint32_t link_size;
    uint32_t link_off;
    uint32_t map_off;
    uint32_t string_ids_size;
    uint32_t string_ids_off;
    uint32_t type_ids_size;
    uint32_t type_ids_off;
    uint32_t proto_ids_size;
    uint32_t proto_ids_off;
    uint32_t field_ids_size;
    uint32_t field_ids_off;
    uint32_t method_ids_size;
    uint32_t method_ids_off;
    uint32_t class_defs_size;
    uint32_t class_defs_off;
    uint32_t data_size;
    uint32_t data_off;
};
#pragma pack(pop)

bool DexEngine::Parse(const std::string& name, const uint8_t* data, size_t size, DexDataInfo& outInfo) {
    if (!data || size < sizeof(DexHeaderRaw)) return false;

    const DexHeaderRaw* hdr = (const DexHeaderRaw*)data;
    if (memcmp(hdr->magic, "dex\n", 4) != 0) return false;

    outInfo.filename = name;
    outInfo.fileSize = hdr->file_size;

    char verStr[5] = { 0 };
    memcpy(verStr, hdr->magic + 4, 3);
    outInfo.version = std::string("v") + verStr;
    outInfo.checksum = hdr->checksum;

    std::ostringstream sha1;
    for (int i = 0; i < 20; ++i) {
        sha1 << std::hex << std::setw(2) << std::setfill('0') << (int)hdr->signature[i];
    }
    outInfo.sha1Hex = sha1.str();

    outInfo.classCount = hdr->class_defs_size;
    outInfo.methodCount = hdr->method_ids_size;
    outInfo.fieldCount = hdr->field_ids_size;
    outInfo.stringCount = hdr->string_ids_size;
    outInfo.protoCount = hdr->proto_ids_size;
    outInfo.typeCount = hdr->type_ids_size;

    return true;
}

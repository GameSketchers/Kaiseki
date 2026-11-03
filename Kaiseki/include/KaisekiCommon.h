#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#if defined(_MSC_VER)
#pragma warning(disable: 4190) // 'identifier' has C-linkage specified, but returns UDT 'KaisekiResult'
#endif
#include <cstdint>
#include <string>
#include <vector>

// ============================================================================
// BELLEK SAHİPLİĞİ KURALI (KAISEKI ABI SÖZLEŞMESİ v1)
// ============================================================================
// 1) Bir fonksiyon std::string*, std::wstring*, std::vector<T>* VEYA bunları
//    içeren bir struct* ÇIKTI parametresi olarak dolduruyorsa, bu DLL'in o nesneyi
//    EXE'nin değil KENDİ CRT heap'iyle doldurduğu anlamına gelir.
// 2) Böyle bir çıktı alan HER fonksiyonun yanında mutlaka eşlik eden bir
//    Kaiseki<Modul>_Free<Yapı>(...) fonksiyonu bulunur ve çağıran taraf bunu
//    kullanmadan önce o nesneyi normal C++ kapsam-dışı (destructor) ile ASLA
//    serbest bırakmaz.
// 3) Ham işaretçi (char*, uint8_t*) döndüren her fonksiyonun da birebir eşleşen
//    bir Free fonksiyonu vardır; çağıran taraf free()/delete KULLANMAZ.
// 4) Tüm modüller dinamik CRT (/MD veya /MDd) ile derlenmelidir.
// ============================================================================

enum class KaisekiStatus : int32_t {
    Ok = 0,
    FileNotFound = 1,
    InvalidFormat = 2,
    AccessDenied = 3,
    OutOfMemory = 4,
    AbiMismatch = 5,
    OperationFailed = 6,
    Unknown = 99
};

struct KaisekiResult {
    KaisekiStatus status = KaisekiStatus::Ok;
    std::string message; // Ok ise boş olabilir; hata durumunda kullanıcı dostu açıklama

    // Safe boolean conversion for if (res) or if (!res)
    explicit operator bool() const {
        return status == KaisekiStatus::Ok;
    }

    bool IsOk() const {
        return status == KaisekiStatus::Ok;
    }

    static KaisekiResult Success() {
        return { KaisekiStatus::Ok, "" };
    }

    static KaisekiResult Fail(KaisekiStatus st, const std::string& msg = "") {
        return { st, msg };
    }
};

// ============================================================================
// GÜVENLİ VE TEK TİP ARSC / AXML DİZGİ HAVUZU (STRING POOL) AYRIŞTIRICISI
// ============================================================================
// DoS koruması (500k üst sınır), taşma korumalı çıkarma-tabanlı sınır kontrolleri,
// UTF-8 ve UTF-16LE desteği ile tüm modüllerde ortak kullanılır.
inline bool SafeReadStringPool(const uint8_t* data, size_t size, size_t offset, std::vector<std::string>& outStrings, size_t* outChunkSize = nullptr) {
    if (!data || offset + 28 > size) return false;
    uint16_t type = *(const uint16_t*)(data + offset);
    if (type != 0x0001) return false; // RES_STRING_POOL_TYPE

    uint32_t chunkSize = *(const uint32_t*)(data + offset + 4);
    if (chunkSize < 28 || offset > size - chunkSize) return false;
    if (outChunkSize) *outChunkSize = chunkSize;

    uint32_t stringCount = *(const uint32_t*)(data + offset + 8);
    const uint32_t kMaxStrings = 500000; // Cap to prevent DoS via huge memory allocation
    if (stringCount > kMaxStrings) return false;

    uint32_t flags = *(const uint32_t*)(data + offset + 16);
    uint32_t stringsStart = *(const uint32_t*)(data + offset + 20);
    bool isUtf8 = (flags & (1 << 8)) != 0;

    if (offset > size - stringsStart) return false;
    size_t poolBase = offset + stringsStart;
    if (poolBase > size) return false;

    const uint32_t* offsets = (const uint32_t*)(data + offset + 28);

    outStrings.clear();
    outStrings.resize(stringCount);

    const uint8_t* end = data + size;

    for (uint32_t i = 0; i < stringCount; ++i) {
        if (offset + 28 + (size_t)(i + 1) * 4 > size) break;
        uint32_t relOff = offsets[i];
        if (relOff > size || poolBase > size - relOff) continue;
        size_t strOffset = poolBase + relOff;
        if (strOffset >= size) continue;

        const uint8_t* p = data + strOffset;

        if (isUtf8) {
            // UTF-8 string: skip char length (1 or 2 bytes)
            if (p >= end) continue;
            if (*p & 0x80) {
                if (p + 2 > end) continue;
                p += 2;
            } else {
                p += 1;
            }

            // Read byte length (1 or 2 bytes)
            if (p >= end) continue;
            size_t byteLen = 0;
            if (*p & 0x80) {
                if (p + 2 > end) continue;
                byteLen = (((*p & 0x7F) << 8) | *(p + 1));
                p += 2;
            } else {
                byteLen = *p;
                p += 1;
            }

            if (byteLen <= (size_t)(end - p)) {
                outStrings[i] = std::string((const char*)p, byteLen);
            }
        } else {
            // UTF-16LE string: length is 1 or 2 16-bit words
            if ((size_t)(end - p) < 2) continue;
            uint16_t w1 = *(const uint16_t*)p;
            size_t charLen = 0;
            if (w1 & 0x8000) {
                if ((size_t)(end - p) < 4) continue;
                uint16_t w2 = *(const uint16_t*)(p + 2);
                charLen = (((w1 & 0x7FFF) << 16) | w2);
                p += 4;
            } else {
                charLen = w1;
                p += 2;
            }

            // Convert UTF-16LE to UTF-8
            const uint16_t* u16 = (const uint16_t*)p;
            size_t maxChars = (size_t)(end - p) / 2;
            if (charLen > maxChars) charLen = maxChars;

            std::string utf8;
            utf8.reserve(charLen);
            for (size_t c = 0; c < charLen; ++c) {
                uint16_t code = u16[c];
                if (code < 0x80) {
                    utf8.push_back((char)code);
                } else if (code < 0x800) {
                    utf8.push_back((char)(0xC0 | (code >> 6)));
                    utf8.push_back((char)(0x80 | (code & 0x3F)));
                } else {
                    utf8.push_back((char)(0xE0 | (code >> 12)));
                    utf8.push_back((char)(0x80 | ((code >> 6) & 0x3F)));
                    utf8.push_back((char)(0x80 | (code & 0x3F)));
                }
            }
            outStrings[i] = utf8;
        }
    }
    return true;
}

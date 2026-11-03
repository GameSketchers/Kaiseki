#include "CryptoEngine.h"
#include <sstream>
#include <iomanip>

#pragma comment(lib, "advapi32.lib")

std::string CryptoEngine::ComputeHash(const uint8_t* data, size_t size, ALG_ID alg) {
    if (!data || size == 0) return "";
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) return "";
    if (!CryptCreateHash(hProv, alg, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return "";
    }
    CryptHashData(hHash, data, (DWORD)size, 0);
    BYTE hash[64];
    DWORD hashLen = sizeof(hash);
    CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0);
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    std::ostringstream ss;
    for (DWORD i = 0; i < hashLen; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

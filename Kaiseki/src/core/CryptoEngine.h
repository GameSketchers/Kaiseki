#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#include <string>
#include <cstdint>

class CryptoEngine {
public:
    static std::string ComputeHash(const uint8_t* data, size_t size, ALG_ID alg);
};

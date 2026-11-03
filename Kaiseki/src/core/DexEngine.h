#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <string>
#include <vector>
#include <cstdint>
#include "ApkCoreApi.h"

class DexEngine {
public:
    static bool Parse(const std::string& name, const uint8_t* data, size_t size, DexDataInfo& outInfo);
};

#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>
#include "KaisekiCommon.h"

#ifdef KAISEKIDEX_EXPORTS
#define KAISEKI_DEX_API __declspec(dllexport)
#else
#define KAISEKI_DEX_API __declspec(dllimport)
#endif

struct DexClassSummary {
    std::string className;       // e.g. "com.example.app.MainActivity"
    std::string rawDescriptor;   // e.g. "Lcom/example/app/MainActivity;"
    std::string superClassName;  // e.g. "android.app.Activity"
    std::string sourceFile;      // e.g. "MainActivity.java"
    uint32_t accessFlags = 0;
    int methodCount = 0;
    int fieldCount = 0;
};

struct DexDisassemblyOutput {
    uint32_t structSize = sizeof(DexDisassemblyOutput);
    bool success = false;
    std::string errorMessage;
    std::string smaliCode;
    std::string javaCode;
    std::vector<DexClassSummary> classes;
};

struct DexXrefItem {
    std::string callerClass;
    std::string callerMethod;
    int lineNumber = 0;
    std::string matchedInstruction;
};

extern "C" {
    KAISEKI_DEX_API bool KaisekiDex_Disassemble(const uint8_t* dexData, size_t dexSize, DexDisassemblyOutput* outOutput);
    KAISEKI_DEX_API bool KaisekiDex_DisassembleFile(const wchar_t* dexFilePath, DexDisassemblyOutput* outOutput);
    KAISEKI_DEX_API void KaisekiDex_FreeDisassembly(DexDisassemblyOutput* output);
    KAISEKI_DEX_API bool KaisekiDex_SmaliToJava(const char* smaliCode, std::string* outJavaCode);
    KAISEKI_DEX_API bool KaisekiDex_DecompileClass(const uint8_t* dexData, size_t dexSize, const char* fullClassName, std::string* outJavaCode, std::string* outSmaliCode, std::string* outFridaScript);
    KAISEKI_DEX_API bool KaisekiDex_DecompileAllClassesParallel(const uint8_t* dexData, size_t dexSize, int threadCount, std::vector<std::string>* outClassNames, std::vector<std::string>* outJavaCodes, std::vector<std::string>* outSmaliCodes, std::vector<std::string>* outFridaCodes);
    KAISEKI_DEX_API void KaisekiDex_FreeDecompileStrings(std::string* javaCode, std::string* smaliCode, std::string* fridaScript);
    KAISEKI_DEX_API bool KaisekiDex_FindXrefs(const char* targetSymbol, const char* fullSmaliOrJavaCode, std::vector<DexXrefItem>* outXrefs, std::string* outFormattedReport);
    KAISEKI_DEX_API void KaisekiDex_FreeXrefs(std::vector<DexXrefItem>* xrefs, std::string* formattedReport);
    KAISEKI_DEX_API bool KaisekiDex_GenerateFridaScript(const char* fullClassName, const std::vector<std::string>& methodNames, std::string* outScript);
    KAISEKI_DEX_API bool KaisekiDex_GenerateXposedSnippet(const char* fullClassName, const std::vector<std::string>& methodNames, std::string* outScript);
    KAISEKI_DEX_API bool KaisekiDex_GenerateMethodCFG(const char* fullClassName, std::string* outCfg);
    KAISEKI_DEX_API void KaisekiDex_FreeString(std::string* s);
    KAISEKI_DEX_API const char* KaisekiDex_GetVersion();
}

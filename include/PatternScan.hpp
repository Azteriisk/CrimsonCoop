#pragma once

#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace CrimsonCoop {

struct PatternByte {
    uint8_t value{0};
    bool isWildcard{false};
};

class PatternScanner {
public:
    // Parses IDA-style pattern strings like "48 89 05 ?? ?? ?? ?? 48 8D"
    static std::vector<PatternByte> ParsePattern(const std::string& patternStr);

    // Scans a specific module (or main game module) for a pattern
    static uintptr_t Scan(HMODULE hModule, const std::string& patternStr);

    // Scans across a candidate ladder (first unique match wins)
    static uintptr_t ScanLadder(HMODULE hModule, const std::vector<std::string>& candidates);

    // Resolves RIP-relative addressing: [InstructionAddress + InstructionLen + disp32]
    static uintptr_t ResolveRipRelative(uintptr_t instructionAddress, int32_t dispOffset, int32_t instructionLength);

    // Resolves global pointer from a RIP-relative instruction
    static uintptr_t* ResolveRipPointer(uintptr_t instructionAddress, int32_t dispOffset, int32_t instructionLength);
};

} // namespace CrimsonCoop

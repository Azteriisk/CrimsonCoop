#include "PatternScan.hpp"
#include <psapi.h>
#include <sstream>
#include <iostream>

namespace CrimsonCoop {

std::vector<PatternByte> PatternScanner::ParsePattern(const std::string& patternStr) {
    std::vector<PatternByte> result;
    std::istringstream stream(patternStr);
    std::string token;

    while (stream >> token) {
        if (token == "?" || token == "??") {
            result.push_back({0, true});
        } else {
            uint8_t byteVal = static_cast<uint8_t>(std::strtoul(token.c_str(), nullptr, 16));
            result.push_back({byteVal, false});
        }
    }
    return result;
}

uintptr_t PatternScanner::Scan(HMODULE hModule, const std::string& patternStr) {
    if (!hModule) {
        hModule = GetModuleHandle(nullptr); // Main game .exe
    }

    MODULEINFO modInfo{};
    if (!GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(MODULEINFO))) {
        return 0;
    }

    const uint8_t* base = reinterpret_cast<const uint8_t*>(modInfo.lpBaseOfDll);
    size_t size = modInfo.SizeOfImage;

    auto pattern = ParsePattern(patternStr);
    if (pattern.empty() || size < pattern.size()) {
        return 0;
    }

    const size_t scanLimit = size - pattern.size();
    for (size_t i = 0; i < scanLimit; ++i) {
        bool match = true;
        for (size_t j = 0; j < pattern.size(); ++j) {
            if (!pattern[j].isWildcard && base[i + j] != pattern[j].value) {
                match = false;
                break;
            }
        }
        if (match) {
            return reinterpret_cast<uintptr_t>(base + i);
        }
    }

    return 0;
}

uintptr_t PatternScanner::ScanLadder(HMODULE hModule, const std::vector<std::string>& candidates) {
    for (const auto& candidate : candidates) {
        uintptr_t match = Scan(hModule, candidate);
        if (match != 0) {
            return match;
        }
    }
    return 0;
}

uintptr_t PatternScanner::ResolveRipRelative(uintptr_t instructionAddress, int32_t dispOffset, int32_t instructionLength) {
    if (instructionAddress == 0) return 0;
    int32_t disp32 = *reinterpret_cast<const int32_t*>(instructionAddress + dispOffset);
    return instructionAddress + instructionLength + disp32;
}

uintptr_t* PatternScanner::ResolveRipPointer(uintptr_t instructionAddress, int32_t dispOffset, int32_t instructionLength) {
    uintptr_t target = ResolveRipRelative(instructionAddress, dispOffset, instructionLength);
    return reinterpret_cast<uintptr_t*>(target);
}

} // namespace CrimsonCoop

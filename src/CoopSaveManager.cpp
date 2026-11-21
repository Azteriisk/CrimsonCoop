#include "CoopSaveManager.hpp"
#include <fstream>
#include <iostream>

namespace CrimsonCoop {

uint32_t CoopSaveManager::ComputeChecksum(const uint8_t* data, size_t length) {
    // Standard CRC32 / Adler-32 style simple checksum for file integrity
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < length; ++i) {
        a = (a + data[i]) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

bool CoopSaveManager::WriteCoopSave(const std::string& filePath, const CoopSaveData& data) {
    std::ofstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    CoopFileHeader header = data.header;
    header.magic = 0x50434443;
    header.formatVersion = 1;

    // Write header placeholder
    file.write(reinterpret_cast<const char*>(&header), sizeof(CoopFileHeader));

    // Write character counts and data
    uint32_t charCount = static_cast<uint32_t>(data.characters.size());
    file.write(reinterpret_cast<const char*>(&charCount), sizeof(charCount));
    for (const auto& c : data.characters) {
        file.write(reinterpret_cast<const char*>(&c), sizeof(CharacterProgressionData));
    }

    // Write quest flags
    uint32_t questCount = static_cast<uint32_t>(data.questFlags.size());
    file.write(reinterpret_cast<const char*>(&questCount), sizeof(questCount));
    for (const auto& q : data.questFlags) {
        file.write(reinterpret_cast<const char*>(&q), sizeof(WorldQuestFlag));
    }

    // Write fast travel points
    uint32_t ftCount = static_cast<uint32_t>(data.unlockedFastTravelPoints.size());
    file.write(reinterpret_cast<const char*>(&ftCount), sizeof(ftCount));
    if (ftCount > 0) {
        file.write(reinterpret_cast<const char*>(data.unlockedFastTravelPoints.data()),
                   ftCount * sizeof(uint32_t));
    }

    return file.good();
}

bool CoopSaveManager::ReadCoopSave(const std::string& filePath, CoopSaveData& outData, std::string& outError) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        outError = "Could not open save file at " + filePath;
        return false;
    }

    CoopFileHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(CoopFileHeader));
    if (!file.good()) {
        outError = "Incomplete save file header.";
        return false;
    }

    if (header.magic != 0x50434443) {
        outError = "Invalid file signature: not a valid .cdcoop save!";
        return false;
    }

    outData.header = header;

    // Read characters
    uint32_t charCount = 0;
    file.read(reinterpret_cast<char*>(&charCount), sizeof(charCount));
    outData.characters.resize(charCount);
    for (uint32_t i = 0; i < charCount; ++i) {
        file.read(reinterpret_cast<char*>(&outData.characters[i]), sizeof(CharacterProgressionData));
    }

    // Read quest flags
    uint32_t questCount = 0;
    file.read(reinterpret_cast<char*>(&questCount), sizeof(questCount));
    outData.questFlags.resize(questCount);
    for (uint32_t i = 0; i < questCount; ++i) {
        file.read(reinterpret_cast<char*>(&outData.questFlags[i]), sizeof(WorldQuestFlag));
    }

    // Read fast travel points
    uint32_t ftCount = 0;
    file.read(reinterpret_cast<char*>(&ftCount), sizeof(ftCount));
    outData.unlockedFastTravelPoints.resize(ftCount);
    if (ftCount > 0) {
        file.read(reinterpret_cast<char*>(outData.unlockedFastTravelPoints.data()),
                  ftCount * sizeof(uint32_t));
    }

    if (!file.good()) {
        outError = "Save file was truncated or corrupted during read.";
        return false;
    }

    return true;
}

} // namespace CrimsonCoop

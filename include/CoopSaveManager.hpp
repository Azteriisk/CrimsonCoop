#pragma once

#include "Common.hpp"
#include <string>
#include <vector>

namespace CrimsonCoop {

#pragma pack(push, 1)
struct CoopFileHeader {
    uint32_t magic{0x50434443}; // 'CDCP' (Crimson Desert Co-Op)
    uint32_t formatVersion{1};
    uint64_t hostSteamId{0};
    uint64_t saveTimestamp{0};
    uint32_t checksum{0};
};

struct CharacterProgressionData {
    CharacterType character;
    uint32_t      level;
    uint64_t      experience;
    uint32_t      health;
    uint32_t      maxHealth;
    uint32_t      stamina;
    uint32_t      skillPointsSpent;
    uint32_t      equippedWeaponId;
    uint32_t      equippedArmorId;
};

struct WorldQuestFlag {
    uint32_t questId;
    uint16_t stageId;
    uint8_t  isCompleted;
};
#pragma pack(pop)

struct CoopSaveData {
    CoopFileHeader header;
    std::vector<CharacterProgressionData> characters; // Kliff, Damiane, Oongka
    std::vector<WorldQuestFlag> questFlags;           // Authoritative quest progression
    std::vector<uint32_t> unlockedFastTravelPoints;
};

class CoopSaveManager {
public:
    // Writes an isolated .cdcoop binary save file
    static bool WriteCoopSave(const std::string& filePath, const CoopSaveData& data);

    // Reads and validates an isolated .cdcoop binary save file
    static bool ReadCoopSave(const std::string& filePath, CoopSaveData& outData, std::string& outError);

private:
    static uint32_t ComputeChecksum(const uint8_t* data, size_t length);
};

} // namespace CrimsonCoop

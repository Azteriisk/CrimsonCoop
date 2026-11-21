#pragma once

#include "Common.hpp"
#include <string>
#include <cstdint>

namespace CrimsonCoop {

struct ModConfig {
    // [General]
    CharacterType preferredCharacter{CharacterType::Damiane};
    bool autoSelectAvailable{true};

    // [Network]
    uint32_t tickRateHz{30};
    uint32_t interpolationDelayMs{50};
    bool useSteamRelay{true};

    // [UI]
    uint32_t toggleKey{0x71}; // VK_F2
    bool showPartyHUD{true};
    float hudOpacity{0.85f};

    // [Advanced]
    bool debugConsole{false};
    uint32_t selfHealWindow{0x200};
};

class Config {
public:
    static bool Load(const std::string& configPath = "CrimsonCoop.ini");
    static bool Save(const std::string& configPath = "CrimsonCoop.ini");

    static const ModConfig& Get() { return s_config; }
    static void Set(const ModConfig& config) { s_config = config; }

    static CharacterType ParseCharacter(const std::string& name);
    static std::string CharacterToConfigString(CharacterType type);

private:
    static ModConfig s_config;
    static std::string s_configPath;
};

} // namespace CrimsonCoop

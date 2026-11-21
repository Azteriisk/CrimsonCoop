#include "Config.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace CrimsonCoop {

ModConfig Config::s_config{};
std::string Config::s_configPath = "CrimsonCoop.ini";

CharacterType Config::ParseCharacter(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "kliff" || lower == "cliff") return CharacterType::Kliff;
    if (lower == "damiane" || lower == "daphene" || lower == "damian") return CharacterType::Damiane;
    if (lower == "oongka" || lower == "orc" || lower == "ogre") return CharacterType::Oongka;
    return CharacterType::None;
}

std::string Config::CharacterToConfigString(CharacterType type) {
    switch (type) {
        case CharacterType::Kliff:   return "Kliff";
        case CharacterType::Damiane: return "Damiane";
        case CharacterType::Oongka:  return "Oongka";
        default:                     return "None";
    }
}

static std::string Trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

bool Config::Load(const std::string& configPath) {
    s_configPath = configPath;
    std::ifstream file(configPath);
    if (!file.is_open()) {
        // Create default config file if it doesn't exist
        std::cout << "[Config] No " << configPath << " found. Writing default configuration.\n";
        return Save(configPath);
    }

    std::string line;
    std::string currentSection;

    while (std::getline(file, line)) {
        // Strip comments
        size_t commentPos = line.find(';');
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }
        commentPos = line.find('#');
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }

        line = Trim(line);
        if (line.empty()) continue;

        // Section header
        if (line.front() == '[' && line.back() == ']') {
            currentSection = Trim(line.substr(1, line.length() - 2));
            std::transform(currentSection.begin(), currentSection.end(), currentSection.begin(), ::tolower);
            continue;
        }

        // Key = Value
        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;

        std::string key = Trim(line.substr(0, eqPos));
        std::string value = Trim(line.substr(eqPos + 1));
        std::string lowerKey = key;
        std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);

        if (currentSection == "general") {
            if (lowerKey == "preferredcharacter") {
                s_config.preferredCharacter = ParseCharacter(value);
            } else if (lowerKey == "autoselectavailable") {
                s_config.autoSelectAvailable = (value == "true" || value == "1");
            }
        } else if (currentSection == "network") {
            if (lowerKey == "tickrate") {
                s_config.tickRateHz = std::clamp(static_cast<uint32_t>(std::stoul(value)), 10u, 120u);
            } else if (lowerKey == "interpolationdelayms") {
                s_config.interpolationDelayMs = std::clamp(static_cast<uint32_t>(std::stoul(value)), 10u, 500u);
            } else if (lowerKey == "usesteamrelay") {
                s_config.useSteamRelay = (value == "true" || value == "1");
            }
        } else if (currentSection == "ui") {
            if (lowerKey == "togglekey") {
                s_config.toggleKey = static_cast<uint32_t>(std::strtoul(value.c_str(), nullptr, 0));
            } else if (lowerKey == "showpartyhud") {
                s_config.showPartyHUD = (value == "true" || value == "1");
            } else if (lowerKey == "hudopacity") {
                s_config.hudOpacity = std::clamp(std::stof(value), 0.1f, 1.0f);
            }
        } else if (currentSection == "advanced") {
            if (lowerKey == "debugconsole") {
                s_config.debugConsole = (value == "true" || value == "1");
            } else if (lowerKey == "selfhealwindow") {
                s_config.selfHealWindow = static_cast<uint32_t>(std::strtoul(value.c_str(), nullptr, 0));
            }
        }
    }

    std::cout << "[Config] Loaded configuration from " << configPath << std::endl;
    return true;
}

bool Config::Save(const std::string& configPath) {
    std::ofstream file(configPath);
    if (!file.is_open()) return false;

    file << "; Crimson Desert 3-Player Co-Op Mod Configuration\n";
    file << "; Greymane Trio: Kliff, Damiane, Oongka\n\n";

    file << "[General]\n";
    file << "; Character to request when joining a session (Kliff, Damiane, Oongka)\n";
    file << "PreferredCharacter = " << CharacterToConfigString(s_config.preferredCharacter) << "\n";
    file << "; If your preferred character is already in use by the host, auto-select next available\n";
    file << "AutoSelectAvailable = " << (s_config.autoSelectAvailable ? "true" : "false") << "\n\n";

    file << "[Network]\n";
    file << "; Network replication tickrate in Hz (20, 30, 60)\n";
    file << "TickRate = " << s_config.tickRateHz << "\n";
    file << "; Interpolation buffer window in milliseconds (smoothes remote movement)\n";
    file << "InterpolationDelayMs = " << s_config.interpolationDelayMs << "\n";
    file << "; Route P2P traffic through Valve Relay Network (SDR) for privacy and NAT traversal\n";
    file << "UseSteamRelay = " << (s_config.useSteamRelay ? "true" : "false") << "\n\n";

    file << "[UI]\n";
    file << "; Virtual Key code to toggle overlay (0x71 = VK_F2, 0x2D = VK_INSERT)\n";
    file << "ToggleKey = 0x" << std::hex << s_config.toggleKey << std::dec << "\n";
    file << "ShowPartyHUD = " << (s_config.showPartyHUD ? "true" : "false") << "\n";
    file << "HudOpacity = " << s_config.hudOpacity << "\n\n";

    file << "[Advanced]\n";
    file << "; Allocate developer console on game startup for real-time diagnostics\n";
    file << "DebugConsole = " << (s_config.debugConsole ? "true" : "false") << "\n";
    file << "; BlackSpace Engine AOB self-healing search radius in bytes\n";
    file << "SelfHealWindow = 0x" << std::hex << s_config.selfHealWindow << std::dec << "\n";

    return file.good();
}

} // namespace CrimsonCoop

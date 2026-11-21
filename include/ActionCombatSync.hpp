#pragma once

#include "Common.hpp"
#include "NetworkPackets.hpp"
#include "SteamTransport.hpp"
#include <unordered_map>
#include <mutex>
#include <string>

namespace CrimsonCoop {

namespace CombatActions {
    inline constexpr uint32_t IDLE                = 0x1000;
    inline constexpr uint32_t LIGHT_ATTACK_1      = 0x1001;
    inline constexpr uint32_t LIGHT_ATTACK_2      = 0x1002;
    inline constexpr uint32_t LIGHT_ATTACK_3      = 0x1003;
    inline constexpr uint32_t HEAVY_ATTACK_CHARGE = 0x1010;
    inline constexpr uint32_t HEAVY_ATTACK_SWING  = 0x1011;
    inline constexpr uint32_t DODGE_ROLL          = 0x1020;
    inline constexpr uint32_t PARRY_COUNTER       = 0x1021;
    inline constexpr uint32_t SKILL_ABILITY_1     = 0x1030; // Kliff: Roar / Damiane: Phantom / Oongka: EarthSlam
    inline constexpr uint32_t SKILL_ABILITY_2     = 0x1031;
    inline constexpr uint32_t MOUNT_HORSE         = 0x1040;
    inline constexpr uint32_t DISMOUNT            = 0x1041;
    inline constexpr uint32_t GLIDE_CROW_WINGS    = 0x1050;
}

struct CombatState {
    float currentHealth{3200.0f};
    float maxHealth{3200.0f};
    float currentStamina{100.0f};
    float maxStamina{100.0f};
    uint32_t lastActionId{0};
    bool isStaggered{false};
};

class ActionCombatSync {
public:
    static bool Install();
    static void Remove();

    // Trigger local combat action and broadcast to peers
    static bool TriggerLocalAction(CharacterType character, uint32_t actionId, float playbackSpeed = 1.0f, int32_t targetEntityUid = 0);

    // Apply inbound action from remote peer
    static bool HandleInboundAction(const PktActionSync& pkt);

    // Replicate combat damage event (Host authoritative)
    static bool HandleCombatHit(const PktCombatEventSync& pkt, bool isHost);

    // Query active combat state
    static CombatState GetCombatState(CharacterType character);

private:
    static bool s_isInstalled;
    static std::mutex s_combatMutex;
    static std::unordered_map<CharacterType, CombatState> s_characterStates;
};

} // namespace CrimsonCoop

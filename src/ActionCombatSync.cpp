#include "ActionCombatSync.hpp"
#include "RemoteProxyManager.hpp"
#include "GuiOverlay.hpp"
#include "Logger.hpp"

namespace CrimsonCoop {

bool ActionCombatSync::s_isInstalled = false;
std::mutex ActionCombatSync::s_combatMutex;
std::unordered_map<CharacterType, CombatState> ActionCombatSync::s_characterStates;

bool ActionCombatSync::Install() {
    if (s_isInstalled) return true;

    // Initialize baseline combat states for the Greymane trio
    {
        std::lock_guard<std::mutex> lock(s_combatMutex);
        s_characterStates[CharacterType::Kliff]   = CombatState{3200.0f, 3200.0f, 120.0f, 120.0f, 0, false};
        s_characterStates[CharacterType::Damiane] = CombatState{2400.0f, 2400.0f, 150.0f, 150.0f, 0, false};
        s_characterStates[CharacterType::Oongka]  = CombatState{4500.0f, 4500.0f,  90.0f,  90.0f, 0, false};
    }

    s_isInstalled = true;
    Logger::Log("[CombatSync] Combat replication and animation synchronization installed.\n");
    return true;
}

void ActionCombatSync::Remove() {
    s_isInstalled = false;
}

bool ActionCombatSync::TriggerLocalAction(CharacterType character, uint32_t actionId, float playbackSpeed, int32_t targetEntityUid) {
    {
        std::lock_guard<std::mutex> lock(s_combatMutex);
        s_characterStates[character].lastActionId = actionId;
    }

    PktActionSync pkt{};
    pkt.header.magic[0] = 'C';
    pkt.header.magic[1] = 'D';
    pkt.header.type = PacketType::ActionSync;
    pkt.header.sequenceNumber = 1;
    pkt.header.timestamp = 0.0;
    pkt.character = character;
    pkt.actionId = actionId;
    pkt.playbackSpeed = playbackSpeed;
    pkt.targetEntityUid = targetEntityUid;

    SteamTransport::BroadcastPacket(&pkt, sizeof(pkt), true);
    Logger::Log("[CombatSync] Local %s executed Action 0x%X (Target: %d)\n",
                CharacterToString(character), actionId, targetEntityUid);
    return true;
}

bool ActionCombatSync::HandleInboundAction(const PktActionSync& pkt) {
    {
        std::lock_guard<std::mutex> lock(s_combatMutex);
        s_characterStates[pkt.character].lastActionId = pkt.actionId;
    }

    // Trigger visual animation on the corresponding proxy model
    RemoteProxyManager::Instance().TriggerRemoteAction(0, pkt.actionId, pkt.playbackSpeed, pkt.targetEntityUid);

    Logger::Log("[CombatSync] Inbound Action 0x%X applied for remote %s.\n",
                pkt.actionId, CharacterToString(pkt.character));
    return true;
}

bool ActionCombatSync::HandleCombatHit(const PktCombatEventSync& pkt, bool isHost) {
    std::lock_guard<std::mutex> lock(s_combatMutex);

    Logger::Log("[CombatSync] Combat Hit: Attacker %s (SteamID: %llu) hit Actor 0x%X for %.1f damage (Crit: %d, Fatal: %d)\n",
                CharacterToString(pkt.attackerChar), pkt.attackerSteamId, pkt.targetActorUid,
                pkt.damageAmount, pkt.isCritical, pkt.isFatal);

    if (isHost) {
        // Host broadcasts the authoritative hit result to all clients
        SteamTransport::BroadcastPacket(&pkt, sizeof(pkt), true);
    }

    // Trigger in-game toast or visual feedback
    if (pkt.isFatal) {
        GuiOverlay::ShowToast(std::string("Greymane Takedown: ") + CharacterToString(pkt.attackerChar) + " vanquished enemy!", 3.0);
    }

    return true;
}

CombatState ActionCombatSync::GetCombatState(CharacterType character) {
    std::lock_guard<std::mutex> lock(s_combatMutex);
    auto it = s_characterStates.find(character);
    return (it != s_characterStates.end()) ? it->second : CombatState{};
}

} // namespace CrimsonCoop

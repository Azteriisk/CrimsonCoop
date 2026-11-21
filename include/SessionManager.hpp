#pragma once

#include "Common.hpp"
#include "NetworkPackets.hpp"
#include <unordered_map>
#include <vector>
#include <optional>
#include <string>

namespace CrimsonCoop {

struct PlayerSessionInfo {
    PlayerId      steamId;
    CharacterType character;
    std::string   playerName;
    bool          isHost;
};

class SessionManager {
public:
    SessionManager(PlayerId hostSteamId, CharacterType hostCharacter, const std::string& hostName);

    // Join Arbitration: Catches character conflicts and rejects or auto-corrects
    PktHandshakeResponse HandleJoinRequest(PlayerId steamId, CharacterType requestedCharacter, const std::string& playerName);

    // Remove player on disconnect
    void HandleDisconnect(PlayerId steamId);

    // In-game Switch Guard: Intercepts single-player radial menu / hotkey attempts
    bool CanLocalPlayerSwitchTo(PlayerId localSteamId, CharacterType targetCharacter, std::string& outReason) const;

    // Switch to an unpiloted/reserve character (Essential for 2-player Duo sessions!)
    bool SwitchToUnoccupied(PlayerId localSteamId, CharacterType targetCharacter, std::string& outReason);

    // Voluntary Role Swap: Allows two active players to trade characters mid-game
    bool InitiateSwapRequest(PlayerId requesterId, PlayerId targetId);
    bool ResolveSwapResponse(PlayerId targetId, bool accepted);

    // Roster query helpers
    bool IsCharacterInUse(CharacterType character) const;
    uint8_t GetAvailableCharacterMask() const;
    std::vector<CharacterType> GetUnoccupiedCharacters() const;
    std::optional<CharacterType> GetFirstAvailableCharacter() const;
    std::optional<PlayerId> GetPlayerUsingCharacter(CharacterType character) const;
    size_t GetActivePlayerCount() const;

    // 2-Player Duo helper: true when exactly 2 players are connected
    bool IsDuoSession() const { return m_roster.size() == 2; }

    PlayerId GetHostSteamId() const { return m_hostSteamId; }
    const std::unordered_map<PlayerId, PlayerSessionInfo>& GetRoster() const { return m_roster; }

    // Local player helpers
    CharacterType GetLocalCharacter() const {
        auto it = m_roster.find(m_hostSteamId);
        return (it != m_roster.end()) ? it->second.character : CharacterType::Kliff;
    }

    bool SwitchLocalCharacter(CharacterType targetChar, std::string& outReason) {
        return SwitchToUnoccupied(m_hostSteamId, targetChar, outReason);
    }

    bool SwapToReserve() {
        auto reserves = GetUnoccupiedCharacters();
        if (reserves.empty()) return false;
        std::string reason;
        return SwitchToUnoccupied(m_hostSteamId, reserves[0], reason);
    }

private:
    PlayerId m_hostSteamId;
    std::unordered_map<PlayerId, PlayerSessionInfo> m_roster;
    std::optional<std::pair<PlayerId, PlayerId>> m_pendingSwap; // requester, target
};

} // namespace CrimsonCoop

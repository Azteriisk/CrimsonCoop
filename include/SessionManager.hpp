#pragma once

#include "Common.hpp"
#include "NetworkPackets.hpp"
#include <unordered_map>
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

    // Voluntary Role Swap: Allows two players to swap characters mid-game
    bool InitiateSwapRequest(PlayerId requesterId, PlayerId targetId);
    bool ResolveSwapResponse(PlayerId targetId, bool accepted);

    // Roster query helpers
    bool IsCharacterInUse(CharacterType character) const;
    uint8_t GetAvailableCharacterMask() const;
    std::optional<CharacterType> GetFirstAvailableCharacter() const;
    std::optional<PlayerId> GetPlayerUsingCharacter(CharacterType character) const;
    size_t GetActivePlayerCount() const;

    PlayerId GetHostSteamId() const { return m_hostSteamId; }
    const std::unordered_map<PlayerId, PlayerSessionInfo>& GetRoster() const { return m_roster; }

private:
    PlayerId m_hostSteamId;
    std::unordered_map<PlayerId, PlayerSessionInfo> m_roster;
    std::optional<std::pair<PlayerId, PlayerId>> m_pendingSwap; // requester, target
};

} // namespace CrimsonCoop

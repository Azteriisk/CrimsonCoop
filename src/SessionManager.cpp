#include "SessionManager.hpp"
#include <iostream>

namespace CrimsonCoop {

SessionManager::SessionManager(PlayerId hostSteamId, CharacterType hostCharacter, const std::string& hostName)
    : m_hostSteamId(hostSteamId) {
    m_roster[hostSteamId] = PlayerSessionInfo{
        .steamId = hostSteamId,
        .character = hostCharacter,
        .playerName = hostName,
        .isHost = true
    };
}

bool SessionManager::IsCharacterInUse(CharacterType character) const {
    if (character == CharacterType::None) return false;
    for (const auto& [_, info] : m_roster) {
        if (info.character == character) {
            return true;
        }
    }
    return false;
}

uint8_t SessionManager::GetAvailableCharacterMask() const {
    uint8_t mask = 0;
    if (!IsCharacterInUse(CharacterType::Kliff))   mask |= (1 << 0);
    if (!IsCharacterInUse(CharacterType::Damiane)) mask |= (1 << 1);
    if (!IsCharacterInUse(CharacterType::Oongka))  mask |= (1 << 2);
    return mask;
}

std::vector<CharacterType> SessionManager::GetUnoccupiedCharacters() const {
    std::vector<CharacterType> unoccupied;
    if (!IsCharacterInUse(CharacterType::Kliff))   unoccupied.push_back(CharacterType::Kliff);
    if (!IsCharacterInUse(CharacterType::Damiane)) unoccupied.push_back(CharacterType::Damiane);
    if (!IsCharacterInUse(CharacterType::Oongka))  unoccupied.push_back(CharacterType::Oongka);
    return unoccupied;
}

bool SessionManager::SwitchToUnoccupied(PlayerId localSteamId, CharacterType targetCharacter, std::string& outReason) {
    if (!CanLocalPlayerSwitchTo(localSteamId, targetCharacter, outReason)) {
        return false;
    }

    auto it = m_roster.find(localSteamId);
    if (it == m_roster.end()) {
        outReason = "Player not in session.";
        return false;
    }

    CharacterType oldChar = it->second.character;
    it->second.character = targetCharacter;
    std::cout << "[SessionManager] " << it->second.playerName << " switched from " 
              << CharacterToString(oldChar) << " to reserve character " 
              << CharacterToString(targetCharacter) << ".\n";
    return true;
}

std::optional<CharacterType> SessionManager::GetFirstAvailableCharacter() const {
    if (!IsCharacterInUse(CharacterType::Kliff))   return CharacterType::Kliff;
    if (!IsCharacterInUse(CharacterType::Damiane)) return CharacterType::Damiane;
    if (!IsCharacterInUse(CharacterType::Oongka))  return CharacterType::Oongka;
    return std::nullopt;
}

std::optional<PlayerId> SessionManager::GetPlayerUsingCharacter(CharacterType character) const {
    for (const auto& [id, info] : m_roster) {
        if (info.character == character) return id;
    }
    return std::nullopt;
}

size_t SessionManager::GetActivePlayerCount() const {
    return m_roster.size();
}

PktHandshakeResponse SessionManager::HandleJoinRequest(PlayerId steamId, CharacterType requestedCharacter, const std::string& playerName) {
    PktHandshakeResponse response{};
    response.header.magic[0] = 'C';
    response.header.magic[1] = 'D';
    response.header.type = PacketType::HandshakeResponse;

    // Check if session is already at capacity (3 players max)
    if (m_roster.size() >= MAX_PLAYERS && m_roster.find(steamId) == m_roster.end()) {
        response.status = HandshakeStatus::LobbyFull;
        response.assignedCharacter = CharacterType::None;
        response.availableCharactersMask = 0;
        return response;
    }

    // Check for character conflict
    if (IsCharacterInUse(requestedCharacter)) {
        response.status = HandshakeStatus::ConflictCharacter;
        response.assignedCharacter = CharacterType::None;
        response.availableCharactersMask = GetAvailableCharacterMask();
        return response;
    }

    // If no character specified, auto-assign first available
    CharacterType finalChar = requestedCharacter;
    if (finalChar == CharacterType::None) {
        auto available = GetFirstAvailableCharacter();
        if (!available.has_value()) {
            response.status = HandshakeStatus::LobbyFull;
            return response;
        }
        finalChar = available.value();
    }

    // Register into roster
    m_roster[steamId] = PlayerSessionInfo{
        .steamId = steamId,
        .character = finalChar,
        .playerName = playerName,
        .isHost = false
    };

    response.status = HandshakeStatus::Accepted;
    response.assignedCharacter = finalChar;
    response.availableCharactersMask = GetAvailableCharacterMask();
    return response;
}

void SessionManager::HandleDisconnect(PlayerId steamId) {
    m_roster.erase(steamId);
    if (m_pendingSwap.has_value()) {
        if (m_pendingSwap->first == steamId || m_pendingSwap->second == steamId) {
            m_pendingSwap.reset();
        }
    }
}

bool SessionManager::CanLocalPlayerSwitchTo(PlayerId localSteamId, CharacterType targetCharacter, std::string& outReason) const {
    auto it = m_roster.find(localSteamId);
    if (it == m_roster.end()) {
        outReason = "Local player is not registered in co-op session.";
        return false;
    }

    if (it->second.character == targetCharacter) {
        outReason = "Already playing as this character.";
        return false;
    }

    auto occupant = GetPlayerUsingCharacter(targetCharacter);
    if (occupant.has_value()) {
        auto occIt = m_roster.find(occupant.value());
        std::string occName = (occIt != m_roster.end()) ? occIt->second.playerName : "Another player";
        outReason = std::string(CharacterToString(targetCharacter)) + " is already controlled by " + occName + "!";
        return false;
    }

    return true;
}

bool SessionManager::InitiateSwapRequest(PlayerId requesterId, PlayerId targetId) {
    if (m_roster.find(requesterId) == m_roster.end() || m_roster.find(targetId) == m_roster.end()) {
        return false;
    }
    m_pendingSwap = {requesterId, targetId};
    return true;
}

bool SessionManager::ResolveSwapResponse(PlayerId targetId, bool accepted) {
    if (!m_pendingSwap.has_value() || m_pendingSwap->second != targetId) {
        return false;
    }

    if (accepted) {
        PlayerId requesterId = m_pendingSwap->first;
        CharacterType temp = m_roster[requesterId].character;
        m_roster[requesterId].character = m_roster[targetId].character;
        m_roster[targetId].character = temp;
    }

    m_pendingSwap.reset();
    return accepted;
}

} // namespace CrimsonCoop

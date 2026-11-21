#pragma once

#include "Common.hpp"
#include "NetworkPackets.hpp"
#include "SessionManager.hpp"
#include "Interpolation.hpp"
#include <string>
#include <vector>
#include <queue>
#include <functional>
#include <mutex>

namespace CrimsonCoop {

enum class ConnectionState : uint8_t {
    Disconnected,
    Connecting,
    Connected,
    HostLobbyActive
};

struct InboundPacket {
    PlayerId senderSteamId{0};
    std::vector<uint8_t> payload;
};

class SteamTransport {
public:
    static bool Initialize(PlayerId localSteamId, bool isHost = false);
    static void Shutdown();

    // Lobby Management
    static bool CreateCoopLobby(uint32_t maxMembers = 3);
    static bool JoinCoopLobby(uint64_t lobbyId);
    static void LeaveLobby();

    // Packet Transmission
    static bool SendPacket(PlayerId targetPeer, const void* data, size_t size, bool reliable = true);
    static bool BroadcastPacket(const void* data, size_t size, bool reliable = true, PlayerId excludePeer = 0);

    // Polling & Dispatch Loop (called every frame on mod tick)
    static void PollAndDispatch(SessionManager* pSession, TransformInterpolator* pInterpolator);

    // Callback registrations
    static void SetOnConflictCallback(std::function<void(uint8_t)> callback) { s_onConflictCallback = callback; }
    static void SetOnSwapRequestCallback(std::function<void(PlayerId, CharacterType)> callback) { s_onSwapRequestCallback = callback; }

    // Test / Mock injection for offline testing
    static void InjectMockPacket(PlayerId senderId, const void* data, size_t size);

    static ConnectionState GetState() { return s_state; }
    static PlayerId GetLocalSteamId() { return s_localSteamId; }
    static uint64_t GetActiveLobbyId() { return s_activeLobbyId; }

private:
    static void DispatchPacket(const InboundPacket& pkt, SessionManager* pSession, TransformInterpolator* pInterpolator);

    static bool s_isInitialized;
    static ConnectionState s_state;
    static PlayerId s_localSteamId;
    static uint64_t s_activeLobbyId;
    static bool s_isHost;

    static std::mutex s_queueMutex;
    static std::queue<InboundPacket> s_mockPacketQueue;

    static std::function<void(uint8_t)> s_onConflictCallback;
    static std::function<void(PlayerId, CharacterType)> s_onSwapRequestCallback;
};

} // namespace CrimsonCoop

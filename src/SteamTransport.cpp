#include "SteamTransport.hpp"
#include "GuiOverlay.hpp"
#include "CoopSaveManager.hpp"
#include <iostream>

namespace CrimsonCoop {

bool SteamTransport::s_isInitialized = false;
ConnectionState SteamTransport::s_state = ConnectionState::Disconnected;
PlayerId SteamTransport::s_localSteamId = 0;
uint64_t SteamTransport::s_activeLobbyId = 0;
bool SteamTransport::s_isHost = false;

std::mutex SteamTransport::s_queueMutex;
std::queue<InboundPacket> SteamTransport::s_mockPacketQueue;

std::function<void(uint8_t)> SteamTransport::s_onConflictCallback = nullptr;
std::function<void(PlayerId, CharacterType)> SteamTransport::s_onSwapRequestCallback = nullptr;

bool SteamTransport::Initialize(PlayerId localSteamId, bool isHost) {
    s_localSteamId = localSteamId;
    s_isHost = isHost;
    s_state = isHost ? ConnectionState::HostLobbyActive : ConnectionState::Connected;
    s_isInitialized = true;

    std::cout << "[SteamTransport] Initialized transport for SteamID: " 
              << localSteamId << " (" << (isHost ? "Host" : "Peer") << ")\n";
    return true;
}

void SteamTransport::Shutdown() {
    LeaveLobby();
    s_isInitialized = false;
    s_state = ConnectionState::Disconnected;
    std::cout << "[SteamTransport] Transport shut down.\n";
}

bool SteamTransport::CreateCoopLobby(uint32_t maxMembers) {
    (void)maxMembers;
    s_isHost = true;
    s_activeLobbyId = 1097752410ULL; // Simulated Steam Lobby ID
    s_state = ConnectionState::HostLobbyActive;
    std::cout << "[SteamTransport] Created Steam Co-Op Lobby: " << s_activeLobbyId << " (Max 3 Greymanes)\n";
    return true;
}

bool SteamTransport::JoinCoopLobby(uint64_t lobbyId) {
    s_activeLobbyId = lobbyId;
    s_isHost = false;
    s_state = ConnectionState::Connecting;
    std::cout << "[SteamTransport] Joining Steam Lobby: " << lobbyId << "...\n";
    return true;
}

void SteamTransport::LeaveLobby() {
    if (s_activeLobbyId != 0) {
        std::cout << "[SteamTransport] Leaving Steam Lobby: " << s_activeLobbyId << "\n";
        s_activeLobbyId = 0;
    }
    s_state = ConnectionState::Disconnected;
}

bool SteamTransport::SendPacket(PlayerId targetPeer, const void* data, size_t size, bool reliable) {
    (void)reliable;
    if (!s_isInitialized || size < sizeof(PacketHeader)) return false;

    // In a live Steam build, calls SteamNetworkingSockets()->SendMessageToConnection()
    // For local mock verification, inject into queue
    InjectMockPacket(s_localSteamId, data, size);
    (void)targetPeer;
    return true;
}

bool SteamTransport::BroadcastPacket(const void* data, size_t size, bool reliable, PlayerId excludePeer) {
    (void)reliable;
    (void)excludePeer;
    if (!s_isInitialized || size < sizeof(PacketHeader)) return false;

    InjectMockPacket(s_localSteamId, data, size);
    return true;
}

void SteamTransport::InjectMockPacket(PlayerId senderId, const void* data, size_t size) {
    std::lock_guard<std::mutex> lock(s_queueMutex);
    InboundPacket pkt;
    pkt.senderSteamId = senderId;
    pkt.payload.resize(size);
    memcpy(pkt.payload.data(), data, size);
    s_mockPacketQueue.push(pkt);
}

void SteamTransport::PollAndDispatch(SessionManager* pSession, TransformInterpolator* pInterpolator) {
    if (!s_isInitialized) return;

    std::queue<InboundPacket> packetsToProcess;
    {
        std::lock_guard<std::mutex> lock(s_queueMutex);
        packetsToProcess.swap(s_mockPacketQueue);
    }

    while (!packetsToProcess.empty()) {
        const auto& pkt = packetsToProcess.front();
        DispatchPacket(pkt, pSession, pInterpolator);
        packetsToProcess.pop();
    }
}

void SteamTransport::DispatchPacket(const InboundPacket& pkt, SessionManager* pSession, TransformInterpolator* pInterpolator) {
    if (pkt.payload.size() < sizeof(PacketHeader)) return;

    const auto* pHeader = reinterpret_cast<const PacketHeader*>(pkt.payload.data());
    if (pHeader->magic[0] != 'C' || pHeader->magic[1] != 'D') return;

    switch (pHeader->type) {
        case PacketType::HandshakeRequest: {
            if (pkt.payload.size() < sizeof(PktHandshakeRequest) || !pSession) break;
            const auto* req = reinterpret_cast<const PktHandshakeRequest*>(pkt.payload.data());
            std::cout << "[SteamTransport] Received HandshakeRequest from " << req->steamId 
                      << " for " << CharacterToString(req->desiredCharacter) << "\n";

            auto resp = pSession->HandleJoinRequest(req->steamId, req->desiredCharacter, "PeerPlayer");
            SendPacket(req->steamId, &resp, sizeof(resp), true);
            break;
        }

        case PacketType::HandshakeResponse: {
            if (pkt.payload.size() < sizeof(PktHandshakeResponse)) break;
            const auto* resp = reinterpret_cast<const PktHandshakeResponse*>(pkt.payload.data());
            std::cout << "[SteamTransport] Received HandshakeResponse: Status " << static_cast<int>(resp->status) << "\n";

            if (resp->status == HandshakeStatus::ConflictCharacter) {
                if (s_onConflictCallback) {
                    s_onConflictCallback(resp->availableCharactersMask);
                }
            } else if (resp->status == HandshakeStatus::Accepted) {
                s_state = ConnectionState::Connected;
                std::cout << "[SteamTransport] Session connected! Deployed as " 
                          << CharacterToString(resp->assignedCharacter) << ".\n";
            }
            break;
        }

        case PacketType::CharacterSwapReq: {
            if (pkt.payload.size() < sizeof(PktCharacterSwapRequest)) break;
            const auto* swapReq = reinterpret_cast<const PktCharacterSwapRequest*>(pkt.payload.data());
            if (s_onSwapRequestCallback) {
                s_onSwapRequestCallback(swapReq->requesterSteamId, CharacterType::None);
            }
            break;
        }

        case PacketType::TransformSync: {
            if (pkt.payload.size() < sizeof(PktTransformSync) || !pInterpolator) break;
            const auto* sync = reinterpret_cast<const PktTransformSync*>(pkt.payload.data());

            TransformSnapshot snapshot;
            snapshot.timestamp = sync->header.timestamp;
            snapshot.position = sync->position;
            snapshot.rotation = sync->rotation;
            snapshot.velocity = sync->velocity;
            pInterpolator->AddSnapshot(snapshot);
            break;
        }

        case PacketType::SaveCheckpointSync: {
            std::cout << "[SteamTransport] Received SaveCheckpointSync. Serializing local co-op progress.\n";
            break;
        }

        default:
            break;
    }
}

} // namespace CrimsonCoop

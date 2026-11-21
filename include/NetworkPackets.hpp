#pragma once

#include "Common.hpp"

namespace CrimsonCoop {

enum class PacketType : uint8_t {
    HandshakeRequest   = 0x01,
    HandshakeResponse  = 0x02,
    CharacterSwapReq   = 0x03,
    CharacterSwapAck   = 0x04,
    TransformSync      = 0x05,
    ActionSync         = 0x06,
    SaveCheckpointSync = 0x07,
    ReserveSwitchSync  = 0x08
};

enum class HandshakeStatus : uint8_t {
    Accepted          = 0x00,
    ConflictCharacter = 0x01,
    LobbyFull         = 0x02,
    VersionMismatch   = 0x03
};

#pragma pack(push, 1)

struct PacketHeader {
    uint8_t    magic[2]{'C', 'D'};
    PacketType type;
    uint32_t   sequenceNumber;
    double     timestamp;
};

// Handshake: Client requests to join with a desired Greymane character
struct PktHandshakeRequest {
    PacketHeader  header;
    PlayerId      steamId;
    CharacterType desiredCharacter;
    uint32_t      modVersion;
};

// Handshake Response: Host accepts or tells client which characters are available
struct PktHandshakeResponse {
    PacketHeader    header;
    HandshakeStatus status;
    CharacterType   assignedCharacter;
    uint8_t         availableCharactersMask; // Bit 1: Kliff, Bit 2: Damiane, Bit 3: Oongka
};

// Runtime Character Swap Request (Player asks teammate to trade characters)
struct PktCharacterSwapRequest {
    PacketHeader header;
    PlayerId     requesterSteamId;
    PlayerId     targetSteamId;
};

struct PktCharacterSwapResponse {
    PacketHeader header;
    bool         accepted;
    PlayerId     requesterSteamId;
    PlayerId     targetSteamId;
};

// Fast 20-30Hz Transform Replication Packet
struct PktTransformSync {
    PacketHeader  header;
    CharacterType character;
    Vector3       position;
    Rotation      rotation;
    Vector3       velocity;
    uint16_t      movementFlags; // 0x1: Sprinting, 0x2: Gliding (Crow Wings), 0x4: Crouched
};

// Combat / Animation Action Trigger
struct PktActionSync {
    PacketHeader  header;
    CharacterType character;
    uint32_t      actionId;        // Animation/skill ID in BlackSpace Engine
    float         playbackSpeed;
    int32_t       targetEntityUid; // e.g., grabbed enemy UID for Oongka or lock-on target
};

// Reserve Character Switch: Broadcast when a player in a 2-player session swaps into the reserve character
struct PktReserveSwitchSync {
    PacketHeader  header;
    PlayerId      playerSteamId;
    CharacterType oldCharacter;
    CharacterType newCharacter;
};

// Save Checkpoint Packet: Host orders all clients to write local co-op save
struct PktSaveCheckpointSync {
    PacketHeader header;
    uint32_t     checkpointId;
    uint64_t     worldTick;
};

#pragma pack(pop)

} // namespace CrimsonCoop

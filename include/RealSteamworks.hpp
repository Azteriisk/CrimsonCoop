#pragma once

#include "Common.hpp"
#include <windows.h>
#include <cstdint>
#include <string>

namespace CrimsonCoop {

// Standard Steamworks Types & Constants
using HSteamNetConnection = uint32_t;
using HSteamListenSocket = uint32_t;
constexpr HSteamNetConnection k_HSteamNetConnection_Invalid = 0;
constexpr HSteamListenSocket k_HSteamListenSocket_Invalid = 0;

constexpr int k_nSteamNetworkingSend_Unreliable = 0;
constexpr int k_nSteamNetworkingSend_Reliable = 8;

#pragma pack(push, 1)
struct SteamNetworkingIdentity {
    uint32_t m_eType{16}; // k_ESteamNetworkingIdentityType_SteamID
    int32_t  m_cbSize{8};
    uint64_t m_steamId64{0};
    uint32_t m_reserved[8]{};

    void SetSteamID64(uint64_t id) {
        m_eType = 16;
        m_cbSize = 8;
        m_steamId64 = id;
    }
    uint64_t GetSteamID64() const { return m_steamId64; }
};

struct SteamNetworkingMessage_t {
    void* m_pData;
    int m_cbSize;
    HSteamNetConnection m_conn;
    SteamNetworkingIdentity m_identityPeer;
    int64_t m_nConnUserData;
    int64_t m_usecTimeReceived;
    int64_t m_nMessageNumber;
    void (*m_pfnFreeData)(SteamNetworkingMessage_t* pMsg);
    void (*m_pfnRelease)(SteamNetworkingMessage_t* pMsg);
    int m_nChannel;
    int m_nFlags;
    int64_t m_nUserData;
    uint16_t m_idxLane;
    uint16_t _pad1[3];

    void Release() {
        if (m_pfnRelease) m_pfnRelease(this);
    }
};
#pragma pack(pop)

// Forward dynamic function pointer types from steam_api64.dll
typedef bool (*tSteamAPI_Init)();
typedef void (*tSteamAPI_RunCallbacks)();
typedef void* (*tSteamClient)();
typedef void* (*tSteamInternal_CreateInterface)(const char* ver);
typedef uint64_t (*tSteamAPI_ISteamUser_GetSteamID)(void* pUser);
typedef void* (*tSteamAPI_SteamNetworkingSockets_v012)();
typedef void* (*tSteamAPI_SteamMatchmaking_v009)();
typedef void* (*tSteamAPI_SteamFriends_v017)();
typedef void* (*tSteamAPI_SteamUser_v021)();

class RealSteamworks {
public:
    static bool Load();
    static bool IsLoaded() { return s_isLoaded; }

    static void RunCallbacks();
    static uint64_t GetLocalSteamID();
    static void OpenInviteOverlay(uint64_t lobbyId);

    // Native ISteamNetworkingSockets VTable dispatchers
    static HSteamListenSocket CreateListenSocketP2P(int nLocalVirtualPort);
    static HSteamNetConnection ConnectP2P(uint64_t hostSteamId, int nRemoteVirtualPort);
    static bool CloseConnection(HSteamNetConnection hConn, int nReason, const char* pszDebug);
    static bool SendMessage(HSteamNetConnection hConn, const void* pData, uint32_t cbData, bool reliable);
    static int ReceiveMessages(HSteamNetConnection hConn, SteamNetworkingMessage_t** ppOutMessages, int nMaxMessages);

private:
    static HMODULE s_hSteamApi;
    static bool s_isLoaded;

    static void* s_pSteamUser;
    static void* s_pSteamFriends;
    static void* s_pSteamMatchmaking;
    static void* s_pSteamNetworkingSockets;

    static tSteamAPI_RunCallbacks s_pfnRunCallbacks;
};

} // namespace CrimsonCoop

#include "RealSteamworks.hpp"
#include <iostream>

namespace CrimsonCoop {

HMODULE RealSteamworks::s_hSteamApi = nullptr;
bool RealSteamworks::s_isLoaded = false;
void* RealSteamworks::s_pSteamUser = nullptr;
void* RealSteamworks::s_pSteamFriends = nullptr;
void* RealSteamworks::s_pSteamMatchmaking = nullptr;
void* RealSteamworks::s_pSteamNetworkingSockets = nullptr;
tSteamAPI_RunCallbacks RealSteamworks::s_pfnRunCallbacks = nullptr;

bool RealSteamworks::Load() {
    if (s_isLoaded) return true;

    // In a running Steam game, steam_api64.dll is already loaded into the process!
    s_hSteamApi = GetModuleHandleA("steam_api64.dll");
    if (!s_hSteamApi) {
        // Fallback: try loading if running standalone or sidecar
        s_hSteamApi = LoadLibraryA("steam_api64.dll");
    }

    if (!s_hSteamApi) {
        std::cout << "[RealSteamworks] steam_api64.dll not found in process space. Operating in fallback mode.\n";
        return false;
    }

    s_pfnRunCallbacks = reinterpret_cast<tSteamAPI_RunCallbacks>(
        GetProcAddress(s_hSteamApi, "SteamAPI_RunCallbacks")
    );

    auto pfnSockets = reinterpret_cast<tSteamAPI_SteamNetworkingSockets_v012>(
        GetProcAddress(s_hSteamApi, "SteamAPI_SteamNetworkingSockets_SteamAPI_v012")
    );
    auto pfnMatch = reinterpret_cast<tSteamAPI_SteamMatchmaking_v009>(
        GetProcAddress(s_hSteamApi, "SteamAPI_SteamMatchmaking_v009")
    );
    auto pfnFriends = reinterpret_cast<tSteamAPI_SteamFriends_v017>(
        GetProcAddress(s_hSteamApi, "SteamAPI_SteamFriends_v017")
    );
    auto pfnUser = reinterpret_cast<tSteamAPI_SteamUser_v021>(
        GetProcAddress(s_hSteamApi, "SteamAPI_SteamUser_v021")
    );

    if (pfnSockets) s_pSteamNetworkingSockets = pfnSockets();
    if (pfnMatch)   s_pSteamMatchmaking = pfnMatch();
    if (pfnFriends) s_pSteamFriends = pfnFriends();
    if (pfnUser)    s_pSteamUser = pfnUser();

    s_isLoaded = (s_pSteamNetworkingSockets != nullptr);
    if (s_isLoaded) {
        std::cout << "[RealSteamworks] Successfully bound to real Steamworks API in process memory!\n";
    }

    return s_isLoaded;
}

void RealSteamworks::RunCallbacks() {
    if (s_pfnRunCallbacks) {
        s_pfnRunCallbacks();
    }
}

uint64_t RealSteamworks::GetLocalSteamID() {
    if (!s_hSteamApi || !s_pSteamUser) return 0;
    auto pfnGetId = reinterpret_cast<tSteamAPI_ISteamUser_GetSteamID>(
        GetProcAddress(s_hSteamApi, "SteamAPI_ISteamUser_GetSteamID")
    );
    if (pfnGetId) {
        return pfnGetId(s_pSteamUser);
    }
    return 0;
}

void RealSteamworks::OpenInviteOverlay(uint64_t lobbyId) {
    if (!s_hSteamApi || !s_pSteamFriends) return;

    typedef void (*tActivateOverlayInvite)(void* pFriends, uint64_t lobbyId);
    auto pfnInvite = reinterpret_cast<tActivateOverlayInvite>(
        GetProcAddress(s_hSteamApi, "SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog")
    );

    if (pfnInvite) {
        std::cout << "[RealSteamworks] Opening real Steam friend invite overlay for lobby: " << lobbyId << std::endl;
        pfnInvite(s_pSteamFriends, lobbyId);
    }
}

// ISteamNetworkingSockets VTable calls
HSteamListenSocket RealSteamworks::CreateListenSocketP2P(int nLocalVirtualPort) {
    if (!s_pSteamNetworkingSockets) return k_HSteamListenSocket_Invalid;

    void** pVtable = *reinterpret_cast<void***>(s_pSteamNetworkingSockets);
    // CreateListenSocketP2P is VTable index 1
    typedef HSteamListenSocket (*tCreateListenSocketP2P)(void* pThis, int nPort, int nOptions, const void* pOptions);
    auto fn = reinterpret_cast<tCreateListenSocketP2P>(pVtable[1]);
    return fn(s_pSteamNetworkingSockets, nLocalVirtualPort, 0, nullptr);
}

HSteamNetConnection RealSteamworks::ConnectP2P(uint64_t hostSteamId, int nRemoteVirtualPort) {
    if (!s_pSteamNetworkingSockets) return k_HSteamNetConnection_Invalid;

    SteamNetworkingIdentity identity;
    identity.SetSteamID64(hostSteamId);

    void** pVtable = *reinterpret_cast<void***>(s_pSteamNetworkingSockets);
    // ConnectP2P is VTable index 2
    typedef HSteamNetConnection (*tConnectP2P)(void* pThis, const SteamNetworkingIdentity* pIdentity, int nPort, int nOptions, const void* pOptions);
    auto fn = reinterpret_cast<tConnectP2P>(pVtable[2]);
    return fn(s_pSteamNetworkingSockets, &identity, nRemoteVirtualPort, 0, nullptr);
}

bool RealSteamworks::CloseConnection(HSteamNetConnection hConn, int nReason, const char* pszDebug) {
    if (!s_pSteamNetworkingSockets || hConn == k_HSteamNetConnection_Invalid) return false;

    void** pVtable = *reinterpret_cast<void***>(s_pSteamNetworkingSockets);
    // CloseConnection is VTable index 4
    typedef bool (*tCloseConnection)(void* pThis, HSteamNetConnection hPeer, int nReason, const char* pszDebug, bool bEnableLinger);
    auto fn = reinterpret_cast<tCloseConnection>(pVtable[4]);
    return fn(s_pSteamNetworkingSockets, hConn, nReason, pszDebug, false);
}

bool RealSteamworks::SendMessage(HSteamNetConnection hConn, const void* pData, uint32_t cbData, bool reliable) {
    if (!s_pSteamNetworkingSockets || hConn == k_HSteamNetConnection_Invalid) return false;

    int flags = reliable ? k_nSteamNetworkingSend_Reliable : k_nSteamNetworkingSend_Unreliable;

    void** pVtable = *reinterpret_cast<void***>(s_pSteamNetworkingSockets);
    // SendMessageToConnection is VTable index 6
    typedef int (*tSendMessageToConnection)(void* pThis, HSteamNetConnection hConn, const void* pData, uint32_t cbData, int nSendFlags, int64_t* pOutMessageNumber);
    auto fn = reinterpret_cast<tSendMessageToConnection>(pVtable[6]);
    return fn(s_pSteamNetworkingSockets, hConn, pData, cbData, flags, nullptr) >= 0;
}

int RealSteamworks::ReceiveMessages(HSteamNetConnection hConn, SteamNetworkingMessage_t** ppOutMessages, int nMaxMessages) {
    if (!s_pSteamNetworkingSockets || hConn == k_HSteamNetConnection_Invalid) return 0;

    void** pVtable = *reinterpret_cast<void***>(s_pSteamNetworkingSockets);
    // ReceiveMessagesOnConnection is VTable index 8
    typedef int (*tReceiveMessagesOnConnection)(void* pThis, HSteamNetConnection hConn, SteamNetworkingMessage_t** ppOutMessages, int nMaxMessages);
    auto fn = reinterpret_cast<tReceiveMessagesOnConnection>(pVtable[8]);
    return fn(s_pSteamNetworkingSockets, hConn, ppOutMessages, nMaxMessages);
}

} // namespace CrimsonCoop

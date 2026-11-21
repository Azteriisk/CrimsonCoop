#include <windows.h>
#include <iostream>
#include "Logger.hpp"
#include "Config.hpp"
#include "SaveRedirector.hpp"
#include "Dx12Hook.hpp"
#include "SessionManager.hpp"
#include "VisualEquipSync.hpp"
#include "ActionCombatSync.hpp"
#include "SteamTransport.hpp"
#include "RemoteProxyManager.hpp"
#include "GuiOverlay.hpp"
#include "Common.hpp"

namespace CrimsonCoop {

extern SessionManager* g_pActiveSession;

DWORD WINAPI ModMainThread(LPVOID lpParam) {
    (void)lpParam;

    // 1. Initialize Logger immediately
    Logger::Init("CrimsonCoop.log");
    Logger::Log("=========================================================\n");
    Logger::Log("  Crimson Desert 3-Player Co-Op Mod Injected             \n");
    Logger::Log("  Trio: Kliff, Damiane, Oongka                           \n");
    Logger::Log("=========================================================\n");

    // 2. Load Configuration
    Config::Load();
    const auto& cfg = Config::Get();

    // 3. Optional Developer Console
    if (cfg.debugConsole) {
        AllocConsole();
        FILE* fp;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        Logger::Log("[Core] Developer console allocated.\n");
    }

    // 4. Initialize Active Greymane Session (Starts in local solo / host mode)
    CharacterType defaultChar = cfg.preferredCharacter;
    if (defaultChar == CharacterType::None) {
        defaultChar = CharacterType::Kliff;
    }
    g_pActiveSession = new SessionManager(76561198000000001ULL, defaultChar, "LocalGreymane");
    Logger::Log("[Core] Active Session created. Initial hero: %s\n", CharacterToString(defaultChar));

    // 5. Install Save Sandbox Hook (Protects single-player saves)
    if (SaveRedirector::Install()) {
        Logger::Log("[Core] Save redirection active: Single-player saves protected.\n");
    } else {
        Logger::Log("[Core] Error: Failed to initialize SaveRedirector!\n");
    }

    // 6. Install DX12 Present & CommandQueue Hooks for In-Game ImGui Overlay (Optional)
    if (cfg.showPartyHUD) {
        if (Dx12Hook::Install()) {
            Logger::Log("[Core] DX12 SwapChain & CommandQueue hooks active: In-game HUD ready.\n");
        } else {
            Logger::Log("[Core] Error: Failed to initialize Dx12Hook!\n");
        }
    } else {
        Logger::Log("[Core] DX12 HUD Overlay disabled in config (Zero graphics overhead mode).\n");
    }

    // 7. Install Visual Equipment Synchronizer
    if (VisualEquipSync::Install()) {
        Logger::Log("[Core] Visual equipment synchronization active.\n");
    }

    // 8. Install Combat Actions & Animation Replication
    if (ActionCombatSync::Install()) {
        Logger::Log("[Core] Combat actions & animation replication active.\n");
    }

    // 9. Initialize Native Steamworks P2P Transport (SDR)
    SteamTransport::Initialize(76561198000000001ULL, true);
    Logger::Log("[Core] Native Steamworks P2P Transport initialized.\n");

    return 0;
}

} // namespace CrimsonCoop

// Interop C API for CrimsonSwitch & external mod companions
extern "C" {

__declspec(dllexport) bool CrimsonCoop_IsMultiplayerActive() {
    if (!CrimsonCoop::g_pActiveSession) return false;
    return CrimsonCoop::g_pActiveSession->GetActivePlayerCount() > 1;
}

__declspec(dllexport) bool CrimsonCoop_IsCharacterOccupied(uint8_t charId) {
    if (!CrimsonCoop::g_pActiveSession) return false;
    CrimsonCoop::CharacterType ct = CrimsonCoop::CharacterType::None;
    if (charId == 1) ct = CrimsonCoop::CharacterType::Kliff;
    else if (charId == 2) ct = CrimsonCoop::CharacterType::Damiane;
    else if (charId == 3) ct = CrimsonCoop::CharacterType::Oongka;
    return CrimsonCoop::g_pActiveSession->IsCharacterInUse(ct);
}

__declspec(dllexport) bool CrimsonCoop_RequestCharacterSwitch(uint8_t charId) {
    if (!CrimsonCoop::g_pActiveSession) return false;
    CrimsonCoop::CharacterType ct = CrimsonCoop::CharacterType::None;
    if (charId == 1) ct = CrimsonCoop::CharacterType::Kliff;
    else if (charId == 2) ct = CrimsonCoop::CharacterType::Damiane;
    else if (charId == 3) ct = CrimsonCoop::CharacterType::Oongka;
    std::string reason;
    bool res = CrimsonCoop::g_pActiveSession->SwitchLocalCharacter(ct, reason);
    if (res) {
        CrimsonCoop::GuiOverlay::ShowToast(std::string("Switched active hero to ") + CrimsonCoop::CharacterToString(ct));
    }
    return res;
}

} // extern "C"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    (void)lpReserved;
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, CrimsonCoop::ModMainThread, hModule, 0, nullptr);
        break;
    case DLL_PROCESS_DETACH:
        CrimsonCoop::VisualEquipSync::Remove();
        CrimsonCoop::ActionCombatSync::Remove();
        CrimsonCoop::SteamTransport::Shutdown();
        CrimsonCoop::Dx12Hook::Remove();
        CrimsonCoop::SaveRedirector::Remove();
        CrimsonCoop::Logger::Shutdown();
        break;
    }
    return TRUE;
}

#include <windows.h>
#include <iostream>
#include "Logger.hpp"
#include "Config.hpp"
#include "SaveRedirector.hpp"
#include "Dx12Hook.hpp"
#include "SessionManager.hpp"
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

    // 6. Install DX12 Present & CommandQueue Hooks for In-Game ImGui Overlay
    if (Dx12Hook::Install()) {
        Logger::Log("[Core] DX12 SwapChain & CommandQueue hooks active: In-game HUD ready.\n");
    } else {
        Logger::Log("[Core] Error: Failed to initialize Dx12Hook!\n");
    }

    return 0;
}

} // namespace CrimsonCoop

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    (void)lpReserved;
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, CrimsonCoop::ModMainThread, hModule, 0, nullptr);
        break;
    case DLL_PROCESS_DETACH:
        CrimsonCoop::Dx12Hook::Remove();
        CrimsonCoop::SaveRedirector::Remove();
        CrimsonCoop::Logger::Shutdown();
        break;
    }
    return TRUE;
}

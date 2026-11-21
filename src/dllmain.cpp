#include <windows.h>
#include <iostream>
#include "SaveRedirector.hpp"
#include "Dx12Hook.hpp"
#include "SessionManager.hpp"
#include "Common.hpp"

namespace CrimsonCoop {

extern SessionManager* g_pActiveSession;

DWORD WINAPI ModMainThread(LPVOID lpParam) {
    (void)lpParam;
    // 1. Allocate a developer console window to view real-time co-op logs
#ifdef _DEBUG
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
#endif

    std::cout << "=========================================================\n";
    std::cout << "  Crimson Desert 3-Player Co-op Mod Loaded               \n";
    std::cout << "  Characters: Kliff, Damiane, Oongka                     \n";
    std::cout << "=========================================================\n";

    // 2. Install the Save Sandbox Hook (Protects single-player saves)
    if (SaveRedirector::Install()) {
        std::cout << "[Core] Save redirection active: Single-player saves protected.\n";
    } else {
        std::cerr << "[Core] Failed to initialize SaveRedirector!\n";
    }

    // 3. Install DX12 Present Hook for In-Game ImGui Overlay
    if (Dx12Hook::Install()) {
        std::cout << "[Core] DX12 SwapChain hook active: In-game HUD ready.\n";
    } else {
        std::cerr << "[Core] Failed to initialize Dx12Hook!\n";
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
        break;
    }
    return TRUE;
}

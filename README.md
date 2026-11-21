# CrimsonCoop

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![DirectX 12](https://img.shields.io/badge/DirectX-12-green.svg)](https://docs.microsoft.com/en-us/windows/win32/direct3d12/direct3d-12-graphics)
[![Steamworks](https://img.shields.io/badge/Steamworks-P2P%20SDR-1b2838.svg)](https://partner.steamgames.com/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

**CrimsonCoop** is a cooperative multiplayer framework and mod architecture designed for *Crimson Desert*. Built in modern C++20 and DirectX 12, it enables players to concurrently explore and fight side-by-side as the three canon mercenaries: **Kliff**, **Damiane**, and **Oongka**.

---

## Key Features

### 1. Dual Play Modes: 3-Player Trio & 2-Player Duo
* **Full 3-Player Co-Op:** All three characters deployed simultaneously, each controlled by an independent human player.
* **2-Player Duo Mode (First-Class Support):** In 2-player games, the unpiloted 3rd character becomes a **Dynamic Reserve Mercenary**. Either player can swap into the reserve character mid-game using their in-game radial wheel, cycling their previous character back into the reserve pool for their teammate.

### 2. Zero-Footprint Standalone Launcher (`CrimsonCoopLauncher.exe`)
* **Pure Vanilla Guarantee:** Base game files on disk are **never modified**.
* **Launch from Steam:** Starts 100% pure vanilla single-player with original save files (`Slot0.sav`) and Steam achievements enabled.
* **Launch via CrimsonCoopLauncher:** Injects the mod purely into RAM for that specific multiplayer session. Once you exit, process memory is freed and the game returns to vanilla.

### 3. Isolated Co-Op Save System (`.cdcoop`)
* Intercepts file I/O via MinHook (`CreateFileW`) to sandbox multiplayer progress into dedicated `CoopSaveGames\Slot0.cdcoop` containers.
* Single-player save data is never touched or overwritten.

### 4. Real Steamworks P2P (Valve Relay Network / SDR)
* Encapsulates native `ISteamNetworkingSockets` and `ISteamMatchmaking` VTables.
* Automated NAT punch-through, packet encryption, and IP privacy with zero port forwarding required.
* Native Steam friend invitation overlay support (`Shift+Tab` invite dialog).

### 5. In-Game DirectX 12 Overlay (Dear ImGui)
* Hooks `IDXGISwapChain::Present` and `ResizeBuffers`.
* Features an on-screen party HUD showing active teammates, latency, and reserve slots.
* Modal dialogs for join-time character conflict arbitration and mutual role-swap negotiation.
* Toggleable via `F2` or `Insert`.

### 6. Patch-Resilient BlackSpace Engine Resolver
* Candidate AOB ladders with wildcard scanning to dynamically locate `ClientActorManagerGlobal` across game patches.
* Traverses the static engine chain to inspect live appearance XML asset paths (`cd_phm_macduff`, `cd_phw_damian`, `cd_phm_oongka`).
* Tracks `world_generation` counters to cleanly detect save-load world teardowns and reallocations.

---

## Project Structure

```
CrimsonCoop/
├── CMakeLists.txt                 # Unified build configuration (MinHook + ImGui + DX12)
├── CrimsonCoop.ini                # Runtime configuration file
├── README.md                      # Documentation & architecture overview
├── docs/
│   └── BlackSpace_Engine_Reference.md # In-depth engine reverse engineering manual
├── include/
│   ├── Common.hpp                 # Character enums, Vector3, math primitives
│   ├── Config.hpp                 # INI settings reader & writer
│   ├── EngineTypes.hpp            # BlackSpace Engine struct offsets & codenames
│   ├── PatternScan.hpp            # AOB scanner with wildcard & RIP support
│   ├── BlackSpaceChain.hpp        # In-memory static chain resolver
│   ├── NetworkPackets.hpp         # Packed binary packet structures
│   ├── SessionManager.hpp         # Roster arbitration, duo reserve logic
│   ├── Interpolation.hpp          # Hermite/linear snapshot smoothing
│   ├── CoopSaveManager.hpp        # .cdcoop binary serializer
│   ├── SaveRedirector.hpp         # MinHook CreateFileW interception
│   ├── RealSteamworks.hpp         # Dynamic steam_api64.dll native driver
│   ├── SteamTransport.hpp         # Steam P2P SDR network transport
│   ├── Dx12Hook.hpp               # DirectX 12 SwapChain hook
│   └── GuiOverlay.hpp             # Dear ImGui party HUD & modals
└── src/
    ├── main.cpp                   # Automated comprehensive verification harness
    ├── launcher.cpp               # Standalone memory injector executable
    ├── dllmain.cpp                # Injected mod runtime DLL entry point
    ├── Config.cpp                 # Configuration implementation
    ├── PatternScan.cpp            # Signature scanner implementation
    ├── BlackSpaceChain.cpp        # Live engine chain traversal
    ├── SessionManager.cpp         # Roster & conflict arbitration
    ├── Interpolation.cpp          # Snapshot buffering math
    ├── CoopSaveManager.cpp        # Save file binary container I/O
    ├── SaveRedirector.cpp         # File path redirection routines
    ├── RealSteamworks.cpp         # Dynamic Steamworks VTable dispatcher
    ├── SteamTransport.cpp         # Packet dispatching & SDR handling
    ├── Dx12Hook.cpp               # SwapChain VTable hooking & WNDPROC routing
    └── GuiOverlay.cpp             # ImGui rendering routines
```

---

## How to Test

### 1. Run the Automated Verification Suite
The test runner validates the entire pipeline (save sandboxing, 3-player arbitration, 2-player duo reserve swapping, transform interpolation, and config parsing) without requiring the game or Steam to be open:

```powershell
cd C:\Users\thera\code\CrimsonCoop
cmake --build build
.\build\cd_coop_harness.exe
```

### 2. Test the Standalone Launcher
Run the zero-footprint launcher to verify executable resolution and memory injection readiness:

```powershell
.\build\CrimsonCoopLauncher.exe
```
* The launcher checks your Steam installation registry and local directories for `CrimsonDesert.exe`.
* If found, it starts the process suspended, injects `CrimsonCoop.dll` into RAM, and resumes execution.

### 3. Customize `CrimsonCoop.ini`
Edit `CrimsonCoop.ini` to test different runtime preferences:
* Change `PreferredCharacter = Oongka` (or `Damiane` / `Kliff`).
* Toggle `TickRate = 60` or `InterpolationDelayMs = 30`.
* Change `ToggleKey = 0x2D` (VK_INSERT) or `0x71` (VK_F2).

---

## License

This project is licensed under the [MIT License](LICENSE).

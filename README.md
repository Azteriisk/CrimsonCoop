# CrimsonCoop

A C++20 cooperative multiplayer framework featuring character arbitration, save isolation, DirectX 12 overlay hooks, and peer-to-peer state synchronization.

## Architecture

- **Session Arbitration:** Prevents character collisions and handles role assignment and swapping across 3 concurrent player slots.
- **Save Isolation:** Intercepts file I/O to redirect campaign progress into dedicated `.cdcoop` binary containers.
- **DirectX 12 Hook & UI:** Renders party telemetry and modal dialogs using Dear ImGui over hooked swapchain calls.
- **Snapshot Interpolation:** Buffers transform updates and performs linear/angular smoothing to prevent latency jitter.

## Build

```powershell
cmake -B build -S .
cmake --build build --config Release
.\build\cd_coop_harness.exe
```

## License

MIT

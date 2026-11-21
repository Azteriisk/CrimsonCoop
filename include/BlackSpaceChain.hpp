#pragma once

#include "Common.hpp"
#include "EngineTypes.hpp"
#include <windows.h>
#include <cstdint>
#include <atomic>
#include <string>
#include <vector>

namespace CrimsonCoop {

class BlackSpaceChain {
public:
    static bool Initialize(HMODULE hGameModule = nullptr);

    // Resolves the currently-controlled protagonist (Kliff, Damiane, or Oongka)
    static CharacterType GetControlledCharacter();

    // Resolves the pointer to the currently-controlled CCOIA
    static uintptr_t GetControlledCcoia();

    // Resolves the Kliff anchor CCOIA
    static uintptr_t GetKliffCcoia();

    // Snapshots all live protagonist CCOIAs from the actor array
    static LiveActorSnapshot GetActorSnapshot();

    // Returns the world-generation counter (bumps on true save-load/fast-travel)
    static uint64_t GetWorldGeneration();

    // Direct classification helper for appearance XML string
    static CharacterType ClassifyActorAppearance(uintptr_t ccoia);

    static bool IsReady() { return s_isInitialized && s_pClientActorManagerGlobal != nullptr; }

private:
    static bool ResolveGlobalSlot(HMODULE hGameModule);
    static bool SafeReadMemory(const void* address, void* buffer, size_t size);

    static bool s_isInitialized;
    static uintptr_t* s_pClientActorManagerGlobal;
    static std::atomic<uintptr_t> s_lastObservedKliffCcoia;
    static std::atomic<uint64_t> s_worldGeneration;
};

} // namespace CrimsonCoop

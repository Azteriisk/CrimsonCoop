#include "BlackSpaceChain.hpp"
#include "PatternScan.hpp"
#include <iostream>
#include <algorithm>

namespace CrimsonCoop {

bool BlackSpaceChain::s_isInitialized = false;
uintptr_t* BlackSpaceChain::s_pClientActorManagerGlobal = nullptr;
std::atomic<uintptr_t> BlackSpaceChain::s_lastObservedKliffCcoia{0};
std::atomic<uint64_t> BlackSpaceChain::s_worldGeneration{1};

bool BlackSpaceChain::SafeReadMemory(const void* address, void* buffer, size_t size) {
    if (!address) return false;
    SIZE_T bytesRead = 0;
    return ReadProcessMemory(GetCurrentProcess(), address, buffer, size, &bytesRead) && (bytesRead == size);
}

bool BlackSpaceChain::ResolveGlobalSlot(HMODULE hGameModule) {
    if (s_pClientActorManagerGlobal != nullptr) {
        return true;
    }

    // Candidate AOB Ladder for ClientActorManagerGlobal from reverse-engineered anchors
    std::vector<std::string> candidates = {
        // P1: PublishStore
        "48 89 ?? ?? ?? ?? ?? 48 8D ?? ?? ?? ?? ?? 48 89 05 ?? ?? ?? ?? 48 89 05",
        // P2: LeaCallBody
        "48 8D 15 ?? ?? ?? ?? 48 8D 4D ?? E8"
    };

    uintptr_t match = PatternScanner::ScanLadder(hGameModule, candidates);
    if (match != 0) {
        // Resolve RIP-relative pointer (typical displacement at +3, length 7)
        s_pClientActorManagerGlobal = PatternScanner::ResolveRipPointer(match, 3, 7);
        std::cout << "[BlackSpaceChain] AOB resolved ClientActorManagerGlobal at: " 
                  << s_pClientActorManagerGlobal << std::endl;
        return true;
    }

    return false;
}

bool BlackSpaceChain::Initialize(HMODULE hGameModule) {
    if (s_isInitialized) return true;

    if (ResolveGlobalSlot(hGameModule)) {
        s_isInitialized = true;
        std::cout << "[BlackSpaceChain] Initialized BlackSpace static chain resolver.\n";
        return true;
    }

    std::cout << "[BlackSpaceChain] Offline mode or standalone mock active.\n";
    s_isInitialized = true;
    return true;
}

uintptr_t BlackSpaceChain::GetKliffCcoia() {
    if (!s_pClientActorManagerGlobal) return 0;

    uintptr_t mgr = 0;
    if (!SafeReadMemory(s_pClientActorManagerGlobal, &mgr, sizeof(uintptr_t)) || !mgr) return 0;

    uintptr_t userActor = 0;
    if (!SafeReadMemory(reinterpret_cast<const void*>(mgr + BlackSpaceOffsets::MGR_TO_USER_ACTOR), &userActor, sizeof(uintptr_t)) || !userActor) return 0;

    uintptr_t subMgr = 0;
    if (!SafeReadMemory(reinterpret_cast<const void*>(userActor + BlackSpaceOffsets::USER_ACTOR_TO_SUB_MGR), &subMgr, sizeof(uintptr_t)) || !subMgr) return 0;

    uintptr_t kliffCcoia = 0;
    if (!SafeReadMemory(reinterpret_cast<const void*>(subMgr + BlackSpaceOffsets::SUB_MGR_TO_KLIFF_CCOIA), &kliffCcoia, sizeof(uintptr_t)) || !kliffCcoia) return 0;

    // Detect save-load world rebuild: Kliff's CCOIA is reallocated on every save-load
    uintptr_t prevKliff = s_lastObservedKliffCcoia.exchange(kliffCcoia);
    if (prevKliff != 0 && prevKliff != kliffCcoia) {
        s_worldGeneration.fetch_add(1);
        std::cout << "[BlackSpaceChain] World rebuild detected! Bumped world-generation to "
                  << s_worldGeneration.load() << std::endl;
    }

    return kliffCcoia;
}

uintptr_t BlackSpaceChain::GetControlledCcoia() {
    if (!s_pClientActorManagerGlobal) return 0;

    uintptr_t mgr = 0;
    if (!SafeReadMemory(s_pClientActorManagerGlobal, &mgr, sizeof(uintptr_t)) || !mgr) return 0;

    uintptr_t userActor = 0;
    if (!SafeReadMemory(reinterpret_cast<const void*>(mgr + BlackSpaceOffsets::MGR_TO_USER_ACTOR), &userActor, sizeof(uintptr_t)) || !userActor) return 0;

    uintptr_t subMgr = 0;
    if (!SafeReadMemory(reinterpret_cast<const void*>(userActor + BlackSpaceOffsets::USER_ACTOR_TO_SUB_MGR), &subMgr, sizeof(uintptr_t)) || !subMgr) return 0;

    uintptr_t controlled = 0;
    SafeReadMemory(reinterpret_cast<const void*>(subMgr + BlackSpaceOffsets::SUB_MGR_TO_CONTROLLED_CCOIA), &controlled, sizeof(uintptr_t));
    return controlled;
}

CharacterType BlackSpaceChain::ClassifyActorAppearance(uintptr_t ccoia) {
    if (!ccoia) return CharacterType::None;

    // Appearance chain: CCOIA + 0x68 -> +0x40 -> +0x40 -> +0x38 (std::string)
    uintptr_t ptr1 = 0;
    if (!SafeReadMemory(reinterpret_cast<const void*>(ccoia + BlackSpaceOffsets::CCOIA_TO_APP_PTR1), &ptr1, sizeof(uintptr_t)) || !ptr1) return CharacterType::None;

    uintptr_t ptr2 = 0;
    if (!SafeReadMemory(reinterpret_cast<const void*>(ptr1 + BlackSpaceOffsets::APP_PTR1_TO_APP_PTR2), &ptr2, sizeof(uintptr_t)) || !ptr2) return CharacterType::None;

    uintptr_t ptr3 = 0;
    if (!SafeReadMemory(reinterpret_cast<const void*>(ptr2 + BlackSpaceOffsets::APP_PTR2_TO_APP_PTR3), &ptr3, sizeof(uintptr_t)) || !ptr3) return CharacterType::None;

    // Read std::string buffer (SSO or heap ptr)
    char pathBuf[256] = {};
    if (SafeReadMemory(reinterpret_cast<const void*>(ptr3 + BlackSpaceOffsets::APP_PTR3_TO_PATH_STRING), pathBuf, sizeof(pathBuf) - 1)) {
        return ClassifyAppearancePath(pathBuf);
    }

    return CharacterType::None;
}

CharacterType BlackSpaceChain::GetControlledCharacter() {
    uintptr_t controlled = GetControlledCcoia();
    if (!controlled) return CharacterType::None;

    // If controlled == Kliff anchor
    uintptr_t kliff = GetKliffCcoia();
    if (controlled == kliff && kliff != 0) {
        return CharacterType::Kliff;
    }

    // Otherwise classify appearance path
    CharacterType charType = ClassifyActorAppearance(controlled);
    if (charType != CharacterType::None) {
        return charType;
    }

    return CharacterType::Kliff; // Default fallback
}

LiveActorSnapshot BlackSpaceChain::GetActorSnapshot() {
    LiveActorSnapshot snapshot{};
    snapshot.kliffCcoia = GetKliffCcoia();
    snapshot.controlledCcoia = GetControlledCcoia();
    snapshot.activeCharacter = GetControlledCharacter();
    snapshot.worldGeneration = s_worldGeneration.load();
    return snapshot;
}

uint64_t BlackSpaceChain::GetWorldGeneration() {
    return s_worldGeneration.load();
}

} // namespace CrimsonCoop

#pragma once

#include "Common.hpp"
#include <cstdint>
#include <cstddef>
#include <string_view>

namespace CrimsonCoop {

/**
 * @brief BlackSpace Engine Reverse-Engineered Memory Offsets
 * 
 * Source Authority: Reverse engineering of pa::ClientActorManager & pa::ClientUserActor
 * 
 * Static Chain Walk:
 *   [ClientActorManagerGlobal] -> pa::ClientActorManager
 *       -> +0x58: pa::ClientUserActor
 *           -> +0x08: CCOIA Sub-Manager (pa::ClientChildOnlyInGameActor sub-manager)
 *               -> +0x30: Kliff CCOIA (Always present anchor)
 *               -> +0x38: Currently controlled CCOIA
 *           -> +0x78: vec<ActorComponent*>
 *               -> vec[2]: pa::ClientChildContainerActorComponent
 *                   -> +0x18: 100-entry actor list (16-byte stride: CCOIA ptr + 0x0101 flag)
 *                       [0] = Kliff (Always present)
 *                       [1] = Damiane (When loaded into world)
 *                       [2] = Oongka (When loaded into world)
 *                       [3+] = Companions / NPCs
 */
namespace BlackSpaceOffsets {
    inline constexpr std::ptrdiff_t MGR_TO_USER_ACTOR            = 0x58;
    inline constexpr std::ptrdiff_t USER_ACTOR_TO_SUB_MGR        = 0x08;
    inline constexpr std::ptrdiff_t SUB_MGR_TO_KLIFF_CCOIA       = 0x30;
    inline constexpr std::ptrdiff_t SUB_MGR_TO_CONTROLLED_CCOIA  = 0x38;
    inline constexpr std::ptrdiff_t USER_ACTOR_TO_VEC_COMPONENTS = 0x78;
    inline constexpr std::ptrdiff_t CHILD_CONTAINER_ACTOR_LIST   = 0x18;
    inline constexpr std::ptrdiff_t CCOIA_EQUIP_COMPONENT        = 0x68;
    inline constexpr std::ptrdiff_t EQUIP_SLOT_OFFSET            = 0x38;

    // Appearance-Config XML asset path chain:
    // CCOIA + 0x68 -> +0x40 -> +0x40 -> +0x38 (std::string)
    inline constexpr std::ptrdiff_t CCOIA_TO_APP_PTR1            = 0x68;
    inline constexpr std::ptrdiff_t APP_PTR1_TO_APP_PTR2         = 0x40;
    inline constexpr std::ptrdiff_t APP_PTR2_TO_APP_PTR3         = 0x40;
    inline constexpr std::ptrdiff_t APP_PTR3_TO_PATH_STRING      = 0x38;
}

namespace ProtagonistCodenames {
    inline constexpr std::string_view KLIFF   = "cd_phm_macduff";
    inline constexpr std::string_view DAMIANE = "cd_phw_damian";
    inline constexpr std::string_view OONGKA  = "cd_phm_oongka";
}

// Helper to classify an appearance config path
inline CharacterType ClassifyAppearancePath(std::string_view path) {
    if (path.find(ProtagonistCodenames::KLIFF) != std::string_view::npos) {
        return CharacterType::Kliff;
    }
    if (path.find(ProtagonistCodenames::DAMIANE) != std::string_view::npos) {
        return CharacterType::Damiane;
    }
    if (path.find(ProtagonistCodenames::OONGKA) != std::string_view::npos) {
        return CharacterType::Oongka;
    }
    return CharacterType::None;
}

struct LiveActorSnapshot {
    uintptr_t kliffCcoia{0};
    uintptr_t damianeCcoia{0};
    uintptr_t oongkaCcoia{0};
    uintptr_t controlledCcoia{0};
    CharacterType activeCharacter{CharacterType::None};
    uint64_t worldGeneration{0};
};

} // namespace CrimsonCoop

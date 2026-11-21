#pragma once

#include "Common.hpp"
#include "NetworkPackets.hpp"
#include "SteamTransport.hpp"
#include <unordered_map>
#include <array>
#include <mutex>

namespace CrimsonCoop {

constexpr size_t MAX_EQUIP_SLOTS = 8;

enum EquipSlot : int16_t {
    SlotMainHand = 0,
    SlotOffHand  = 1,
    SlotChest    = 2,
    SlotHelmet   = 3,
    SlotGloves   = 4,
    SlotBoots    = 5,
    SlotCloak    = 6,
    SlotRanged   = 7
};

struct CharacterLoadout {
    std::array<int16_t, MAX_EQUIP_SLOTS> slotItemIds{};
    std::array<uint32_t, MAX_EQUIP_SLOTS> cosmeticFlags{};

    CharacterLoadout() {
        slotItemIds.fill(-1); // -1 or 0xFFFF represents unequipped / naked slot
        cosmeticFlags.fill(0);
    }
};

class VisualEquipSync {
public:
    static bool Install();
    static void Remove();

    // Hook callback invoked when the game alters any actor's visual equipment
    static int64_t OnVisualEquipChange(void* bodyComp, int16_t slotId, int16_t itemId, void* itemData);

    // Apply remote equipment packet onto a peer's proxy actor
    static bool ApplyRemoteEquip(const PktVisualEquipSync& pkt);

    // Broadcast full 8-slot loadout to a newly joined player
    static void SynchronizeInitialLoadout(PlayerId targetPeerId, CharacterType character);

    // Query cached equipment
    static CharacterLoadout GetCachedLoadout(CharacterType character);

private:
    static bool s_isInstalled;
    static std::mutex s_loadoutMutex;
    static std::unordered_map<CharacterType, CharacterLoadout> s_cachedLoadouts;
};

} // namespace CrimsonCoop

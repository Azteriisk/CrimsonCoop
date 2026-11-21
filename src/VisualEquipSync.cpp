#include "VisualEquipSync.hpp"
#include "RemoteProxyManager.hpp"
#include "PatternScan.hpp"
#include "Logger.hpp"
#include <MinHook.h>
#include <iostream>

namespace CrimsonCoop {

typedef int64_t(WINAPI* tVisualEquipChange)(void* bodyComp, int16_t slotId, int16_t itemId, void* itemData);
static tVisualEquipChange oVisualEquipChange = nullptr;

bool VisualEquipSync::s_isInstalled = false;
std::mutex VisualEquipSync::s_loadoutMutex;
std::unordered_map<CharacterType, CharacterLoadout> VisualEquipSync::s_cachedLoadouts;

// Pattern from BlackSpace Engine Reference Section 6.4 (VisualEquipChange P1)
static const char* VISUAL_EQUIP_CHANGE_AOB = "48 89 5C 24 10 48 89 74 24 20 66 44 89 44 24 18 55 57 41 54 41 56 41 57";

int64_t VisualEquipSync::OnVisualEquipChange(void* bodyComp, int16_t slotId, int16_t itemId, void* itemData) {
    if (slotId >= 0 && slotId < static_cast<int16_t>(MAX_EQUIP_SLOTS)) {
        std::lock_guard<std::mutex> lock(s_loadoutMutex);

        // Assume local host is Kliff initially, or query active session character
        CharacterType localChar = CharacterType::Kliff;
        s_cachedLoadouts[localChar].slotItemIds[slotId] = itemId;

        Logger::Log("[VisualEquip] Local equipment change: %s Slot %d -> Item %d\n",
                    CharacterToString(localChar), slotId, itemId);

        // Broadcast over Steamworks P2P to connected peers
        PktVisualEquipSync pkt{};
        pkt.header.magic[0] = 'C';
        pkt.header.magic[1] = 'D';
        pkt.header.type = PacketType::VisualEquipSync;
        pkt.header.sequenceNumber = 1;
        pkt.header.timestamp = 0.0;
        pkt.playerSteamId = 76561198000000001ULL;
        pkt.character = localChar;
        pkt.slotId = slotId;
        pkt.itemId = itemId;
        pkt.cosmeticFlags = 0;

        SteamTransport::BroadcastPacket(&pkt, sizeof(pkt), true);
    }

    if (oVisualEquipChange) {
        return oVisualEquipChange(bodyComp, slotId, itemId, itemData);
    }
    return 0;
}

bool VisualEquipSync::ApplyRemoteEquip(const PktVisualEquipSync& pkt) {
    if (pkt.slotId < 0 || pkt.slotId >= static_cast<int16_t>(MAX_EQUIP_SLOTS)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(s_loadoutMutex);
    s_cachedLoadouts[pkt.character].slotItemIds[pkt.slotId] = pkt.itemId;
    s_cachedLoadouts[pkt.character].cosmeticFlags[pkt.slotId] = pkt.cosmeticFlags;

    Logger::Log("[VisualEquip] Applied remote loadout: %s Slot %d = Item %d\n",
                CharacterToString(pkt.character), pkt.slotId, pkt.itemId);

    // If proxy exists in world, invoke trampoline on its body component
    const auto* proxy = RemoteProxyManager::Instance().GetProxyByCharacter(pkt.character);
    if (proxy && proxy->equipCompAddress != 0 && oVisualEquipChange) {
        oVisualEquipChange(reinterpret_cast<void*>(proxy->equipCompAddress), pkt.slotId, pkt.itemId, nullptr);
    }

    return true;
}

void VisualEquipSync::SynchronizeInitialLoadout(PlayerId targetPeerId, CharacterType character) {
    std::lock_guard<std::mutex> lock(s_loadoutMutex);
    const auto& loadout = s_cachedLoadouts[character];

    for (int16_t i = 0; i < static_cast<int16_t>(MAX_EQUIP_SLOTS); ++i) {
        if (loadout.slotItemIds[i] != -1) {
            PktVisualEquipSync pkt{};
            pkt.header.magic[0] = 'C';
            pkt.header.magic[1] = 'D';
            pkt.header.type = PacketType::VisualEquipSync;
            pkt.playerSteamId = 76561198000000001ULL;
            pkt.character = character;
            pkt.slotId = i;
            pkt.itemId = loadout.slotItemIds[i];
            pkt.cosmeticFlags = loadout.cosmeticFlags[i];

            SteamTransport::SendPacket(targetPeerId, &pkt, sizeof(pkt), true);
        }
    }
}

CharacterLoadout VisualEquipSync::GetCachedLoadout(CharacterType character) {
    std::lock_guard<std::mutex> lock(s_loadoutMutex);
    return s_cachedLoadouts[character];
}

bool VisualEquipSync::Install() {
    if (s_isInstalled) return true;

    uintptr_t funcAddr = PatternScanner::Scan(GetModuleHandleW(nullptr), VISUAL_EQUIP_CHANGE_AOB);
    if (funcAddr != 0) {
        if (MH_CreateHook(reinterpret_cast<LPVOID>(funcAddr),
                          reinterpret_cast<LPVOID>(&OnVisualEquipChange),
                          reinterpret_cast<LPVOID*>(&oVisualEquipChange)) == MH_OK) {
            MH_EnableHook(reinterpret_cast<LPVOID>(funcAddr));
            Logger::Log("[VisualEquip] Hooked VisualEquipChange at 0x%p\n", (void*)funcAddr);
        }
    } else {
        Logger::Log("[VisualEquip] Operating in standalone emulation / offline mode.\n");
    }

    s_isInstalled = true;
    return true;
}

void VisualEquipSync::Remove() {
    if (!s_isInstalled) return;
    s_isInstalled = false;
}

} // namespace CrimsonCoop

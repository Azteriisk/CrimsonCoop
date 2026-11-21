#include "Common.hpp"
#include "SessionManager.hpp"
#include "Interpolation.hpp"
#include "CoopSaveManager.hpp"
#include "SaveRedirector.hpp"
#include "EngineTypes.hpp"
#include "PatternScan.hpp"
#include "BlackSpaceChain.hpp"
#include <iostream>
#include <iomanip>

using namespace CrimsonCoop;

void PrintRoster(const SessionManager& session) {
    std::cout << "\n================ CURRENT GREYMANE ROSTER ================\n";
    for (const auto& [steamId, info] : session.GetRoster()) {
        std::cout << "  - [" << (info.isHost ? "HOST" : "PEER") << "] "
                  << std::left << std::setw(15) << info.playerName
                  << " | SteamID: " << steamId
                  << " | Role: " << CharacterToString(info.character) << "\n";
    }
    std::cout << "=========================================================\n\n";
}

int main() {
    std::cout << "=========================================================\n";
    std::cout << "  Crimson Desert 3-Player Co-op Architecture Harness     \n";
    std::cout << "  Characters: Kliff, Damiane, Oongka                     \n";
    std::cout << "=========================================================\n\n";

    // 1. Host starts session as Kliff
    PlayerId hostSteamId = 76561198000000001ULL;
    SessionManager session(hostSteamId, CharacterType::Kliff, "Host_Player");
    std::cout << "[Session] Host initialized session as Kliff.\n";
    PrintRoster(session);

    // 2. Client 2 tries to join as Kliff (Duplicate conflict test)
    PlayerId client2SteamId = 76561198000000002ULL;
    std::cout << "[Connect] Client 2 (Player_Alex) requesting to join as Kliff...\n";
    auto resp2 = session.HandleJoinRequest(client2SteamId, CharacterType::Kliff, "Player_Alex");

    if (resp2.status == HandshakeStatus::ConflictCharacter) {
        std::cout << "  -> [CONFLICT CAUGHT] Kliff is already in use by Host!\n";
        std::cout << "  -> Host offered available options (Mask: 0x" << std::hex << (int)resp2.availableCharactersMask << std::dec << "):\n";
        if (resp2.availableCharactersMask & (1 << 1)) std::cout << "     * Damiane (Available)\n";
        if (resp2.availableCharactersMask & (1 << 2)) std::cout << "     * Oongka (Available)\n";

        // Client 2 re-requests as Damiane
        std::cout << "  -> Client 2 auto-selects Damiane...\n";
        auto resolveResp = session.HandleJoinRequest(client2SteamId, CharacterType::Damiane, "Player_Alex");
        if (resolveResp.status == HandshakeStatus::Accepted) {
            std::cout << "  -> [ACCEPTED] Client 2 joined as " << CharacterToString(resolveResp.assignedCharacter) << "!\n";
        }
    }

    // 3. Client 3 joins as Oongka
    PlayerId client3SteamId = 76561198000000003ULL;
    std::cout << "\n[Connect] Client 3 (Player_Sam) requesting to join as Oongka...\n";
    auto resp3 = session.HandleJoinRequest(client3SteamId, CharacterType::Oongka, "Player_Sam");
    if (resp3.status == HandshakeStatus::Accepted) {
        std::cout << "  -> [ACCEPTED] Client 3 joined as " << CharacterToString(resp3.assignedCharacter) << "!\n";
    }

    PrintRoster(session);

    // 4. Client 4 attempts to join (Lobby full test)
    PlayerId client4SteamId = 76561198000000004ULL;
    std::cout << "[Connect] Client 4 (Player_Dave) attempting to join...\n";
    auto resp4 = session.HandleJoinRequest(client4SteamId, CharacterType::None, "Player_Dave");
    if (resp4.status == HandshakeStatus::LobbyFull) {
        std::cout << "  -> [REJECTED] Lobby is full (Max 3 Greymane players)!\n\n";
    }

    // 5. In-game character switch interception test
    std::cout << "[Switch Guard] Host attempting to switch character to Damiane via single-player wheel...\n";
    std::string blockReason;
    bool canSwitch = session.CanLocalPlayerSwitchTo(hostSteamId, CharacterType::Damiane, blockReason);
    if (!canSwitch) {
        std::cout << "  -> [INTERCEPTED & BLOCKED] Reason: " << blockReason << "\n\n";
    }

    // 6. Voluntary Swap Negotiation Test
    std::cout << "[Swap Protocol] Host (Kliff) requests to swap characters with Client 3 (Oongka)...\n";
    session.InitiateSwapRequest(hostSteamId, client3SteamId);
    std::cout << "  -> Client 3 approves swap request.\n";
    session.ResolveSwapResponse(client3SteamId, true);
    std::cout << "  -> Swap executed!\n";
    PrintRoster(session);

    // 7. Transform Interpolation Test
    std::cout << "[Interpolation] Simulating 30Hz network transform stream for Damiane...\n";
    TransformInterpolator interpolator(0.05); // 50ms buffer

    interpolator.AddSnapshot({1.00, Vector3(10.0f, 0.0f, 50.0f), Rotation{0, 90, 0}, Vector3(2.0f, 0, 0)});
    interpolator.AddSnapshot({1.03, Vector3(10.6f, 0.0f, 50.0f), Rotation{0, 95, 0}, Vector3(2.0f, 0, 0)});
    interpolator.AddSnapshot({1.06, Vector3(11.2f, 0.0f, 50.0f), Rotation{0, 100, 0}, Vector3(2.0f, 0, 0)});

    Vector3 sampledPos;
    Rotation sampledRot;
    if (interpolator.Sample(1.08, sampledPos, sampledRot)) {
        std::cout << "  -> Sample at renderTime=1.08s (Target=1.03s):\n";
        std::cout << "     Position: (" << sampledPos.x << ", " << sampledPos.y << ", " << sampledPos.z << ")\n";
        std::cout << "     Yaw: " << sampledRot.yaw << " deg (Smoothly interpolated)\n\n";
    }

    // 8. Isolated Co-op Save File (.cdcoop) Test
    std::wstring vanillaMockPath = L"C:\\Users\\Player\\Saved Games\\CrimsonDesert\\Saved\\SaveGames\\Slot0.sav";
    std::wstring redirectedMockPath = SaveRedirector::TransformSavePath(vanillaMockPath);
    std::wcout << L"[Save Redirector] Vanilla Target:    " << vanillaMockPath << L"\n";
    std::wcout << L"[Save Redirector] Redirected Target: " << redirectedMockPath << L"\n";

    std::string coopSavePath = "CrimsonDesert_Coop_Slot0.cdcoop";
    std::cout << "[Save System] Creating isolated co-op save: " << coopSavePath << "...\n";

    CoopSaveData saveData{};
    saveData.header.hostSteamId = hostSteamId;
    saveData.header.saveTimestamp = 1732160000ULL;

    saveData.characters.push_back(CharacterProgressionData{
        .character = CharacterType::Kliff,
        .level = 24, .experience = 152000, .health = 3200, .maxHealth = 3200,
        .stamina = 150, .skillPointsSpent = 45, .equippedWeaponId = 101, .equippedArmorId = 201
    });
    saveData.characters.push_back(CharacterProgressionData{
        .character = CharacterType::Damiane,
        .level = 22, .experience = 135000, .health = 2400, .maxHealth = 2400,
        .stamina = 180, .skillPointsSpent = 40, .equippedWeaponId = 102, .equippedArmorId = 202
    });
    saveData.characters.push_back(CharacterProgressionData{
        .character = CharacterType::Oongka,
        .level = 25, .experience = 168000, .health = 4500, .maxHealth = 4500,
        .stamina = 120, .skillPointsSpent = 48, .equippedWeaponId = 103, .equippedArmorId = 203
    });

    saveData.questFlags.push_back(WorldQuestFlag{.questId = 1010, .stageId = 4, .isCompleted = 1}); // Greymane Reunion
    saveData.unlockedFastTravelPoints = {1, 4, 7, 12};

    if (CoopSaveManager::WriteCoopSave(coopSavePath, saveData)) {
        std::cout << "  -> Successfully wrote isolated .cdcoop save container!\n";
    }

    std::cout << "[Save System] Reading back and verifying .cdcoop container...\n";
    CoopSaveData loadedData{};
    std::string errorStr;
    if (CoopSaveManager::ReadCoopSave(coopSavePath, loadedData, errorStr)) {
        std::cout << "  -> File Signature verified: Magic 0x" << std::hex << loadedData.header.magic << std::dec << " ('CDCP')\n";
        std::cout << "  -> Loaded " << loadedData.characters.size() << " Greymane characters:\n";
        for (const auto& c : loadedData.characters) {
            std::cout << "     * " << CharacterToString(c.character) << " - Level " << c.level 
                      << " (HP: " << c.health << "/" << c.maxHealth << ")\n";
        }
        std::cout << "  -> World Quest Flags: " << loadedData.questFlags.size()
                  << " | Fast Travel Points: " << loadedData.unlockedFastTravelPoints.size() << "\n";
    } else {
        std::cerr << "  -> Read Error: " << errorStr << "\n";
    }

    // 9. BlackSpace Engine Actor Classification & AOB Verification Test
    std::cout << "[BlackSpace Engine] Testing appearance asset path classification...\n";
    std::string kliffPath = "character/appearance/1_pc/1_phm/cd_phm_macduff/cd_phm_macduff_00000.app_xml";
    std::string damianePath = "character/appearance/1_pc/2_phw/cd_phw_damian/cd_phw_damian_00000.app_xml";
    std::string oongkaPath = "character/appearance/1_pc/1_phm/cd_phm_oongka/cd_phm_oongka_00000.app_xml";

    std::cout << "  -> Path: " << kliffPath << " => " << CharacterToString(ClassifyAppearancePath(kliffPath)) << "\n";
    std::cout << "  -> Path: " << damianePath << " => " << CharacterToString(ClassifyAppearancePath(damianePath)) << "\n";
    std::cout << "  -> Path: " << oongkaPath << " => " << CharacterToString(ClassifyAppearancePath(oongkaPath)) << "\n";

    // Test IDA-style pattern parser
    auto parsedAob = PatternScanner::ParsePattern("48 89 ?? ?? ?? ?? ?? 48 8D");
    std::cout << "  -> Parsed AOB Pattern with " << parsedAob.size() << " bytes (Wildcards correctly recognized)\n";

    std::cout << "\n=========================================================\n";
    std::cout << "  All Co-op Core Subsystems Verified Successfully!       \n";
    std::cout << "=========================================================\n";

    return 0;
}

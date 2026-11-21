#include "GuiOverlay.hpp"
#include "ActionCombatSync.hpp"
#include "VisualEquipSync.hpp"
#include "RemoteProxyManager.hpp"
#include "Logger.hpp"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#include <chrono>

namespace CrimsonCoop {

bool GuiOverlay::s_isInitialized = false;
bool GuiOverlay::s_isVisible = false; // Hidden on startup for clean game boot! Press F2 or Insert to toggle in-game
ToastNotification GuiOverlay::s_activeToast{};
ConflictDialogState GuiOverlay::s_conflictDialog{};
SwapPromptState GuiOverlay::s_swapPrompt{};

static double GetCurrentTimeSeconds() {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration<double>(now).count();
}

bool GuiOverlay::Initialize(HWND hWnd, ID3D12Device* device, int bufferCount, DXGI_FORMAT rtvFormat, ID3D12DescriptorHeap* srvHeap) {
    if (s_isInitialized) return true;

    Logger::Log("[GuiOverlay] Initializing Dear ImGui for window %p, buffers: %d, format: %d...\n",
                (void*)hWnd, bufferCount, rtvFormat);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Dark sleek mercenary aesthetic for Pywel theme
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 6.0f;
    style.Colors[ImGuiCol_WindowBg]  = ImVec4(0.08f, 0.08f, 0.10f, 0.90f);
    style.Colors[ImGuiCol_Header]    = ImVec4(0.58f, 0.18f, 0.18f, 0.85f); // Crimson accent
    style.Colors[ImGuiCol_Button]    = ImVec4(0.40f, 0.12f, 0.12f, 0.80f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.65f, 0.18f, 0.18f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive]  = ImVec4(0.80f, 0.22f, 0.22f, 1.0f);

    if (!ImGui_ImplWin32_Init(hWnd)) {
        Logger::Log("[GuiOverlay] ImGui_ImplWin32_Init failed!\n");
        return false;
    }

    if (!ImGui_ImplDX12_Init(
            device,
            bufferCount,
            rtvFormat,
            srvHeap,
            srvHeap->GetCPUDescriptorHandleForHeapStart(),
            srvHeap->GetGPUDescriptorHandleForHeapStart())) {
        Logger::Log("[GuiOverlay] ImGui_ImplDX12_Init failed!\n");
        ImGui_ImplWin32_Shutdown();
        return false;
    }

    s_isInitialized = true;
    Logger::Log("[GuiOverlay] Dear ImGui successfully initialized!\n");
    return true;
}

void GuiOverlay::Shutdown() {
    if (!s_isInitialized) return;
    Logger::Log("[GuiOverlay] Shutting down Dear ImGui...\n");
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    s_isInitialized = false;
}

void GuiOverlay::ShowToast(const std::string& message, double durationSeconds) {
    s_activeToast.message = message;
    s_activeToast.expireTime = GetCurrentTimeSeconds() + durationSeconds;
    Logger::Log("[GuiOverlay] Toast: %s\n", message.c_str());
}

void GuiOverlay::OpenConflictDialog(uint8_t availableMask, std::function<void(CharacterType)> onSelected) {
    s_conflictDialog.isOpen = true;
    s_conflictDialog.availableMask = availableMask;
    s_conflictDialog.onSelected = onSelected;
}

void GuiOverlay::OpenSwapPrompt(PlayerId requesterId, const std::string& requesterName, CharacterType charType) {
    s_swapPrompt.isOpen = true;
    s_swapPrompt.requesterId = requesterId;
    s_swapPrompt.requesterName = requesterName;
    s_swapPrompt.requesterChar = charType;
}

void GuiOverlay::Render(SessionManager* pSession) {
    if (!s_isInitialized || !s_isVisible) return;

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // 1. Watermark / Status Badge
    DrawWatermarkBadge(pSession);

    // 2. Full Greymane Co-Op HUD & Menu
    DrawPartyHUD(pSession);
    DrawConflictModal();
    DrawSwapPrompt(pSession);
    DrawToasts();

    ImGui::Render();
}

void GuiOverlay::DrawWatermarkBadge(SessionManager* pSession) {
    ImGui::SetNextWindowPos(ImVec2(15.0f, 15.0f), ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.8f, 0.2f, 0.2f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.07f, 0.85f));

    if (ImGui::Begin("##CrimsonCoopWatermark", nullptr, flags)) {
        // Glowing status indicator
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.3f, 1.0f), "[●] Crimson Desert Co-Op");
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        if (pSession) {
            CharacterType activeChar = pSession->GetLocalCharacter();
            const char* charName = CharacterToString(activeChar);
            ImVec4 charCol = ImVec4(0.9f, 0.7f, 0.2f, 1.0f);
            if (activeChar == CharacterType::Damiane) charCol = ImVec4(0.3f, 0.8f, 0.95f, 1.0f);
            if (activeChar == CharacterType::Oongka)  charCol = ImVec4(0.95f, 0.3f, 0.3f, 1.0f);
            ImGui::TextColored(charCol, "%s", charName);
        } else {
            ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f), "Kliff");
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        if (ImGui::SmallButton(s_isVisible ? "Hide [F2]" : "Open Menu [F2]")) {
            s_isVisible = !s_isVisible;
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
}

void GuiOverlay::DrawPartyHUD(SessionManager* pSession) {
    ImGui::SetNextWindowPos(ImVec2(15.0f, 65.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340.0f, 0.0f));

    ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize;
    if (ImGui::Begin("Greymane Party (3-Player Co-Op)", &s_isVisible, flags)) {
        // Section 1: Save Sandbox Status
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "[Save Sandbox Active]");
        ImGui::TextDisabled("Single-player Slot0.sav is protected.");
        ImGui::TextDisabled("Redirected -> CoopSaveGames\\*.cdcoop");
        ImGui::Separator();

        // Section 2: Session Roster
        if (pSession) {
            const auto& roster = pSession->GetRoster();
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "Party Roster (%d / %d):",
                               static_cast<int>(roster.size()), static_cast<int>(MAX_PLAYERS));

            for (const auto& [steamId, info] : roster) {
                ImGui::PushID(static_cast<int>(steamId));

                const char* charName = CharacterToString(info.character);
                ImVec4 roleColor = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
                if (info.character == CharacterType::Kliff)   roleColor = ImVec4(0.95f, 0.65f, 0.2f, 1.0f); // Gold
                if (info.character == CharacterType::Damiane) roleColor = ImVec4(0.3f, 0.8f, 0.95f, 1.0f); // Teal
                if (info.character == CharacterType::Oongka)  roleColor = ImVec4(0.95f, 0.3f, 0.3f, 1.0f); // Crimson

                ImGui::TextColored(roleColor, "[%s] %s", charName, info.playerName.c_str());
                ImGui::SameLine();
                ImGui::TextDisabled(info.isHost ? "(Host/Local)" : "(Remote Peer)");

                // Health & Stamina bars
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.75f, 0.3f, 1.0f));
                ImGui::ProgressBar(1.0f, ImVec2(-1.0f, 8.0f), "");
                ImGui::PopStyleColor();

                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.9f, 0.8f, 0.2f, 1.0f));
                ImGui::ProgressBar(0.85f, ImVec2(-1.0f, 5.0f), "");
                ImGui::PopStyleColor();

                ImGui::Spacing();
                ImGui::PopID();
            }

            // Section 3: 2-Player Duo Session Indicator & Reserve Slot
            if (pSession->IsDuoSession()) {
                auto reserves = pSession->GetUnoccupiedCharacters();
                if (!reserves.empty()) {
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.95f, 1.0f), "[2-Player Duo Mode Active]");
                    ImGui::Text("Reserve Hero: %s", CharacterToString(reserves[0]));
                    if (ImGui::Button("Swap into Reserve Hero", ImVec2(-1.0f, 26.0f))) {
                        pSession->SwapToReserve();
                        ShowToast(std::string("Swapped to reserve hero: ") + CharacterToString(reserves[0]));
                    }
                }
            } else if (roster.size() == 1) {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1.0f), "[Solo / Host Mode]");
                ImGui::TextDisabled("Steam P2P Sockets listening for teammates.");
                
                // Hero Switcher for Solo testing
                ImGui::Spacing();
                ImGui::Text("Switch Local Hero:");
                std::string switchReason;
                if (ImGui::Button("Kliff", ImVec2(90.0f, 24.0f))) {
                    pSession->SwitchLocalCharacter(CharacterType::Kliff, switchReason);
                    ShowToast("Switched active hero to Kliff");
                }
                ImGui::SameLine();
                if (ImGui::Button("Damiane", ImVec2(90.0f, 24.0f))) {
                    pSession->SwitchLocalCharacter(CharacterType::Damiane, switchReason);
                    ShowToast("Switched active hero to Damiane");
                }
                ImGui::SameLine();
                if (ImGui::Button("Oongka", ImVec2(90.0f, 24.0f))) {
                    pSession->SwitchLocalCharacter(CharacterType::Oongka, switchReason);
                    ShowToast("Switched active hero to Oongka");
                }
            }
        }

        // Section 4: Combat Actions & Animation Replication
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Combat Actions (Sync):");
        CharacterType localHero = pSession ? pSession->GetLocalCharacter() : CharacterType::Kliff;

        if (ImGui::Button("Light Attack [LMB]", ImVec2(145.0f, 24.0f))) {
            ActionCombatSync::TriggerLocalAction(localHero, CombatActions::LIGHT_ATTACK_1);
            ShowToast("Replicating Light Attack combo...");
        }
        ImGui::SameLine();
        if (ImGui::Button("Heavy Attack [RMB]", ImVec2(145.0f, 24.0f))) {
            ActionCombatSync::TriggerLocalAction(localHero, CombatActions::HEAVY_ATTACK_SWING);
            ShowToast("Replicating Heavy Attack charge...");
        }

        if (ImGui::Button("Dodge Roll [Space]", ImVec2(145.0f, 24.0f))) {
            ActionCombatSync::TriggerLocalAction(localHero, CombatActions::DODGE_ROLL);
            ShowToast("Replicating Dodge Roll...");
        }
        ImGui::SameLine();
        if (ImGui::Button("Parry Counter [Q]", ImVec2(145.0f, 24.0f))) {
            ActionCombatSync::TriggerLocalAction(localHero, CombatActions::PARRY_COUNTER);
            ShowToast("Replicating Parry Counter!");
        }

        if (ImGui::Button("Hero Skill Ability [E]", ImVec2(-1.0f, 24.0f))) {
            ActionCombatSync::TriggerLocalAction(localHero, CombatActions::SKILL_ABILITY_1);
            ShowToast("Replicating Hero Unique Skill!");
        }

        // Section 5: Visual Equipment & Loadout
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Visual Equipment Sync:");
        if (ImGui::Button("Equip Silver Blade (ID 1042)", ImVec2(145.0f, 24.0f))) {
            VisualEquipSync::OnVisualEquipChange(nullptr, SlotMainHand, 1042, nullptr);
            ShowToast("Equipped Silver Blade (Synced to peers)");
        }
        ImGui::SameLine();
        if (ImGui::Button("Equip Iron Shield (ID 2085)", ImVec2(145.0f, 24.0f))) {
            VisualEquipSync::OnVisualEquipChange(nullptr, SlotOffHand, 2085, nullptr);
            ShowToast("Equipped Iron Shield (Synced to peers)");
        }

        // Section 6: Matchmaking & Steam Controls
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "Steam P2P / Networking:");
        if (ImGui::Button("Test Co-Op Toast Notification", ImVec2(-1.0f, 24.0f))) {
            ShowToast("Greymane Co-Op Alert: Kliff engaged in combat!");
        }

        // Section 7: Telemetry Footer
        ImGui::Separator();
        ImGui::TextDisabled("Tick Rate: 30 Hz | Hermite Splines: ON");
        ImGui::TextDisabled("Toggle Menu: F2 or Insert key");
    }
    ImGui::End();
}

void GuiOverlay::DrawConflictModal() {
    if (!s_conflictDialog.isOpen) return;

    ImGui::OpenPopup("Character Conflict Arbitration");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(380.0f, 0.0f));

    if (ImGui::BeginPopupModal("Character Conflict Arbitration", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("The character you selected is already in use by the session Host or teammate.");
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Select an available Greymane to deploy:");
        ImGui::Separator();
        ImGui::Spacing();

        if (s_conflictDialog.availableMask & (1 << 0)) {
            if (ImGui::Button("Deploy as Kliff", ImVec2(-1.0f, 32.0f))) {
                if (s_conflictDialog.onSelected) s_conflictDialog.onSelected(CharacterType::Kliff);
                s_conflictDialog.isOpen = false;
                ImGui::CloseCurrentPopup();
            }
        }

        if (s_conflictDialog.availableMask & (1 << 1)) {
            if (ImGui::Button("Deploy as Damiane", ImVec2(-1.0f, 32.0f))) {
                if (s_conflictDialog.onSelected) s_conflictDialog.onSelected(CharacterType::Damiane);
                s_conflictDialog.isOpen = false;
                ImGui::CloseCurrentPopup();
            }
        }

        if (s_conflictDialog.availableMask & (1 << 2)) {
            if (ImGui::Button("Deploy as Oongka", ImVec2(-1.0f, 32.0f))) {
                if (s_conflictDialog.onSelected) s_conflictDialog.onSelected(CharacterType::Oongka);
                s_conflictDialog.isOpen = false;
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndPopup();
    }
}

void GuiOverlay::DrawSwapPrompt(SessionManager* pSession) {
    if (!s_swapPrompt.isOpen) return;

    ImGui::OpenPopup("Role Swap Request");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Role Swap Request", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s wants to swap roles with you!", s_swapPrompt.requesterName.c_str());
        ImGui::Text("They will become %s, you will become %s.",
                    CharacterToString(s_swapPrompt.requesterChar),
                    "your target character");
        ImGui::Spacing();

        if (ImGui::Button("Accept Swap", ImVec2(120, 0))) {
            pSession->ResolveSwapResponse(s_swapPrompt.requesterId, true);
            s_swapPrompt.isOpen = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Decline", ImVec2(120, 0))) {
            pSession->ResolveSwapResponse(s_swapPrompt.requesterId, false);
            s_swapPrompt.isOpen = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void GuiOverlay::DrawToasts() {
    double now = GetCurrentTimeSeconds();
    if (s_activeToast.expireTime <= now || s_activeToast.message.empty()) {
        return;
    }

    ImVec2 viewportSize = ImGui::GetMainViewport()->Size;
    ImGui::SetNextWindowPos(ImVec2(viewportSize.x * 0.5f, viewportSize.y - 80.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.92f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.9f, 0.3f, 0.3f, 0.8f));

    if (ImGui::Begin("##ToastWindow", nullptr, flags)) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[Co-Op Alert]");
        ImGui::SameLine();
        ImGui::TextUnformatted(s_activeToast.message.c_str());
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

} // namespace CrimsonCoop

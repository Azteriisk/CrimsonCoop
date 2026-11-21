#include "GuiOverlay.hpp"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#include <chrono>

namespace CrimsonCoop {

bool GuiOverlay::s_isInitialized = false;
bool GuiOverlay::s_isVisible = true;
ToastNotification GuiOverlay::s_activeToast{};
ConflictDialogState GuiOverlay::s_conflictDialog{};
SwapPromptState GuiOverlay::s_swapPrompt{};

static double GetCurrentTimeSeconds() {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration<double>(now).count();
}

bool GuiOverlay::Initialize(HWND hWnd, ID3D12Device* device, int bufferCount, ID3D12DescriptorHeap* srvHeap) {
    if (s_isInitialized) return true;

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
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.10f, 0.85f);
    style.Colors[ImGuiCol_Header]   = ImVec4(0.58f, 0.18f, 0.18f, 0.80f); // Crimson accent

    if (!ImGui_ImplWin32_Init(hWnd)) {
        return false;
    }

    if (!ImGui_ImplDX12_Init(
            device,
            bufferCount,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            srvHeap,
            srvHeap->GetCPUDescriptorHandleForHeapStart(),
            srvHeap->GetGPUDescriptorHandleForHeapStart())) {
        ImGui_ImplWin32_Shutdown();
        return false;
    }

    s_isInitialized = true;
    return true;
}

void GuiOverlay::Shutdown() {
    if (!s_isInitialized) return;
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    s_isInitialized = false;
}

void GuiOverlay::ShowToast(const std::string& message, double durationSeconds) {
    s_activeToast.message = message;
    s_activeToast.expireTime = GetCurrentTimeSeconds() + durationSeconds;
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
    if (!s_isInitialized) return;

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (s_isVisible && pSession != nullptr) {
        DrawPartyHUD(pSession);
        DrawConflictModal();
        DrawSwapPrompt(pSession);
        DrawToasts();
    }

    ImGui::Render();
}

void GuiOverlay::DrawPartyHUD(SessionManager* pSession) {
    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(280.0f, 0.0f));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize;
    if (ImGui::Begin("Greymane Party (3P Co-Op)", nullptr, flags)) {
        const auto& roster = pSession->GetRoster();

        for (const auto& [steamId, info] : roster) {
            ImGui::PushID(static_cast<int>(steamId));

            // Character Icon / Name header
            const char* charName = CharacterToString(info.character);
            ImVec4 roleColor = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
            if (info.character == CharacterType::Kliff)   roleColor = ImVec4(0.9f, 0.6f, 0.2f, 1.0f); // Gold
            if (info.character == CharacterType::Damiane) roleColor = ImVec4(0.3f, 0.7f, 0.9f, 1.0f); // Teal
            if (info.character == CharacterType::Oongka)  roleColor = ImVec4(0.9f, 0.3f, 0.3f, 1.0f); // Crimson

            ImGui::TextColored(roleColor, "[%s] %s", charName, info.playerName.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled(info.isHost ? "(Host)" : "(Peer)");

            // Simulated Health and Stamina bars
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.7f, 0.3f, 1.0f));
            ImGui::ProgressBar(0.85f, ImVec2(-1.0f, 10.0f), "");
            ImGui::PopStyleColor();

            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.9f, 0.8f, 0.2f, 1.0f));
            ImGui::ProgressBar(0.60f, ImVec2(-1.0f, 6.0f), "");
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::PopID();
        }

        // 2-Player Duo Session Indicator & Reserve Slot
        if (pSession->IsDuoSession()) {
            auto reserves = pSession->GetUnoccupiedCharacters();
            if (!reserves.empty()) {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "[2P Duo Mode Active]");
                ImGui::TextDisabled("Reserve Character: %s", CharacterToString(reserves[0]));
                ImGui::TextWrapped("Either player can swap into %s anytime using the radial wheel.", CharacterToString(reserves[0]));
            }
        } else if (roster.size() < MAX_PLAYERS) {
            ImGui::TextDisabled("Waiting for %d Greymane companion(s)...", static_cast<int>(MAX_PLAYERS - roster.size()));
        }
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
    ImGui::SetNextWindowBgAlpha(0.90f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav;

    if (ImGui::Begin("##ToastWindow", nullptr, flags)) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[Co-Op Warning]");
        ImGui::SameLine();
        ImGui::TextUnformatted(s_activeToast.message.c_str());
    }
    ImGui::End();
}

} // namespace CrimsonCoop

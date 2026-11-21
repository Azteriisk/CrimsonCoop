#pragma once

#include "Common.hpp"
#include "SessionManager.hpp"
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <string>
#include <functional>

namespace CrimsonCoop {

struct ToastNotification {
    std::string message;
    double expireTime{0.0};
};

struct ConflictDialogState {
    bool isOpen{false};
    uint8_t availableMask{0};
    std::function<void(CharacterType)> onSelected;
};

struct SwapPromptState {
    bool isOpen{false};
    PlayerId requesterId{0};
    std::string requesterName;
    CharacterType requesterChar{CharacterType::None};
};

class GuiOverlay {
public:
    static bool Initialize(HWND hWnd, ID3D12Device* device, int bufferCount, ID3D12DescriptorHeap* srvHeap);
    static void Shutdown();

    // Renders the Co-Op HUD and modals into the current DX12 frame
    static void Render(SessionManager* pSession);

    // Shows in-game toast notifications (e.g., "Cannot switch: Oongka is taken!")
    static void ShowToast(const std::string& message, double durationSeconds = 4.0);

    // Shows character conflict dialog when connecting
    static void OpenConflictDialog(uint8_t availableMask, std::function<void(CharacterType)> onSelected);

    // Shows swap request prompt
    static void OpenSwapPrompt(PlayerId requesterId, const std::string& requesterName, CharacterType charType);

    static bool IsVisible() { return s_isVisible; }
    static void ToggleVisible() { s_isVisible = !s_isVisible; }

private:
    static void DrawPartyHUD(SessionManager* pSession);
    static void DrawConflictModal();
    static void DrawSwapPrompt(SessionManager* pSession);
    static void DrawToasts();

    static bool s_isInitialized;
    static bool s_isVisible;
    static ToastNotification s_activeToast;
    static ConflictDialogState s_conflictDialog;
    static SwapPromptState s_swapPrompt;
};

} // namespace CrimsonCoop

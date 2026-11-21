#pragma once
#include <windows.h>
#include <string>

namespace CrimsonCoop {

class VisualWatermark {
public:
    static bool Initialize(HWND hGameWnd);
    static void UpdateStatus(const std::wstring& heroName, bool isCoopActive = true);
    static void Shutdown();
    static bool IsActive();

private:
    static DWORD WINAPI WatermarkThread(LPVOID lpParam);
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    static HWND s_hWnd;
    static HWND s_hGameWnd;
    static HANDLE s_hThread;
    static bool s_running;
    static std::wstring s_heroName;
    static bool s_isCoopActive;
};

} // namespace CrimsonCoop

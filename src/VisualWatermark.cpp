#include "VisualWatermark.hpp"
#include "Logger.hpp"
#include <iostream>

namespace CrimsonCoop {

HWND VisualWatermark::s_hWnd = nullptr;
HWND VisualWatermark::s_hGameWnd = nullptr;
HANDLE VisualWatermark::s_hThread = nullptr;
bool VisualWatermark::s_running = false;
std::wstring VisualWatermark::s_heroName = L"Kliff";
bool VisualWatermark::s_isCoopActive = true;

LRESULT CALLBACK VisualWatermark::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);

        // Double buffering
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right - rc.left, rc.bottom - rc.top);
        HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

        // Fill color-keyed transparent background (Black RGB(0,0,0) is keyed out)
        HBRUSH bgBrush = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(memDC, &rc, bgBrush);
        DeleteObject(bgBrush);

        // Draw badge pill
        RECT badgeRc = { rc.left + 4, rc.top + 4, rc.right - 4, rc.bottom - 4 };
        HBRUSH pillBrush = CreateSolidBrush(RGB(20, 16, 22));
        HPEN borderPen = CreatePen(PS_SOLID, 2, RGB(180, 40, 40));
        HGDIOBJ oldBrush = SelectObject(memDC, pillBrush);
        HGDIOBJ oldPen = SelectObject(memDC, borderPen);
        RoundRect(memDC, badgeRc.left, badgeRc.top, badgeRc.right, badgeRc.bottom, 12, 12);
        SelectObject(memDC, oldBrush);
        SelectObject(memDC, oldPen);
        DeleteObject(pillBrush);
        DeleteObject(borderPen);

        // Text setup
        SetBkMode(memDC, TRANSPARENT);
        HFONT hFont = CreateFontW(
            17, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
        );
        HGDIOBJ oldFont = SelectObject(memDC, hFont);

        // Draw glowing bullet / asterisk "*" in green
        SetTextColor(memDC, RGB(50, 240, 90));
        RECT dotRc = { badgeRc.left + 12, badgeRc.top + 5, badgeRc.left + 30, badgeRc.bottom - 5 };
        DrawTextW(memDC, L"★", -1, &dotRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // Draw mod text
        SetTextColor(memDC, RGB(240, 240, 245));
        std::wstring label = L" CrimsonCoop [ACTIVE: " + s_heroName + L"]";
        RECT textRc = { badgeRc.left + 28, badgeRc.top + 5, badgeRc.right - 10, badgeRc.bottom - 5 };
        DrawTextW(memDC, label.c_str(), -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        SelectObject(memDC, oldFont);
        DeleteObject(hFont);

        // Blit to screen
        BitBlt(hdc, 0, 0, rc.right - rc.left, rc.bottom - rc.top, memDC, 0, 0, SRCCOPY);

        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);

        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_NCHITTEST:
        return HTTRANSPARENT; // Click-through
    default:
        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }
}

DWORD WINAPI VisualWatermark::WatermarkThread(LPVOID lpParam) {
    (void)lpParam;
    HINSTANCE hInstance = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"CrimsonDesertCoopBadgeClass";
    RegisterClassExW(&wc);

    const int width = 310;
    const int height = 44;

    s_hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        wc.lpszClassName,
        L"CrimsonCoopWatermark",
        WS_POPUP | WS_VISIBLE,
        100, 100, width, height,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!s_hWnd) {
        Logger::Log("[VisualWatermark] Failed to create watermark overlay window.\n");
        return 1;
    }

    // Set transparency color key (RGB 0,0,0 is transparent) with 92% opacity
    SetLayeredWindowAttributes(s_hWnd, RGB(0, 0, 0), 235, LWA_COLORKEY | LWA_ALPHA);
    ShowWindow(s_hWnd, SW_SHOWNOACTIVATE);
    UpdateWindow(s_hWnd);

    Logger::Log("[VisualWatermark] Overlay badge active at top-right of screen.\n");

    MSG msg;
    while (s_running) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        // Align watermark to top-right of game window
        if (s_hGameWnd && IsWindow(s_hGameWnd)) {
            RECT rcGame;
            if (GetWindowRect(s_hGameWnd, &rcGame)) {
                int targetX = rcGame.right - width - 16;
                int targetY = rcGame.top + 16;
                if (targetX < rcGame.left) targetX = rcGame.left + 16;

                SetWindowPos(s_hWnd, HWND_TOPMOST, targetX, targetY, width, height,
                             SWP_NOACTIVATE | SWP_NOREDRAW | SWP_NOSIZE);
            }

            // Also keep window title marked so user can see it in taskbar/title
            static std::wstring s_lastTitle;
            std::wstring expectedTitle = L"Crimson Desert [* CrimsonCoop Active | Hero: " + s_heroName + L"]";
            if (s_lastTitle != expectedTitle) {
                s_lastTitle = expectedTitle;
                SetWindowTextW(s_hGameWnd, expectedTitle.c_str());
            }
        }

        Sleep(33); // ~30Hz reposition
    }

    if (s_hWnd) {
        DestroyWindow(s_hWnd);
        s_hWnd = nullptr;
    }
    UnregisterClassW(wc.lpszClassName, hInstance);
    return 0;
}

bool VisualWatermark::Initialize(HWND hGameWnd) {
    if (s_running) return true;
    s_hGameWnd = hGameWnd;
    s_running = true;

    s_hThread = CreateThread(nullptr, 0, WatermarkThread, nullptr, 0, nullptr);
    return s_hThread != nullptr;
}

void VisualWatermark::UpdateStatus(const std::wstring& heroName, bool isCoopActive) {
    s_heroName = heroName;
    s_isCoopActive = isCoopActive;
    if (s_hWnd && IsWindow(s_hWnd)) {
        InvalidateRect(s_hWnd, nullptr, TRUE);
    }
}

void VisualWatermark::Shutdown() {
    s_running = false;
    if (s_hThread) {
        WaitForSingleObject(s_hThread, 1000);
        CloseHandle(s_hThread);
        s_hThread = nullptr;
    }
}

bool VisualWatermark::IsActive() {
    return s_running && s_hWnd != nullptr;
}

} // namespace CrimsonCoop

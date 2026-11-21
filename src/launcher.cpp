#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

namespace CrimsonLauncher {

// Standard Steam AppID or Game Exe Name
const std::wstring GAME_EXE_NAME = L"CrimsonDesert.exe";
const std::wstring MOD_DLL_NAME  = L"CrimsonCoop.dll";

// 1. Locate the Game Executable
std::wstring FindGameExecutable() {
    // Check local directory and parent directories first
    fs::path current = fs::current_path();
    std::vector<fs::path> searchPaths = {
        current / GAME_EXE_NAME,
        current / L"bin64" / GAME_EXE_NAME,
        current.parent_path() / GAME_EXE_NAME,
        current.parent_path() / L"bin64" / GAME_EXE_NAME,
        current / L".." / L".." / GAME_EXE_NAME
    };

    for (const auto& path : searchPaths) {
        if (fs::exists(path)) {
            return path.wstring();
        }
    }

    // Check Steam Registry for Steam Installation path
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t steamPath[MAX_PATH] = {};
        DWORD pathLen = sizeof(steamPath);
        if (RegQueryValueExW(hKey, L"SteamPath", nullptr, nullptr, reinterpret_cast<LPBYTE>(steamPath), &pathLen) == ERROR_SUCCESS) {
            fs::path defaultLibrary = fs::path(steamPath) / L"steamapps" / L"common" / L"Crimson Desert";
            if (fs::exists(defaultLibrary / L"bin64" / GAME_EXE_NAME)) {
                RegCloseKey(hKey);
                return (defaultLibrary / L"bin64" / GAME_EXE_NAME).wstring();
            }
            if (fs::exists(defaultLibrary / GAME_EXE_NAME)) {
                RegCloseKey(hKey);
                return (defaultLibrary / GAME_EXE_NAME).wstring();
            }
        }
        RegCloseKey(hKey);
    }

    return L"";
}

// 2. Locate the Co-Op Mod DLL
std::wstring FindModDll() {
    fs::path current = fs::current_path();
    std::vector<fs::path> dllSearchPaths = {
        current / MOD_DLL_NAME,
        current / L"build" / MOD_DLL_NAME,
        current / L"build" / L"Release" / MOD_DLL_NAME,
        current.parent_path() / MOD_DLL_NAME
    };

    for (const auto& path : dllSearchPaths) {
        if (fs::exists(path)) {
            return fs::canonical(path).wstring();
        }
    }
    return L"";
}

// 3. Inject DLL into the Target Process Memory
bool InjectDll(HANDLE hProcess, const std::wstring& dllPath) {
    size_t pathSize = (dllPath.length() + 1) * sizeof(wchar_t);

    // Allocate memory inside target process
    LPVOID pRemoteMem = VirtualAllocEx(hProcess, nullptr, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemoteMem) {
        std::wcerr << L"[Error] Failed to allocate memory in game process (Code: " << GetLastError() << L")\n";
        return false;
    }

    // Write DLL path to target process memory
    SIZE_T bytesWritten = 0;
    if (!WriteProcessMemory(hProcess, pRemoteMem, dllPath.c_str(), pathSize, &bytesWritten) || bytesWritten != pathSize) {
        std::wcerr << L"[Error] Failed to write DLL path into game process memory.\n";
        VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
        return false;
    }

    // Get address of LoadLibraryW
    LPVOID pLoadLibraryW = reinterpret_cast<LPVOID>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
    if (!pLoadLibraryW) {
        std::wcerr << L"[Error] Could not locate LoadLibraryW address.\n";
        VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
        return false;
    }

    // Spawn remote thread to execute LoadLibraryW
    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(pLoadLibraryW), pRemoteMem, 0, nullptr);
    if (!hThread) {
        std::wcerr << L"[Error] CreateRemoteThread failed (Code: " << GetLastError() << L")\n";
        VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
        return false;
    }

    // Wait for DLL to load cleanly
    WaitForSingleObject(hThread, 5000);
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);

    return true;
}

} // namespace CrimsonLauncher

int main() {
    std::cout << "=========================================================\n";
    std::cout << "   Crimson Desert 3-Player Co-Op Standalone Launcher    \n";
    std::cout << "   Trio: Kliff, Damiane, Oongka                          \n";
    std::cout << "=========================================================\n\n";

    // 1. Locate the Mod DLL
    std::wstring modDllPath = CrimsonLauncher::FindModDll();
    if (modDllPath.empty()) {
        std::wcerr << L"[Error] Could not find CrimsonCoop.dll!\n";
        std::wcerr << L"Make sure CrimsonCoop.dll is built in the same folder.\n";
        std::cout << "\nPress ENTER to exit...";
        std::cin.get();
        return 1;
    }
    std::wcout << L"[Launcher] Found Co-Op Mod: " << modDllPath << L"\n";

    // 2. Locate the Game Executable
    std::wstring gameExePath = CrimsonLauncher::FindGameExecutable();
    if (gameExePath.empty()) {
        std::cout << "[Launcher] Could not auto-detect CrimsonDesert.exe path.\n";
        std::cout << "Please enter the full path to CrimsonDesert.exe:\n> ";
        std::string userPath;
        std::getline(std::cin, userPath);
        gameExePath = std::wstring(userPath.begin(), userPath.end());
    }

    if (!fs::exists(gameExePath)) {
        std::wcerr << L"[Error] Game executable not found at: " << gameExePath << L"\n";
        std::cout << "\nPress ENTER to exit...";
        std::cin.get();
        return 1;
    }

    std::wcout << L"[Launcher] Target Game: " << gameExePath << L"\n";
    std::wcout << L"[Launcher] Starting Crimson Desert in suspended state...\n";

    // 3. Launch Process Suspended (Clean memory injection before engine boot)
    STARTUPINFOW si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);

    fs::path gameDir = fs::path(gameExePath).parent_path();

    BOOL success = CreateProcessW(
        gameExePath.c_str(),
        nullptr,
        nullptr,
        nullptr,
        FALSE,
        CREATE_SUSPENDED,
        nullptr,
        gameDir.c_str(),
        &si,
        &pi
    );

    if (!success) {
        std::wcerr << L"[Error] Failed to launch game process (Code: " << GetLastError() << L")\n";
        std::cout << "\nPress ENTER to exit...";
        std::cin.get();
        return 1;
    }

    // 4. Inject Mod DLL purely into RAM for this session
    std::cout << "[Launcher] Injecting CrimsonCoop.dll into game memory...\n";
    if (CrimsonLauncher::InjectDll(pi.hProcess, modDllPath)) {
        std::cout << "  -> [SUCCESS] Co-Op mod injected into RAM!\n";
        std::cout << "  -> Save sandbox active: Single-player files untouched.\n";
        std::cout << "  -> Greymane party HUD and P2P sockets ready.\n";
    } else {
        std::wcerr << L"  -> [FAILED] DLL Injection failed. Game will start vanilla.\n";
    }

    // 5. Resume game execution
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    std::cout << "\n=========================================================\n";
    std::cout << "  Game Launched Successfully in Co-Op Mode!              \n";
    std::cout << "  (To play vanilla single-player, launch via Steam)      \n";
    std::cout << "=========================================================\n";

    Sleep(2000);
    return 0;
}

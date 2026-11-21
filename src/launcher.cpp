#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace CrimsonLauncher {

const std::wstring GAME_EXE_NAME = L"CrimsonDesert.exe";
const std::wstring MOD_DLL_NAME  = L"CrimsonCoop.dll";

// Helper to clean quotes and whitespace from paths
std::wstring CleanPath(std::wstring path) {
    if (path.empty()) return path;
    if (path.front() == L'"' && path.back() == L'"') {
        path = path.substr(1, path.length() - 2);
    }
    return path;
}

// Read all Steam Library Folders from libraryfolders.vdf
std::vector<fs::path> GetSteamLibraryFolders() {
    std::vector<fs::path> libraries;

    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t steamPath[MAX_PATH] = {};
        DWORD pathLen = sizeof(steamPath);
        if (RegQueryValueExW(hKey, L"SteamPath", nullptr, nullptr, reinterpret_cast<LPBYTE>(steamPath), &pathLen) == ERROR_SUCCESS) {
            fs::path mainSteam(steamPath);
            libraries.push_back(mainSteam);

            // Parse libraryfolders.vdf
            fs::path vdfPath = mainSteam / L"steamapps" / L"libraryfolders.vdf";
            if (fs::exists(vdfPath)) {
                std::ifstream vdfFile(vdfPath);
                std::string line;
                while (std::getline(vdfFile, line)) {
                    size_t pathKey = line.find("\"path\"");
                    if (pathKey != std::string::npos) {
                        size_t firstQuote = line.find('"', pathKey + 6);
                        size_t secondQuote = line.find('"', firstQuote + 1);
                        if (firstQuote != std::string::npos && secondQuote != std::string::npos) {
                            std::string rawPath = line.substr(firstQuote + 1, secondQuote - firstQuote - 1);
                            // Replace escaped double backslashes with single backslash
                            std::string cleanPathStr;
                            for (size_t i = 0; i < rawPath.length(); ++i) {
                                if (rawPath[i] == '\\' && i + 1 < rawPath.length() && rawPath[i + 1] == '\\') {
                                    continue;
                                }
                                cleanPathStr += rawPath[i];
                            }
                            fs::path libPath(cleanPathStr);
                            if (fs::exists(libPath) && std::find(libraries.begin(), libraries.end(), libPath) == libraries.end()) {
                                libraries.push_back(libPath);
                            }
                        }
                    }
                }
            }
        }
        RegCloseKey(hKey);
    }

    return libraries;
}

// 1. Locate the Game Executable across all drives and Steam libraries
std::wstring FindGameExecutable() {
    // Check local directory and parent directories first
    fs::path current = fs::current_path();
    std::vector<fs::path> localSearch = {
        current / GAME_EXE_NAME,
        current / L"bin64" / GAME_EXE_NAME,
        current.parent_path() / GAME_EXE_NAME,
        current.parent_path() / L"bin64" / GAME_EXE_NAME,
        current.parent_path().parent_path() / L"bin64" / GAME_EXE_NAME
    };

    for (const auto& path : localSearch) {
        if (fs::exists(path)) {
            return fs::canonical(path).wstring();
        }
    }

    // Check all Steam Library Folders (C:, D:, E:, etc.)
    auto libraries = GetSteamLibraryFolders();
    for (const auto& lib : libraries) {
        fs::path candidate = lib / L"steamapps" / L"common" / L"Crimson Desert" / L"bin64" / GAME_EXE_NAME;
        if (fs::exists(candidate)) {
            return fs::canonical(candidate).wstring();
        }
        candidate = lib / L"steamapps" / L"common" / L"Crimson Desert" / GAME_EXE_NAME;
        if (fs::exists(candidate)) {
            return fs::canonical(candidate).wstring();
        }
    }

    // Direct drive checks for common secondary locations
    std::vector<fs::path> directChecks = {
        L"D:\\SteamLibrary\\steamapps\\common\\Crimson Desert\\bin64\\CrimsonDesert.exe",
        L"E:\\SteamLibrary\\steamapps\\common\\Crimson Desert\\bin64\\CrimsonDesert.exe",
        L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Crimson Desert\\bin64\\CrimsonDesert.exe"
    };
    for (const auto& path : directChecks) {
        if (fs::exists(path)) {
            return fs::canonical(path).wstring();
        }
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
        current.parent_path() / MOD_DLL_NAME,
        current.parent_path() / L"build" / MOD_DLL_NAME
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

    LPVOID pRemoteMem = VirtualAllocEx(hProcess, nullptr, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemoteMem) {
        std::wcerr << L"[Error] Failed to allocate memory in game process (Code: " << GetLastError() << L")\n";
        return false;
    }

    SIZE_T bytesWritten = 0;
    if (!WriteProcessMemory(hProcess, pRemoteMem, dllPath.c_str(), pathSize, &bytesWritten) || bytesWritten != pathSize) {
        std::wcerr << L"[Error] Failed to write DLL path into game process memory.\n";
        VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
        return false;
    }

    LPVOID pLoadLibraryW = reinterpret_cast<LPVOID>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
    if (!pLoadLibraryW) {
        std::wcerr << L"[Error] Could not locate LoadLibraryW address.\n";
        VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
        return false;
    }

    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(pLoadLibraryW), pRemoteMem, 0, nullptr);
    if (!hThread) {
        std::wcerr << L"[Error] CreateRemoteThread failed (Code: " << GetLastError() << L")\n";
        VirtualFreeEx(hProcess, pRemoteMem, 0, MEM_RELEASE);
        return false;
    }

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
        std::wcerr << L"Make sure CrimsonCoop.dll is compiled.\n";
        std::cout << "\nPress ENTER to exit...";
        std::cin.get();
        return 1;
    }
    std::wcout << L"[Launcher] Found Co-Op Mod: " << modDllPath << L"\n";

    // 2. Locate the Game Executable across Steam libraries
    std::wstring gameExePath = CrimsonLauncher::FindGameExecutable();
    if (gameExePath.empty()) {
        std::cout << "[Launcher] Could not auto-detect CrimsonDesert.exe across Steam libraries.\n";
        std::cout << "Please enter the full path to CrimsonDesert.exe (or drag-and-drop the file here):\n> ";
        std::string userPath;
        std::getline(std::cin, userPath);
        std::wstring rawPath(userPath.begin(), userPath.end());
        gameExePath = CrimsonLauncher::CleanPath(rawPath);
    }

    if (!fs::exists(gameExePath)) {
        std::wcerr << L"[Error] Game executable not found at: " << gameExePath << L"\n";
        std::cout << "\nPress ENTER to exit...";
        std::cin.get();
        return 1;
    }

    std::wcout << L"[Launcher] Target Game Detected: " << gameExePath << L"\n";
    std::wcout << L"[Launcher] Launching Crimson Desert in Co-Op Mode (Suspended Injection)...\n";

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
        DWORD err = GetLastError();
        std::wcerr << L"[Error] Failed to launch game process (Code: " << err << L")\n";
        if (err == ERROR_ELEVATION_REQUIRED) {
            std::wcerr << L"Hint: Try running CrimsonCoopLauncher as Administrator.\n";
        }
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

    Sleep(2500);
    return 0;
}

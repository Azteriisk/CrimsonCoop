#include "SaveRedirector.hpp"
#include "Logger.hpp"
#include <MinHook.h>
#include <iostream>
#include <filesystem>
#include <algorithm>

namespace CrimsonCoop {

namespace fs = std::filesystem;

bool SaveRedirector::s_isInstalled = false;

// Original function pointer for CreateFileW
typedef HANDLE(WINAPI* tCreateFileW)(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile
);

static tCreateFileW oCreateFileW = nullptr;

bool SaveRedirector::IsSavePath(const std::wstring& path) {
    if (path.empty()) return false;

    // Convert to lowercase for case-insensitive matching
    std::wstring lowerPath = path;
    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::towlower);

    // Checks for typical save game folders and .sav extensions
    bool hasSaveDir = (lowerPath.find(L"savegames") != std::wstring::npos) ||
                      (lowerPath.find(L"saved\\save") != std::wstring::npos);
    bool hasSavExt = (lowerPath.length() >= 4 && lowerPath.substr(lowerPath.length() - 4) == L".sav");

    if (!hasSaveDir || !hasSavExt) return false;

    // Do NOT redirect settings, profiles, audio, or graphics configuration files!
    // Check only the filename so "C:\Users\..." in the directory path is not falsely matched
    std::wstring filename;
    try {
        fs::path p(path);
        filename = p.filename().wstring();
        std::transform(filename.begin(), filename.end(), filename.begin(), ::towlower);
    } catch (...) {
        return false;
    }

    if (filename.find(L"user") != std::wstring::npos ||
        filename.find(L"setting") != std::wstring::npos ||
        filename.find(L"profile") != std::wstring::npos ||
        filename.find(L"config") != std::wstring::npos ||
        filename.find(L"graphic") != std::wstring::npos ||
        filename.find(L"engine") != std::wstring::npos) {
        return false;
    }

    return true;
}

std::wstring SaveRedirector::TransformSavePath(const std::wstring& originalPath) {
    if (!IsSavePath(originalPath)) {
        return originalPath;
    }

    std::wstring transformed = originalPath;

    // Replace ".sav" extension with ".cdcoop"
    size_t extPos = transformed.rfind(L".sav");
    if (extPos != std::wstring::npos) {
        transformed.replace(extPos, 4, L".cdcoop");
    }

    // Replace "SaveGames" with "CoopSaveGames"
    size_t dirPos = transformed.find(L"SaveGames");
    if (dirPos != std::wstring::npos) {
        transformed.replace(dirPos, 9, L"CoopSaveGames");
    } else {
        dirPos = transformed.find(L"savegames");
        if (dirPos != std::wstring::npos) {
            transformed.replace(dirPos, 9, L"CoopSaveGames");
        }
    }

    return transformed;
}

// Hooked CreateFileW implementation
static HANDLE WINAPI Hook_CreateFileW(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile
) {
    if (lpFileName != nullptr) {
        std::wstring original(lpFileName);

        if (SaveRedirector::IsSavePath(original)) {
            std::wstring redirected = SaveRedirector::TransformSavePath(original);

            // Ensure the CoopSaveGames directory exists
            try {
                fs::path p(redirected);
                if (p.has_parent_path()) {
                    fs::create_directories(p.parent_path());
                }

                // If the co-op save doesn't exist yet, auto-seed it from the vanilla save
                // so the user's current campaign progress is seamlessly loaded into co-op!
                if (!fs::exists(p) && fs::exists(original)) {
                    fs::copy_file(original, p, fs::copy_options::skip_existing);
                }
            } catch (...) {
                // Ignore filesystem errors if directory already exists
            }

            return oCreateFileW(
                redirected.c_str(),
                dwDesiredAccess,
                dwShareMode,
                lpSecurityAttributes,
                dwCreationDisposition,
                dwFlagsAndAttributes,
                hTemplateFile
            );
        }
    }

    return oCreateFileW(
        lpFileName,
        dwDesiredAccess,
        dwShareMode,
        lpSecurityAttributes,
        dwCreationDisposition,
        dwFlagsAndAttributes,
        hTemplateFile
    );
}

bool SaveRedirector::Install() {
    if (s_isInstalled) return true;

    MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
        Logger::Log("[SaveRedirector] MH_Initialize failed: %s\n", MH_StatusToString(status));
        return false;
    }

    status = MH_CreateHookApi(
        L"kernel32.dll",
        "CreateFileW",
        reinterpret_cast<LPVOID>(&Hook_CreateFileW),
        reinterpret_cast<LPVOID*>(&oCreateFileW)
    );

    if (status != MH_OK) {
        Logger::Log("[SaveRedirector] MH_CreateHookApi CreateFileW failed: %s\n", MH_StatusToString(status));
        return false;
    }

    status = MH_EnableHook(MH_ALL_HOOKS);
    if (status != MH_OK) {
        Logger::Log("[SaveRedirector] MH_EnableHook failed: %s\n", MH_StatusToString(status));
        return false;
    }

    s_isInstalled = true;
    Logger::Log("[SaveRedirector] Hooked CreateFileW successfully.\n");
    return true;
}

void SaveRedirector::Remove() {
    if (!s_isInstalled) return;

    MH_DisableHook(MH_ALL_HOOKS);
    s_isInstalled = false;
    Logger::Log("[SaveRedirector] Hooks removed.\n");
}

} // namespace CrimsonCoop

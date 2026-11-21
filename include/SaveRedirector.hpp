#pragma once

#include <windows.h>
#include <string>

namespace CrimsonCoop {

class SaveRedirector {
public:
    // Installs MinHook on Windows file API (CreateFileW)
    static bool Install();

    // Removes hooks cleanly during DLL unload
    static void Remove();

    // Pure path transformation logic: converts .sav in SaveGames to .cdcoop in CoopSaveGames
    static std::wstring TransformSavePath(const std::wstring& originalPath);

    // Checks if a given path targets single-player save data
    static bool IsSavePath(const std::wstring& path);

    static bool IsInstalled() { return s_isInstalled; }

private:
    static bool s_isInstalled;
};

} // namespace CrimsonCoop

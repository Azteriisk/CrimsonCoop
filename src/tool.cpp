#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include "CoopSaveManager.hpp"
#include "Common.hpp"

namespace fs = std::filesystem;
using namespace CrimsonCoop;

void PrintBanner() {
    std::cout << "=========================================================\n";
    std::cout << "   Crimson Desert Co-Op Mod Diagnostic & MCP Inspector   \n";
    std::cout << "   Trio: Kliff, Damiane, Oongka                          \n";
    std::cout << "=========================================================\n\n";
}

DWORD GetGameProcessId() {
    DWORD pid = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe32{};
        pe32.dwSize = sizeof(pe32);
        if (Process32FirstW(hSnap, &pe32)) {
            do {
                if (_wcsicmp(pe32.szExeFile, L"CrimsonDesert.exe") == 0) {
                    pid = pe32.th32ProcessID;
                    break;
                }
            } while (Process32NextW(hSnap, &pe32));
        }
        CloseHandle(hSnap);
    }
    return pid;
}

void InspectProcess() {
    std::cout << "[1] Game Process Diagnostic:\n";
    DWORD pid = GetGameProcessId();
    if (pid != 0) {
        std::cout << "  -> CrimsonDesert.exe is RUNNING! (PID: " << pid << ")\n";
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProcess) {
            FILETIME createTime, exitTime, kernelTime, userTime;
            if (GetProcessTimes(hProcess, &createTime, &exitTime, &kernelTime, &userTime)) {
                std::cout << "  -> Process handle verified and healthy.\n";
            }
            CloseHandle(hProcess);
        }
    } else {
        std::cout << "  -> CrimsonDesert.exe is NOT currently running.\n";
    }
    std::cout << "\n";
}

void InspectLog() {
    std::cout << "[2] Diagnostic Log (CrimsonCoop.log):\n";
    std::vector<fs::path> logPaths = {
        "CrimsonCoop.log",
        "C:\\Users\\thera\\code\\CrimsonCoop\\CrimsonCoop.log",
        "D:\\SteamLibrary\\steamapps\\common\\Crimson Desert\\bin64\\CrimsonCoop.log"
    };

    bool found = false;
    for (const auto& p : logPaths) {
        if (fs::exists(p)) {
            std::cout << "  -> Found log at: " << p.string() << " (Size: " << fs::file_size(p) << " bytes)\n";
            std::ifstream f(p);
            std::string line;
            std::vector<std::string> lines;
            while (std::getline(f, line)) {
                lines.push_back(line);
            }
            size_t start = lines.size() > 8 ? lines.size() - 8 : 0;
            std::cout << "  --- Recent Log Entries ---\n";
            for (size_t i = start; i < lines.size(); ++i) {
                std::cout << "    " << lines[i] << "\n";
            }
            found = true;
            break;
        }
    }
    if (!found) {
        std::cout << "  -> No log file generated yet (Run launcher to generate initial logs).\n";
    }
    std::cout << "\n";
}

void InspectSaveFiles() {
    std::cout << "[3] Co-Op Save Container Sandboxing (.cdcoop):\n";
    std::string testPath = "CrimsonDesert_Coop_Slot0.cdcoop";
    if (fs::exists(testPath)) {
        CoopSaveData loaded{};
        std::string err;
        if (CoopSaveManager::ReadCoopSave(testPath, loaded, err)) {
            std::cout << "  -> Save Container: " << testPath << " (Valid: YES)\n";
            std::cout << "  -> Checksum: 0x" << std::hex << loaded.header.checksum << std::dec << "\n";
            std::cout << "  -> Synchronized Greymane Heroes:\n";
            for (const auto& c : loaded.characters) {
                std::cout << "     * " << CharacterToString(c.character)
                          << " - Level " << c.level << " (HP: " << c.health << "/" << c.maxHealth << ")\n";
            }
            std::cout << "  -> World Quest Flags: " << loaded.questFlags.size() << "\n";
            std::cout << "  -> Fast Travel Nodes: " << loaded.unlockedFastTravelPoints.size() << "\n";
        }
    } else {
        std::cout << "  -> No .cdcoop save in current directory.\n";
    }
    std::cout << "\n";
}

int main(int argc, char* argv[]) {
    PrintBanner();
    InspectProcess();
    InspectLog();
    InspectSaveFiles();
    std::cout << "Diagnostic inspection complete.\n";
    return 0;
}

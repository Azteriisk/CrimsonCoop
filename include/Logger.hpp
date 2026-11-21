#pragma once
#include <string>

namespace CrimsonCoop {

class Logger {
public:
    static void Init(const std::string& filename = "CrimsonCoop.log");
    static void Log(const char* fmt, ...);
    static void Shutdown();

private:
    static std::string s_logPath;
    static std::string s_secondaryPath;
    static bool s_isInitialized;
};

} // namespace CrimsonCoop

#pragma once

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>

namespace CrimsonCoop {

class Dx12Hook {
public:
    static bool Install();
    static void Remove();
    static bool IsInstalled() { return s_isInstalled; }

private:
    static bool s_isInstalled;
};

} // namespace CrimsonCoop

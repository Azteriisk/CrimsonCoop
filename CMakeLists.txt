cmake_minimum_required(VERSION 3.20)
project(CrimsonDesertCoop LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(UNICODE _UNICODE)

include(FetchContent)

# 1. MinHook
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG v1.3.3
)
FetchContent_GetProperties(minhook)
if(NOT minhook_POPULATED)
    FetchContent_Populate(minhook)
endif()

add_library(minhook STATIC
    ${minhook_SOURCE_DIR}/src/buffer.c
    ${minhook_SOURCE_DIR}/src/hook.c
    ${minhook_SOURCE_DIR}/src/trampoline.c
    ${minhook_SOURCE_DIR}/src/hde/hde64.c
    ${minhook_SOURCE_DIR}/src/hde/hde32.c
)
target_compile_options(minhook PRIVATE -include intrin.h)
target_include_directories(minhook PUBLIC
    ${minhook_SOURCE_DIR}/include
)

# 2. Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.9
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

include_directories(
    include
    ${minhook_SOURCE_DIR}/include
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

# Common library sources
set(CORE_SOURCES
    src/Logger.cpp
    src/Interpolation.cpp
    src/SessionManager.cpp
    src/RemoteProxyManager.cpp
    src/VisualEquipSync.cpp
    src/ActionCombatSync.cpp
    src/CoopSaveManager.cpp
    src/SaveRedirector.cpp
    src/GuiOverlay.cpp
    src/Dx12Hook.cpp
    src/PatternScan.cpp
    src/BlackSpaceChain.cpp
    src/Config.cpp
    src/RealSteamworks.cpp
    src/SteamTransport.cpp
)

# Executable Test Harness
add_executable(cd_coop_harness
    ${CORE_SOURCES}
    src/main.cpp
)
target_link_libraries(cd_coop_harness PRIVATE minhook imgui d3d12 dxgi)

# Injected Mod DLL (CrimsonCoop.dll)
add_library(CrimsonCoop SHARED
    ${CORE_SOURCES}
    src/dllmain.cpp
)
target_link_libraries(CrimsonCoop PRIVATE minhook imgui d3d12 dxgi)

# Dedicated Standalone Co-Op Launcher (Zero permanent game file modifications)
add_executable(CrimsonCoopLauncher
    src/launcher.cpp
)

# Diagnostic & MCP Inspector CLI Tool
add_executable(CrimsonCoopTool
    src/tool.cpp
    src/CoopSaveManager.cpp
)

if (MSVC)
    target_compile_options(cd_coop_harness PRIVATE /W4 /permissive-)
    target_compile_options(CrimsonCoop PRIVATE /W4 /permissive-)
    target_compile_options(CrimsonCoopLauncher PRIVATE /W4 /permissive-)
    target_compile_options(CrimsonCoopTool PRIVATE /W4 /permissive-)
else()
    target_compile_options(cd_coop_harness PRIVATE -Wall -Wextra -pedantic -Wno-unused-parameter)
    target_compile_options(CrimsonCoop PRIVATE -Wall -Wextra -pedantic -Wno-unused-parameter)
    target_compile_options(CrimsonCoopLauncher PRIVATE -Wall -Wextra -pedantic -Wno-unused-parameter)
    target_compile_options(CrimsonCoopTool PRIVATE -Wall -Wextra -pedantic -Wno-unused-parameter)
endif()

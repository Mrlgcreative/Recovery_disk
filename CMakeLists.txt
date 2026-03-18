cmake_minimum_required(VERSION 3.20)
project(hdd_unlock VERSION 1.0.0 LANGUAGES C CXX)

# ── Standard C++20 ──────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)   # -std=c++20 strict (pas gnu++20)
set(CMAKE_C_STANDARD            11)

# ── Dependances via FetchContent ───────────────────────────────────────────
include(FetchContent)

# Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

# spdlog (F12 — Logger)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.15.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(spdlog)

# Catch2 (F18 — Tests unitaires)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

# ── Detection OS → sources specifiques ─────────────────────────────────────
if(WIN32)
    message(STATUS "[hdd_unlock] OS : Windows — DeviceIoControl / IOCTL")
    set(OS_SOURCES src/os/win_ata.cpp)
    set(OS_LIBS    ntdll setupapi cfgmgr32)
elseif(UNIX AND NOT APPLE)
    message(STATUS "[hdd_unlock] OS : Linux — ioctl SG_IO")
    set(OS_SOURCES src/os/linux_ata.cpp)
    set(OS_LIBS    "")
else()
    message(FATAL_ERROR "[hdd_unlock] OS non supporte (Windows ou Linux uniquement).")
endif()

# ── Sources communes ────────────────────────────────────────────────────────
set(COMMON_SOURCES
    src/core/ata_interface.cpp
    src/core/ata_utils.cpp
    src/core/device.cpp
    src/core/vsc_engine.cpp
    src/core/ram_patcher.cpp
    src/vendors/wd_handler.cpp
    src/vendors/seagate_handler.cpp
    src/vendors/toshiba_handler.cpp
    src/vendors/hgst_handler.cpp
    src/sa/sa_parser.cpp
    src/sa/sa_backup.cpp
    src/ui/cli.cpp
    src/ui/logger.cpp
    src/ui/report.cpp
)

# ── Librairie statique hdd_core ─────────────────────────────────────────────
add_library(hdd_core STATIC
    ${COMMON_SOURCES}
    ${OS_SOURCES}
)
target_include_directories(hdd_core PUBLIC include)
target_link_libraries(hdd_core PUBLIC ${OS_LIBS} spdlog::spdlog)

# ── Warnings maximaux ────────────────────────────────────────────────────────
if(MSVC)
    target_compile_options(hdd_core PRIVATE /W4 /WX /wd4200)
    target_compile_definitions(hdd_core PRIVATE UNICODE _UNICODE)
else()
    target_compile_options(hdd_core PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
    )
endif()

# ── Executable principal CLI ─────────────────────────────────────────────────
add_executable(hdd_unlock src/main.cpp)
target_link_libraries(hdd_unlock PRIVATE hdd_core)

# ── Test legacy F01 (retrocompatibilite) ─────────────────────────────────────
add_executable(test_f01 tests/test_f01_compile.cpp)
target_link_libraries(test_f01 PRIVATE hdd_core)

# ── Tests Catch2 (F18) ──────────────────────────────────────────────────────
add_executable(test_all
    tests/test_main.cpp
    tests/test_identify.cpp
    tests/test_vendor_detect.cpp
    tests/test_sa_parser.cpp
    tests/test_config.cpp
    tests/test_ram_patcher.cpp
)
target_link_libraries(test_all PRIVATE hdd_core Catch2::Catch2WithMain)

# ── Interface graphique (Dear ImGui + DirectX11) ─────────────────────────────
if(WIN32)
    set(IMGUI_SOURCES
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
    )

    add_executable(hdd_unlock_gui WIN32
        src/main_gui.cpp
        src/ui/gui.cpp
        ${IMGUI_SOURCES}
        resources/app.rc
    )
    target_include_directories(hdd_unlock_gui PRIVATE
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(hdd_unlock_gui PRIVATE
        hdd_core
        d3d11 dxgi d3dcompiler dwmapi
        windowscodecs ole32
    )

    if(MSVC)
        target_compile_options(hdd_unlock_gui PRIVATE /W4)
    else()
        target_compile_options(hdd_unlock_gui PRIVATE
            -Wall -Wextra -Wpedantic
            -Wno-unused-parameter
        )
    endif()

    message(STATUS "[hdd_unlock] GUI : Dear ImGui + DirectX11 (Windows)")
else()
    message(STATUS "[hdd_unlock] GUI : non disponible (Windows uniquement)")
endif()

# ── Message de fin ───────────────────────────────────────────────────────────
message(STATUS "")
message(STATUS "================================================")
message(STATUS "  HDD Password Recovery Tool v${PROJECT_VERSION}")
message(STATUS "  Compiler : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Standard : C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Features : F01-F18 (CLI + GUI + VSC + Logger)")
if(WIN32)
    message(STATUS "  OS target: Windows (DeviceIoControl)")
else()
    message(STATUS "  OS target: Linux (ioctl SG_IO)")
endif()
message(STATUS "================================================")
message(STATUS "")
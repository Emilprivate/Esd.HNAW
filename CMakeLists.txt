cmake_minimum_required(VERSION 3.15)
project(HoldfastMenu VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Resolve MinHook (local first, fallback to fetch)
set(MINHOOK_DIR "${CMAKE_SOURCE_DIR}/ext/minhook")
if (EXISTS "${MINHOOK_DIR}/CMakeLists.txt")
    add_subdirectory("${MINHOOK_DIR}")
else()
    message(STATUS "MinHook not found in ext/minhook, fetching from GitHub...")
    FetchContent_Declare(
        minhook
        GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
        GIT_TAG master
    )
    FetchContent_MakeAvailable(minhook)
endif()

# Resolve Dear ImGui (local first, fallback to fetch)
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/ext/imgui")
if (NOT EXISTS "${IMGUI_DIR}/imgui.cpp")
    message(STATUS "Dear ImGui not found in ext/imgui, fetching from GitHub...")
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG master
    )
    FetchContent_MakeAvailable(imgui)
    set(IMGUI_DIR "${imgui_SOURCE_DIR}")
endif()

file(GLOB IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)

add_library(${PROJECT_NAME} SHARED
    src/core/main.cpp
    src/core/hooking/hook.cpp
    src/core/mono/mono_resolver.cpp
    src/features/aimbot/aimbot.cpp
    src/features/aimbot/weapon_type_names.cpp
    src/features/experiments/experiments.cpp
    src/features/esp/esp.cpp
    src/features/esp/esp_mono.cpp
    src/features/esp/esp_chams.cpp
    src/features/esp/esp_render.cpp
    src/features/esp/esp_settings.cpp
    src/ui/gui.cpp
    src/ui/gui_theme.cpp
    src/config/config.cpp
    ${IMGUI_SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ext/minhook/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    minhook
    d3d11
    dxgi
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    SUFFIX ".dll"
    OUTPUT_NAME "esd.hnaw"
)

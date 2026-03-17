#include <thread>
#include <windows.h>

#include "config/config.h"
#include "core/hooking/hook.h"
#include "core/mono/mono_resolver.h"
#include "ui/gui.h"

DWORD WINAPI MainThread(LPVOID lpReserved) {
    GUI::bShouldUnload = false;
    GUI::bUnloadRequested = false;
    GUI::bMenuOpen = false;

    AppConfig::LoadConfigFromDisk();
    MonoResolver::ResolveAll();

    if (!Hook::Init()) {
        FreeLibraryAndExitThread((HMODULE)lpReserved, 0);
        return 0;
    }

    while (!GUI::bUnloadRequested &&
        !GetAsyncKeyState(AppConfig::unloadPrimaryKey) &&
        !GetAsyncKeyState(AppConfig::unloadSecondaryKey)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    GUI::bUnloadRequested = false;
    GUI::bMenuOpen = false;
    GUI::bShouldUnload = true;

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    Hook::Remove();

    FreeLibraryAndExitThread((HMODULE)lpReserved, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr);
            break;
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}

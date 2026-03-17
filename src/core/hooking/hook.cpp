#include "core/hooking/hook.h"

#include "core/hnaw_offsets.h"
#include "features/aimbot/aimbot.h"
#include "ui/gui.h"

#include <algorithm>
#include <cmath>
#include <MinHook.h>
#include <d3d11.h>
#include <dxgi.h>
#include <windows.h>

typedef HRESULT(__stdcall* tPresent)(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags);
typedef UINT(WINAPI* tGetRawInputData)(HRAWINPUT hRawInput, UINT uiCommand, LPVOID pData, PUINT pcbSize, UINT cbSizeHeader);
typedef UINT(WINAPI* tGetRawInputBuffer)(PRAWINPUT pData, PUINT pcbSize, UINT cbSizeHeader);
typedef float(__fastcall* tGetReloadDuration)(void* self, void* arg1, void* arg2);
typedef void(__fastcall* tOwnerProcessFirearmWeaponInput)(void* self, void* arg1, void* arg2);
typedef void(__fastcall* tOwnerShootActiveFirearm)(void* self, bool isDryShot);
typedef bool(__fastcall* tWeaponHolderGetCanShootFirearm)(void* self, void* arg1);
typedef bool(__fastcall* tPlayerAnimationGetCanShootFirearm)(void* self, void* arg1);
typedef void(__fastcall* tOwnerFinishedReloadState)(void* self, void* sender, void* eventArgs);

namespace {
    tPresent oPresent = nullptr;
    tGetRawInputData oGetRawInputData = nullptr;
    tGetRawInputBuffer oGetRawInputBuffer = nullptr;
    tGetReloadDuration oGetReloadDuration = nullptr;
    tOwnerProcessFirearmWeaponInput oOwnerProcessFirearmWeaponInput = nullptr;
    tOwnerShootActiveFirearm oOwnerShootActiveFirearm = nullptr;
    tWeaponHolderGetCanShootFirearm oWeaponHolderGetCanShootFirearm = nullptr;
    tPlayerAnimationGetCanShootFirearm oPlayerAnimationGetCanShootFirearm = nullptr;

    HWND gGameWindow = nullptr;
    WNDPROC gOriginalWndProc = nullptr;
    void* gPresentAddress = nullptr;
    void* gGetRawInputDataAddress = nullptr;
    void* gGetRawInputBufferAddress = nullptr;
    void* gGetReloadDurationAddress = nullptr;
    void* gOwnerProcessFirearmWeaponInputAddress = nullptr;
    void* gOwnerShootActiveFirearmAddress = nullptr;
    void* gWeaponHolderGetCanShootFirearmAddress = nullptr;
    void* gPlayerAnimationGetCanShootFirearmAddress = nullptr;
    tOwnerFinishedReloadState gOwnerFinishedReloadStateMethod = nullptr;

    float __fastcall hkGetReloadDuration(void* self, void* arg1, void* arg2) {
        if (!oGetReloadDuration) {
            return 0.0f;
        }

        const float originalDuration = oGetReloadDuration(self, arg1, arg2);
        if (!Aimbot::ReloadSpeedEnabled()) {
            return originalDuration;
        }
        constexpr float kMinReloadDurationSeconds = 0.02f;
        return kMinReloadDurationSeconds;
    }

    void __fastcall hkOwnerProcessFirearmWeaponInput(void* self, void* arg1, void* arg2) {
        if (oOwnerProcessFirearmWeaponInput) {
            oOwnerProcessFirearmWeaponInput(self, arg1, arg2);
        }

        if (self && Aimbot::ReloadSpeedEnabled() && gOwnerFinishedReloadStateMethod) {
            gOwnerFinishedReloadStateMethod(self, nullptr, nullptr);
        }
    }

    void __fastcall hkOwnerShootActiveFirearm(void* self, bool isDryShot) {
        if (oOwnerShootActiveFirearm) {
            oOwnerShootActiveFirearm(self, isDryShot);
        }

        if (!self || isDryShot || !Aimbot::FireRateEnabled()) {
            return;
        }

        if (HnawOffsets::weaponHolderLastFiredTime) {
            const uintptr_t lastFiredAddr = reinterpret_cast<uintptr_t>(self) + HnawOffsets::weaponHolderLastFiredTime;
            __try {
                *reinterpret_cast<double*>(lastFiredAddr) = 0.0;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
        }

        if (HnawOffsets::ownerWeaponHolderQueuedFireFirearm) {
            const uintptr_t queuedAddr = reinterpret_cast<uintptr_t>(self) + HnawOffsets::ownerWeaponHolderQueuedFireFirearm;
            __try {
                *reinterpret_cast<bool*>(queuedAddr) = false;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
        }

        if (gOwnerFinishedReloadStateMethod) {
            gOwnerFinishedReloadStateMethod(self, nullptr, nullptr);
        }
    }

    bool __fastcall hkWeaponHolderGetCanShootFirearm(void* self, void* arg1) {
        if (!oWeaponHolderGetCanShootFirearm) {
            return false;
        }

        if (self && Aimbot::FireRateEnabled() && HnawOffsets::weaponHolderLastFiredTime) {
            const uintptr_t lastFiredAddr = reinterpret_cast<uintptr_t>(self) + HnawOffsets::weaponHolderLastFiredTime;
            __try {
                *reinterpret_cast<double*>(lastFiredAddr) = 0.0;
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }

            return true;
        }

        return oWeaponHolderGetCanShootFirearm(self, arg1);
    }

    bool __fastcall hkPlayerAnimationGetCanShootFirearm(void* self, void* arg1) {
        if (!oPlayerAnimationGetCanShootFirearm) {
            return false;
        }

        if (Aimbot::FireRateEnabled()) {
            return true;
        }

        return oPlayerAnimationGetCanShootFirearm(self, arg1);
    }

    LRESULT CALLBACK DummyWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }

    LRESULT CALLBACK HookedWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (GUI::HandleWndProc(hWnd, msg, wParam, lParam)) {
            return 1;
        }

        return CallWindowProc(gOriginalWndProc, hWnd, msg, wParam, lParam);
    }

    void EnsureWndProcHook() {
        if (!gGameWindow) {
            return;
        }

        const WNDPROC currentWndProc = reinterpret_cast<WNDPROC>(GetWindowLongPtr(gGameWindow, GWLP_WNDPROC));
        if (currentWndProc == HookedWndProc) {
            return;
        }

        gOriginalWndProc = currentWndProc;
        SetWindowLongPtr(gGameWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(HookedWndProc));
    }

    bool ResolvePresentAddress() {
        if (gPresentAddress) {
            return true;
        }

        WNDCLASSEXA wc{};
        wc.cbSize = sizeof(WNDCLASSEXA);
        wc.lpfnWndProc = DummyWndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = "EsdHnawDummyWindow";

        if (!RegisterClassExA(&wc)) {
            return false;
        }

        HWND dummyWindow = CreateWindowExA(
            0,
            wc.lpszClassName,
            "EsdHnawDummyWindow",
            WS_OVERLAPPEDWINDOW,
            0,
            0,
            100,
            100,
            nullptr,
            nullptr,
            wc.hInstance,
            nullptr);

        if (!dummyWindow) {
            UnregisterClassA(wc.lpszClassName, wc.hInstance);
            return false;
        }

        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount = 1;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = dummyWindow;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        IDXGISwapChain* swapChain = nullptr;
        ID3D11Device* device = nullptr;
        ID3D11DeviceContext* context = nullptr;
        D3D_FEATURE_LEVEL featureLevel{};

        const HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            0,
            nullptr,
            0,
            D3D11_SDK_VERSION,
            &sd,
            &swapChain,
            &device,
            &featureLevel,
            &context);

        if (SUCCEEDED(hr) && swapChain) {
            void** vtable = *reinterpret_cast<void***>(swapChain);
            gPresentAddress = vtable[8];
        }

        if (context) {
            context->Release();
        }
        if (device) {
            device->Release();
        }
        if (swapChain) {
            swapChain->Release();
        }

        DestroyWindow(dummyWindow);
        UnregisterClassA(wc.lpszClassName, wc.hInstance);

        return gPresentAddress != nullptr;
    }

    HRESULT __stdcall hkPresent(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags) {
        if (GUI::bShouldUnload) {
            GUI::bUnloadRequested = true;
            return oPresent(swapChain, syncInterval, flags);
        }

        if (!GUI::bInitialized && swapChain) {
            ID3D11Device* device = nullptr;
            if (SUCCEEDED(swapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&device))) && device) {
                ID3D11DeviceContext* context = nullptr;
                device->GetImmediateContext(&context);

                DXGI_SWAP_CHAIN_DESC sd{};
                if (SUCCEEDED(swapChain->GetDesc(&sd))) {
                    gGameWindow = sd.OutputWindow;
                }

                EnsureWndProcHook();

                if (gGameWindow && context) {
                    GUI::Init(gGameWindow, swapChain, device, context);
                }

                if (context) {
                    context->Release();
                }
                device->Release();
            }
        }

        EnsureWndProcHook();

        GUI::Render();
        return oPresent(swapChain, syncInterval, flags);
    }

    UINT WINAPI hkGetRawInputData(HRAWINPUT hRawInput, UINT uiCommand, LPVOID pData, PUINT pcbSize, UINT cbSizeHeader) {
        const UINT result = oGetRawInputData(hRawInput, uiCommand, pData, pcbSize, cbSizeHeader);

        if (!GUI::bMenuOpen || result == static_cast<UINT>(-1) || !pData || !pcbSize) {
            return result;
        }

        if (uiCommand != RID_INPUT || *pcbSize < sizeof(RAWINPUT)) {
            return result;
        }

        RAWINPUT* rawInput = reinterpret_cast<RAWINPUT*>(pData);
        if (rawInput->header.dwType != RIM_TYPEMOUSE) {
            return result;
        }

        rawInput->data.mouse.usFlags = 0;
        rawInput->data.mouse.ulButtons = 0;
        rawInput->data.mouse.usButtonFlags = 0;
        rawInput->data.mouse.usButtonData = 0;
        rawInput->data.mouse.ulRawButtons = 0;
        rawInput->data.mouse.lLastX = 0;
        rawInput->data.mouse.lLastY = 0;

        return result;
    }

    UINT WINAPI hkGetRawInputBuffer(PRAWINPUT pData, PUINT pcbSize, UINT cbSizeHeader) {
        const UINT result = oGetRawInputBuffer(pData, pcbSize, cbSizeHeader);

        if (!GUI::bMenuOpen || result == static_cast<UINT>(-1) || !pData || !pcbSize || result == 0) {
            return result;
        }

        PRAWINPUT current = pData;
        for (UINT index = 0; index < result; ++index) {
            if (current->header.dwType == RIM_TYPEMOUSE) {
                current->data.mouse.usFlags = 0;
                current->data.mouse.ulButtons = 0;
                current->data.mouse.usButtonFlags = 0;
                current->data.mouse.usButtonData = 0;
                current->data.mouse.ulRawButtons = 0;
                current->data.mouse.lLastX = 0;
                current->data.mouse.lLastY = 0;
            }

            current = reinterpret_cast<PRAWINPUT>(reinterpret_cast<BYTE*>(current) + current->header.dwSize);
        }

        return result;
    }
}

bool Hook::Init() {
    if (MH_Initialize() != MH_OK) {
        return false;
    }

    if (!ResolvePresentAddress()) {
        MH_Uninitialize();
        return false;
    }

    HMODULE user32Module = GetModuleHandleA("user32.dll");
    if (user32Module) {
        gGetRawInputDataAddress = reinterpret_cast<void*>(GetProcAddress(user32Module, "GetRawInputData"));
        gGetRawInputBufferAddress = reinterpret_cast<void*>(GetProcAddress(user32Module, "GetRawInputBuffer"));
    }
    if (!gGetRawInputDataAddress || !gGetRawInputBufferAddress) {
        MH_Uninitialize();
        return false;
    }

    if (MH_CreateHook(gPresentAddress, &hkPresent, reinterpret_cast<void**>(&oPresent)) != MH_OK) {
        MH_Uninitialize();
        return false;
    }

    if (MH_EnableHook(gPresentAddress) != MH_OK) {
        MH_RemoveHook(gPresentAddress);
        MH_Uninitialize();
        return false;
    }

    if (MH_CreateHook(gGetRawInputDataAddress, &hkGetRawInputData, reinterpret_cast<void**>(&oGetRawInputData)) != MH_OK) {
        MH_DisableHook(gPresentAddress);
        MH_RemoveHook(gPresentAddress);
        MH_Uninitialize();
        return false;
    }

    if (MH_EnableHook(gGetRawInputDataAddress) != MH_OK) {
        MH_RemoveHook(gGetRawInputDataAddress);
        MH_DisableHook(gPresentAddress);
        MH_RemoveHook(gPresentAddress);
        MH_Uninitialize();
        return false;
    }

    if (MH_CreateHook(gGetRawInputBufferAddress, &hkGetRawInputBuffer, reinterpret_cast<void**>(&oGetRawInputBuffer)) != MH_OK) {
        MH_DisableHook(gGetRawInputDataAddress);
        MH_RemoveHook(gGetRawInputDataAddress);
        MH_DisableHook(gPresentAddress);
        MH_RemoveHook(gPresentAddress);
        MH_Uninitialize();
        return false;
    }

    if (MH_EnableHook(gGetRawInputBufferAddress) != MH_OK) {
        MH_RemoveHook(gGetRawInputBufferAddress);
        MH_DisableHook(gGetRawInputDataAddress);
        MH_RemoveHook(gGetRawInputDataAddress);
        MH_DisableHook(gPresentAddress);
        MH_RemoveHook(gPresentAddress);
        MH_Uninitialize();
        return false;
    }

    if (HnawOffsets::hookClientWeaponHolderGetReloadDuration) {
        gGetReloadDurationAddress = reinterpret_cast<void*>(HnawOffsets::hookClientWeaponHolderGetReloadDuration);
        if (MH_CreateHook(gGetReloadDurationAddress, &hkGetReloadDuration, reinterpret_cast<void**>(&oGetReloadDuration)) == MH_OK) {
            if (MH_EnableHook(gGetReloadDurationAddress) != MH_OK) {
                MH_RemoveHook(gGetReloadDurationAddress);
                gGetReloadDurationAddress = nullptr;
                oGetReloadDuration = nullptr;
            }
        } else {
            gGetReloadDurationAddress = nullptr;
            oGetReloadDuration = nullptr;
        }
    }

    if (HnawOffsets::methodOwnerWeaponHolderFinishedPlayerAnimationStateSMBOnFinishedState) {
        gOwnerFinishedReloadStateMethod = reinterpret_cast<tOwnerFinishedReloadState>(HnawOffsets::methodOwnerWeaponHolderFinishedPlayerAnimationStateSMBOnFinishedState);
    }

    if (HnawOffsets::hookOwnerWeaponHolderProcessFirearmWeaponInput) {
        gOwnerProcessFirearmWeaponInputAddress = reinterpret_cast<void*>(HnawOffsets::hookOwnerWeaponHolderProcessFirearmWeaponInput);
        if (MH_CreateHook(gOwnerProcessFirearmWeaponInputAddress, &hkOwnerProcessFirearmWeaponInput, reinterpret_cast<void**>(&oOwnerProcessFirearmWeaponInput)) == MH_OK) {
            if (MH_EnableHook(gOwnerProcessFirearmWeaponInputAddress) != MH_OK) {
                MH_RemoveHook(gOwnerProcessFirearmWeaponInputAddress);
                gOwnerProcessFirearmWeaponInputAddress = nullptr;
                oOwnerProcessFirearmWeaponInput = nullptr;
            }
        } else {
            gOwnerProcessFirearmWeaponInputAddress = nullptr;
            oOwnerProcessFirearmWeaponInput = nullptr;
        }
    }

    if (HnawOffsets::hookOwnerWeaponHolderShootActiveFirearm) {
        gOwnerShootActiveFirearmAddress = reinterpret_cast<void*>(HnawOffsets::hookOwnerWeaponHolderShootActiveFirearm);
        if (MH_CreateHook(gOwnerShootActiveFirearmAddress, &hkOwnerShootActiveFirearm, reinterpret_cast<void**>(&oOwnerShootActiveFirearm)) == MH_OK) {
            if (MH_EnableHook(gOwnerShootActiveFirearmAddress) != MH_OK) {
                MH_RemoveHook(gOwnerShootActiveFirearmAddress);
                gOwnerShootActiveFirearmAddress = nullptr;
                oOwnerShootActiveFirearm = nullptr;
            }
        } else {
            gOwnerShootActiveFirearmAddress = nullptr;
            oOwnerShootActiveFirearm = nullptr;
        }
    }

    if (HnawOffsets::hookWeaponHolderGetCanShootFirearm) {
        gWeaponHolderGetCanShootFirearmAddress = reinterpret_cast<void*>(HnawOffsets::hookWeaponHolderGetCanShootFirearm);
        if (MH_CreateHook(gWeaponHolderGetCanShootFirearmAddress, &hkWeaponHolderGetCanShootFirearm, reinterpret_cast<void**>(&oWeaponHolderGetCanShootFirearm)) == MH_OK) {
            if (MH_EnableHook(gWeaponHolderGetCanShootFirearmAddress) != MH_OK) {
                MH_RemoveHook(gWeaponHolderGetCanShootFirearmAddress);
                gWeaponHolderGetCanShootFirearmAddress = nullptr;
                oWeaponHolderGetCanShootFirearm = nullptr;
            }
        } else {
            gWeaponHolderGetCanShootFirearmAddress = nullptr;
            oWeaponHolderGetCanShootFirearm = nullptr;
        }
    }

    if (HnawOffsets::hookPlayerAnimationHandlerGetCanShootFirearm) {
        gPlayerAnimationGetCanShootFirearmAddress = reinterpret_cast<void*>(HnawOffsets::hookPlayerAnimationHandlerGetCanShootFirearm);
        if (MH_CreateHook(gPlayerAnimationGetCanShootFirearmAddress, &hkPlayerAnimationGetCanShootFirearm, reinterpret_cast<void**>(&oPlayerAnimationGetCanShootFirearm)) == MH_OK) {
            if (MH_EnableHook(gPlayerAnimationGetCanShootFirearmAddress) != MH_OK) {
                MH_RemoveHook(gPlayerAnimationGetCanShootFirearmAddress);
                gPlayerAnimationGetCanShootFirearmAddress = nullptr;
                oPlayerAnimationGetCanShootFirearm = nullptr;
            }
        } else {
            gPlayerAnimationGetCanShootFirearmAddress = nullptr;
            oPlayerAnimationGetCanShootFirearm = nullptr;
        }
    }

    return true;
}

void Hook::Remove() {
    if (gGameWindow && gOriginalWndProc && reinterpret_cast<WNDPROC>(GetWindowLongPtr(gGameWindow, GWLP_WNDPROC)) == HookedWndProc) {
        SetWindowLongPtr(gGameWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(gOriginalWndProc));
        gOriginalWndProc = nullptr;
    }

    if (gPresentAddress) {
        MH_DisableHook(gPresentAddress);
        MH_RemoveHook(gPresentAddress);
        gPresentAddress = nullptr;
    }

    if (gGetRawInputDataAddress) {
        MH_DisableHook(gGetRawInputDataAddress);
        MH_RemoveHook(gGetRawInputDataAddress);
        gGetRawInputDataAddress = nullptr;
    }

    if (gGetRawInputBufferAddress) {
        MH_DisableHook(gGetRawInputBufferAddress);
        MH_RemoveHook(gGetRawInputBufferAddress);
        gGetRawInputBufferAddress = nullptr;
    }

    if (gGetReloadDurationAddress) {
        MH_DisableHook(gGetReloadDurationAddress);
        MH_RemoveHook(gGetReloadDurationAddress);
        gGetReloadDurationAddress = nullptr;
        oGetReloadDuration = nullptr;
    }

    if (gOwnerProcessFirearmWeaponInputAddress) {
        MH_DisableHook(gOwnerProcessFirearmWeaponInputAddress);
        MH_RemoveHook(gOwnerProcessFirearmWeaponInputAddress);
        gOwnerProcessFirearmWeaponInputAddress = nullptr;
        oOwnerProcessFirearmWeaponInput = nullptr;
    }

    if (gOwnerShootActiveFirearmAddress) {
        MH_DisableHook(gOwnerShootActiveFirearmAddress);
        MH_RemoveHook(gOwnerShootActiveFirearmAddress);
        gOwnerShootActiveFirearmAddress = nullptr;
        oOwnerShootActiveFirearm = nullptr;
    }

    if (gWeaponHolderGetCanShootFirearmAddress) {
        MH_DisableHook(gWeaponHolderGetCanShootFirearmAddress);
        MH_RemoveHook(gWeaponHolderGetCanShootFirearmAddress);
        gWeaponHolderGetCanShootFirearmAddress = nullptr;
        oWeaponHolderGetCanShootFirearm = nullptr;
    }

    if (gPlayerAnimationGetCanShootFirearmAddress) {
        MH_DisableHook(gPlayerAnimationGetCanShootFirearmAddress);
        MH_RemoveHook(gPlayerAnimationGetCanShootFirearmAddress);
        gPlayerAnimationGetCanShootFirearmAddress = nullptr;
        oPlayerAnimationGetCanShootFirearm = nullptr;
    }
    gOwnerFinishedReloadStateMethod = nullptr;

    GUI::Shutdown();
    MH_Uninitialize();
}

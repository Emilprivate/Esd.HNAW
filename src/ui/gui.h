#pragma once

#include <d3d11.h>
#include <dxgi.h>
#include <windows.h>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

namespace GUI {
    inline bool bInitialized = false;
    inline bool bMenuOpen = false;
    inline bool bShouldUnload = false;
    inline bool bUnloadRequested = false;

    void Init(HWND window, IDXGISwapChain* swapChain, ID3D11Device* device, ID3D11DeviceContext* context);
    void Render();
    void Shutdown();
    bool HandleWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
}

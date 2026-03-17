## Esd.HNAW (Scaffold)

Minimal x64 DLL scaffold for Unity DX11 games to verify an ImGui menu hook pipeline.

### Included

- MinHook-based `IDXGISwapChain::Present` hook
- Dear ImGui init/render via Win32 + DX11 backends
- Insert key menu toggle
- Delete/End unload keys
- Runtime Mono auto-resolver (Assembly-CSharp field offsets + method pointers)
- CMake + `build.bat`

### Build

```bat
build.bat
```

Or explicitly:

```bat
build.bat Release
```

Output DLL path:

`build/bin/Release/esd.hnaw.dll`

### Notes

- This is just the UI/bootstrap layer (no game feature logic yet).
- Resolver status and sample values are shown in the menu under **Mono Auto-Resolver**.
- Intended next step is adding game-specific modules under `src/features/` and consuming resolved offsets from `src/core/hnaw_offsets.h`.

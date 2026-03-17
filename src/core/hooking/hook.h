#pragma once

namespace Hook {
    bool Init();
    void Remove();
    bool IsNoRecoilHookActive();
    bool IsNoSpreadHookActive();
    bool IsAutoReloadReady();
    bool IsArtilleryAimingContextActive();
}

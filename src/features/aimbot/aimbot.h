#pragma once

namespace Aimbot {
    bool& Enabled();
    bool& RequireKey();
    int& AimKey();
    float& FovPixels();
    float& Smooth();
    int& TargetBone();
    int& TeamFilterMode();
    bool& DrawFovCircle();
    float* FovColorRgb();
    bool& DropCompensationEnabled();
    bool& UseWeaponBallistics();
    float& DefaultMuzzleVelocity();
    float& DefaultGravity();
    bool& VisibilityCheckEnabled();
    bool& ReloadSpeedEnabled();
    float& ReloadSpeedMultiplier();
    bool& FireRateEnabled();
    float& FireRateMultiplier();
    bool& NoRecoilEnabled();
    bool& NoSpreadEnabled();
    int LastWeaponType();
    const char* LastWeaponTypeName();
    float LastResolvedVelocity();
    float LastResolvedGravity();
    bool LastUsedWeaponBallistics();
    void Run(bool menuOpen);
    const char* LastStatus();
}

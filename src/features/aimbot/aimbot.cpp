#include "features/aimbot/aimbot.h"
#include "features/aimbot/weapon_type_names.h"

#include "core/hnaw_offsets.h"
#include "features/esp/esp_internal.h"

#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <unordered_map>
#include <windows.h>

using namespace EspInternal;

namespace {
    struct MotionSample {
        Vec3 position{};
        uint64_t timeMs = 0;
        bool valid = false;
    };

    std::unordered_map<int, MotionSample> gTargetMotionSamples;

    bool IsValidVec3(const Vec3& value) {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }

    bool IsZeroVec3(const Vec3& value) {
        return std::fabs(value.x) < 0.001f && std::fabs(value.y) < 0.001f && std::fabs(value.z) < 0.001f;
    }

    bool TryGetSpawnData(void* roundPlayer, void*& outPlayerBase, void*& outSpawnData) {
        outPlayerBase = nullptr;
        outSpawnData = nullptr;

        if (!roundPlayer || !HnawOffsets::roundPlayerPlayerBase) {
            return false;
        }

        if (!SafeRead(reinterpret_cast<uintptr_t>(roundPlayer) + HnawOffsets::roundPlayerPlayerBase, outPlayerBase) || !outPlayerBase) {
            outPlayerBase = nullptr;
            return false;
        }

        if (HnawOffsets::playerBasePlayerStartData) {
            SafeRead(reinterpret_cast<uintptr_t>(outPlayerBase) + HnawOffsets::playerBasePlayerStartData, outSpawnData);
        }

        return outPlayerBase != nullptr;
    }

    void ResolveTeamData(void* spawnData, int& outFaction, int& outSquad) {
        outFaction = -1;
        outSquad = -1;

        if (!spawnData) {
            return;
        }

        if (HnawOffsets::playerSpawnDataFaction) {
            int faction = 0;
            if (SafeRead(reinterpret_cast<uintptr_t>(spawnData) + HnawOffsets::playerSpawnDataFaction, faction)) {
                outFaction = faction;
            }
        }

        if (HnawOffsets::playerSpawnDataSquadID) {
            int squad = 0;
            if (SafeRead(reinterpret_cast<uintptr_t>(spawnData) + HnawOffsets::playerSpawnDataSquadID, squad)) {
                outSquad = squad;
            }
        }
    }

    bool TrySelectTargetWorldPos(void* roundPlayer, void* playerBase, void* spawnData, const Vec3& feet, int targetBone, Vec3& outTarget) {
        std::vector<Vec3> bones;
        if (TryGetBonePositions(roundPlayer, playerBase, spawnData, bones) && !bones.empty()) {
            int boneIndex = 5;
            switch (targetBone) {
                case 1:
                    boneIndex = 3;
                    break;
                case 2:
                    boneIndex = 0;
                    break;
                default:
                    boneIndex = 5;
                    break;
            }

            if (boneIndex >= 0 && boneIndex < static_cast<int>(bones.size())) {
                const Vec3 bone = bones[static_cast<size_t>(boneIndex)];
                if (IsValidVec3(bone) && !IsZeroVec3(bone)) {
                    outTarget = bone;
                    return true;
                }
            }
        }

        outTarget = feet;
        switch (targetBone) {
            case 1:
                outTarget.y += 1.25f;
                break;
            case 2:
                outTarget.y += 0.90f;
                break;
            default:
                outTarget.y += 1.78f;
                break;
        }
        return true;
    }

    void ApplyMouseDelta(float deltaX, float deltaY) {
        const float clampLimit = 120.0f;
        deltaX = std::clamp(deltaX, -clampLimit, clampLimit);
        deltaY = std::clamp(deltaY, -clampLimit, clampLimit);

        if (std::fabs(deltaX) < 0.01f && std::fabs(deltaY) < 0.01f) {
            return;
        }

        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dx = static_cast<LONG>(std::lround(deltaX));
        input.mi.dy = static_cast<LONG>(std::lround(deltaY));
        input.mi.dwFlags = MOUSEEVENTF_MOVE;
        SendInput(1, &input, sizeof(input));
    }

    bool TryReadRoundPlayerNetworkId(void* roundPlayer, int& outNetworkId) {
        outNetworkId = -1;
        if (!roundPlayer || !HnawOffsets::roundPlayerNetworkPlayerID) {
            return false;
        }

        int networkId = -1;
        if (!SafeRead(reinterpret_cast<uintptr_t>(roundPlayer) + HnawOffsets::roundPlayerNetworkPlayerID, networkId) || networkId < 0) {
            return false;
        }

        outNetworkId = networkId;
        return true;
    }

    bool EstimateTargetVelocity(void* roundPlayer, const Vec3& currentPosition, Vec3& outVelocity) {
        outVelocity = Vec3{ 0.0f, 0.0f, 0.0f };

        int networkId = -1;
        if (!TryReadRoundPlayerNetworkId(roundPlayer, networkId)) {
            return false;
        }

        const uint64_t nowMs = GetTickCount64();
        MotionSample& sample = gTargetMotionSamples[networkId];

        bool hasVelocity = false;
        if (sample.valid && nowMs > sample.timeMs) {
            const float dt = static_cast<float>(nowMs - sample.timeMs) * 0.001f;
            if (dt >= 0.01f && dt <= 0.50f) {
                outVelocity.x = (currentPosition.x - sample.position.x) / dt;
                outVelocity.y = (currentPosition.y - sample.position.y) / dt;
                outVelocity.z = (currentPosition.z - sample.position.z) / dt;

                const float speed = std::sqrt((outVelocity.x * outVelocity.x) + (outVelocity.y * outVelocity.y) + (outVelocity.z * outVelocity.z));
                if (std::isfinite(speed) && speed > 0.0f) {
                    const float maxSpeed = 32.0f;
                    if (speed > maxSpeed) {
                        const float scale = maxSpeed / speed;
                        outVelocity.x *= scale;
                        outVelocity.y *= scale;
                        outVelocity.z *= scale;
                    }
                    hasVelocity = true;
                }
            }
        }

        sample.position = currentPosition;
        sample.timeMs = nowMs;
        sample.valid = true;

        if (gTargetMotionSamples.size() > 1024) {
            gTargetMotionSamples.clear();
        }

        return hasVelocity;
    }

    float SolveInterceptTime(const Vec3& shooterPosition, const Vec3& targetPosition, const Vec3& targetVelocity, float projectileSpeed) {
        const Vec3 rel{
            targetPosition.x - shooterPosition.x,
            targetPosition.y - shooterPosition.y,
            targetPosition.z - shooterPosition.z
        };

        const float vv = (targetVelocity.x * targetVelocity.x) + (targetVelocity.y * targetVelocity.y) + (targetVelocity.z * targetVelocity.z);
        const float rr = (rel.x * rel.x) + (rel.y * rel.y) + (rel.z * rel.z);
        const float rv = (rel.x * targetVelocity.x) + (rel.y * targetVelocity.y) + (rel.z * targetVelocity.z);
        const float speedSq = projectileSpeed * projectileSpeed;

        const float a = vv - speedSq;
        const float b = 2.0f * rv;
        const float c = rr;

        if (std::fabs(a) < 0.0001f) {
            if (std::fabs(b) < 0.0001f) {
                return projectileSpeed > 1.0f ? std::sqrt(rr) / projectileSpeed : -1.0f;
            }
            const float t = -c / b;
            return t > 0.0f ? t : -1.0f;
        }

        const float disc = (b * b) - (4.0f * a * c);
        if (disc < 0.0f) {
            return -1.0f;
        }

        const float sqrtDisc = std::sqrt(disc);
        const float inv = 1.0f / (2.0f * a);
        const float t0 = (-b - sqrtDisc) * inv;
        const float t1 = (-b + sqrtDisc) * inv;

        float best = -1.0f;
        if (t0 > 0.0f) {
            best = t0;
        }
        if (t1 > 0.0f && (best < 0.0f || t1 < best)) {
            best = t1;
        }
        return best;
    }

    float SolveHorizontalInterceptTime(const Vec3& shooterPosition, const Vec3& targetPosition, const Vec3& targetVelocity, float projectileSpeed) {
        const float relX = targetPosition.x - shooterPosition.x;
        const float relZ = targetPosition.z - shooterPosition.z;
        const float velX = targetVelocity.x;
        const float velZ = targetVelocity.z;

        const float vv = (velX * velX) + (velZ * velZ);
        const float rr = (relX * relX) + (relZ * relZ);
        const float rv = (relX * velX) + (relZ * velZ);
        const float speedSq = projectileSpeed * projectileSpeed;

        const float a = vv - speedSq;
        const float b = 2.0f * rv;
        const float c = rr;

        if (std::fabs(a) < 0.0001f) {
            if (std::fabs(b) < 0.0001f) {
                return projectileSpeed > 1.0f ? std::sqrt(rr) / projectileSpeed : -1.0f;
            }
            const float t = -c / b;
            return t > 0.0f ? t : -1.0f;
        }

        const float disc = (b * b) - (4.0f * a * c);
        if (disc < 0.0f) {
            return -1.0f;
        }

        const float sqrtDisc = std::sqrt(disc);
        const float inv = 1.0f / (2.0f * a);
        const float t0 = (-b - sqrtDisc) * inv;
        const float t1 = (-b + sqrtDisc) * inv;

        float best = -1.0f;
        if (t0 > 0.0f) {
            best = t0;
        }
        if (t1 > 0.0f && (best < 0.0f || t1 < best)) {
            best = t1;
        }
        return best;
    }

    bool TryGetWeaponTypeBallisticsHint(int weaponType, float& outVelocity, float& outGravity) {
        if (weaponType < 0) {
            return false;
        }

        std::string name = Aimbot::GetWeaponTypeName(static_cast<uint16_t>(weaponType));
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        if (name.find("rifle") != std::string::npos) {
            outVelocity = 220.0f;
            outGravity = 9.81f;
            return true;
        }
        if (name.find("musket") != std::string::npos) {
            outVelocity = 170.0f;
            outGravity = 9.81f;
            return true;
        }
        if (name.find("carbine") != std::string::npos) {
            outVelocity = 190.0f;
            outGravity = 9.81f;
            return true;
        }
        if (name.find("pistol") != std::string::npos) {
            outVelocity = 125.0f;
            outGravity = 9.81f;
            return true;
        }
        if (name.find("blunderbuss") != std::string::npos) {
            outVelocity = 95.0f;
            outGravity = 9.81f;
            return true;
        }

        return false;
    }
}

namespace Aimbot {
    static bool gEnabled = false;
    static bool gRequireKey = true;
    static int gAimKey = VK_RBUTTON;
    static float gFovPixels = 140.0f;
    static float gSmooth = 6.0f;
    static int gTargetBone = 0;
    static int gTeamFilterMode = 2;
    static bool gDrawFovCircle = true;
    static float gFovColorRgb[3] = { 0.20f, 0.85f, 0.35f };
    static bool gDropCompEnabled = true;
    static bool gUseWeaponBallistics = true;
    static float gDefaultMuzzleVelocity = 100.0f;
    static float gDefaultGravity = 9.81f;
    static bool gReloadSpeedEnabled = false;
    static float gReloadSpeedMultiplier = 1.0f;
    static bool gFireRateEnabled = false;
    static float gFireRateMultiplier = 1.0f;
    static std::string gLastStatus = "idle";
    static int gLastWeaponType = -1;
    static float gLastResolvedVelocity = 0.0f;
    static float gLastResolvedGravity = 0.0f;
    static bool gLastUsedWeaponBallistics = false;

    static MonoMethod* gWeaponHolderGetActiveWeaponDetailsMethod = nullptr;
    static MonoMethod* gWeaponResolveFirearmPropertiesMethod = nullptr;
    static MonoMethod* gWeaponResolveFirearmPropertiesMethodAlt = nullptr;
    static MonoMethod* gWeaponHolderGetActiveWeaponTypeMethod = nullptr;
    static MonoMethod* gWeaponGetWeaponTypeMethod = nullptr;

    static bool IsLikelyWeaponType(int value) {
        return value >= 0 && value <= 4096;
    }

    static bool IsInvalidWeaponType(uint16_t value) {
        return value == 0xFFFF;
    }

    bool& Enabled() {
        return gEnabled;
    }

    bool& RequireKey() {
        return gRequireKey;
    }

    int& AimKey() {
        return gAimKey;
    }

    float& FovPixels() {
        return gFovPixels;
    }

    float& Smooth() {
        return gSmooth;
    }

    int& TargetBone() {
        return gTargetBone;
    }

    int& TeamFilterMode() {
        return gTeamFilterMode;
    }

    bool& DrawFovCircle() {
        return gDrawFovCircle;
    }

    float* FovColorRgb() {
        return gFovColorRgb;
    }

    bool& DropCompensationEnabled() {
        return gDropCompEnabled;
    }

    bool& UseWeaponBallistics() {
        return gUseWeaponBallistics;
    }

    float& DefaultMuzzleVelocity() {
        return gDefaultMuzzleVelocity;
    }

    float& DefaultGravity() {
        return gDefaultGravity;
    }

    bool& ReloadSpeedEnabled() {
        return gReloadSpeedEnabled;
    }

    float& ReloadSpeedMultiplier() {
        return gReloadSpeedMultiplier;
    }

    bool& FireRateEnabled() {
        return gFireRateEnabled;
    }

    float& FireRateMultiplier() {
        return gFireRateMultiplier;
    }

    int LastWeaponType() {
        return gLastWeaponType;
    }

    const char* LastWeaponTypeName() {
        if (gLastWeaponType < 0) {
            return "Unknown";
        }
        return GetWeaponTypeName(static_cast<uint16_t>(gLastWeaponType));
    }

    float LastResolvedVelocity() {
        return gLastResolvedVelocity;
    }

    float LastResolvedGravity() {
        return gLastResolvedGravity;
    }

    bool LastUsedWeaponBallistics() {
        return gLastUsedWeaponBallistics;
    }

    static bool TryGetActiveWeaponType(void* weaponHolder, int& outWeaponType) {
        outWeaponType = -1;
        if (!weaponHolder) {
            return false;
        }

        MonoClass* holderClass = gMonoApi.monoObjectGetClass(reinterpret_cast<MonoObject*>(weaponHolder));
        if (!holderClass) {
            return false;
        }

        if (!gWeaponHolderGetActiveWeaponTypeMethod) {
            gWeaponHolderGetActiveWeaponTypeMethod = gMonoApi.monoClassGetMethodFromName(holderClass, "get_ActiveWeaponType", 0);
            if (!gWeaponHolderGetActiveWeaponTypeMethod) {
                gWeaponHolderGetActiveWeaponTypeMethod = gMonoApi.monoClassGetMethodFromName(holderClass, "GetActiveWeaponType", 0);
            }
        }

        if (gWeaponHolderGetActiveWeaponTypeMethod) {
            MonoObject* boxedValue = InvokeMethod(gWeaponHolderGetActiveWeaponTypeMethod, weaponHolder, nullptr);
            if (boxedValue) {
                void* unboxed = gMonoApi.monoObjectUnbox(boxedValue);
                if (unboxed) {
                    const uint16_t raw = *reinterpret_cast<uint16_t*>(unboxed);
                    if (!IsInvalidWeaponType(raw)) {
                        outWeaponType = static_cast<int>(raw);
                        if (IsLikelyWeaponType(outWeaponType)) {
                            return true;
                        }
                    }
                }
            }
        }

        const char* fieldNames[] = {
            "ActiveWeaponType",
            "activeWeaponType",
            "_activeWeaponType",
            "<ActiveWeaponType>k__BackingField",
            "lastFiredFirearmWeaponType",
            "previousWeaponType"
        };

        MonoClassField* field = TryGetFieldByNames(holderClass, fieldNames, std::size(fieldNames));
        if (!field) {
            return false;
        }

        uint16_t value = 0xFFFF;
        gMonoApi.monoFieldGetValue(reinterpret_cast<MonoObject*>(weaponHolder), field, &value);
        if (IsInvalidWeaponType(value)) {
            return false;
        }
        outWeaponType = static_cast<int>(value);
        return IsLikelyWeaponType(outWeaponType);
    }

    static bool TryGetWeaponTypeFromDetails(MonoObject* weaponDetails, int& outWeaponType) {
        outWeaponType = -1;
        if (!weaponDetails) {
            return false;
        }

        MonoClass* weaponClass = gMonoApi.monoObjectGetClass(weaponDetails);
        if (!weaponClass) {
            return false;
        }

        if (!gWeaponGetWeaponTypeMethod) {
            gWeaponGetWeaponTypeMethod = gMonoApi.monoClassGetMethodFromName(weaponClass, "get_weaponType", 0);
            if (!gWeaponGetWeaponTypeMethod) {
                gWeaponGetWeaponTypeMethod = gMonoApi.monoClassGetMethodFromName(weaponClass, "GetWeaponType", 0);
            }
        }

        if (gWeaponGetWeaponTypeMethod) {
            MonoObject* boxedValue = InvokeMethod(gWeaponGetWeaponTypeMethod, weaponDetails, nullptr);
            if (boxedValue) {
                void* unboxed = gMonoApi.monoObjectUnbox(boxedValue);
                if (unboxed) {
                    const uint16_t raw = *reinterpret_cast<uint16_t*>(unboxed);
                    if (!IsInvalidWeaponType(raw)) {
                        outWeaponType = static_cast<int>(raw);
                        if (IsLikelyWeaponType(outWeaponType)) {
                            return true;
                        }
                    }
                }
            }
        }

        const char* fieldNames[] = { "weaponType", "WeaponType", "_weaponType", "<weaponType>k__BackingField" };
        MonoClassField* field = TryGetFieldByNames(weaponClass, fieldNames, std::size(fieldNames));
        if (!field) {
            return false;
        }

        uint16_t value = 0xFFFF;
        gMonoApi.monoFieldGetValue(weaponDetails, field, &value);
        if (IsInvalidWeaponType(value)) {
            return false;
        }
        outWeaponType = static_cast<int>(value);
        return IsLikelyWeaponType(outWeaponType);
    }

    static MonoObject* TryGetActiveWeaponDetails(void* weaponHolder) {
        if (!weaponHolder) {
            return nullptr;
        }

        MonoClass* holderClass = gMonoApi.monoObjectGetClass(reinterpret_cast<MonoObject*>(weaponHolder));
        if (!holderClass) {
            return nullptr;
        }

        if (!gWeaponHolderGetActiveWeaponDetailsMethod) {
            gWeaponHolderGetActiveWeaponDetailsMethod = gMonoApi.monoClassGetMethodFromName(holderClass, "get_ActiveWeaponDetails", 0);
            if (!gWeaponHolderGetActiveWeaponDetailsMethod) {
                gWeaponHolderGetActiveWeaponDetailsMethod = gMonoApi.monoClassGetMethodFromName(holderClass, "GetActiveWeaponDetails", 0);
            }
        }

        if (!gWeaponHolderGetActiveWeaponDetailsMethod) {
            return nullptr;
        }

        return InvokeMethod(gWeaponHolderGetActiveWeaponDetailsMethod, weaponHolder, nullptr);
    }

    static MonoObject* TryResolveFirearmProperties(MonoObject* weaponDetails) {
        if (!weaponDetails) {
            return nullptr;
        }

        MonoClass* weaponClass = gMonoApi.monoObjectGetClass(weaponDetails);
        if (!weaponClass) {
            return nullptr;
        }

        if (!gWeaponResolveFirearmPropertiesMethod && !gWeaponResolveFirearmPropertiesMethodAlt) {
            gWeaponResolveFirearmPropertiesMethod = gMonoApi.monoClassGetMethodFromName(weaponClass, "ResolveFirearmWeaponProperties", 0);
            gWeaponResolveFirearmPropertiesMethodAlt = gMonoApi.monoClassGetMethodFromName(weaponClass, "ResolveFirearmWeaponProperties", 1);
        }

        if (gWeaponResolveFirearmPropertiesMethod) {
            MonoObject* props = InvokeMethod(gWeaponResolveFirearmPropertiesMethod, weaponDetails, nullptr);
            if (props) {
                return props;
            }
        }

        if (gWeaponResolveFirearmPropertiesMethodAlt) {
            int gameType = 0;
            void* args[1] = { &gameType };
            return InvokeMethod(gWeaponResolveFirearmPropertiesMethodAlt, weaponDetails, args);
        }

        return nullptr;
    }

    static bool TryReadFirearmBallistics(MonoObject* firearmProps, float& outVelocity, float& outGravity) {
        if (!firearmProps) {
            return false;
        }

        MonoClass* propsClass = gMonoApi.monoObjectGetClass(firearmProps);
        if (!propsClass) {
            return false;
        }

        float minVelocity = 0.0f;
        float maxVelocity = 0.0f;
        float minGravity = 0.0f;
        float maxGravity = 0.0f;

        MonoClassField* minVelField = gMonoApi.monoClassGetFieldFromName(propsClass, "minProjectileMuzzleVelocity");
        MonoClassField* maxVelField = gMonoApi.monoClassGetFieldFromName(propsClass, "maxProjectileMuzzleVelocity");
        MonoClassField* minGravField = gMonoApi.monoClassGetFieldFromName(propsClass, "minProjectileGravity");
        MonoClassField* maxGravField = gMonoApi.monoClassGetFieldFromName(propsClass, "maxProjectileGravity");

        if (!minVelField || !maxVelField || !minGravField || !maxGravField) {
            return false;
        }

        gMonoApi.monoFieldGetValue(firearmProps, minVelField, &minVelocity);
        gMonoApi.monoFieldGetValue(firearmProps, maxVelField, &maxVelocity);
        gMonoApi.monoFieldGetValue(firearmProps, minGravField, &minGravity);
        gMonoApi.monoFieldGetValue(firearmProps, maxGravField, &maxGravity);

        if (!std::isfinite(minVelocity) || !std::isfinite(maxVelocity) ||
            !std::isfinite(minGravity) || !std::isfinite(maxGravity)) {
            return false;
        }

        outVelocity = (minVelocity + maxVelocity) * 0.5f;
        outGravity = (minGravity + maxGravity) * 0.5f;
        return outVelocity > 1.0f && outGravity > 0.0f;
    }

    const char* LastStatus() {
        return gLastStatus.c_str();
    }

    void Run(bool menuOpen) {
        gLastStatus = "idle";

        if (gDrawFovCircle && gFovPixels > 1.0f) {
            ImGuiIO& io = ImGui::GetIO();
            const ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
            const ImU32 color = IM_COL32(
                static_cast<int>(std::clamp(gFovColorRgb[0], 0.0f, 1.0f) * 255.0f),
                static_cast<int>(std::clamp(gFovColorRgb[1], 0.0f, 1.0f) * 255.0f),
                static_cast<int>(std::clamp(gFovColorRgb[2], 0.0f, 1.0f) * 255.0f),
                170);
            ImGui::GetBackgroundDrawList()->AddCircle(center, gFovPixels, color, 64, 1.3f);
        }

        if (!gEnabled) {
            gLastStatus = "disabled";
            return;
        }

        if (menuOpen) {
            gLastStatus = "menu open";
            return;
        }

        if (gRequireKey) {
            if ((GetAsyncKeyState(gAimKey) & 0x8000) == 0) {
                gLastStatus = "key up";
                return;
            }
        }

        if (!EnsureMonoSymbols() || !HnawOffsets::roundPlayerPlayerTransform) {
            gLastStatus = "mono symbols unavailable";
            return;
        }

        AttachMonoThread();

        void* manager = GetRoundPlayerManagerInstance();
        MonoObject* playerListObject = nullptr;
        if (manager) {
            playerListObject = InvokeMethod(gGetAllRoundPlayersMethod, manager, nullptr);
        }
        if (!playerListObject) {
            playerListObject = InvokeMethod(gGetAllRoundPlayersMethod, nullptr, nullptr);
        }
        if (!playerListObject) {
            gLastStatus = manager ? "player list null" : "manager null";
            return;
        }

        void* items = nullptr;
        int size = 0;
        std::vector<void*> methodElements;
        const bool useRawCollection = TryReadCollection(playerListObject, items, size);
        const bool useMethodCollection = !useRawCollection && TryEnumerateCollectionByMethods(playerListObject, methodElements);

        if (!useRawCollection && !useMethodCollection) {
            gLastStatus = "unsupported collection";
            return;
        }

        MonoObject* camera = InvokeMethod(gCameraGetMainMethod, nullptr, nullptr);
        if (!camera) {
            gLastStatus = "camera null";
            return;
        }

        void* localRoundPlayer = nullptr;
        if (manager && HnawOffsets::clientRoundPlayerManagerLocalPlayer) {
            SafeRead(reinterpret_cast<uintptr_t>(manager) + HnawOffsets::clientRoundPlayerManagerLocalPlayer, localRoundPlayer);
        }

        Vec3 localPosition{};
        bool hasLocalPosition = false;
        if (localRoundPlayer && HnawOffsets::roundPlayerPlayerTransform) {
            void* localTransform = nullptr;
            if (SafeRead(reinterpret_cast<uintptr_t>(localRoundPlayer) + HnawOffsets::roundPlayerPlayerTransform, localTransform) && localTransform) {
                hasLocalPosition = InvokeMethodVec3(gTransformGetPositionMethod, localTransform, nullptr, localPosition);
            }
        }

        float muzzleVelocity = gDefaultMuzzleVelocity;
        float gravity = gDefaultGravity;
        gLastWeaponType = -1;
        gLastUsedWeaponBallistics = false;
        if (gDropCompEnabled && gUseWeaponBallistics && localRoundPlayer && HnawOffsets::roundPlayerWeaponHolder) {
            void* weaponHolder = nullptr;
            if (SafeRead(reinterpret_cast<uintptr_t>(localRoundPlayer) + HnawOffsets::roundPlayerWeaponHolder, weaponHolder) && weaponHolder) {
                MonoObject* weaponDetails = TryGetActiveWeaponDetails(weaponHolder);
                if (!TryGetActiveWeaponType(weaponHolder, gLastWeaponType)) {
                    TryGetWeaponTypeFromDetails(weaponDetails, gLastWeaponType);
                }
                MonoObject* firearmProps = TryResolveFirearmProperties(weaponDetails);
                float weaponVelocity = 0.0f;
                float weaponGravity = 0.0f;
                if (TryReadFirearmBallistics(firearmProps, weaponVelocity, weaponGravity)) {
                    muzzleVelocity = weaponVelocity;
                    gravity = weaponGravity;
                    gLastUsedWeaponBallistics = true;
                }

                if (!gLastUsedWeaponBallistics && TryGetWeaponTypeBallisticsHint(gLastWeaponType, weaponVelocity, weaponGravity)) {
                    muzzleVelocity = weaponVelocity;
                    gravity = weaponGravity;
                    gLastUsedWeaponBallistics = true;
                }
            }
        }
        gLastResolvedVelocity = muzzleVelocity;
        gLastResolvedGravity = gravity;

        int localFaction = -1;
        int localSquad = -1;
        if (localRoundPlayer) {
            void* localPlayerBase = nullptr;
            void* localSpawnData = nullptr;
            if (TryGetSpawnData(localRoundPlayer, localPlayerBase, localSpawnData)) {
                ResolveTeamData(localSpawnData, localFaction, localSquad);
            }
        }

        const int safeCount = useRawCollection
            ? std::clamp(size, 0, 256)
            : std::clamp(static_cast<int>(methodElements.size()), 0, 256);

        ImGuiIO& io = ImGui::GetIO();
        const ImVec2 screenCenter(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);

        float bestDist = gFovPixels;
        ImVec2 bestScreen{};
        bool hasTarget = false;

        for (int i = 0; i < safeCount; ++i) {
            void* roundPlayer = useRawCollection
                ? GetManagedArrayElement(items, i)
                : methodElements[static_cast<size_t>(i)];
            if (!roundPlayer || roundPlayer == localRoundPlayer) {
                continue;
            }

            void* transformObject = nullptr;
            if (!SafeRead(reinterpret_cast<uintptr_t>(roundPlayer) + HnawOffsets::roundPlayerPlayerTransform, transformObject) || !transformObject) {
                continue;
            }

            Vec3 feet{};
            if (!InvokeMethodVec3(gTransformGetPositionMethod, transformObject, nullptr, feet)) {
                continue;
            }
            if (IsZeroVec3(feet)) {
                continue;
            }

            void* playerBase = nullptr;
            void* spawnData = nullptr;
            int faction = -1;
            int squad = -1;
            if (TryGetSpawnData(roundPlayer, playerBase, spawnData)) {
                ResolveTeamData(spawnData, faction, squad);
            }

            bool hasTeamData = false;
            bool isTeammate = false;
            if (localFaction >= 0 && faction >= 0) {
                hasTeamData = true;
                isTeammate = (localFaction == faction);
            } else if (localSquad >= 0 && squad >= 0) {
                hasTeamData = true;
                isTeammate = (localSquad == squad);
            }

            if (gTeamFilterMode == 1 && hasTeamData && !isTeammate) {
                continue;
            }
            if (gTeamFilterMode == 2 && hasTeamData && isTeammate) {
                continue;
            }

            Vec3 targetWorld{};
            if (!TrySelectTargetWorldPos(roundPlayer, playerBase, spawnData, feet, gTargetBone, targetWorld)) {
                continue;
            }

            if (gDropCompEnabled && muzzleVelocity > 1.0f && gravity > 0.0f && hasLocalPosition) {
                Vec3 targetVelocity{};
                EstimateTargetVelocity(roundPlayer, feet, targetVelocity);

                // Character animation can introduce noisy vertical velocity in bone positions.
                targetVelocity.y = 0.0f;
                const float horizontalSpeed = std::sqrt((targetVelocity.x * targetVelocity.x) + (targetVelocity.z * targetVelocity.z));
                if (horizontalSpeed > 14.0f) {
                    const float clampScale = 14.0f / horizontalSpeed;
                    targetVelocity.x *= clampScale;
                    targetVelocity.z *= clampScale;
                }

                float travelTime = SolveHorizontalInterceptTime(localPosition, targetWorld, targetVelocity, muzzleVelocity);
                if (!std::isfinite(travelTime) || travelTime <= 0.0f) {
                    const float dx = targetWorld.x - localPosition.x;
                    const float dz = targetWorld.z - localPosition.z;
                    const float horizontalDistance = std::sqrt((dx * dx) + (dz * dz));
                    travelTime = horizontalDistance / muzzleVelocity;
                }

                if (std::isfinite(travelTime) && travelTime > 0.0f && travelTime < 6.0f) {
                    targetWorld.x += targetVelocity.x * travelTime;
                    targetWorld.z += targetVelocity.z * travelTime;

                    // Server-side trajectory uses discrete segment integration; this factor keeps compensation aligned in practice.
                    const float effectiveGravity = std::clamp(gravity * 0.55f, 0.0f, 25.0f);
                    const float drop = 0.5f * effectiveGravity * travelTime * travelTime;
                    targetWorld.y += drop;
                }
            }

            ImVec2 targetScreen{};
            if (!WorldToScreen(camera, targetWorld, targetScreen)) {
                continue;
            }

            const float dx = targetScreen.x - screenCenter.x;
            const float dy = targetScreen.y - screenCenter.y;
            const float dist = std::sqrt((dx * dx) + (dy * dy));
            if (dist > bestDist) {
                continue;
            }

            bestDist = dist;
            bestScreen = targetScreen;
            hasTarget = true;
        }

        if (!hasTarget) {
            gLastStatus = "no target";
            return;
        }

        const float rawDx = bestScreen.x - screenCenter.x;
        const float rawDy = bestScreen.y - screenCenter.y;
        const float smooth = std::clamp(gSmooth, 1.0f, 40.0f);
        ApplyMouseDelta(rawDx / smooth, rawDy / smooth);
        gLastStatus = "aiming";
    }
}

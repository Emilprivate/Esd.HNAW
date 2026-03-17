#include "features/esp/esp_internal.h"

#include "features/aimbot/weapon_type_names.h"

#include "core/hnaw_offsets.h"
#include "core/hooking/hook.h"
#include "ui/gui.h"

#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

using namespace EspInternal;

namespace {
    struct VisibilityEntry {
        bool visible = true;
        uint64_t updatedMs = 0;
    };

    struct RadarPoint {
        float dx = 0.0f;
        float dz = 0.0f;
        bool enemy = true;
        bool visible = true;
    };

    std::unordered_map<int, VisibilityEntry> gVisibilityCache;
    uint64_t gLastVisibilityUpdateMs = 0;
    MonoMethod* gCameraGetTransformMethod = nullptr;
    MonoMethod* gTransformGetForwardMethod = nullptr;
    MonoMethod* gTransformGetEulerAnglesMethod = nullptr;
    uint64_t gLastCannonContextMs = 0;
    Vec3 gLastAimForward{ 0.0f, 0.0f, 1.0f };
    bool gLastAimForwardValid = false;
    void* gLastAimingContextObject = nullptr;
    uint64_t gLastAimingContextUpdatedMs = 0;
    Vec3 gLastCannonAimOrigin{ 0.0f, 0.0f, 0.0f };
    Vec3 gLastCannonAimForward{ 0.0f, 0.0f, 1.0f };
    bool gLastCannonAimValid = false;
    uint64_t gLastCannonAimUpdatedMs = 0;
    Vec3 gCachedCannonImpactPoint{};
    bool gCachedCannonImpactValid = false;
    uint64_t gCachedCannonImpactUpdatedMs = 0;
    uint64_t gLastCannonPredictionUpdateMs = 0;

    bool TryGetNetworkId(void* roundPlayer, int& outNetworkId) {
        outNetworkId = -1;
        if (!roundPlayer || !HnawOffsets::roundPlayerNetworkPlayerID) {
            return false;
        }

        int value = -1;
        if (!SafeRead(reinterpret_cast<uintptr_t>(roundPlayer) + HnawOffsets::roundPlayerNetworkPlayerID, value) || value < 0) {
            return false;
        }

        outNetworkId = value;
        return true;
    }

    bool HasLineOfSight(const Vec3& origin, const Vec3& target) {
        if (!gPhysicsLinecastMethod) {
            return true;
        }

        const float dx = target.x - origin.x;
        const float dy = target.y - origin.y;
        const float dz = target.z - origin.z;
        const float distance = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
        if (!std::isfinite(distance) || distance < 0.20f) {
            return true;
        }

        // Stop just short of the target to avoid target collider self-blocking.
        const float shrink = (std::min)(0.25f, distance * 0.25f);
        const float keep = (std::max)(0.0f, (distance - shrink) / distance);

        Vec3 shortenedTarget = target;
        shortenedTarget.x = origin.x + (dx * keep);
        shortenedTarget.y = origin.y + (dy * keep);
        shortenedTarget.z = origin.z + (dz * keep);

        Vec3 originCopy = origin;
        bool blocked = false;
        bool invokeOk = false;

        __try {
            if (gPhysicsLinecastParamCount >= 3) {
                int layerMask = -1;
                void* args[3] = { &originCopy, &shortenedTarget, &layerMask };
                invokeOk = InvokeMethodBool(gPhysicsLinecastMethod, nullptr, args, blocked);
            } else {
                void* args[2] = { &originCopy, &shortenedTarget };
                invokeOk = InvokeMethodBool(gPhysicsLinecastMethod, nullptr, args, blocked);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return true;
        }

        if (!invokeOk) {
            return true;
        }

        return !blocked;
    }

    bool InvokePhysicsLinecast(const Vec3& origin, const Vec3& target, bool& outBlocked) {
        outBlocked = false;
        if (!gPhysicsLinecastMethod) {
            return false;
        }

        Vec3 originCopy = origin;
        Vec3 targetCopy = target;
        bool invokeOk = false;

        __try {
            if (gPhysicsLinecastParamCount >= 3) {
                int layerMask = -1;
                void* args[3] = { &originCopy, &targetCopy, &layerMask };
                invokeOk = InvokeMethodBool(gPhysicsLinecastMethod, nullptr, args, outBlocked);
            } else {
                void* args[2] = { &originCopy, &targetCopy };
                invokeOk = InvokeMethodBool(gPhysicsLinecastMethod, nullptr, args, outBlocked);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }

        return invokeOk;
    }

    bool SegmentIntersectsWorld(const Vec3& start, const Vec3& end) {
        bool blocked = false;
        if (!InvokePhysicsLinecast(start, end, blocked)) {
            return false;
        }
        return blocked;
    }

    Vec3 LerpVec3(const Vec3& a, const Vec3& b, float t) {
        return Vec3{
            a.x + ((b.x - a.x) * t),
            a.y + ((b.y - a.y) * t),
            a.z + ((b.z - a.z) * t)
        };
    }

    bool RefineImpactPoint(const Vec3& start, const Vec3& end, Vec3& outImpact) {
        if (!SegmentIntersectsWorld(start, end)) {
            return false;
        }

        Vec3 lo = start;
        Vec3 hi = end;
        for (int i = 0; i < 8; ++i) {
            const Vec3 mid = LerpVec3(lo, hi, 0.5f);
            if (SegmentIntersectsWorld(lo, mid)) {
                hi = mid;
            } else {
                lo = mid;
            }
        }

        outImpact = hi;
        return true;
    }

    void ResolveCannonBallisticsFromWeaponType(int weaponType, float& outVelocity, float& outGravity) {
        outVelocity = std::clamp(gCannonImpactVelocity, 40.0f, 300.0f);
        outGravity = std::clamp(gCannonImpactGravity, 0.0f, 30.0f);

        if (weaponType < 0 || weaponType > 65535) {
            return;
        }

        std::string weaponName = Aimbot::GetWeaponTypeName(static_cast<uint16_t>(weaponType));
        std::transform(weaponName.begin(), weaponName.end(), weaponName.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        if (weaponName.find("rocket") != std::string::npos) {
            outVelocity *= 0.62f;
            outGravity *= 0.55f;
            return;
        }
        if (weaponName.find("mortar") != std::string::npos) {
            outVelocity *= 0.72f;
            outGravity *= 1.05f;
            return;
        }
        if (weaponName.find("howitzer") != std::string::npos) {
            outVelocity *= 0.80f;
            outGravity *= 1.00f;
            return;
        }
        if (weaponName.find("carronade") != std::string::npos || weaponName.find("buckshot") != std::string::npos) {
            outVelocity *= 0.74f;
            return;
        }
        if (weaponName.find("swivel") != std::string::npos) {
            outVelocity *= 1.10f;
            return;
        }
        if (weaponName.find("fieldcannon") != std::string::npos ||
            weaponName.find("coastal") != std::string::npos ||
            weaponName.find("24pounder") != std::string::npos ||
            weaponName.find("9pounder") != std::string::npos) {
            outVelocity *= 1.08f;
        }
    }

    bool IsCannonContextWeaponType(int weaponType) {
        if (weaponType < 0 || weaponType > 65535) {
            return false;
        }

        std::string name = Aimbot::GetWeaponTypeName(static_cast<uint16_t>(weaponType));
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        return name.find("cannon") != std::string::npos ||
            name.find("artillery") != std::string::npos ||
            name.find("howitzer") != std::string::npos ||
            name.find("mortar") != std::string::npos ||
            name.find("deckgun") != std::string::npos ||
            name.find("coastgun") != std::string::npos ||
            name.find("broadside") != std::string::npos ||
            name.find("carronade") != std::string::npos ||
            name.find("swivel") != std::string::npos ||
            name.find("rocket") != std::string::npos ||
            name.find("turret") != std::string::npos;
    }

    bool TryGetWeaponTypeFromHolder(void* weaponHolder, int& outWeaponType) {
        outWeaponType = -1;
        if (!weaponHolder || !gMonoApi.monoObjectGetClass || !gMonoApi.monoClassGetMethodFromName || !gMonoApi.monoObjectUnbox) {
            return false;
        }

        MonoClass* holderClass = gMonoApi.monoObjectGetClass(reinterpret_cast<MonoObject*>(weaponHolder));
        if (!holderClass) {
            return false;
        }

        MonoMethod* getActiveWeaponType = gMonoApi.monoClassGetMethodFromName(holderClass, "get_ActiveWeaponType", 0);
        if (!getActiveWeaponType) {
            getActiveWeaponType = gMonoApi.monoClassGetMethodFromName(holderClass, "GetActiveWeaponType", 0);
        }

        if (getActiveWeaponType) {
            MonoObject* boxed = InvokeMethod(getActiveWeaponType, weaponHolder, nullptr);
            if (boxed) {
                void* raw = gMonoApi.monoObjectUnbox(boxed);
                if (raw) {
                    const uint16_t value = *reinterpret_cast<uint16_t*>(raw);
                    outWeaponType = static_cast<int>(value);
                    return true;
                }
            }
        }

        const char* fieldNames[] = {
            "ActiveWeaponType",
            "activeWeaponType",
            "_activeWeaponType",
            "<ActiveWeaponType>k__BackingField"
        };
        MonoClassField* field = TryGetFieldByNames(holderClass, fieldNames, std::size(fieldNames));
        if (!field || !gMonoApi.monoFieldGetValue) {
            return false;
        }

        uint16_t value = 0xFFFF;
        gMonoApi.monoFieldGetValue(reinterpret_cast<MonoObject*>(weaponHolder), field, &value);
        if (value == 0xFFFF) {
            return false;
        }

        outWeaponType = static_cast<int>(value);
        return true;
    }

    bool IsLocalPlayerInCannonContext(void* localRoundPlayer) {
        if (!localRoundPlayer || !HnawOffsets::roundPlayerWeaponHolder) {
            return false;
        }

        void* weaponHolder = nullptr;
        if (!SafeRead(reinterpret_cast<uintptr_t>(localRoundPlayer) + HnawOffsets::roundPlayerWeaponHolder, weaponHolder) || !weaponHolder) {
            return false;
        }

        int weaponType = -1;
        if (!TryGetWeaponTypeFromHolder(weaponHolder, weaponType)) {
            return false;
        }

        return IsCannonContextWeaponType(weaponType);
    }

    bool HasAnyMethod(MonoClass* klass, const char* const* methodNames, size_t methodCount) {
        if (!klass || !methodNames || methodCount == 0) {
            return false;
        }

        for (size_t i = 0; i < methodCount; ++i) {
            if (gMonoApi.monoClassGetMethodFromName(klass, methodNames[i], 0)) {
                return true;
            }
        }
        return false;
    }

    bool IsLikelyArtilleryInteractableObject(MonoObject* interactableObject) {
        if (!interactableObject || !gMonoApi.monoObjectGetClass) {
            return false;
        }

        MonoClass* objectClass = gMonoApi.monoObjectGetClass(interactableObject);
        if (!objectClass) {
            return false;
        }

        const char* artilleryMethodNames[] = {
            "IsAimingState",
            "ReadyToFire",
            "AttachPlayerToAimArty",
            "HandlePlayersOccupyingCannonMovement",
            "SetCannonBarrelAngle",
            "SetHowitzerBarrelAngle",
            "FireCannon",
            "FireArtillery",
            "FireHowitzer"
        };

        return HasAnyMethod(objectClass, artilleryMethodNames, std::size(artilleryMethodNames));
    }

    bool TryReadBoolFieldByNames(MonoObject* object, const char* const* fieldNames, size_t fieldCount, bool& outValue) {
        outValue = false;
        if (!object || !fieldNames || fieldCount == 0 || !gMonoApi.monoObjectGetClass || !gMonoApi.monoFieldGetValue) {
            return false;
        }

        MonoClass* klass = gMonoApi.monoObjectGetClass(object);
        if (!klass) {
            return false;
        }

        MonoClassField* field = TryGetFieldByNames(klass, fieldNames, fieldCount);
        if (!field) {
            return false;
        }

        uint8_t raw = 0;
        gMonoApi.monoFieldGetValue(object, field, &raw);
        outValue = (raw != 0);
        return true;
    }

    MonoObject* TryReadObjectFieldByNames(MonoObject* object, const char* const* fieldNames, size_t fieldCount) {
        if (!object || !fieldNames || fieldCount == 0 || !gMonoApi.monoObjectGetClass || !gMonoApi.monoFieldGetValue) {
            return nullptr;
        }

        MonoClass* klass = gMonoApi.monoObjectGetClass(object);
        if (!klass) {
            return nullptr;
        }

        MonoClassField* field = TryGetFieldByNames(klass, fieldNames, fieldCount);
        if (!field) {
            return nullptr;
        }

        MonoObject* value = nullptr;
        gMonoApi.monoFieldGetValue(object, field, &value);
        return value;
    }

    bool IsLocalPlayerInCannonContextByState(void* localRoundPlayer) {
        if (!localRoundPlayer || !gMonoApi.monoObjectGetClass || !gMonoApi.monoClassGetMethodFromName) {
            return false;
        }

        MonoClass* playerClass = gMonoApi.monoObjectGetClass(reinterpret_cast<MonoObject*>(localRoundPlayer));
        if (!playerClass) {
            return false;
        }

        const char* directTrueStateMethods[] = {
            "get_IsAimingArtillery",
            "get_IsUsingArtillery",
            "get_IsUsingCannon",
            "get_IsOnArtillery",
            "get_IsOnCannon",
            "IsAimingArtillery",
            "IsUsingArtillery",
            "IsUsingCannon",
            "IsOnArtillery",
            "IsOnCannon"
        };
        for (const char* methodName : directTrueStateMethods) {
            MonoMethod* method = gMonoApi.monoClassGetMethodFromName(playerClass, methodName, 0);
            if (!method) {
                continue;
            }
            bool value = false;
            if (InvokeMethodBool(method, localRoundPlayer, nullptr, value) && value) {
                return true;
            }
        }

        const char* directTrueStateFields[] = {
            "isAimingArtillery",
            "IsAimingArtillery",
            "isUsingArtillery",
            "IsUsingArtillery",
            "isUsingCannon",
            "IsUsingCannon",
            "isOnArtillery",
            "IsOnArtillery",
            "isOnCannon",
            "IsOnCannon",
            "<isAimingArtillery>k__BackingField",
            "<isUsingArtillery>k__BackingField",
            "<isUsingCannon>k__BackingField",
            "<isOnArtillery>k__BackingField",
            "<isOnCannon>k__BackingField"
        };
        bool directState = false;
        if (TryReadBoolFieldByNames(reinterpret_cast<MonoObject*>(localRoundPlayer), directTrueStateFields, std::size(directTrueStateFields), directState) && directState) {
            return true;
        }

        const char* interactableGetterMethods[] = {
            "get_CurrentInteractableObject",
            "get_OccupiedInteractableObject",
            "get_InteractingInteractableObject",
            "get_UsingInteractableObject",
            "get_AimedInteractableObject",
            "GetCurrentInteractableObject",
            "GetOccupiedInteractableObject",
            "GetInteractingInteractableObject",
            "GetUsingInteractableObject",
            "GetAimedInteractableObject"
        };

        for (const char* methodName : interactableGetterMethods) {
            MonoMethod* method = gMonoApi.monoClassGetMethodFromName(playerClass, methodName, 0);
            if (!method) {
                continue;
            }

            MonoObject* interactableObject = InvokeMethod(method, localRoundPlayer, nullptr);
            if (IsLikelyArtilleryInteractableObject(interactableObject)) {
                return true;
            }
        }

        const char* interactableFieldNames[] = {
            "currentInteractableObject",
            "CurrentInteractableObject",
            "occupiedInteractableObject",
            "OccupiedInteractableObject",
            "interactingInteractableObject",
            "InteractingInteractableObject",
            "usingInteractableObject",
            "UsingInteractableObject",
            "mountedInteractableObject",
            "MountedInteractableObject",
            "aimedInteractableObject",
            "AimedInteractableObject",
            "activeInteractableObject",
            "ActiveInteractableObject",
            "<currentInteractableObject>k__BackingField",
            "<occupiedInteractableObject>k__BackingField",
            "<interactingInteractableObject>k__BackingField",
            "<usingInteractableObject>k__BackingField",
            "<mountedInteractableObject>k__BackingField",
            "<aimedInteractableObject>k__BackingField",
            "<activeInteractableObject>k__BackingField"
        };
        MonoObject* interactableFromField = TryReadObjectFieldByNames(reinterpret_cast<MonoObject*>(localRoundPlayer), interactableFieldNames, std::size(interactableFieldNames));
        if (IsLikelyArtilleryInteractableObject(interactableFromField)) {
            return true;
        }

        return false;
    }

    bool IsLocalPlayerInAnyCannonContext(void* localRoundPlayer) {
        if (!localRoundPlayer) {
            return false;
        }

        if (IsLocalPlayerInCannonContext(localRoundPlayer)) {
            return true;
        }

        return IsLocalPlayerInCannonContextByState(localRoundPlayer);
    }

    bool TryGetLocalPlayerForward(void* manager, Vec3& outForward) {
        outForward = Vec3{ 0.0f, 0.0f, 1.0f };
        if (!manager || !HnawOffsets::clientRoundPlayerManagerLocalPlayer || !HnawOffsets::roundPlayerPlayerTransform) {
            return false;
        }

        void* localRoundPlayer = nullptr;
        if (!SafeRead(reinterpret_cast<uintptr_t>(manager) + HnawOffsets::clientRoundPlayerManagerLocalPlayer, localRoundPlayer) || !localRoundPlayer) {
            return false;
        }

        void* localTransform = nullptr;
        if (!SafeRead(reinterpret_cast<uintptr_t>(localRoundPlayer) + HnawOffsets::roundPlayerPlayerTransform, localTransform) || !localTransform) {
            return false;
        }

        if (!gTransformGetForwardMethod) {
            MonoClass* transformClass = gMonoApi.monoObjectGetClass(reinterpret_cast<MonoObject*>(localTransform));
            if (transformClass) {
                gTransformGetForwardMethod = gMonoApi.monoClassGetMethodFromName(transformClass, "get_forward", 0);
                if (!gTransformGetEulerAnglesMethod) {
                    gTransformGetEulerAnglesMethod = gMonoApi.monoClassGetMethodFromName(transformClass, "get_eulerAngles", 0);
                }
            }
        }

        bool gotForward = false;
        if (gTransformGetForwardMethod) {
            gotForward = InvokeMethodVec3(gTransformGetForwardMethod, localTransform, nullptr, outForward);
        }

        if (!gotForward) {
            if (!gTransformGetEulerAnglesMethod) {
                MonoClass* transformClass = gMonoApi.monoObjectGetClass(reinterpret_cast<MonoObject*>(localTransform));
                if (transformClass) {
                    gTransformGetEulerAnglesMethod = gMonoApi.monoClassGetMethodFromName(transformClass, "get_eulerAngles", 0);
                }
            }

            Vec3 euler{};
            if (!gTransformGetEulerAnglesMethod || !InvokeMethodVec3(gTransformGetEulerAnglesMethod, localTransform, nullptr, euler)) {
                return false;
            }

            const float pitchRad = euler.x * (3.1415926535f / 180.0f);
            const float yawRad = euler.y * (3.1415926535f / 180.0f);
            const float cp = std::cos(pitchRad);
            outForward.x = std::sin(yawRad) * cp;
            outForward.y = -std::sin(pitchRad);
            outForward.z = std::cos(yawRad) * cp;
        }

        const float horizontalLen = std::sqrt((outForward.x * outForward.x) + (outForward.z * outForward.z));
        if (!std::isfinite(horizontalLen) || horizontalLen < 0.001f) {
            return false;
        }

        outForward.x /= horizontalLen;
        outForward.y = 0.0f;
        outForward.z /= horizontalLen;
        return true;
    }

    bool TryGetCameraOriginForward(MonoObject* camera, Vec3& outOrigin, Vec3& outForward) {
        if (!camera) {
            return false;
        }

        if (!gCameraGetTransformMethod) {
            MonoClass* cameraClass = gMonoApi.monoObjectGetClass(camera);
            if (cameraClass) {
                gCameraGetTransformMethod = gMonoApi.monoClassGetMethodFromName(cameraClass, "get_transform", 0);
            }
        }

        if (!gCameraGetTransformMethod) {
            return false;
        }

        MonoObject* transform = InvokeMethod(gCameraGetTransformMethod, camera, nullptr);
        if (!transform) {
            return false;
        }

        if (!gTransformGetForwardMethod) {
            if (gTransformClass) {
                gTransformGetForwardMethod = gMonoApi.monoClassGetMethodFromName(gTransformClass, "get_forward", 0);
                if (!gTransformGetEulerAnglesMethod) {
                    gTransformGetEulerAnglesMethod = gMonoApi.monoClassGetMethodFromName(gTransformClass, "get_eulerAngles", 0);
                }
            }
            if (!gTransformGetForwardMethod) {
                MonoClass* transformClass = gMonoApi.monoObjectGetClass(transform);
                if (transformClass) {
                    gTransformGetForwardMethod = gMonoApi.monoClassGetMethodFromName(transformClass, "get_forward", 0);
                    if (!gTransformGetEulerAnglesMethod) {
                        gTransformGetEulerAnglesMethod = gMonoApi.monoClassGetMethodFromName(transformClass, "get_eulerAngles", 0);
                    }
                }
            }
        }

        if (!gTransformGetPositionMethod || !gTransformGetForwardMethod) {
            return false;
        }

        if (!InvokeMethodVec3(gTransformGetPositionMethod, transform, nullptr, outOrigin)) {
            return false;
        }

        bool gotForward = false;
        if (gTransformGetForwardMethod) {
            gotForward = InvokeMethodVec3(gTransformGetForwardMethod, transform, nullptr, outForward);
        }

        if (!gotForward) {
            if (!gTransformGetEulerAnglesMethod) {
                MonoClass* transformClass = gMonoApi.monoObjectGetClass(transform);
                if (transformClass) {
                    gTransformGetEulerAnglesMethod = gMonoApi.monoClassGetMethodFromName(transformClass, "get_eulerAngles", 0);
                }
            }

            Vec3 euler{};
            if (!gTransformGetEulerAnglesMethod || !InvokeMethodVec3(gTransformGetEulerAnglesMethod, transform, nullptr, euler)) {
                return false;
            }

            const float pitchRad = euler.x * (3.1415926535f / 180.0f);
            const float yawRad = euler.y * (3.1415926535f / 180.0f);
            const float cp = std::cos(pitchRad);
            outForward.x = std::sin(yawRad) * cp;
            outForward.y = -std::sin(pitchRad);
            outForward.z = std::cos(yawRad) * cp;
        }

        const float length = std::sqrt((outForward.x * outForward.x) + (outForward.y * outForward.y) + (outForward.z * outForward.z));
        if (!std::isfinite(length) || length < 0.01f) {
            return false;
        }

        outForward.x /= length;
        outForward.y /= length;
        outForward.z /= length;
        return true;
    }

    bool EnsureTransformForwardMethod() {
        if (gTransformGetForwardMethod) {
            return true;
        }
        if (!gMonoApi.monoClassGetMethodFromName) {
            return false;
        }
        if (gTransformClass) {
            gTransformGetForwardMethod = gMonoApi.monoClassGetMethodFromName(gTransformClass, "get_forward", 0);
            if (!gTransformGetEulerAnglesMethod) {
                gTransformGetEulerAnglesMethod = gMonoApi.monoClassGetMethodFromName(gTransformClass, "get_eulerAngles", 0);
            }
        }
        return gTransformGetForwardMethod != nullptr;
    }

    bool TryInvokeVec3MethodByNames(void* instance, MonoClass* klass, const char* const* methodNames, size_t methodCount, Vec3& outVec3) {
        if (!instance || !klass || !methodNames || methodCount == 0 || !gMonoApi.monoClassGetMethodFromName) {
            return false;
        }

        for (size_t i = 0; i < methodCount; ++i) {
            MonoMethod* method = gMonoApi.monoClassGetMethodFromName(klass, methodNames[i], 0);
            if (!method) {
                continue;
            }
            if (InvokeMethodVec3(method, instance, nullptr, outVec3)) {
                return true;
            }
        }

        return false;
    }

    MonoObject* TryInvokeObjectMethodByNames(void* instance, MonoClass* klass, const char* const* methodNames, size_t methodCount) {
        if (!instance || !klass || !methodNames || methodCount == 0 || !gMonoApi.monoClassGetMethodFromName) {
            return nullptr;
        }

        for (size_t i = 0; i < methodCount; ++i) {
            MonoMethod* method = gMonoApi.monoClassGetMethodFromName(klass, methodNames[i], 0);
            if (!method) {
                continue;
            }

            MonoObject* result = InvokeMethod(method, instance, nullptr);
            if (result) {
                return result;
            }
        }

        return nullptr;
    }

    bool TryInvokeFloatMethodByNames(void* instance, MonoClass* klass, const char* const* methodNames, size_t methodCount, float& outValue) {
        outValue = 0.0f;
        if (!instance || !klass || !methodNames || methodCount == 0 || !gMonoApi.monoClassGetMethodFromName || !gMonoApi.monoObjectUnbox) {
            return false;
        }

        for (size_t i = 0; i < methodCount; ++i) {
            MonoMethod* method = gMonoApi.monoClassGetMethodFromName(klass, methodNames[i], 0);
            if (!method) {
                continue;
            }

            MonoObject* boxed = InvokeMethod(method, instance, nullptr);
            if (!boxed) {
                continue;
            }

            void* raw = gMonoApi.monoObjectUnbox(boxed);
            if (!raw) {
                continue;
            }

            float value = *reinterpret_cast<float*>(raw);
            if (!std::isfinite(value)) {
                continue;
            }

            outValue = value;
            return true;
        }

        return false;
    }

    bool TryReadFloatFieldByNames(MonoObject* object, const char* const* fieldNames, size_t fieldCount, float& outValue) {
        outValue = 0.0f;
        if (!object || !fieldNames || fieldCount == 0 || !gMonoApi.monoObjectGetClass || !gMonoApi.monoFieldGetValue) {
            return false;
        }

        MonoClass* klass = gMonoApi.monoObjectGetClass(object);
        if (!klass) {
            return false;
        }

        MonoClassField* field = TryGetFieldByNames(klass, fieldNames, fieldCount);
        if (!field) {
            return false;
        }

        float value = 0.0f;
        gMonoApi.monoFieldGetValue(object, field, &value);
        if (!std::isfinite(value)) {
            return false;
        }

        outValue = value;
        return true;
    }

    bool TryGetCannonAimFromContextObject(void* aimingContextObject, Vec3& outOrigin, Vec3& outForward) {
        __try {
        if (!aimingContextObject || !gMonoApi.monoObjectGetClass || !gMonoApi.monoClassGetMethodFromName) {
            return false;
        }

        MonoObject* contextObject = reinterpret_cast<MonoObject*>(aimingContextObject);
        MonoClass* contextClass = gMonoApi.monoObjectGetClass(contextObject);
        if (!contextClass) {
            return false;
        }

        bool hasOrigin = false;
        bool hasForward = false;

        const char* aimerFieldNames[] = { "cannonAimer" };
        MonoClassField* aimerField = TryGetFieldByNames(contextClass, aimerFieldNames, 1);
        if (aimerField) {
            MonoObject* aimerObj = nullptr;
            gMonoApi.monoFieldGetValue(contextObject, aimerField, &aimerObj);
            
            if (aimerObj) {
                MonoClass* aimerClass = gMonoApi.monoObjectGetClass(aimerObj);

                const char* carriageNames[] = { "carriageTransform" };
                MonoClassField* carriageField = TryGetFieldByNames(aimerClass, carriageNames, 1);

                const char* barrelNames[] = { "barrelTransform" };
                MonoClassField* barrelField = TryGetFieldByNames(aimerClass, barrelNames, 1);
                
                if (carriageField && barrelField) {
                    MonoObject* carriageObj = nullptr;
                    gMonoApi.monoFieldGetValue(aimerObj, carriageField, &carriageObj);

                    MonoObject* barrelObj = nullptr;
                    gMonoApi.monoFieldGetValue(aimerObj, barrelField, &barrelObj);

                    if (carriageObj && barrelObj) {
                        MonoClass* transformClass = gMonoApi.monoObjectGetClass(barrelObj);
                        
                        const char* getPos[] = { "get_position" };
                        hasOrigin = TryInvokeVec3MethodByNames(barrelObj, transformClass, getPos, 1, outOrigin);
                        
                        const char* getFwd[] = { "get_forward" };
                        Vec3 barrelForward{};
                        Vec3 carriageForward{};
                        bool hasBarrel = TryInvokeVec3MethodByNames(barrelObj, transformClass, getFwd, 1, barrelForward);
                        bool hasCarriage = TryInvokeVec3MethodByNames(carriageObj, transformClass, getFwd, 1, carriageForward);
                        
                        if (hasOrigin && hasBarrel && hasCarriage) {
                            // The barrel forward gives us the proper vertical pitch, but if it doesn't inherit carriage yaw
                            // we need to rely on carriage for the X/Z yaw.
                            // However, if barrel IS a child of carriage, barrelForward is perfectly fine.
                            // Let's test if barrel has yaw. If its X/Z is zeroed, we combine them.
                            // Actually, just reading them both for safety.
                            
                            // Check if barrel has any horizontal direction. If it inherits from carriage, barrel forward will have X/Z.
                            outForward = barrelForward;
                            return true;
                        }
                    }
                }
            }
        }

        const char* originMethods[] = {
            "get_FireOriginPosition",
            "GetFireOriginPosition",
            "get_MuzzleWorldPosition",
            "GetMuzzleWorldPosition",
            "get_BarrelWorldPosition",
            "GetBarrelWorldPosition",
            "get_AimOriginPosition",
            "GetAimOriginPosition"
        };
        hasOrigin = TryInvokeVec3MethodByNames(aimingContextObject, contextClass, originMethods, std::size(originMethods), outOrigin);

        const char* forwardMethods[] = {
            "get_FireDirection",
            "GetFireDirection",
            "get_AimDirection",
            "GetAimDirection",
            "get_BarrelForward",
            "GetBarrelForward",
            "get_MuzzleForward",
            "GetMuzzleForward",
            "get_CurrentAimForward",
            "GetCurrentAimForward"
        };
        hasForward = TryInvokeVec3MethodByNames(aimingContextObject, contextClass, forwardMethods, std::size(forwardMethods), outForward);

        if (!hasForward) {
            float yawDegrees = 0.0f;
            float pitchDegrees = 0.0f;

            const char* yawMethodNames[] = {
                "get_AimYaw",
                "GetAimYaw",
                "get_CannonYaw",
                "GetCannonYaw",
                "get_CurrentYaw",
                "GetCurrentYaw",
                "get_HorizontalAngle",
                "GetHorizontalAngle",
                "get_BarrelYaw",
                "GetBarrelYaw",
                "get_Yaw",
                "GetYaw"
            };
            const char* pitchMethodNames[] = {
                "get_AimPitch",
                "GetAimPitch",
                "get_CannonPitch",
                "GetCannonPitch",
                "get_CurrentPitch",
                "GetCurrentPitch",
                "get_VerticalAngle",
                "GetVerticalAngle",
                "get_BarrelPitch",
                "GetBarrelPitch",
                "get_Pitch",
                "GetPitch"
            };

            bool hasYaw = TryInvokeFloatMethodByNames(aimingContextObject, contextClass, yawMethodNames, std::size(yawMethodNames), yawDegrees);
            bool hasPitch = TryInvokeFloatMethodByNames(aimingContextObject, contextClass, pitchMethodNames, std::size(pitchMethodNames), pitchDegrees);

            if (!hasYaw || !hasPitch) {
                const char* yawFieldNames[] = {
                    "aimYaw",
                    "AimYaw",
                    "cannonYaw",
                    "CannonYaw",
                    "currentYaw",
                    "CurrentYaw",
                    "horizontalAngle",
                    "HorizontalAngle",
                    "barrelYaw",
                    "BarrelYaw",
                    "yaw",
                    "Yaw",
                    "m_aimYaw",
                    "_aimYaw"
                };
                const char* pitchFieldNames[] = {
                    "aimPitch",
                    "AimPitch",
                    "cannonPitch",
                    "CannonPitch",
                    "currentPitch",
                    "CurrentPitch",
                    "verticalAngle",
                    "VerticalAngle",
                    "barrelPitch",
                    "BarrelPitch",
                    "pitch",
                    "Pitch",
                    "m_aimPitch",
                    "_aimPitch"
                };

                if (!hasYaw) {
                    hasYaw = TryReadFloatFieldByNames(contextObject, yawFieldNames, std::size(yawFieldNames), yawDegrees);
                }
                if (!hasPitch) {
                    hasPitch = TryReadFloatFieldByNames(contextObject, pitchFieldNames, std::size(pitchFieldNames), pitchDegrees);
                }
            }

            if (hasYaw && hasPitch) {
                const float yawRad = yawDegrees * (3.1415926535f / 180.0f);
                const float pitchRad = pitchDegrees * (3.1415926535f / 180.0f);
                const float cp = std::cos(pitchRad);
                outForward.x = std::sin(yawRad) * cp;
                outForward.y = -std::sin(pitchRad);
                outForward.z = std::cos(yawRad) * cp;
                hasForward = std::isfinite(outForward.x) && std::isfinite(outForward.y) && std::isfinite(outForward.z);
            }
        }

        const char* transformMethods[] = {
            "get_BarrelTransform",
            "GetBarrelTransform",
            "get_CannonBarrelTransform",
            "GetCannonBarrelTransform",
            "get_MuzzleTransform",
            "GetMuzzleTransform",
            "get_FireOriginTransform",
            "GetFireOriginTransform",
            "get_AimTransform",
            "GetAimTransform",
            "get_transform",
            "GetTransform"
        };

        MonoObject* aimTransform = TryInvokeObjectMethodByNames(aimingContextObject, contextClass, transformMethods, std::size(transformMethods));

        if (!aimTransform) {
            const char* transformFieldNames[] = {
                "barrelTransform",
                "BarrelTransform",
                "cannonBarrelTransform",
                "CannonBarrelTransform",
                "m_barrelTransform",
                "_barrelTransform",
                "m_cannonBarrelTransform",
                "_cannonBarrelTransform",
                "muzzleTransform",
                "MuzzleTransform",
                "m_muzzleTransform",
                "_muzzleTransform",
                "fireOriginTransform",
                "FireOriginTransform",
                "m_fireOriginTransform",
                "_fireOriginTransform"
            };

            aimTransform = TryReadObjectFieldByNames(contextObject, transformFieldNames, std::size(transformFieldNames));
        }

        if (aimTransform) {
            if (!hasOrigin && gTransformGetPositionMethod) {
                hasOrigin = InvokeMethodVec3(gTransformGetPositionMethod, aimTransform, nullptr, outOrigin);
            }

            if (!hasForward && EnsureTransformForwardMethod()) {
                hasForward = InvokeMethodVec3(gTransformGetForwardMethod, aimTransform, nullptr, outForward);
            }

            if (!hasForward) {
                if (!gTransformGetEulerAnglesMethod) {
                    MonoClass* transformClass = gMonoApi.monoObjectGetClass(aimTransform);
                    if (transformClass) {
                        gTransformGetEulerAnglesMethod = gMonoApi.monoClassGetMethodFromName(transformClass, "get_eulerAngles", 0);
                    }
                }

                Vec3 euler{};
                if (gTransformGetEulerAnglesMethod && InvokeMethodVec3(gTransformGetEulerAnglesMethod, aimTransform, nullptr, euler)) {
                    const float pitchRad = euler.x * (3.1415926535f / 180.0f);
                    const float yawRad = euler.y * (3.1415926535f / 180.0f);
                    const float cp = std::cos(pitchRad);
                    outForward.x = std::sin(yawRad) * cp;
                    outForward.y = -std::sin(pitchRad);
                    outForward.z = std::cos(yawRad) * cp;
                    hasForward = true;
                }
            }
        }

        if (!hasOrigin || !hasForward) {
            return false;
        }

        const float length = std::sqrt((outForward.x * outForward.x) + (outForward.y * outForward.y) + (outForward.z * outForward.z));
        if (!std::isfinite(length) || length < 0.001f) {
            return false;
        }

        outForward.x /= length;
        outForward.y /= length;
        outForward.z /= length;
        return std::isfinite(outOrigin.x) && std::isfinite(outOrigin.y) && std::isfinite(outOrigin.z);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    bool SolveFlatImpactPoint(const Vec3& origin, const Vec3& forward, float velocity, float gravity, float groundY, Vec3& outImpact) {
        if (!std::isfinite(velocity) || velocity < 1.0f) {
            return false;
        }

        gravity = std::clamp(gravity, 0.0f, 60.0f);

        Vec3 previous = origin;
        
        constexpr float kStepSeconds = 0.1f;
        constexpr int kMaxSegments = 400; 
        for (int i = 1; i <= kMaxSegments; ++i) {
            float t = i * kStepSeconds;
            Vec3 current{
                origin.x + forward.x * velocity * t,
                origin.y + (forward.y * velocity - gravity * t) * t,
                origin.z + forward.z * velocity * t
            };

            const bool crossedGround = ((previous.y >= groundY) && (current.y <= groundY)) ||
                ((previous.y <= groundY) && (current.y >= groundY));
            if (crossedGround) {
                const float denom = previous.y - current.y;
                const float blend = (std::fabs(denom) > 0.0001f) ? ((previous.y - groundY) / denom) : 0.0f;
                const float clampedBlend = std::clamp(blend, 0.0f, 1.0f);
                outImpact.x = previous.x + ((current.x - previous.x) * clampedBlend);
                outImpact.y = groundY;
                outImpact.z = previous.z + ((current.z - previous.z) * clampedBlend);
                return true;
            }
            previous = current;
        }

        return false;
    }

    bool TryGetLocalPlayerFeetY(void* manager, float& outFeetY) {
        outFeetY = 0.0f;
        if (!manager || !HnawOffsets::clientRoundPlayerManagerLocalPlayer || !HnawOffsets::roundPlayerPlayerTransform || !gTransformGetPositionMethod) {
            return false;
        }

        void* localRoundPlayer = nullptr;
        if (!SafeRead(reinterpret_cast<uintptr_t>(manager) + HnawOffsets::clientRoundPlayerManagerLocalPlayer, localRoundPlayer) || !localRoundPlayer) {
            return false;
        }

        void* localTransform = nullptr;
        if (!SafeRead(reinterpret_cast<uintptr_t>(localRoundPlayer) + HnawOffsets::roundPlayerPlayerTransform, localTransform) || !localTransform) {
            return false;
        }

        Vec3 localPos{};
        if (!InvokeMethodVec3(gTransformGetPositionMethod, localTransform, nullptr, localPos)) {
            return false;
        }

        if (!std::isfinite(localPos.y)) {
            return false;
        }

        outFeetY = localPos.y;
        return true;
    }

    void DrawCannonOverlay(ImDrawList* draw, const Vec3& localFeet, const std::vector<RadarPoint>& points, const Vec3* impactWorld, float camForwardX, float camForwardZ, bool hasCameraForward) {
        if (!draw || !gCannonMapEnabled) {
            return;
        }

        const ImGuiIO& io = ImGui::GetIO();
        const float panelSize = std::clamp(gCannonMapSizePx, 140.0f, 420.0f);
        const float rangeMeters = std::clamp(gCannonMapRangeMeters, 50.0f, 1200.0f);
        const float half = panelSize * 0.5f;

        if (!std::isfinite(gCannonMapPosY) || gCannonMapPosY <= 0.0f) {
            gCannonMapPosY = io.DisplaySize.y - panelSize - 48.0f;
        }

        gCannonMapPosX = std::clamp(gCannonMapPosX, 6.0f, io.DisplaySize.x - panelSize - 6.0f);
        gCannonMapPosY = std::clamp(gCannonMapPosY, 6.0f, io.DisplaySize.y - panelSize - 6.0f);

        if (GUI::bMenuOpen) {
            ImGui::SetNextWindowPos(ImVec2(gCannonMapPosX, gCannonMapPosY), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(panelSize, panelSize), ImGuiCond_Always);
            constexpr ImGuiWindowFlags kDragWindowFlags =
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoBackground |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoNav |
                ImGuiWindowFlags_NoFocusOnAppearing;
            ImGui::Begin("##CannonMapDragHandle", nullptr, kDragWindowFlags);
            ImGui::InvisibleButton("##CannonMapDragButton", ImVec2(panelSize, panelSize));
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
                const ImVec2 delta = ImGui::GetIO().MouseDelta;
                gCannonMapPosX += delta.x;
                gCannonMapPosY += delta.y;
                gCannonMapPosX = std::clamp(gCannonMapPosX, 6.0f, io.DisplaySize.x - panelSize - 6.0f);
                gCannonMapPosY = std::clamp(gCannonMapPosY, 6.0f, io.DisplaySize.y - panelSize - 6.0f);
            }
            ImGui::End();
        }

        const ImVec2 panelMin(gCannonMapPosX, gCannonMapPosY);
        const ImVec2 panelMax(panelMin.x + panelSize, panelMin.y + panelSize);
        const ImVec2 center(panelMin.x + half, panelMin.y + half);

        draw->AddRectFilled(panelMin, panelMax, IM_COL32(10, 14, 18, 190), 8.0f);
        draw->AddRect(panelMin, panelMax, IM_COL32(180, 200, 215, 180), 8.0f, 0, 1.2f);
        draw->AddCircle(center, half * 0.95f, IM_COL32(130, 150, 170, 100), 48, 1.0f);
        draw->AddLine(ImVec2(center.x - half, center.y), ImVec2(center.x + half, center.y), IM_COL32(140, 160, 180, 80), 1.0f);
        draw->AddLine(ImVec2(center.x, center.y - half), ImVec2(center.x, center.y + half), IM_COL32(140, 160, 180, 80), 1.0f);

        float headingX = 0.0f;
        float headingY = -1.0f;
        if (hasCameraForward) {
            const float horizontalLen = std::sqrt((camForwardX * camForwardX) + (camForwardZ * camForwardZ));
            if (std::isfinite(horizontalLen) && horizontalLen > 0.001f) {
                headingX = camForwardX / horizontalLen;
                headingY = -camForwardZ / horizontalLen;
            }
        }

        const float scale = half / rangeMeters;
        for (const RadarPoint& point : points) {
            float rx = point.dx * scale;
            float ry = -point.dz * scale;
            const float radial = std::sqrt((rx * rx) + (ry * ry));
            const float maxRadial = half - 7.0f;
            if (radial > maxRadial && radial > 0.001f) {
                const float clampScale = maxRadial / radial;
                rx *= clampScale;
                ry *= clampScale;
            }

            const ImVec2 marker(center.x + rx, center.y + ry);
            const ImU32 color = point.enemy
                ? (point.visible ? IM_COL32(255, 96, 78, 245) : IM_COL32(190, 80, 72, 210))
                : IM_COL32(95, 190, 255, 230);
            const float markerRadius = point.enemy ? 3.2f : 2.8f;
            draw->AddCircleFilled(marker, markerRadius, color, 14);
        }

        draw->AddCircleFilled(center, 3.2f, IM_COL32(248, 240, 188, 255), 16);
        const float perpX = -headingY;
        const float perpY = headingX;
        const ImVec2 arrowTip(center.x + (headingX * 18.0f), center.y + (headingY * 18.0f));
        const ImVec2 arrowBase(center.x - (headingX * 8.0f), center.y - (headingY * 8.0f));
        const ImVec2 arrowLeft(arrowBase.x + (perpX * 5.0f), arrowBase.y + (perpY * 5.0f));
        const ImVec2 arrowRight(arrowBase.x - (perpX * 5.0f), arrowBase.y - (perpY * 5.0f));
        draw->AddTriangleFilled(arrowTip, arrowLeft, arrowRight, IM_COL32(230, 235, 245, 240));
        draw->AddText(ImVec2(panelMin.x + 8.0f, panelMin.y + 6.0f), IM_COL32(230, 235, 240, 240), "Cannon Map");

        char rangeLabel[32]{};
        std::snprintf(rangeLabel, sizeof(rangeLabel), "%.0fm", rangeMeters);
        draw->AddText(ImVec2(panelMin.x + panelSize - 52.0f, panelMin.y + panelSize - 22.0f), IM_COL32(190, 205, 220, 220), rangeLabel);

        if (!impactWorld) {
            return;
        }

        if (!std::isfinite(impactWorld->x) || !std::isfinite(impactWorld->z) || !std::isfinite(localFeet.x) || !std::isfinite(localFeet.z)) {
            return;
        }

        const float impactDx = impactWorld->x - localFeet.x;
        const float impactDz = impactWorld->z - localFeet.z;
        float ix = impactDx * scale;
        float iy = -impactDz * scale;
        const float radial = std::sqrt((ix * ix) + (iy * iy));
        const float maxRadial = half - 6.0f;
        if (radial > maxRadial && radial > 0.001f) {
            const float clampScale = maxRadial / radial;
            ix *= clampScale;
            iy *= clampScale;
        }

        const ImVec2 impact(center.x + ix, center.y + iy);
        draw->AddCircle(impact, 8.0f, IM_COL32(255, 220, 100, 240), 24, 2.0f);
        draw->AddLine(ImVec2(impact.x - 10.0f, impact.y), ImVec2(impact.x + 10.0f, impact.y), IM_COL32(255, 220, 100, 210), 1.2f);
        draw->AddLine(ImVec2(impact.x, impact.y - 10.0f), ImVec2(impact.x, impact.y + 10.0f), IM_COL32(255, 220, 100, 210), 1.2f);
    }
}

void PlayerBoxes::UpdateCannonImpactPredictionFromGameThread(bool isAimingState, void* aimingContextObject) {
    if (!isAimingState) {
        if ((GetTickCount64() - gCachedCannonImpactUpdatedMs) > 500) {
            gCachedCannonImpactValid = false;
        }
        if ((GetTickCount64() - gLastCannonAimUpdatedMs) > 500) {
            gLastCannonAimValid = false;
        }
        if ((GetTickCount64() - gLastAimingContextUpdatedMs) > 900) {
            gLastAimingContextObject = nullptr;
        }
        return;
    }

    if (!EnsureMonoSymbols()) {
        return;
    }

    const uint64_t nowMs = GetTickCount64();
    if (aimingContextObject) {
        gLastAimingContextObject = aimingContextObject;
        gLastAimingContextUpdatedMs = nowMs;
    }
    if ((nowMs - gLastCannonPredictionUpdateMs) < 20) {
        return;
    }
    gLastCannonPredictionUpdateMs = nowMs;

    AttachMonoThread();

    Vec3 origin{};
    Vec3 forward{};
    bool hasAim = TryGetCannonAimFromContextObject(aimingContextObject, origin, forward);

    Vec3 cameraOrigin{};
    Vec3 cameraForward{};
    bool hasCameraAim = false;
    MonoObject* camera = InvokeMethod(gCameraGetMainMethod, nullptr, nullptr);
    if (camera) {
        hasCameraAim = TryGetCameraOriginForward(camera, cameraOrigin, cameraForward);
    }

    if (!hasAim && hasCameraAim) {
        origin = cameraOrigin;
        forward = cameraForward;
        hasAim = true;
    } else if (hasAim && hasCameraAim) {
        // Keep the true physical cannon forward from the game state
        // Camera doesn't represent the true orientation of mounted artillery
    }

    if (!hasAim) {
        return;
    }

    gLastCannonAimOrigin = origin;
    gLastCannonAimForward = forward;
    gLastCannonAimValid = true;
    gLastCannonAimUpdatedMs = nowMs;

    void* manager = GetRoundPlayerManagerInstance();
    int weaponType = -1;
    if (manager && HnawOffsets::clientRoundPlayerManagerLocalPlayer && HnawOffsets::roundPlayerWeaponHolder) {
        void* localRoundPlayer = nullptr;
        if (SafeRead(reinterpret_cast<uintptr_t>(manager) + HnawOffsets::clientRoundPlayerManagerLocalPlayer, localRoundPlayer) && localRoundPlayer) {
            void* weaponHolder = nullptr;
            if (SafeRead(reinterpret_cast<uintptr_t>(localRoundPlayer) + HnawOffsets::roundPlayerWeaponHolder, weaponHolder) && weaponHolder) {
                TryGetWeaponTypeFromHolder(weaponHolder, weaponType);
            }
        }
    }

    float velocity = 0.0f;
    float gravity = 0.0f;
    ResolveCannonBallisticsFromWeaponType(weaponType, velocity, gravity);

    constexpr float kMuzzleStartOffset = 2.0f;
    Vec3 pos{
        origin.x + (forward.x * kMuzzleStartOffset),
        origin.y + (forward.y * kMuzzleStartOffset),
        origin.z + (forward.z * kMuzzleStartOffset)
    };

    Vec3 previous = pos;
    constexpr float kStepSeconds = 0.1f;
    constexpr int kMaxSegments = 400;
    for (int i = 1; i <= kMaxSegments; ++i) {
        float t = i * kStepSeconds;
        Vec3 next{
            pos.x + forward.x * velocity * t,
            pos.y + (forward.y * velocity - gravity * t) * t,
            pos.z + forward.z * velocity * t
        };

        if (i >= 2) {
            Vec3 refined{};
            if (RefineImpactPoint(previous, next, refined)) {
                gCachedCannonImpactPoint = refined;
                gCachedCannonImpactValid = true;
                gCachedCannonImpactUpdatedMs = nowMs;
                return;
            }
        }

        previous = next;

        if (!std::isfinite(previous.x) || !std::isfinite(previous.y) || !std::isfinite(previous.z)) {
            break;
        }

        if (std::fabs(previous.x - pos.x) > 4000.0f || std::fabs(previous.z - pos.z) > 4000.0f || previous.y < (pos.y - 1500.0f)) {
            break;
        }
    }

    float localFeetY = origin.y;
    TryGetLocalPlayerFeetY(manager, localFeetY);

    Vec3 fallback{};
    if (SolveFlatImpactPoint(pos, forward, velocity, gravity, localFeetY, fallback)) {
        gCachedCannonImpactPoint = fallback;
        gCachedCannonImpactValid = true;
        gCachedCannonImpactUpdatedMs = nowMs;
        return;
    }

    const float horizontalSpeed = std::sqrt((forward.x * forward.x) + (forward.z * forward.z)) * velocity;
    float fallbackDistance = std::clamp(gCannonMapRangeMeters * 0.9f, 40.0f, 1800.0f);
    if (horizontalSpeed > 0.01f && gravity > 0.01f) {
        const float dy = pos.y - localFeetY;
        const float vy = forward.y * velocity;
        const float disc = (vy * vy) + (4.0f * gravity * dy);
        if (disc >= 0.0f) {
            const float t = (vy + std::sqrt(disc)) / (2.0f * gravity);
            if (std::isfinite(t) && t > 0.0f) {
                fallbackDistance = std::clamp(horizontalSpeed * t, 15.0f, 1800.0f);
            }
        }
    }

    fallback.x = pos.x + (forward.x * fallbackDistance);
    fallback.z = pos.z + (forward.z * fallbackDistance);
    fallback.y = localFeetY;

    gCachedCannonImpactPoint = fallback;
    gCachedCannonImpactValid = true;
    gCachedCannonImpactUpdatedMs = nowMs;
}

void PlayerBoxes::UpdateVisibilityCacheFromGameThread() {
    if (!EnsureMonoSymbols() || !HnawOffsets::roundPlayerPlayerTransform || !gPhysicsLinecastMethod) {
        return;
    }

    const uint64_t nowMs = GetTickCount64();
    if ((nowMs - gLastVisibilityUpdateMs) < 33) {
        return;
    }
    gLastVisibilityUpdateMs = nowMs;

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
        return;
    }

    void* items = nullptr;
    int size = 0;
    std::vector<void*> methodElements;
    const bool useRawCollection = TryReadCollection(playerListObject, items, size);
    const bool useMethodCollection = !useRawCollection && TryEnumerateCollectionByMethods(playerListObject, methodElements);
    if (!useRawCollection && !useMethodCollection) {
        return;
    }

    void* localRoundPlayer = nullptr;
    if (manager && HnawOffsets::clientRoundPlayerManagerLocalPlayer) {
        SafeRead(reinterpret_cast<uintptr_t>(manager) + HnawOffsets::clientRoundPlayerManagerLocalPlayer, localRoundPlayer);
    }
    if (!localRoundPlayer) {
        return;
    }

    void* localTransform = nullptr;
    if (!SafeRead(reinterpret_cast<uintptr_t>(localRoundPlayer) + HnawOffsets::roundPlayerPlayerTransform, localTransform) || !localTransform) {
        return;
    }

    Vec3 localFeet{};
    if (!InvokeMethodVec3(gTransformGetPositionMethod, localTransform, nullptr, localFeet)) {
        return;
    }

    Vec3 localEye = localFeet;
    localEye.y += 1.62f;

    const int safeCount = useRawCollection
        ? std::clamp(size, 0, 256)
        : std::clamp(static_cast<int>(methodElements.size()), 0, 256);

    for (int i = 0; i < safeCount; ++i) {
        void* roundPlayer = useRawCollection
            ? GetManagedArrayElement(items, i)
            : methodElements[static_cast<size_t>(i)];
        if (!roundPlayer || roundPlayer == localRoundPlayer) {
            continue;
        }

        int networkId = -1;
        if (!TryGetNetworkId(roundPlayer, networkId)) {
            continue;
        }

        void* transformObject = nullptr;
        if (!SafeRead(reinterpret_cast<uintptr_t>(roundPlayer) + HnawOffsets::roundPlayerPlayerTransform, transformObject) || !transformObject) {
            continue;
        }

        Vec3 feet{};
        if (!InvokeMethodVec3(gTransformGetPositionMethod, transformObject, nullptr, feet) || !std::isfinite(feet.x) || !std::isfinite(feet.y) || !std::isfinite(feet.z)) {
            continue;
        }

        Vec3 head = feet;
        head.y += 1.78f;

        const bool visible = HasLineOfSight(localEye, head);
        gVisibilityCache[networkId] = VisibilityEntry{ visible, nowMs };
    }

    for (auto it = gVisibilityCache.begin(); it != gVisibilityCache.end();) {
        if ((nowMs - it->second.updatedMs) > 2000) {
            it = gVisibilityCache.erase(it);
        } else {
            ++it;
        }
    }
}

bool PlayerBoxes::IsRoundPlayerVisibleCached(void* roundPlayer) {
    if (!roundPlayer || !gPhysicsLinecastMethod) {
        return true;
    }

    int networkId = -1;
    if (!TryGetNetworkId(roundPlayer, networkId)) {
        return true;
    }

    const uint64_t nowMs = GetTickCount64();
    const auto it = gVisibilityCache.find(networkId);
    if (it == gVisibilityCache.end()) {
        return true;
    }

    if ((nowMs - it->second.updatedMs) > 1000) {
        return true;
    }

    return it->second.visible;
}

void PlayerBoxes::Render() {
    gLastPlayersSeen = 0;
    gLastProjected = 0;
    gLastDrawn = 0;
    gTrueBonesSuccess = 0;
    gTrueBonesActorInitFail = 0;
    gTrueBonesModelFail = 0;
    gTrueBonesTransformFail = 0;
    gChamsModelsResolved = 0;
    gChamsRenderableItemsResolved = 0;
    gChamsClientItemsResolved = 0;
    gChamsRenderersResolved = 0;
    gChamsMaterialsResolved = 0;
    gChamsColorCalls = 0;
    gChamsAttemptedPlayers = 0;
    int trueBonePlayersDrawn = 0;

    const bool anyInfoEnabled = gShowName || gShowDistance || gShowHealth || gShowNetworkId || gShowClassRank || gShowFaction;
    const bool anyEspEnabled = gEnabled || gSkeletonEnabled || gChamsEnabled || gHealthBarEnabled || anyInfoEnabled || gCannonMapEnabled;
    if (!anyEspEnabled) {
        gLastStatus = "disabled";
        return;
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
        gLastStatus = manager ? "GetAllRoundPlayers returned null (manager found)" : "manager null";
        return;
    }

    void* items = nullptr;
    int size = 0;
    std::vector<void*> methodElements;
    const bool useRawCollection = TryReadCollection(playerListObject, items, size);
    const bool useMethodCollection = !useRawCollection && TryEnumerateCollectionByMethods(playerListObject, methodElements);

    if (!useRawCollection && !useMethodCollection) {
        gLastStatus = "unsupported player collection";
        return;
    }

    const int safeCount = useRawCollection
        ? std::clamp(size, 0, 256)
        : std::clamp(static_cast<int>(methodElements.size()), 0, 256);
    gLastPlayersSeen = safeCount;
    MonoObject* camera = InvokeMethod(gCameraGetMainMethod, nullptr, nullptr);
    if (!camera) {
        gLastStatus = "camera null";
        return;
    }

    Vec3 earlyCameraOrigin{};
    Vec3 earlyCameraForward{};
    const bool hasEarlyCameraPose = TryGetCameraOriginForward(camera, earlyCameraOrigin, earlyCameraForward);

    Vec3 localFeet{};
    bool hasLocalFeet = false;
    bool cannonContextActive = false;
    int localFactionValue = -1;
    int localSquadValue = -1;
    if (manager && HnawOffsets::clientRoundPlayerManagerLocalPlayer && HnawOffsets::roundPlayerPlayerTransform) {
        void* localRoundPlayer = nullptr;
        if (SafeRead(reinterpret_cast<uintptr_t>(manager) + HnawOffsets::clientRoundPlayerManagerLocalPlayer, localRoundPlayer) && localRoundPlayer) {
            void* localTransformObject = nullptr;
            if (SafeRead(reinterpret_cast<uintptr_t>(localRoundPlayer) + HnawOffsets::roundPlayerPlayerTransform, localTransformObject) && localTransformObject) {
                hasLocalFeet = InvokeMethodVec3(gTransformGetPositionMethod, localTransformObject, nullptr, localFeet);
            }

            cannonContextActive = IsLocalPlayerInAnyCannonContext(localRoundPlayer);
            if (cannonContextActive) {
                gLastCannonContextMs = GetTickCount64();
            }

            if (HnawOffsets::roundPlayerPlayerBase) {
                void* localPlayerBase = nullptr;
                if (SafeRead(reinterpret_cast<uintptr_t>(localRoundPlayer) + HnawOffsets::roundPlayerPlayerBase, localPlayerBase) && localPlayerBase) {
                    if (HnawOffsets::playerBasePlayerStartData) {
                        void* localSpawnData = nullptr;
                        if (SafeRead(reinterpret_cast<uintptr_t>(localPlayerBase) + HnawOffsets::playerBasePlayerStartData, localSpawnData) && localSpawnData) {
                            if (HnawOffsets::playerSpawnDataFaction) {
                                int rawFaction = 0;
                                if (SafeRead(reinterpret_cast<uintptr_t>(localSpawnData) + HnawOffsets::playerSpawnDataFaction, rawFaction)) {
                                    localFactionValue = rawFaction;
                                }
                            }

                            if (HnawOffsets::playerSpawnDataSquadID) {
                                int rawSquad = 0;
                                if (SafeRead(reinterpret_cast<uintptr_t>(localSpawnData) + HnawOffsets::playerSpawnDataSquadID, rawSquad)) {
                                    localSquadValue = rawSquad;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (Hook::IsArtilleryAimingContextActive()) {
        cannonContextActive = true;
        gLastCannonContextMs = GetTickCount64();
    }

    if (!hasLocalFeet && hasEarlyCameraPose) {
        localFeet = earlyCameraOrigin;
        hasLocalFeet = true;
    }

    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const uint64_t nowMs = GetTickCount64();
    const bool cannonContextSticky = cannonContextActive || ((nowMs - gLastCannonContextMs) < 1800);
    const bool shouldRenderCannonMap = gCannonMapEnabled && hasLocalFeet && (!gCannonMapRequireContext || cannonContextSticky);
    std::vector<RadarPoint> radarPoints;
    if (shouldRenderCannonMap) {
        radarPoints.reserve(static_cast<size_t>(safeCount));
    }

    for (int i = 0; i < safeCount; ++i) {
        void* roundPlayer = useRawCollection
            ? GetManagedArrayElement(items, i)
            : methodElements[static_cast<size_t>(i)];
        if (!roundPlayer) {
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

        if (std::fabs(feet.x) < 0.001f && std::fabs(feet.y) < 0.001f && std::fabs(feet.z) < 0.001f) {
            continue;
        }

        float distanceToLocalMeters = -1.0f;
        bool inBoxRange = true;
        bool inSkeletonRange = true;
        bool inInfoRange = true;
        bool inChamsRange = true;
        if (hasLocalFeet) {
            const float dx = feet.x - localFeet.x;
            const float dy = feet.y - localFeet.y;
            const float dz = feet.z - localFeet.z;
            distanceToLocalMeters = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));

            if (gPerFeatureDistanceLimitsEnabled && std::isfinite(distanceToLocalMeters)) {
                inBoxRange = (distanceToLocalMeters <= gMaxBoxDistanceMeters);
                inSkeletonRange = (distanceToLocalMeters <= gMaxSkeletonDistanceMeters);
                inInfoRange = (distanceToLocalMeters <= gMaxInfoDistanceMeters);
                inChamsRange = (distanceToLocalMeters <= gMaxChamsDistanceMeters);
            }
        }

        int healthValue = -1;
        int networkIdValue = -1;
        int classRankValue = -1;
        int factionValue = -1;
        int squadValue = -1;
        void* playerBase = nullptr;
        void* spawnData = nullptr;

        if (HnawOffsets::roundPlayerPlayerBase) {
            if (SafeRead(reinterpret_cast<uintptr_t>(roundPlayer) + HnawOffsets::roundPlayerPlayerBase, playerBase) && playerBase) {
                if (HnawOffsets::playerBaseHealth) {
                    float rawHealth = 0.0f;
                    if (SafeRead(reinterpret_cast<uintptr_t>(playerBase) + HnawOffsets::playerBaseHealth, rawHealth)) {
                        if (std::isfinite(rawHealth) && rawHealth >= 0.0f && rawHealth <= 10000.0f) {
                            healthValue = static_cast<int>(std::lround(rawHealth));
                        }
                    }
                }

                if (HnawOffsets::playerBasePlayerStartData) {
                    if (SafeRead(reinterpret_cast<uintptr_t>(playerBase) + HnawOffsets::playerBasePlayerStartData, spawnData) && spawnData) {
                        if (HnawOffsets::playerSpawnDataClassRank) {
                            int rawClassRank = 0;
                            if (SafeRead(reinterpret_cast<uintptr_t>(spawnData) + HnawOffsets::playerSpawnDataClassRank, rawClassRank)) {
                                classRankValue = rawClassRank;
                            }
                        }

                        if (HnawOffsets::playerSpawnDataFaction) {
                            int rawFaction = 0;
                            if (SafeRead(reinterpret_cast<uintptr_t>(spawnData) + HnawOffsets::playerSpawnDataFaction, rawFaction)) {
                                factionValue = rawFaction;
                            }
                        }

                        if (HnawOffsets::playerSpawnDataSquadID) {
                            int rawSquad = 0;
                            if (SafeRead(reinterpret_cast<uintptr_t>(spawnData) + HnawOffsets::playerSpawnDataSquadID, rawSquad)) {
                                squadValue = rawSquad;
                            }
                        }
                    }
                }
            }
        }

        if (HnawOffsets::roundPlayerNetworkPlayerID) {
            int rawNetworkId = 0;
            if (SafeRead(reinterpret_cast<uintptr_t>(roundPlayer) + HnawOffsets::roundPlayerNetworkPlayerID, rawNetworkId)) {
                networkIdValue = rawNetworkId;
            }
        }

        bool hasTeamData = false;
        bool isTeammate = false;
        if (localFactionValue >= 0 && factionValue >= 0) {
            hasTeamData = true;
            isTeammate = (localFactionValue == factionValue);
        } else if (localSquadValue >= 0 && squadValue >= 0) {
            hasTeamData = true;
            isTeammate = (localSquadValue == squadValue);
        }

        if (gTeamFilterMode == 1 && hasTeamData && !isTeammate) {
            continue;
        }
        if (gTeamFilterMode == 2 && hasTeamData && isTeammate) {
            continue;
        }

        if (healthValue == 0) {
            continue;
        }

        if (shouldRenderCannonMap && std::isfinite(distanceToLocalMeters) && distanceToLocalMeters >= 0.0f && distanceToLocalMeters <= gCannonMapRangeMeters) {
            const bool shouldPlot = !hasTeamData || !isTeammate || gCannonMapShowTeammates;
            if (shouldPlot) {
                const bool visible = PlayerBoxes::IsRoundPlayerVisibleCached(roundPlayer);
                radarPoints.push_back(RadarPoint{
                    feet.x - localFeet.x,
                    feet.z - localFeet.z,
                    !hasTeamData || !isTeammate,
                    visible
                });
            }
        }

        float* activeColor = gEnemyColorRgb;
        if (gUseTeamColors && hasTeamData) {
            activeColor = isTeammate ? gTeamColorRgb : gEnemyColorRgb;
        }

        const bool wantsBox = (gEnabled || gHealthBarEnabled) && inBoxRange;
        const bool wantsSkeleton = gSkeletonEnabled && inSkeletonRange;
        const bool wantsInfo = anyInfoEnabled && inInfoRange;
        const bool wantsChams = gChamsEnabled && gChamsAlpha > 0.0f && inChamsRange;
        if (!wantsBox && !wantsSkeleton && !wantsInfo && !wantsChams) {
            continue;
        }

        Vec3 head = feet;
        head.y += 1.78f;

        if (gVisibilityMode == 1) {
            if (!hasLocalFeet) {
                continue;
            }

            if (!PlayerBoxes::IsRoundPlayerVisibleCached(roundPlayer)) {
                continue;
            }
        }

        if (wantsChams) {
            ++gChamsAttemptedPlayers;
            const Color4 modelChamsColor{ activeColor[0], activeColor[1], activeColor[2], gChamsAlpha };
            ApplyPlayerModelChams(roundPlayer, playerBase, spawnData, modelChamsColor);
        }

        const bool wants2d = wantsBox || wantsSkeleton || wantsInfo;
        if (!wants2d) {
            ++gLastDrawn;
            continue;
        }

        ImVec2 feetScreen{};
        ImVec2 headScreen{};
        if (!WorldToScreen(camera, feet, feetScreen) || !WorldToScreen(camera, head, headScreen)) {
            continue;
        }
        ++gLastProjected;

        const float height = feetScreen.y - headScreen.y;
        if (height < 6.0f || height > 1500.0f) {
            continue;
        }

        const float width = height * 0.5f;
        const ImVec2 topLeft(headScreen.x - (width * 0.5f), headScreen.y);
        const ImVec2 bottomRight(headScreen.x + (width * 0.5f), feetScreen.y);

        const ImU32 outlineColor = ToColor32(activeColor, 1.0f);

        if (gHealthBarEnabled && inBoxRange && healthValue >= 0) {
            const float hp01 = std::clamp(static_cast<float>(healthValue) / 100.0f, 0.0f, 1.0f);
            const float barWidth = 4.0f;
            const float barPad = 3.0f;
            const ImVec2 barMin(topLeft.x - barPad - barWidth, topLeft.y);
            const ImVec2 barMax(topLeft.x - barPad, bottomRight.y);

            draw->AddRectFilled(barMin, barMax, IM_COL32(0, 0, 0, 170));

            const float fillTop = barMax.y - ((barMax.y - barMin.y) * hp01);
            const int hpR = static_cast<int>((1.0f - hp01) * 255.0f);
            const int hpG = static_cast<int>(hp01 * 255.0f);
            draw->AddRectFilled(ImVec2(barMin.x + 1.0f, fillTop), ImVec2(barMax.x - 1.0f, barMax.y - 1.0f), IM_COL32(hpR, hpG, 40, 235));
            draw->AddRect(barMin, barMax, IM_COL32(20, 20, 20, 230));
        }

        if (gEnabled && inBoxRange) {
            if (gFilled && gFillAlpha > 0.0f) {
                draw->AddRectFilled(topLeft, bottomRight, ToColor32(activeColor, gFillAlpha));
            }

            if (gCornerMode) {
                DrawCornerBox(draw, topLeft, bottomRight, outlineColor, gThickness);
            } else {
                draw->AddRect(topLeft, bottomRight, outlineColor, 0.0f, 0, gThickness);
            }
        }

        if (wantsSkeleton) {
            const ImU32 skeletonColor = ToColor32(activeColor, 1.0f);
            bool usedTrueBones = false;

            std::vector<Vec3> boneWorldPositions;
            if (TryGetBonePositions(roundPlayer, playerBase, spawnData, boneWorldPositions) && boneWorldPositions.size() >= 8) {
                std::vector<ImVec2> boneScreenPositions(boneWorldPositions.size());
                std::vector<bool> boneVisible(boneWorldPositions.size(), false);
                for (size_t boneIndex = 0; boneIndex < boneWorldPositions.size(); ++boneIndex) {
                    const Vec3& bone = boneWorldPositions[boneIndex];
                    if (std::fabs(bone.x) < 0.001f && std::fabs(bone.y) < 0.001f && std::fabs(bone.z) < 0.001f) {
                        boneVisible[boneIndex] = false;
                        continue;
                    }

                    const float bdx = bone.x - feet.x;
                    const float bdy = bone.y - feet.y;
                    const float bdz = bone.z - feet.z;
                    const float boneDistSq = (bdx * bdx) + (bdy * bdy) + (bdz * bdz);
                    if (boneDistSq > (12.0f * 12.0f)) {
                        boneVisible[boneIndex] = false;
                        continue;
                    }

                    boneVisible[boneIndex] = WorldToScreen(camera, bone, boneScreenPositions[boneIndex]);
                }

                int drawnBoneLinks = 0;
                int drawnLegLinks = 0;
                auto drawLinks = [&](const int (*links)[2], size_t linkCount) {
                    for (size_t linkIndex = 0; linkIndex < linkCount; ++linkIndex) {
                        const int a = links[linkIndex][0];
                        const int b = links[linkIndex][1];
                        if (a < 0 || b < 0 || a >= static_cast<int>(boneWorldPositions.size()) || b >= static_cast<int>(boneWorldPositions.size())) {
                            continue;
                        }
                        if (!boneVisible[static_cast<size_t>(a)] || !boneVisible[static_cast<size_t>(b)]) {
                            continue;
                        }

                        draw->AddLine(boneScreenPositions[static_cast<size_t>(a)], boneScreenPositions[static_cast<size_t>(b)], skeletonColor, gSkeletonThickness);
                        ++drawnBoneLinks;
                    }
                };

                if (boneWorldPositions.size() >= 20) {
                    constexpr int kFullBodyLinks[][2] = {
                        { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 4 }, { 4, 5 },
                        { 2, 6 }, { 6, 7 }, { 7, 8 }, { 8, 9 },
                        { 2, 10 }, { 10, 11 }, { 11, 12 }, { 12, 13 },
                        { 0, 14 }, { 14, 15 }, { 15, 16 },
                        { 0, 17 }, { 17, 18 }, { 18, 19 }
                    };
                    drawLinks(kFullBodyLinks, std::size(kFullBodyLinks));

                    constexpr int kLegLinks[][2] = {
                        { 0, 14 }, { 14, 15 }, { 15, 16 },
                        { 0, 17 }, { 17, 18 }, { 18, 19 }
                    };
                    for (const auto& link : kLegLinks) {
                        const int a = link[0];
                        const int b = link[1];
                        if (a < 0 || b < 0 || a >= static_cast<int>(boneWorldPositions.size()) || b >= static_cast<int>(boneWorldPositions.size())) {
                            continue;
                        }
                        if (boneVisible[static_cast<size_t>(a)] && boneVisible[static_cast<size_t>(b)]) {
                            ++drawnLegLinks;
                        }
                    }
                } else {
                    constexpr int kTorsoLinks[][2] = {
                        { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 4 }, { 4, 5 },
                        { 3, 6 }, { 3, 7 }
                    };
                    drawLinks(kTorsoLinks, std::size(kTorsoLinks));
                }

                if (boneVisible.size() > 5 && boneVisible[5]) {
                    draw->AddCircle(boneScreenPositions[5], 4.0f, skeletonColor, 12, gSkeletonThickness);
                }

                if (drawnLegLinks == 0) {
                    const float boxW = bottomRight.x - topLeft.x;
                    const float boxH = bottomRight.y - topLeft.y;
                    const float cx = topLeft.x + (boxW * 0.5f);

                    ImVec2 pelvis(cx, topLeft.y + (boxH * 0.58f));
                    if (!boneScreenPositions.empty() && boneVisible[0]) {
                        pelvis = boneScreenPositions[0];
                    }

                    const ImVec2 lKnee(pelvis.x - (boxW * 0.14f), topLeft.y + (boxH * 0.78f));
                    const ImVec2 lFoot(pelvis.x - (boxW * 0.14f), topLeft.y + (boxH * 0.98f));
                    const ImVec2 rKnee(pelvis.x + (boxW * 0.14f), topLeft.y + (boxH * 0.78f));
                    const ImVec2 rFoot(pelvis.x + (boxW * 0.14f), topLeft.y + (boxH * 0.98f));

                    draw->AddLine(pelvis, lKnee, skeletonColor, gSkeletonThickness);
                    draw->AddLine(lKnee, lFoot, skeletonColor, gSkeletonThickness);
                    draw->AddLine(pelvis, rKnee, skeletonColor, gSkeletonThickness);
                    draw->AddLine(rKnee, rFoot, skeletonColor, gSkeletonThickness);
                }

                usedTrueBones = (drawnBoneLinks >= 4);
            }

            if (usedTrueBones) {
                ++trueBonePlayersDrawn;
            }

            if (!usedTrueBones) {
                const float boxW = bottomRight.x - topLeft.x;
                const float boxH = bottomRight.y - topLeft.y;
                const float cx = topLeft.x + (boxW * 0.5f);
                const float yTop = topLeft.y;

                const ImVec2 head(cx, yTop + (boxH * 0.10f));
                const ImVec2 neck(cx, yTop + (boxH * 0.20f));
                const ImVec2 pelvis(cx, yTop + (boxH * 0.58f));

                const ImVec2 lShoulder(cx - (boxW * 0.18f), yTop + (boxH * 0.24f));
                const ImVec2 lElbow(cx - (boxW * 0.28f), yTop + (boxH * 0.38f));
                const ImVec2 lHand(cx - (boxW * 0.34f), yTop + (boxH * 0.53f));

                const ImVec2 rShoulder(cx + (boxW * 0.18f), yTop + (boxH * 0.24f));
                const ImVec2 rElbow(cx + (boxW * 0.28f), yTop + (boxH * 0.38f));
                const ImVec2 rHand(cx + (boxW * 0.34f), yTop + (boxH * 0.53f));

                const ImVec2 lKnee(cx - (boxW * 0.16f), yTop + (boxH * 0.78f));
                const ImVec2 lFoot(cx - (boxW * 0.16f), yTop + (boxH * 0.98f));
                const ImVec2 rKnee(cx + (boxW * 0.16f), yTop + (boxH * 0.78f));
                const ImVec2 rFoot(cx + (boxW * 0.16f), yTop + (boxH * 0.98f));

                draw->AddLine(head, neck, skeletonColor, gSkeletonThickness);
                draw->AddLine(neck, pelvis, skeletonColor, gSkeletonThickness);

                draw->AddLine(neck, lShoulder, skeletonColor, gSkeletonThickness);
                draw->AddLine(lShoulder, lElbow, skeletonColor, gSkeletonThickness);
                draw->AddLine(lElbow, lHand, skeletonColor, gSkeletonThickness);

                draw->AddLine(neck, rShoulder, skeletonColor, gSkeletonThickness);
                draw->AddLine(rShoulder, rElbow, skeletonColor, gSkeletonThickness);
                draw->AddLine(rElbow, rHand, skeletonColor, gSkeletonThickness);

                draw->AddLine(pelvis, lKnee, skeletonColor, gSkeletonThickness);
                draw->AddLine(lKnee, lFoot, skeletonColor, gSkeletonThickness);

                draw->AddLine(pelvis, rKnee, skeletonColor, gSkeletonThickness);
                draw->AddLine(rKnee, rFoot, skeletonColor, gSkeletonThickness);
            }
        }

        std::vector<std::string> infoLines;
        if (wantsInfo) {
            auto appendInfo = [&](const std::string& part) {
                if (!part.empty()) {
                    infoLines.push_back(part);
                }
            };

            if (gShowName) {
                if (networkIdValue >= 0) {
                    appendInfo(std::string("P") + std::to_string(networkIdValue));
                } else {
                    appendInfo("Player");
                }
            }

            if (gShowDistance && hasLocalFeet && std::isfinite(distanceToLocalMeters) && distanceToLocalMeters >= 0.0f) {
                char distanceBuffer[32]{};
                std::snprintf(distanceBuffer, sizeof(distanceBuffer), "%.1fm", distanceToLocalMeters);
                appendInfo(distanceBuffer);
            }

            if (gShowHealth && healthValue >= 0) {
                appendInfo(std::string("HP ") + std::to_string(healthValue));
            }

            if (gShowNetworkId && networkIdValue >= 0) {
                appendInfo(std::string("ID ") + std::to_string(networkIdValue));
            }

            if (gShowClassRank && classRankValue >= 0) {
                appendInfo(std::string("R ") + std::to_string(classRankValue));
            }

            if (gShowFaction && factionValue >= 0) {
                appendInfo(std::string("F ") + std::to_string(factionValue));
            }
        }

        if (wantsInfo && !infoLines.empty()) {
            const float lineHeight = ImGui::GetTextLineHeight();
            float maxTextWidth = 0.0f;
            for (const std::string& line : infoLines) {
                maxTextWidth = (std::max)(maxTextWidth, ImGui::CalcTextSize(line.c_str()).x);
            }

            ImVec2 textBase{};
            switch (gInfoPosition) {
                case 0:
                    textBase = ImVec2(topLeft.x - maxTextWidth - 6.0f, topLeft.y);
                    break;
                case 2:
                    textBase = ImVec2(topLeft.x + ((bottomRight.x - topLeft.x) - maxTextWidth) * 0.5f, topLeft.y - (lineHeight * static_cast<float>(infoLines.size())) - 2.0f);
                    break;
                case 3:
                    textBase = ImVec2(topLeft.x + ((bottomRight.x - topLeft.x) - maxTextWidth) * 0.5f, bottomRight.y + 2.0f);
                    break;
                case 1:
                default:
                    textBase = ImVec2(bottomRight.x + 6.0f, topLeft.y);
                    break;
            }

            for (size_t lineIndex = 0; lineIndex < infoLines.size(); ++lineIndex) {
                const ImVec2 textPos(textBase.x, textBase.y + (lineHeight * static_cast<float>(lineIndex)));
                draw->AddText(textPos, IM_COL32(245, 245, 245, 255), infoLines[lineIndex].c_str());
            }
        }
        ++gLastDrawn;
    }

    if (shouldRenderCannonMap) {
        Vec3 impact{};
        Vec3 origin{};
        Vec3 shotForward{};
        bool hasShotForward = false;

        Vec3 cameraOrigin{};
        Vec3 cameraForward{};
        const bool hasCameraShotForward = TryGetCameraOriginForward(camera, cameraOrigin, cameraForward);

        if (cannonContextSticky && gLastCannonAimValid && (nowMs - gLastCannonAimUpdatedMs) <= 700) {
            origin = gLastCannonAimOrigin;
            shotForward = gLastCannonAimForward;
            hasShotForward = true;
        } else if (hasCameraShotForward) {
            origin = cameraOrigin;
            shotForward = cameraForward;
            hasShotForward = true;
        } else if (hasEarlyCameraPose) {
            origin = earlyCameraOrigin;
            shotForward = earlyCameraForward;
            hasShotForward = true;
        }

        if (hasShotForward) {
            gLastAimForward = shotForward;
            gLastAimForwardValid = true;
        } else if (gLastAimForwardValid) {
            shotForward = gLastAimForward;
            hasShotForward = true;
        }

        Vec3 headingForward{};
        bool hasHeadingForward = false;
        bool hasHeadingFromImpact = false;

        if (cannonContextSticky && gLastCannonAimValid && (nowMs - gLastCannonAimUpdatedMs) <= 700) {
            headingForward = gLastCannonAimForward;
            // Flatten the heading to stay parallel to ground for minimap rotation
            headingForward.y = 0.0f;
            const float hlen2 = std::sqrt((headingForward.x * headingForward.x) + (headingForward.z * headingForward.z));
            if (hlen2 > 0.001f) {
                headingForward.x /= hlen2;
                headingForward.z /= hlen2;
                hasHeadingForward = true;
            }
        } else if (hasEarlyCameraPose) {
            headingForward = earlyCameraForward;
            hasHeadingForward = true;
        }

        if (!hasHeadingForward && cannonContextSticky && gCachedCannonImpactValid && (nowMs - gCachedCannonImpactUpdatedMs) <= 700) {
            const float dx = gCachedCannonImpactPoint.x - origin.x;
            const float dz = gCachedCannonImpactPoint.z - origin.z;
            const float hlen = std::sqrt((dx * dx) + (dz * dz));
            if (std::isfinite(hlen) && hlen > 0.25f) {
                headingForward.x = dx / hlen;
                headingForward.y = 0.0f;
                headingForward.z = dz / hlen;
                hasHeadingForward = true;
                hasHeadingFromImpact = true;
            }
        }

        if (!hasHeadingForward && hasShotForward) {
            headingForward = shotForward;
            hasHeadingForward = true;
        } else if (TryGetLocalPlayerForward(manager, headingForward)) {
            hasHeadingForward = true;
        }

        Vec3 markerForward{};
        bool hasMarkerForward = false;
        if (hasHeadingFromImpact) {
            markerForward = headingForward;
            hasMarkerForward = true;
        } else if (hasShotForward) {
            markerForward = shotForward;
            hasMarkerForward = true;
        } else if (hasHeadingForward) {
            markerForward = headingForward;
            hasMarkerForward = true;
        } else if (gLastAimForwardValid) {
            markerForward = gLastAimForward;
            hasMarkerForward = true;
        }

        Vec3 markerOrigin = origin;
        if (!hasShotForward) {
            markerOrigin = localFeet;
            markerOrigin.y += 1.6f;
        }

        bool hasImpact = false;
        if (gCannonImpactMarkerEnabled) {
            if (cannonContextSticky && gCachedCannonImpactValid && (nowMs - gCachedCannonImpactUpdatedMs) <= 700) {
                impact = gCachedCannonImpactPoint;
                hasImpact = true;
            }

            if (hasMarkerForward) {
                if (!hasImpact) {
                    hasImpact = SolveFlatImpactPoint(markerOrigin, markerForward, gCannonImpactVelocity, gCannonImpactGravity, localFeet.y, impact);
                }
                if (!hasImpact) {
                    const float fallbackDistance = std::clamp(gCannonMapRangeMeters * 0.9f, 40.0f, 1200.0f);
                    impact.x = markerOrigin.x + (markerForward.x * fallbackDistance);
                    impact.y = localFeet.y;
                    impact.z = markerOrigin.z + (markerForward.z * fallbackDistance);
                    hasImpact = true;
                }
            }

            if (!hasImpact && gCachedCannonImpactValid && (nowMs - gCachedCannonImpactUpdatedMs) <= 600) {
                impact = gCachedCannonImpactPoint;
                hasImpact = true;
            }

            if (hasImpact) {
                hasImpact = std::isfinite(impact.x) && std::isfinite(impact.y) && std::isfinite(impact.z);
            }
        }
        DrawCannonOverlay(draw, localFeet, radarPoints, hasImpact ? &impact : nullptr, headingForward.x, headingForward.z, hasHeadingForward);
    }

    if (gLastDrawn > 0) {
        if (gSkeletonEnabled) {
            gLastStatus = std::string("ok (true bones ") + std::to_string(trueBonePlayersDrawn) + "/" + std::to_string(gLastDrawn) + ")";
        } else {
            gLastStatus = "ok";
        }
    } else if (gLastProjected > 0) {
        gLastStatus = "projected but filtered";
    } else {
        gLastStatus = "no projected players";
    }
}


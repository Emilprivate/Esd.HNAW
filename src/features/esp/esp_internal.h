#pragma once

#include "features/esp/esp.h"

#include "imgui.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <windows.h>

struct MonoDomain;
struct MonoAssembly;
struct MonoImage;
struct MonoObject;
struct MonoClass;
struct MonoClassField;
struct MonoMethod;
struct MonoType;
struct MonoVTable;
struct MonoString;

namespace EspInternal {
    struct Vec3 {
        float x;
        float y;
        float z;
    };

    struct Color4 {
        float r;
        float g;
        float b;
        float a;
    };

    using MonoGetRootDomainFn = MonoDomain* (*)();
    using MonoThreadAttachFn = void* (*)(MonoDomain*);
    using MonoDomainAssemblyOpenFn = MonoAssembly* (*)(MonoDomain*, const char*);
    using MonoAssemblyGetImageFn = MonoImage* (*)(MonoAssembly*);
    using MonoClassFromNameFn = MonoClass* (*)(MonoImage*, const char*, const char*);
    using MonoObjectGetClassFn = MonoClass* (*)(MonoObject*);
    using MonoClassGetParentFn = MonoClass* (*)(MonoClass*);
    using MonoClassGetFieldFromNameFn = MonoClassField* (*)(MonoClass*, const char*);
    using MonoFieldGetValueFn = void (*)(MonoObject*, MonoClassField*, void*);
    using MonoClassGetMethodFromNameFn = MonoMethod* (*)(MonoClass*, const char*, int);
    using MonoClassVTableFn = MonoVTable* (*)(MonoDomain*, MonoClass*);
    using MonoFieldStaticGetValueFn = void (*)(MonoVTable*, MonoClassField*, void*);
    using MonoRuntimeInvokeFn = MonoObject* (*)(MonoMethod*, void*, void**, MonoObject**);
    using MonoObjectUnboxFn = void* (*)(MonoObject*);
    using MonoStringNewFn = MonoString* (*)(MonoDomain*, const char*);

    struct MonoApi {
        MonoGetRootDomainFn monoGetRootDomain = nullptr;
        MonoThreadAttachFn monoThreadAttach = nullptr;
        MonoDomainAssemblyOpenFn monoDomainAssemblyOpen = nullptr;
        MonoAssemblyGetImageFn monoAssemblyGetImage = nullptr;
        MonoClassFromNameFn monoClassFromName = nullptr;
        MonoObjectGetClassFn monoObjectGetClass = nullptr;
        MonoClassGetParentFn monoClassGetParent = nullptr;
        MonoClassGetFieldFromNameFn monoClassGetFieldFromName = nullptr;
        MonoFieldGetValueFn monoFieldGetValue = nullptr;
        MonoClassGetMethodFromNameFn monoClassGetMethodFromName = nullptr;
        MonoClassVTableFn monoClassVTable = nullptr;
        MonoFieldStaticGetValueFn monoFieldStaticGetValue = nullptr;
        MonoRuntimeInvokeFn monoRuntimeInvoke = nullptr;
        MonoObjectUnboxFn monoObjectUnbox = nullptr;
        MonoStringNewFn monoStringNew = nullptr;
    };

    extern MonoApi gMonoApi;
    extern bool gMonoApiLoaded;
    extern bool gMonoApiFailed;

    extern MonoDomain* gDomain;
    extern MonoImage* gGameImage;
    extern MonoImage* gUnityImage;
    extern MonoImage* gUnityPhysicsImage;
    extern MonoClass* gClientComponentReferenceManagerClass;
    extern MonoClass* gClientRoundPlayerManagerClass;
    extern MonoClass* gCameraClass;
    extern MonoClass* gTransformClass;
    extern MonoClass* gAnimatorClass;
    extern MonoClass* gRendererClass;
    extern MonoClass* gMaterialClass;
    extern MonoClass* gShaderClass;
    extern MonoClass* gPhysicsClass;
    extern MonoClass* gPlayerActorInitializerClass;
    extern MonoClass* gPlayerSpawnDataClass;
    extern MonoClass* gPlayerStartDataClass;
    extern MonoClass* gModelBonePositionsClass;
    extern MonoClass* gRoundPlayerClass;

    extern MonoClassField* gClientRoundPlayerManagerField;
    extern MonoMethod* gClientComponentReferenceManagerGetInstanceMethod;
    extern MonoMethod* gClientRoundPlayerManagerGetInstanceMethod;
    extern MonoMethod* gCameraWorldToScreenPointMethod;
    extern MonoMethod* gAnimatorGetBoneTransformMethod;
    extern MonoMethod* gRendererGetMaterialMethod;
    extern MonoMethod* gRendererGetSharedMaterialMethod;
    extern MonoMethod* gRendererGetMaterialsMethod;
    extern MonoMethod* gRendererGetSharedMaterialsMethod;
    extern MonoMethod* gRendererSetShadowCastingModeMethod;
    extern MonoMethod* gRendererSetReceiveShadowsMethod;
    extern MonoMethod* gMaterialSetColorMethod;
    extern MonoMethod* gMaterialSetColorByNameMethod;
    extern MonoMethod* gMaterialSetShaderMethod;
    extern MonoMethod* gMaterialSetFloatByNameMethod;
    extern MonoMethod* gMaterialSetIntByNameMethod;
    extern MonoMethod* gShaderFindMethod;
    extern MonoMethod* gPhysicsLinecastMethod;
    extern int gPhysicsLinecastParamCount;
    extern MonoMethod* gPlayerActorInitializerGetCurrentModelMethod;
    extern MonoMethod* gPlayerSpawnDataGetPlayerActorInitializerMethod;
    extern MonoMethod* gPlayerStartDataGetPlayerActorInitializerMethod;
    extern MonoMethod* gModelBonePositionsResolveModeBoneMethod;
    extern MonoMethod* gRoundPlayerGetPlayerStartDataMethod;
    extern bool gMonoSymbolsReady;
    extern bool gMonoSymbolsAttempted;

    template <typename T>
    bool SafeRead(uintptr_t address, T& outValue) {
        __try {
            outValue = *reinterpret_cast<T*>(address);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    extern bool gEnabled;
    extern bool gCornerMode;
    extern bool gFilled;
    extern float gFillAlpha;
    extern float gThickness;
    extern float gColorRgb[3];
    extern bool gUseTeamColors;
    extern float gTeamColorRgb[3];
    extern float gEnemyColorRgb[3];
    extern int gTeamFilterMode;
    extern int gVisibilityMode;
    extern bool gPerFeatureDistanceLimitsEnabled;
    extern float gMaxBoxDistanceMeters;
    extern float gMaxSkeletonDistanceMeters;
    extern float gMaxInfoDistanceMeters;
    extern float gMaxChamsDistanceMeters;
    extern int gInfoPosition;
    extern bool gSkeletonEnabled;
    extern float gSkeletonThickness;
    extern float gSkeletonColorRgb[3];
    extern bool gChamsEnabled;
    extern float gChamsAlpha;
    extern bool gChamsSolidMode;
    extern float gChamsBrightness;
    extern bool gShowName;
    extern bool gShowDistance;
    extern bool gShowHealth;
    extern bool gHealthBarEnabled;
    extern bool gShowNetworkId;
    extern bool gShowClassRank;
    extern bool gShowFaction;
    extern bool gCannonMapEnabled;
    extern bool gCannonMapRequireContext;
    extern float gCannonMapPosX;
    extern float gCannonMapPosY;
    extern float gCannonMapSizePx;
    extern float gCannonMapRangeMeters;
    extern bool gCannonMapShowTeammates;
    extern bool gCannonImpactMarkerEnabled;
    extern float gCannonImpactVelocity;
    extern float gCannonImpactGravity;
    extern int gLastPlayersSeen;
    extern int gLastProjected;
    extern int gLastDrawn;
    extern int gTrueBonesActorInitFail;
    extern int gTrueBonesModelFail;
    extern int gTrueBonesTransformFail;
    extern int gTrueBonesSuccess;
    extern int gChamsModelsResolved;
    extern int gChamsRenderableItemsResolved;
    extern int gChamsClientItemsResolved;
    extern int gChamsRenderersResolved;
    extern int gChamsMaterialsResolved;
    extern int gChamsColorCalls;
    extern int gChamsAttemptedPlayers;
    extern std::string gLastStatus;
    extern std::string gLastDebugString;

    extern MonoMethod* gGetAllRoundPlayersMethod;
    extern MonoMethod* gCameraGetMainMethod;
    extern MonoMethod* gTransformGetPositionMethod;

    ImU32 ToColor32(const float* rgb, float alpha01);
    void DrawCornerBox(ImDrawList* draw, const ImVec2& topLeft, const ImVec2& bottomRight, ImU32 color, float thickness);

    bool EnsureMonoSymbols();
    void AttachMonoThread();
    MonoObject* InvokeMethod(MonoMethod* method, void* instance, void** params);
    bool InvokeMethodBool(MonoMethod* method, void* instance, void** params, bool& outValue);
    bool InvokeMethodVec3(MonoMethod* method, void* instance, void** params, Vec3& outValue);
    bool TryReadCollection(void* collectionObject, void*& outItems, int& outCount);
    bool TryEnumerateCollectionByMethods(MonoObject* collectionObject, std::vector<void*>& outElements);
    void* GetRoundPlayerManagerInstance();
    bool WorldToScreen(MonoObject* camera, const Vec3& world, ImVec2& screen);
    void* GetManagedArrayElement(void* managedArrayObject, int index);
    MonoClassField* TryGetFieldByNames(MonoClass* klass, const char* const* names, size_t count);
    bool TryGetBonePositions(void* roundPlayer, void* playerBase, void* spawnData, std::vector<Vec3>& outBones);
    void ApplyPlayerModelChams(void* roundPlayer, void* playerBase, void* spawnData, const Color4& color);
}

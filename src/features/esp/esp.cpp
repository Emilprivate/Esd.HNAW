#include "features/esp/esp_internal.h"

#include <algorithm>
#include <string>

namespace EspInternal {
    MonoApi gMonoApi{};
    bool gMonoApiLoaded = false;
    bool gMonoApiFailed = false;

    MonoDomain* gDomain = nullptr;
    MonoImage* gGameImage = nullptr;
    MonoImage* gUnityImage = nullptr;
    MonoImage* gUnityPhysicsImage = nullptr;
    MonoClass* gClientComponentReferenceManagerClass = nullptr;
    MonoClass* gClientRoundPlayerManagerClass = nullptr;
    MonoClass* gCameraClass = nullptr;
    MonoClass* gTransformClass = nullptr;
    MonoClass* gAnimatorClass = nullptr;
    MonoClass* gRendererClass = nullptr;
    MonoClass* gMaterialClass = nullptr;
    MonoClass* gShaderClass = nullptr;
    MonoClass* gPhysicsClass = nullptr;
    MonoClass* gPlayerActorInitializerClass = nullptr;
    MonoClass* gPlayerSpawnDataClass = nullptr;
    MonoClass* gPlayerStartDataClass = nullptr;
    MonoClass* gModelBonePositionsClass = nullptr;
    MonoClass* gRoundPlayerClass = nullptr;

    MonoClassField* gClientRoundPlayerManagerField = nullptr;
    MonoMethod* gGetAllRoundPlayersMethod = nullptr;
    MonoMethod* gClientComponentReferenceManagerGetInstanceMethod = nullptr;
    MonoMethod* gClientRoundPlayerManagerGetInstanceMethod = nullptr;
    MonoMethod* gCameraGetMainMethod = nullptr;
    MonoMethod* gCameraWorldToScreenPointMethod = nullptr;
    MonoMethod* gTransformGetPositionMethod = nullptr;
    MonoMethod* gAnimatorGetBoneTransformMethod = nullptr;
    MonoMethod* gRendererGetMaterialMethod = nullptr;
    MonoMethod* gRendererGetSharedMaterialMethod = nullptr;
    MonoMethod* gRendererGetMaterialsMethod = nullptr;
    MonoMethod* gRendererGetSharedMaterialsMethod = nullptr;
    MonoMethod* gRendererSetShadowCastingModeMethod = nullptr;
    MonoMethod* gRendererSetReceiveShadowsMethod = nullptr;
    MonoMethod* gMaterialSetColorMethod = nullptr;
    MonoMethod* gMaterialSetColorByNameMethod = nullptr;
    MonoMethod* gMaterialSetShaderMethod = nullptr;
    MonoMethod* gMaterialSetFloatByNameMethod = nullptr;
    MonoMethod* gMaterialSetIntByNameMethod = nullptr;
    MonoMethod* gShaderFindMethod = nullptr;
    MonoMethod* gPhysicsLinecastMethod = nullptr;
    int gPhysicsLinecastParamCount = 0;
    MonoMethod* gPlayerActorInitializerGetCurrentModelMethod = nullptr;
    MonoMethod* gPlayerSpawnDataGetPlayerActorInitializerMethod = nullptr;
    MonoMethod* gPlayerStartDataGetPlayerActorInitializerMethod = nullptr;
    MonoMethod* gModelBonePositionsResolveModeBoneMethod = nullptr;
    MonoMethod* gRoundPlayerGetPlayerStartDataMethod = nullptr;
    bool gMonoSymbolsReady = false;
    bool gMonoSymbolsAttempted = false;

    bool gEnabled = true;
    bool gCornerMode = false;
    bool gFilled = false;
    float gFillAlpha = 0.20f;
    float gThickness = 1.5f;
    float gColorRgb[3] = { 0.27f, 0.86f, 0.47f };
    bool gUseTeamColors = true;
    float gTeamColorRgb[3] = { 0.30f, 0.70f, 1.00f };
    float gEnemyColorRgb[3] = { 1.00f, 0.35f, 0.35f };
    int gTeamFilterMode = 0;
    int gVisibilityMode = 0;
    bool gPerFeatureDistanceLimitsEnabled = false;
    float gMaxBoxDistanceMeters = 200.0f;
    float gMaxSkeletonDistanceMeters = 200.0f;
    float gMaxInfoDistanceMeters = 200.0f;
    float gMaxChamsDistanceMeters = 200.0f;
    int gInfoPosition = 1;
    bool gSkeletonEnabled = false;
    float gSkeletonThickness = 1.2f;
    float gSkeletonColorRgb[3] = { 1.00f, 1.00f, 1.00f };
    bool gChamsEnabled = false;
    float gChamsAlpha = 0.28f;
    bool gChamsSolidMode = false;
    float gChamsBrightness = 2.2f;
    bool gShowName = true;
    bool gShowDistance = true;
    bool gShowHealth = true;
    bool gHealthBarEnabled = true;
    bool gShowNetworkId = false;
    bool gShowClassRank = false;
    bool gShowFaction = false;
    bool gCannonMapEnabled = false;
    bool gCannonMapRequireContext = false;
    float gCannonMapPosX = 26.0f;
    float gCannonMapPosY = 0.0f;
    float gCannonMapSizePx = 220.0f;
    float gCannonMapRangeMeters = 300.0f;
    bool gCannonMapShowTeammates = false;
    bool gCannonImpactMarkerEnabled = true;
    float gCannonImpactVelocity = 145.0f;
    float gCannonImpactGravity = 9.81f;
    int gLastPlayersSeen = 0;
    int gLastProjected = 0;
    int gLastDrawn = 0;
    int gTrueBonesActorInitFail = 0;
    int gTrueBonesModelFail = 0;
    int gTrueBonesTransformFail = 0;
    int gTrueBonesSuccess = 0;
    int gChamsModelsResolved = 0;
    int gChamsRenderableItemsResolved = 0;
    int gChamsClientItemsResolved = 0;
    int gChamsRenderersResolved = 0;
    int gChamsMaterialsResolved = 0;
    int gChamsColorCalls = 0;
    int gChamsAttemptedPlayers = 0;
    std::string gLastStatus = "idle";
    std::string gLastDebugString;

    ImU32 ToColor32(const float* rgb, float alpha01) {
        const int r = static_cast<int>(std::clamp(rgb[0], 0.0f, 1.0f) * 255.0f);
        const int g = static_cast<int>(std::clamp(rgb[1], 0.0f, 1.0f) * 255.0f);
        const int b = static_cast<int>(std::clamp(rgb[2], 0.0f, 1.0f) * 255.0f);
        const int a = static_cast<int>(std::clamp(alpha01, 0.0f, 1.0f) * 255.0f);
        return IM_COL32(r, g, b, a);
    }

    void DrawCornerBox(ImDrawList* draw, const ImVec2& topLeft, const ImVec2& bottomRight, ImU32 color, float thickness) {
        if (!draw) {
            return;
        }

        const float width = bottomRight.x - topLeft.x;
        const float height = bottomRight.y - topLeft.y;
        if (width <= 1.0f || height <= 1.0f) {
            return;
        }

        const float cornerW = width * 0.28f;
        const float cornerH = height * 0.28f;

        draw->AddLine(topLeft, ImVec2(topLeft.x + cornerW, topLeft.y), color, thickness);
        draw->AddLine(topLeft, ImVec2(topLeft.x, topLeft.y + cornerH), color, thickness);

        draw->AddLine(ImVec2(bottomRight.x - cornerW, topLeft.y), ImVec2(bottomRight.x, topLeft.y), color, thickness);
        draw->AddLine(ImVec2(bottomRight.x, topLeft.y), ImVec2(bottomRight.x, topLeft.y + cornerH), color, thickness);

        draw->AddLine(ImVec2(topLeft.x, bottomRight.y - cornerH), ImVec2(topLeft.x, bottomRight.y), color, thickness);
        draw->AddLine(ImVec2(topLeft.x, bottomRight.y), ImVec2(topLeft.x + cornerW, bottomRight.y), color, thickness);

        draw->AddLine(ImVec2(bottomRight.x - cornerW, bottomRight.y), bottomRight, color, thickness);
        draw->AddLine(ImVec2(bottomRight.x, bottomRight.y - cornerH), bottomRight, color, thickness);
    }
}

using namespace EspInternal;


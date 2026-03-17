#include "features/esp/esp_internal.h"

using namespace EspInternal;

bool& PlayerBoxes::Enabled() {
    return gEnabled;
}

bool& PlayerBoxes::CornerMode() {
    return gCornerMode;
}

bool& PlayerBoxes::Filled() {
    return gFilled;
}

float& PlayerBoxes::FillAlpha() {
    return gFillAlpha;
}

float& PlayerBoxes::Thickness() {
    return gThickness;
}

float* PlayerBoxes::ColorRgb() {
    return gColorRgb;
}

bool& PlayerBoxes::UseTeamColors() {
    return gUseTeamColors;
}

float* PlayerBoxes::TeamColorRgb() {
    return gTeamColorRgb;
}

float* PlayerBoxes::EnemyColorRgb() {
    return gEnemyColorRgb;
}

int& PlayerBoxes::TeamFilterMode() {
    return gTeamFilterMode;
}

int& PlayerBoxes::VisibilityMode() {
    return gVisibilityMode;
}

bool& PlayerBoxes::PerFeatureDistanceLimitsEnabled() {
    return gPerFeatureDistanceLimitsEnabled;
}

float& PlayerBoxes::MaxBoxDistanceMeters() {
    return gMaxBoxDistanceMeters;
}

float& PlayerBoxes::MaxSkeletonDistanceMeters() {
    return gMaxSkeletonDistanceMeters;
}

float& PlayerBoxes::MaxInfoDistanceMeters() {
    return gMaxInfoDistanceMeters;
}

float& PlayerBoxes::MaxChamsDistanceMeters() {
    return gMaxChamsDistanceMeters;
}

int& PlayerBoxes::InfoPosition() {
    return gInfoPosition;
}

bool& PlayerBoxes::SkeletonEnabled() {
    return gSkeletonEnabled;
}

float& PlayerBoxes::SkeletonThickness() {
    return gSkeletonThickness;
}

float* PlayerBoxes::SkeletonColorRgb() {
    return gSkeletonColorRgb;
}

bool& PlayerBoxes::ChamsEnabled() {
    return gChamsEnabled;
}

float& PlayerBoxes::ChamsAlpha() {
    return gChamsAlpha;
}

bool& PlayerBoxes::ChamsSolidMode() {
    return gChamsSolidMode;
}

float& PlayerBoxes::ChamsBrightness() {
    return gChamsBrightness;
}

bool& PlayerBoxes::ShowName() {
    return gShowName;
}

bool& PlayerBoxes::ShowDistance() {
    return gShowDistance;
}

bool& PlayerBoxes::ShowHealth() {
    return gShowHealth;
}

bool& PlayerBoxes::HealthBarEnabled() {
    return gHealthBarEnabled;
}

bool& PlayerBoxes::ShowNetworkId() {
    return gShowNetworkId;
}

bool& PlayerBoxes::ShowClassRank() {
    return gShowClassRank;
}

bool& PlayerBoxes::ShowFaction() {
    return gShowFaction;
}

const char* PlayerBoxes::LastStatus() {
    return gLastStatus.c_str();
}

int PlayerBoxes::LastPlayersSeen() {
    return gLastPlayersSeen;
}

int PlayerBoxes::LastProjected() {
    return gLastProjected;
}

int PlayerBoxes::LastDrawn() {
    return gLastDrawn;
}

const char* PlayerBoxes::BuildDebugString() {
    gLastDebugString = "Boxes status: " + gLastStatus +
        "\nPlayers: " + std::to_string(gLastPlayersSeen) +
        "\nProjected: " + std::to_string(gLastProjected) +
        "\nDrawn: " + std::to_string(gLastDrawn) +
        "\nVisibility mode: " + std::to_string(gVisibilityMode) +
        "\nPer-feature distance limits: " + std::to_string(gPerFeatureDistanceLimitsEnabled ? 1 : 0) +
        "\nMax distance box/skeleton/info/chams: " +
        std::to_string(gMaxBoxDistanceMeters) + "/" +
        std::to_string(gMaxSkeletonDistanceMeters) + "/" +
        std::to_string(gMaxInfoDistanceMeters) + "/" +
        std::to_string(gMaxChamsDistanceMeters) +
        "\nTrueBone success/fail AI/model/transform: " +
        std::to_string(gTrueBonesSuccess) + "/" +
        std::to_string(gTrueBonesActorInitFail) + "/" +
        std::to_string(gTrueBonesModelFail) + "/" +
        std::to_string(gTrueBonesTransformFail) +
        "\nChams model/renderItems/clientItems/renderers/materials/calls: " +
        std::to_string(gChamsModelsResolved) + "/" +
        std::to_string(gChamsRenderableItemsResolved) + "/" +
        std::to_string(gChamsClientItemsResolved) + "/" +
        std::to_string(gChamsRenderersResolved) + "/" +
        std::to_string(gChamsMaterialsResolved) + "/" +
        std::to_string(gChamsColorCalls) +
        "\nChams enabled/alpha/solid/brightness/attempted: " +
        std::to_string(gChamsEnabled ? 1 : 0) + "/" +
        std::to_string(gChamsAlpha) + "/" +
        std::to_string(gChamsSolidMode ? 1 : 0) + "/" +
        std::to_string(gChamsBrightness) + "/" +
        std::to_string(gChamsAttemptedPlayers);
    return gLastDebugString.c_str();
}

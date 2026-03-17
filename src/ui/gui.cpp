#include "ui/gui.h"

#include "config/config.h"
#include "core/hnaw_offsets.h"
#include "core/hooking/hook.h"
#include "core/mono/mono_resolver.h"
#include "features/aimbot/aimbot.h"
#include "features/experiments/experiments.h"
#include "features/esp/esp.h"
#include "ui/gui_theme.h"

#include <array>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#include <winuser.h>

namespace {
    ID3D11Device* gDevice = nullptr;
    ID3D11DeviceContext* gContext = nullptr;
    IDXGISwapChain* gSwapChain = nullptr;
    ID3D11RenderTargetView* gMainRenderTargetView = nullptr;
    HWND gWindow = nullptr;
    bool gMenuInputCaptured = false;
    bool gCursorForcedVisible = false;
    bool gRequestUnloadFromUi = false;

    struct SymbolRow {
        const char* name;
        const uintptr_t* value;
    };

    constexpr std::array<SymbolRow, 15> kClassRows = {{
        { "HoldfastGame.ClientComponentReferenceManager", &HnawOffsets::classClientComponentReferenceManager },
        { "HoldfastGame.ServerAzureBackendManager", &HnawOffsets::classServerAzureBackendManager },
        { "HoldfastGame.ClientRoundPlayerManager", &HnawOffsets::classClientRoundPlayerManager },
        { "HoldfastGame.RoundPlayer", &HnawOffsets::classRoundPlayer },
        { "HoldfastGame.PlayerBase", &HnawOffsets::classPlayerBase },
        { "HoldfastGame.PlayerInitialDetails", &HnawOffsets::classPlayerInitialDetails },
        { "HoldfastGame.PlayerSpawnData", &HnawOffsets::classPlayerSpawnData },
        { "HoldfastGame.ClientWeaponHolder", &HnawOffsets::classClientWeaponHolder },
        { "HoldfastGame.RoundPlayerInformation", &HnawOffsets::classRoundPlayerInformation },
        { "HoldfastGame.ClientSpectatorManager", &HnawOffsets::classClientSpectatorManager },
        { "HoldfastGame.ClientRemoteConsoleAccessManager", &HnawOffsets::classClientRemoteConsoleAccessManager },
        { "UnityEngine.Camera", &HnawOffsets::classCamera },
        { "UnityEngine.Transform", &HnawOffsets::classTransform },
        { "UnityEngine.RaycastHit", &HnawOffsets::classRaycastHit },
        { "UnityEngine.Time", &HnawOffsets::classTime }
    }};

    constexpr std::array<SymbolRow, 31> kFieldRows = {{
        { "clientRoundPlayerManager", &HnawOffsets::clientRoundPlayerManager },
        { "RoundPlayer.PlayerBase", &HnawOffsets::roundPlayerPlayerBase },
        { "RoundPlayer.PlayerTransform", &HnawOffsets::roundPlayerPlayerTransform },
        { "RoundPlayer.NetworkPlayerID", &HnawOffsets::roundPlayerNetworkPlayerID },
        { "TransformData.position", &HnawOffsets::transformDataPosition },
        { "PlayerBase.<Health>k__BackingField", &HnawOffsets::playerBaseHealth },
        { "PlayerSpawnData.SquadID", &HnawOffsets::playerSpawnDataSquadID },
        { "ClientRoundPlayerManager.LocalPlayer", &HnawOffsets::clientRoundPlayerManagerLocalPlayer },
        { "ClientRoundPlayerManager.CurrentRoundPlayerInformation", &HnawOffsets::clientRoundPlayerManagerCurrentRoundPlayerInformation },
        { "RoundPlayerInformation.InitialDetails", &HnawOffsets::roundPlayerInformationInitialDetails },
        { "PlayerInitialDetails.displayname", &HnawOffsets::playerInitialDetailsDisplayname },
        { "PlayerSpawnData.ClassRank", &HnawOffsets::playerSpawnDataClassRank },
        { "PlayerBase.<PlayerStartData>k__BackingField", &HnawOffsets::playerBasePlayerStartData },
        { "PlayerSpawnData.Faction", &HnawOffsets::playerSpawnDataFaction },
        { "RoundPlayer.WeaponHolder", &HnawOffsets::roundPlayerWeaponHolder },
        { "ClientComponentReferenceManager.clientAdminBroadcastMessageManager", &HnawOffsets::clientAdminBroadcastMessageManager },
        { "ClientComponentReferenceManager.clientSpectatorManager", &HnawOffsets::clientSpectatorManager },
        { "ClientComponentReferenceManager.clientRemoteConsoleAccessManager", &HnawOffsets::clientRemoteConsoleAccessManager },
        { "ClientComponentReferenceManager.clientRPCExecutionManager", &HnawOffsets::clientRPCExecutionManager },
        { "ClientSpectatorManager.currentlySpectatingPlayer", &HnawOffsets::clientSpectatorManagerCurrentlySpectatingPlayer },
        { "PlayerSpawnData.PlayerActorInitializer", &HnawOffsets::playerSpawnDataPlayerActorInitializer },
        { "ModelProperties.modelBonePositions", &HnawOffsets::modelPropertiesModelBonePositions },
        { "ModelBonePositions.rootBone", &HnawOffsets::modelBonePositionsRootBone },
        { "ModelBonePositions.hip", &HnawOffsets::modelBonePositionsHip },
        { "ModelBonePositions.lowerSpine", &HnawOffsets::modelBonePositionsLowerSpine },
        { "ModelBonePositions.middleSpine", &HnawOffsets::modelBonePositionsMiddleSpine },
        { "ModelBonePositions.chest", &HnawOffsets::modelBonePositionsChest },
        { "ModelBonePositions.neck", &HnawOffsets::modelBonePositionsNeck },
        { "ModelBonePositions.head", &HnawOffsets::modelBonePositionsHead },
        { "ModelBonePositions.leftHand", &HnawOffsets::modelBonePositionsLeftHand },
        { "ModelBonePositions.rightHand", &HnawOffsets::modelBonePositionsRightHand }
    }};

    constexpr std::array<SymbolRow, 11> kMethodRows = {{
        { "GetAllRoundPlayers", &HnawOffsets::methodGetAllRoundPlayers },
        { "Camera.get_main", &HnawOffsets::methodCameraGetMain },
        { "Camera.WorldToScreenPoint", &HnawOffsets::methodCameraWorldToScreenPoint },
        { "Transform.get_position", &HnawOffsets::methodTransformGetPosition },
        { "Transform.set_localScale", &HnawOffsets::methodTransformSetLocalScale },
        { "RoundPlayer.get_PlayerStartData", &HnawOffsets::methodRoundPlayerGetPlayerStartData },
        { "RoundPlayer.SetRotation", &HnawOffsets::methodRoundPlayerSetRotation },
        { "RoundPlayer.ThrowPlayerToFloor_func", &HnawOffsets::methodRoundPlayerThrowPlayerToFloor },
        { "RoundPlayer.ExecutePlayerAction", &HnawOffsets::methodRoundPlayerExecutePlayerAction },
        { "PlayerActorInitializer.get_CurrentModel", &HnawOffsets::methodPlayerActorInitializerGetCurrentModel },
        { "ClientWeaponHolder.get_ActiveWeaponDetails", &HnawOffsets::methodWeaponHolderGetActiveWeaponDetails }
    }};

    constexpr std::array<SymbolRow, 7> kHookRows = {{
        { "ClientWeaponHolder.GetReloadDuration", &HnawOffsets::hookClientWeaponHolderGetReloadDuration },
        { "ClientWeaponHolder.CalculateFirearmShotTrajectory", &HnawOffsets::hookClientWeaponHolderCalculateFirearmShotTrajectory },
        { "ServerAzureBackendManager.AuthenticatePlayer", &HnawOffsets::hookServerAzureBackendManagerAuthenticatePlayer },
        { "ClientConnectionManager.JoinServer", &HnawOffsets::hookClientConnectionManagerJoinServer },
        { "PlayerBase.get_CanRun", &HnawOffsets::hookPlayerBaseGetCanRun },
        { "UnityEngine.Debug.LogError", &HnawOffsets::hookUnityDebugLogError },
        { "OwnerPacketToServer.WriteBitStream", &HnawOffsets::hookOwnerPacketToServerWriteBitStream }
    }};

    void DrawBallisticsDebugTab();

    void CreateRenderTarget() {
        if (!gSwapChain || !gDevice || gMainRenderTargetView) {
            return;
        }

        ID3D11Texture2D* backBuffer = nullptr;
        if (SUCCEEDED(gSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer))) && backBuffer) {
            gDevice->CreateRenderTargetView(backBuffer, nullptr, &gMainRenderTargetView);
            backBuffer->Release();
        }
    }

    void CleanupRenderTarget() {
        if (gMainRenderTargetView) {
            gMainRenderTargetView->Release();
            gMainRenderTargetView = nullptr;
        }
    }

    void DrawSymbolTable(const char* tableId, const SymbolRow* rows, size_t count) {
        if (!ImGui::BeginTable(tableId, 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
            return;
        }

        ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthStretch, 0.62f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.24f);
        ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthStretch, 0.14f);
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < count; ++i) {
            const uintptr_t value = *rows[i].value;
            const bool found = (value != 0);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(rows[i].name);

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("0x%llX", static_cast<unsigned long long>(value));

            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(found ? ImVec4(0.35f, 0.85f, 0.45f, 1.0f) : ImVec4(0.90f, 0.45f, 0.30f, 1.0f), found ? "Found" : "Missing");
        }

        ImGui::EndTable();
    }

    std::string BuildResolverDump() {
        std::string out;
        auto append = [&](const char* name, uintptr_t value) {
            char line[256]{};
            std::snprintf(line, sizeof(line), "%s: 0x%llX\n", name, static_cast<unsigned long long>(value));
            out += line;
        };

        out += "[Classes]\n";
        for (const auto& row : kClassRows) {
            append(row.name, *row.value);
        }
        out += "\n[Field Offsets]\n";
        for (const auto& row : kFieldRows) {
            append(row.name, *row.value);
        }
        out += "\n[Methods]\n";
        for (const auto& row : kMethodRows) {
            append(row.name, *row.value);
        }
        out += "\n[Hooks]\n";
        for (const auto& row : kHookRows) {
            append(row.name, *row.value);
        }

        return out;
    }

    void DrawOverviewTab() {
        ImGui::Text("Resolver: %s", HnawOffsets::status.c_str());
        ImGui::Text("Mono attached: %s", HnawOffsets::monoAttached ? "yes" : "no");
        ImGui::Text("No recoil hook: %s", Hook::IsNoRecoilHookActive() ? "active" : "inactive");
        ImGui::Text("No spread hook: %s", Hook::IsNoSpreadHookActive() ? "active" : "inactive");
        ImGui::Text("Auto reload pipeline: %s", Hook::IsAutoReloadReady() ? "ready" : "missing symbols");

        ImGui::Spacing();
        ImGui::Text("Classes resolved: %d/%d", HnawOffsets::resolvedClassCount, HnawOffsets::requiredClassCount);
        ImGui::Text("Fields resolved: %d/%d", HnawOffsets::resolvedFieldCount, HnawOffsets::requiredFieldCount);
        ImGui::Text("Methods resolved: %d/%d", HnawOffsets::resolvedMethodCount, HnawOffsets::requiredMethodCount);

        if (HnawOffsets::unresolvedRequiredCount > 0 && !HnawOffsets::unresolvedSummary.empty()) {
            ImGui::Spacing();
            ImGui::TextUnformatted("Missing symbols");
            ImGui::BeginChild("MissingSymbols", ImVec2(0.0f, 120.0f), true);
            ImGui::TextWrapped("%s", HnawOffsets::unresolvedSummary.c_str());
            ImGui::EndChild();
        }
    }

    void DrawResolverDataTab() {
        if (ImGui::CollapsingHeader("Class Handles", ImGuiTreeNodeFlags_DefaultOpen)) {
            DrawSymbolTable("ClassHandleTable", kClassRows.data(), kClassRows.size());
        }

        if (ImGui::CollapsingHeader("Field Offsets", ImGuiTreeNodeFlags_DefaultOpen)) {
            DrawSymbolTable("FieldOffsetTable", kFieldRows.data(), kFieldRows.size());
        }

        if (ImGui::CollapsingHeader("Methods", ImGuiTreeNodeFlags_DefaultOpen)) {
            DrawSymbolTable("MethodTable", kMethodRows.data(), kMethodRows.size());
        }

        if (ImGui::CollapsingHeader("Hook Targets", ImGuiTreeNodeFlags_DefaultOpen)) {
            DrawSymbolTable("HookTable", kHookRows.data(), kHookRows.size());
        }
    }

    void DrawToolsTab() {
        if (ImGui::Button("Re-resolve symbols", ImVec2(180.0f, 0.0f))) {
            MonoResolver::ResolveAll();
        }
        ImGui::SameLine();
        if (ImGui::Button("Copy full dump", ImVec2(150.0f, 0.0f))) {
            const std::string dump = BuildResolverDump();
            ImGui::SetClipboardText(dump.c_str());
        }

        ImGui::Spacing();
        ImGui::Separator();
        if (ImGui::Button("Unload DLL", ImVec2(140.0f, 0.0f))) {
            gRequestUnloadFromUi = true;
        }
    }

    void DrawEspDebugTab() {
        ImGui::Text("ESP status: %s", PlayerBoxes::LastStatus());
        ImGui::Text("Players: %d", PlayerBoxes::LastPlayersSeen());
        ImGui::Text("Projected: %d", PlayerBoxes::LastProjected());
        ImGui::Text("Drawn: %d", PlayerBoxes::LastDrawn());

        ImGui::Spacing();
        if (ImGui::Button("Copy ESP debug", ImVec2(170.0f, 0.0f))) {
            ImGui::SetClipboardText(PlayerBoxes::BuildDebugString());
        }
    }

    void DrawBallisticsDebugTab() {
        ImGui::TextUnformatted("Aimbot Ballistics");
        ImGui::Separator();

        ImGui::Text("Weapon type: %d", Aimbot::LastWeaponType());
        ImGui::Text("Weapon name: %s", Aimbot::LastWeaponTypeName());
        ImGui::Text("Velocity: %.1f", Aimbot::LastResolvedVelocity());
        ImGui::Text("Gravity: %.2f", Aimbot::LastResolvedGravity());
        ImGui::Text("Source: %s", Aimbot::LastUsedWeaponBallistics() ? "weapon" : "default");
    }

    void DrawEspTab() {
        ImGui::Indent();

        if (ImGui::BeginTable("EspFeatureGrid", 2, ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("Left", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableSetupColumn("Right", ImGuiTableColumnFlags_WidthStretch, 1.0f);

                ImGui::TableNextColumn();
                if (ImGui::CollapsingHeader("Box", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Checkbox("Show boxes", &PlayerBoxes::Enabled());
                    ImGui::Checkbox("Corner boxes", &PlayerBoxes::CornerMode());
                    ImGui::SameLine();
                    ImGui::Checkbox("Filled", &PlayerBoxes::Filled());
                    ImGui::SliderFloat("Fill alpha", &PlayerBoxes::FillAlpha(), 0.0f, 1.0f, "%.2f");
                    ImGui::SliderFloat("Thickness", &PlayerBoxes::Thickness(), 0.5f, 4.0f, "%.1f");
                }

                if (ImGui::CollapsingHeader("Team", ImGuiTreeNodeFlags_DefaultOpen)) {
                    static const char* teamFilterItems[] = { "All", "Team only", "Enemy only" };
                    ImGui::Combo("Render", &PlayerBoxes::TeamFilterMode(), teamFilterItems, IM_ARRAYSIZE(teamFilterItems));

                    static const char* visibilityItems[] = { "Off", "Line-of-sight only" };
                    ImGui::Combo("Visibility", &PlayerBoxes::VisibilityMode(), visibilityItems, IM_ARRAYSIZE(visibilityItems));

                    ImGui::Checkbox("Per-feature distance limits", &PlayerBoxes::PerFeatureDistanceLimitsEnabled());
                    if (PlayerBoxes::PerFeatureDistanceLimitsEnabled()) {
                        ImGui::SliderFloat("Box max distance (m)", &PlayerBoxes::MaxBoxDistanceMeters(), 5.0f, 1000.0f, "%.0f m");
                        if (ImGui::Button("Copy box -> all")) {
                            const float value = PlayerBoxes::MaxBoxDistanceMeters();
                            PlayerBoxes::MaxSkeletonDistanceMeters() = value;
                            PlayerBoxes::MaxInfoDistanceMeters() = value;
                            PlayerBoxes::MaxChamsDistanceMeters() = value;
                        }
                        ImGui::SliderFloat("Skeleton max distance (m)", &PlayerBoxes::MaxSkeletonDistanceMeters(), 5.0f, 1000.0f, "%.0f m");
                        ImGui::SliderFloat("Info max distance (m)", &PlayerBoxes::MaxInfoDistanceMeters(), 5.0f, 1000.0f, "%.0f m");
                        ImGui::SliderFloat("Chams max distance (m)", &PlayerBoxes::MaxChamsDistanceMeters(), 5.0f, 1000.0f, "%.0f m");
                    }

                    ImGui::Checkbox("Use team colors", &PlayerBoxes::UseTeamColors());
                    if (PlayerBoxes::UseTeamColors()) {
                        ImGui::ColorEdit3("Team color", PlayerBoxes::TeamColorRgb());
                        ImGui::ColorEdit3("Enemy color", PlayerBoxes::EnemyColorRgb());
                    }
                }

                ImGui::TableNextColumn();
                if (ImGui::CollapsingHeader("Info", ImGuiTreeNodeFlags_DefaultOpen)) {
                    static const char* infoPositionItems[] = { "Left", "Right", "Above", "Below" };
                    ImGui::Combo("Info position", &PlayerBoxes::InfoPosition(), infoPositionItems, IM_ARRAYSIZE(infoPositionItems));
                    ImGui::Checkbox("Show name", &PlayerBoxes::ShowName());
                    ImGui::SameLine();
                    ImGui::Checkbox("Show distance", &PlayerBoxes::ShowDistance());
                    ImGui::Checkbox("Show health", &PlayerBoxes::ShowHealth());
                    ImGui::SameLine();
                    ImGui::Checkbox("Health bar", &PlayerBoxes::HealthBarEnabled());
                    ImGui::SameLine();
                    ImGui::Checkbox("Show network ID", &PlayerBoxes::ShowNetworkId());
                    ImGui::Checkbox("Show class rank", &PlayerBoxes::ShowClassRank());
                    ImGui::SameLine();
                    ImGui::Checkbox("Show faction", &PlayerBoxes::ShowFaction());
                }

                if (ImGui::CollapsingHeader("Skeleton", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Checkbox("Enabled##Skeleton", &PlayerBoxes::SkeletonEnabled());
                    if (PlayerBoxes::SkeletonEnabled()) {
                        ImGui::SliderFloat("Skeleton thickness", &PlayerBoxes::SkeletonThickness(), 0.5f, 4.0f, "%.1f");
                    }
                }

                if (ImGui::CollapsingHeader("Chams", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Checkbox("Enabled##Chams", &PlayerBoxes::ChamsEnabled());
                    if (PlayerBoxes::ChamsEnabled()) {
                        ImGui::SliderFloat("Chams alpha", &PlayerBoxes::ChamsAlpha(), 0.0f, 1.0f, "%.2f");
                        ImGui::Checkbox("Solid chams mode", &PlayerBoxes::ChamsSolidMode());
                        if (PlayerBoxes::ChamsSolidMode()) {
                            ImGui::SliderFloat("Chams brightness", &PlayerBoxes::ChamsBrightness(), 1.0f, 8.0f, "%.1f");
                        }
                    }
                }

                if (ImGui::CollapsingHeader("Cannon Assist", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Checkbox("Enable cannon mini-map", &PlayerBoxes::CannonMapEnabled());
                    if (PlayerBoxes::CannonMapEnabled()) {
                        ImGui::Checkbox("Require artillery/cannon context", &PlayerBoxes::CannonMapRequireContext());
                        ImGui::SliderFloat("Map size", &PlayerBoxes::CannonMapSizePx(), 140.0f, 420.0f, "%.0f px");
                        ImGui::SliderFloat("Map range", &PlayerBoxes::CannonMapRangeMeters(), 50.0f, 1200.0f, "%.0f m");
                        ImGui::Checkbox("Show teammates on map", &PlayerBoxes::CannonMapShowTeammates());

                        ImGui::Separator();
                        ImGui::Checkbox("Predicted impact marker", &PlayerBoxes::CannonImpactMarkerEnabled());
                        if (PlayerBoxes::CannonImpactMarkerEnabled()) {
                            ImGui::SliderFloat("Impact velocity", &PlayerBoxes::CannonImpactVelocity(), 40.0f, 260.0f, "%.0f m/s");
                            ImGui::SliderFloat("Impact gravity", &PlayerBoxes::CannonImpactGravity(), 0.0f, 20.0f, "%.2f");
                        }
                    }
                }

            ImGui::EndTable();
        }

        ImGui::Unindent();
    }

    void DrawAimbotTab() {
        ImGui::Indent();

        ImGui::Checkbox("Enable aimbot", &Aimbot::Enabled());
        ImGui::Checkbox("Hold key to aim", &Aimbot::RequireKey());

        static const char* keyLabels[] = { "RMB", "LMB", "Shift", "Alt", "Ctrl" };
        static const int keyValues[] = { VK_RBUTTON, VK_LBUTTON, VK_SHIFT, VK_MENU, VK_CONTROL };
        int selectedKey = 0;
        for (int i = 0; i < static_cast<int>(IM_ARRAYSIZE(keyValues)); ++i) {
            if (Aimbot::AimKey() == keyValues[i]) {
                selectedKey = i;
                break;
            }
        }

        if (Aimbot::RequireKey()) {
            if (ImGui::Combo("Aim key", &selectedKey, keyLabels, IM_ARRAYSIZE(keyLabels))) {
                Aimbot::AimKey() = keyValues[selectedKey];
            }
        } else {
            ImGui::BeginDisabled();
            ImGui::Combo("Aim key", &selectedKey, keyLabels, IM_ARRAYSIZE(keyLabels));
            ImGui::EndDisabled();
        }

        ImGui::SliderFloat("FOV (pixels)", &Aimbot::FovPixels(), 40.0f, 400.0f, "%.0f");
        ImGui::SliderFloat("Smooth", &Aimbot::Smooth(), 1.0f, 20.0f, "%.1f");

        ImGui::Checkbox("Draw FOV circle", &Aimbot::DrawFovCircle());
        if (Aimbot::DrawFovCircle()) {
            ImGui::ColorEdit3("FOV color", Aimbot::FovColorRgb());
        }

        ImGui::Separator();
        ImGui::Checkbox("Compensate bullet drop", &Aimbot::DropCompensationEnabled());
        if (Aimbot::DropCompensationEnabled()) {
            ImGui::Checkbox("Use weapon ballistics", &Aimbot::UseWeaponBallistics());
            ImGui::SliderFloat("Default velocity", &Aimbot::DefaultMuzzleVelocity(), 40.0f, 300.0f, "%.0f m/s");
            ImGui::SliderFloat("Default gravity", &Aimbot::DefaultGravity(), 0.0f, 20.0f, "%.2f");
            ImGui::Spacing();
            ImGui::TextUnformatted("See Debug > Ballistics for resolved values.");
        }

        static const char* boneLabels[] = { "Head", "Chest", "Pelvis" };
        ImGui::Combo("Target bone", &Aimbot::TargetBone(), boneLabels, IM_ARRAYSIZE(boneLabels));

        static const char* teamFilterItems[] = { "All", "Team only", "Enemy only" };
        ImGui::Combo("Target filter", &Aimbot::TeamFilterMode(), teamFilterItems, IM_ARRAYSIZE(teamFilterItems));
        ImGui::Checkbox("Visibility check (LOS)", &Aimbot::VisibilityCheckEnabled());

        ImGui::Spacing();
        ImGui::Text("Status: %s", Aimbot::LastStatus());

        ImGui::Unindent();
    }

    void DrawMiscTab() {
        ImGui::Indent();

        ImGui::TextUnformatted("Reload");
        ImGui::Separator();
        ImGui::Checkbox("Enable reload speed", &Aimbot::ReloadSpeedEnabled());
        if (Aimbot::ReloadSpeedEnabled()) {
            ImGui::TextUnformatted("Instant reload enabled.");
        }

        ImGui::Spacing();
        ImGui::TextUnformatted("Fire Rate");
        ImGui::Separator();
        ImGui::Checkbox("Enable fire rate", &Aimbot::FireRateEnabled());
        if (Aimbot::FireRateEnabled()) {
            ImGui::TextUnformatted("Maximum rapid-fire mode enabled.");
        }

        ImGui::Spacing();
        ImGui::TextUnformatted("Recoil");
        ImGui::Separator();
        ImGui::Checkbox("Enable no recoil", &Aimbot::NoRecoilEnabled());

        ImGui::Spacing();
        ImGui::TextUnformatted("Accuracy");
        ImGui::Separator();
        ImGui::Checkbox("Enable no spread", &Aimbot::NoSpreadEnabled());
        if (Aimbot::NoSpreadEnabled()) {
            ImGui::TextUnformatted("Horizontal firearm spread is forced to zero.");
        }

        ImGui::Unindent();
    }

    void DrawConfigTab() {
        static char profileNameBuffer[64]{};
        static int selectedProfileIndex = -1;

        std::vector<std::string> profiles = AppConfig::ListProfilesFromDisk();
        if (selectedProfileIndex >= static_cast<int>(profiles.size())) {
            selectedProfileIndex = profiles.empty() ? -1 : 0;
        }

        const std::string active = AppConfig::GetActiveProfileName();
        ImGui::Text("Active profile: %s", active.empty() ? "(none)" : active.c_str());
        if (!AppConfig::LastConfigError().empty()) {
            ImGui::TextWrapped("%s", AppConfig::LastConfigError().c_str());
        }

        ImGui::Separator();
        ImGui::InputText("Profile name", profileNameBuffer, IM_ARRAYSIZE(profileNameBuffer));

        if (ImGui::Button("Save New", ImVec2(120.0f, 0.0f))) {
            if (AppConfig::SaveProfileToDisk(profileNameBuffer)) {
                std::memset(profileNameBuffer, 0, sizeof(profileNameBuffer));
                profiles = AppConfig::ListProfilesFromDisk();
                selectedProfileIndex = profiles.empty() ? -1 : static_cast<int>(profiles.size() - 1);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Overwrite By Name", ImVec2(150.0f, 0.0f))) {
            AppConfig::OverwriteProfileToDisk(profileNameBuffer);
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Saved profiles");
        if (ImGui::BeginListBox("##ConfigProfiles", ImVec2(-FLT_MIN, 220.0f))) {
            for (int i = 0; i < static_cast<int>(profiles.size()); ++i) {
                const bool isSelected = (i == selectedProfileIndex);
                if (ImGui::Selectable(profiles[static_cast<size_t>(i)].c_str(), isSelected)) {
                    selectedProfileIndex = i;
                }
            }
            ImGui::EndListBox();
        }

        const bool hasSelection = selectedProfileIndex >= 0 && selectedProfileIndex < static_cast<int>(profiles.size());
        if (!hasSelection) {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Load Selected", ImVec2(130.0f, 0.0f)) && hasSelection) {
            AppConfig::LoadProfileFromDisk(profiles[static_cast<size_t>(selectedProfileIndex)]);
        }
        ImGui::SameLine();
        if (ImGui::Button("Overwrite Selected", ImVec2(140.0f, 0.0f)) && hasSelection) {
            AppConfig::OverwriteProfileToDisk(profiles[static_cast<size_t>(selectedProfileIndex)]);
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete Selected", ImVec2(130.0f, 0.0f)) && hasSelection) {
            if (AppConfig::DeleteProfileFromDisk(profiles[static_cast<size_t>(selectedProfileIndex)])) {
                profiles = AppConfig::ListProfilesFromDisk();
                if (profiles.empty()) {
                    selectedProfileIndex = -1;
                } else if (selectedProfileIndex >= static_cast<int>(profiles.size())) {
                    selectedProfileIndex = static_cast<int>(profiles.size() - 1);
                }
            }
        }

        if (ImGui::Button("Rename Selected -> Name", ImVec2(220.0f, 0.0f)) && hasSelection) {
            if (AppConfig::RenameProfileOnDisk(profiles[static_cast<size_t>(selectedProfileIndex)], profileNameBuffer)) {
                profiles = AppConfig::ListProfilesFromDisk();
                const std::string activeNow = AppConfig::GetActiveProfileName();
                selectedProfileIndex = -1;
                for (int i = 0; i < static_cast<int>(profiles.size()); ++i) {
                    if (profiles[static_cast<size_t>(i)] == activeNow) {
                        selectedProfileIndex = i;
                        break;
                    }
                }
            }
        }

        if (!hasSelection) {
            ImGui::EndDisabled();
        }
    }

    void ApplyMenuInputCaptureState(bool menuOpen) {
        if (!ImGui::GetCurrentContext()) {
            return;
        }

        if (menuOpen == gMenuInputCaptured) {
            return;
        }

        ImGuiIO& io = ImGui::GetIO();
        if (menuOpen) {
            io.MouseDrawCursor = true;
            ClipCursor(nullptr);
            ReleaseCapture();

            if (!gCursorForcedVisible) {
                while (ShowCursor(TRUE) < 0) {
                }
                gCursorForcedVisible = true;
            }
        } else {
            io.MouseDrawCursor = false;

            if (gCursorForcedVisible) {
                while (ShowCursor(FALSE) >= 0) {
                }
                gCursorForcedVisible = false;
            }
        }

        gMenuInputCaptured = menuOpen;
    }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void GUI::Init(HWND window, IDXGISwapChain* swapChain, ID3D11Device* device, ID3D11DeviceContext* context) {
    if (bInitialized || !window || !swapChain || !device || !context) {
        return;
    }

    gWindow = window;
    gSwapChain = swapChain;
    gDevice = device;
    gContext = context;

    gSwapChain->AddRef();
    gDevice->AddRef();
    gContext->AddRef();
    CreateRenderTarget();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    GUI::UITheme::Apply();

    ImGui_ImplWin32_Init(gWindow);
    ImGui_ImplDX11_Init(gDevice, gContext);

    bInitialized = true;
}

bool GUI::HandleWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!bInitialized || !bMenuOpen) {
        return false;
    }

    ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);

    switch (msg) {
        case WM_INPUT:
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MBUTTONDBLCLK:
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
        case WM_XBUTTONDBLCLK:
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_CHAR:
            return true;
        default:
            return false;
    }
}

void GUI::Render() {
    if (!bInitialized) {
        return;
    }

    static bool wasToggleKeyDown = false;
    const bool toggleKeyDown = (GetAsyncKeyState(AppConfig::menuToggleKey) & 0x8000) != 0;
    if (toggleKeyDown && !wasToggleKeyDown) {
        bMenuOpen = !bMenuOpen;
    }
    wasToggleKeyDown = toggleKeyDown;

    ApplyMenuInputCaptureState(bMenuOpen);

    static bool wasUnloadPrimaryDown = false;
    static bool wasUnloadSecondaryDown = false;
    const bool unloadPrimaryDown = (GetAsyncKeyState(AppConfig::unloadPrimaryKey) & 0x8000) != 0;
    const bool unloadSecondaryDown = (GetAsyncKeyState(AppConfig::unloadSecondaryKey) & 0x8000) != 0;
    if ((unloadPrimaryDown && !wasUnloadPrimaryDown) || (unloadSecondaryDown && !wasUnloadSecondaryDown)) {
        bUnloadRequested = true;
    }
    wasUnloadPrimaryDown = unloadPrimaryDown;
    wasUnloadSecondaryDown = unloadSecondaryDown;

    if (!gMainRenderTargetView) {
        CreateRenderTarget();
    }

    if (gRequestUnloadFromUi) {
        bUnloadRequested = true;
        bMenuOpen = false;
        gRequestUnloadFromUi = false;
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    PlayerBoxes::Render();
    Aimbot::Run(bMenuOpen);
    Experiments::Update();

    if (bMenuOpen) {
        ImGui::SetNextWindowSize(ImVec2(980.0f, 640.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(AppConfig::menuTitle.c_str(), &bMenuOpen, ImGuiWindowFlags_NoCollapse)) {
            if (ImGui::BeginTabBar("RootTabs", ImGuiTabBarFlags_None)) {
                if (ImGui::BeginTabItem("Features")) {
                    if (ImGui::BeginTabBar("FeatureTabs", ImGuiTabBarFlags_None)) {
                        if (ImGui::BeginTabItem("Aimbot")) {
                            DrawAimbotTab();
                            ImGui::EndTabItem();
                        }

                        if (ImGui::BeginTabItem("ESP")) {
                            DrawEspTab();
                            ImGui::EndTabItem();
                        }

                        if (ImGui::BeginTabItem("Misc")) {
                            DrawMiscTab();
                            ImGui::EndTabItem();
                        }

                        ImGui::EndTabBar();
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Debug")) {
                    if (ImGui::BeginTabBar("DebugTabs", ImGuiTabBarFlags_None)) {
                        if (ImGui::BeginTabItem("Overview")) {
                            DrawOverviewTab();
                            ImGui::EndTabItem();
                        }

                        if (ImGui::BeginTabItem("Resolver")) {
                            DrawResolverDataTab();
                            ImGui::EndTabItem();
                        }

                        if (ImGui::BeginTabItem("ESP Debug")) {
                            DrawEspDebugTab();
                            ImGui::EndTabItem();
                        }

                        if (ImGui::BeginTabItem("Ballistics")) {
                            DrawBallisticsDebugTab();
                            ImGui::EndTabItem();
                        }

                        if (ImGui::BeginTabItem("Experiments")) {
                            Experiments::DrawPanel();
                            ImGui::EndTabItem();
                        }

                        if (ImGui::BeginTabItem("Tools")) {
                            DrawToolsTab();
                            ImGui::EndTabItem();
                        }

                        ImGui::EndTabBar();
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Config")) {
                    DrawConfigTab();
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }

    ImGui::Render();
    if (gMainRenderTargetView) {
        gContext->OMSetRenderTargets(1, &gMainRenderTargetView, nullptr);
    }
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void GUI::Shutdown() {
    if (!bInitialized) {
        return;
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (gCursorForcedVisible) {
        while (ShowCursor(FALSE) >= 0) {
        }
        gCursorForcedVisible = false;
    }
    gMenuInputCaptured = false;

    CleanupRenderTarget();

    if (gContext) {
        gContext->Release();
        gContext = nullptr;
    }
    if (gDevice) {
        gDevice->Release();
        gDevice = nullptr;
    }
    if (gSwapChain) {
        gSwapChain->Release();
        gSwapChain = nullptr;
    }
    gWindow = nullptr;

    bInitialized = false;
}

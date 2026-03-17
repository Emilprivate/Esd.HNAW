#include "features/esp/esp_internal.h"

#include "core/hnaw_offsets.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace EspInternal;

namespace {
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
        void* args[2] = { &originCopy, &shortenedTarget };
        bool blocked = false;
        if (!InvokeMethodBool(gPhysicsLinecastMethod, nullptr, args, blocked)) {
            return true;
        }

        return !blocked;
    }
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
    const bool anyEspEnabled = gEnabled || gSkeletonEnabled || gChamsEnabled || gHealthBarEnabled || anyInfoEnabled;
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

    Vec3 localFeet{};
    bool hasLocalFeet = false;
    int localFactionValue = -1;
    int localSquadValue = -1;
    if (manager && HnawOffsets::clientRoundPlayerManagerLocalPlayer && HnawOffsets::roundPlayerPlayerTransform) {
        void* localRoundPlayer = nullptr;
        if (SafeRead(reinterpret_cast<uintptr_t>(manager) + HnawOffsets::clientRoundPlayerManagerLocalPlayer, localRoundPlayer) && localRoundPlayer) {
            void* localTransformObject = nullptr;
            if (SafeRead(reinterpret_cast<uintptr_t>(localRoundPlayer) + HnawOffsets::roundPlayerPlayerTransform, localTransformObject) && localTransformObject) {
                hasLocalFeet = InvokeMethodVec3(gTransformGetPositionMethod, localTransformObject, nullptr, localFeet);
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

    ImDrawList* draw = ImGui::GetBackgroundDrawList();
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

            Vec3 localEye = localFeet;
            localEye.y += 1.62f;
            if (!HasLineOfSight(localEye, head)) {
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


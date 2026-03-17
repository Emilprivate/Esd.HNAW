#pragma once

#include <cstdint>
#include <string>

namespace HnawOffsets {
    inline uintptr_t classClientComponentReferenceManager = 0;
    inline uintptr_t classServerAzureBackendManager = 0;
    inline uintptr_t classClientRoundPlayerManager = 0;
    inline uintptr_t classRoundPlayer = 0;
    inline uintptr_t classPlayerBase = 0;
    inline uintptr_t classPlayerInitialDetails = 0;
    inline uintptr_t classPlayerSpawnData = 0;
    inline uintptr_t classClientWeaponHolder = 0;
    inline uintptr_t classRoundPlayerInformation = 0;
    inline uintptr_t classClientSpectatorManager = 0;
    inline uintptr_t classClientRemoteConsoleAccessManager = 0;
    inline uintptr_t classCamera = 0;
    inline uintptr_t classTransform = 0;
    inline uintptr_t classRaycastHit = 0;
    inline uintptr_t classTime = 0;

    inline uintptr_t clientRoundPlayerManager = 0;
    inline uintptr_t roundPlayerPlayerBase = 0;
    inline uintptr_t roundPlayerPlayerTransform = 0;
    inline uintptr_t roundPlayerNetworkPlayerID = 0;
    inline uintptr_t transformDataPosition = 0;
    inline uintptr_t playerBaseHealth = 0;
    inline uintptr_t playerSpawnDataSquadID = 0;
    inline uintptr_t clientRoundPlayerManagerLocalPlayer = 0;
    inline uintptr_t clientRoundPlayerManagerCurrentRoundPlayerInformation = 0;
    inline uintptr_t roundPlayerInformationInitialDetails = 0;
    inline uintptr_t playerInitialDetailsDisplayname = 0;
    inline uintptr_t playerSpawnDataClassRank = 0;
    inline uintptr_t playerBasePlayerStartData = 0;
    inline uintptr_t playerSpawnDataFaction = 0;
    inline uintptr_t roundPlayerWeaponHolder = 0;
    inline uintptr_t weaponHolderLastFiredTime = 0;
    inline uintptr_t weaponHolderPlayerFirearmAmmoHandler = 0;
    inline uintptr_t clientAdminBroadcastMessageManager = 0;
    inline uintptr_t clientSpectatorManager = 0;
    inline uintptr_t clientRemoteConsoleAccessManager = 0;
    inline uintptr_t clientRPCExecutionManager = 0;
    inline uintptr_t clientSpectatorManagerCurrentlySpectatingPlayer = 0;
    inline uintptr_t playerSpawnDataPlayerActorInitializer = 0;
    inline uintptr_t modelPropertiesModelBonePositions = 0;
    inline uintptr_t modelBonePositionsRootBone = 0;
    inline uintptr_t modelBonePositionsHip = 0;
    inline uintptr_t modelBonePositionsLowerSpine = 0;
    inline uintptr_t modelBonePositionsMiddleSpine = 0;
    inline uintptr_t modelBonePositionsChest = 0;
    inline uintptr_t modelBonePositionsNeck = 0;
    inline uintptr_t modelBonePositionsHead = 0;
    inline uintptr_t modelBonePositionsLeftHand = 0;
    inline uintptr_t modelBonePositionsRightHand = 0;
    inline uintptr_t ownerWeaponHolderQueuedFireFirearm = 0;
    inline uintptr_t ownerWeaponHolderQueuedFirearmReload = 0;

    inline uintptr_t methodGetAllRoundPlayers = 0;
    inline uintptr_t methodCameraGetMain = 0;
    inline uintptr_t methodCameraWorldToScreenPoint = 0;
    inline uintptr_t methodTransformGetPosition = 0;
    inline uintptr_t methodTransformSetLocalScale = 0;
    inline uintptr_t methodRoundPlayerGetPlayerStartData = 0;
    inline uintptr_t methodRoundPlayerSetRotation = 0;
    inline uintptr_t methodRoundPlayerThrowPlayerToFloor = 0;
    inline uintptr_t methodRoundPlayerExecutePlayerAction = 0;
    inline uintptr_t methodPlayerActorInitializerGetCurrentModel = 0;
    inline uintptr_t methodWeaponHolderGetActiveWeaponDetails = 0;
    inline uintptr_t methodPlayerFirearmAmmoHandlerGetEquippedFirearmLoadedAmmo = 0;
    inline uintptr_t methodPlayerFirearmAmmoHandlerGetCanReloadFirearm = 0;
    inline uintptr_t methodPlayerFirearmAmmoHandlerRefillCurrentFirearmAmmo = 0;

    inline uintptr_t hookClientWeaponHolderGetReloadDuration = 0;
    inline uintptr_t hookOwnerWeaponHolderProcessFirearmWeaponInput = 0;
    inline uintptr_t hookOwnerWeaponHolderShootActiveFirearm = 0;
    inline uintptr_t hookWeaponHolderGetCanShootFirearm = 0;
    inline uintptr_t hookPlayerAnimationHandlerGetCanShootFirearm = 0;
    inline uintptr_t hookOwnerWeaponRecoilOnFiringActiveWeapon = 0;
    inline uintptr_t hookClientWeaponHolderCalculateFirearmShotTrajectory = 0;
    inline uintptr_t hookClientCannonInteractableObjectBehaviourGetIsAimingState = 0;
    inline uintptr_t hookClientMoveableCannonInteractableObjectBehaviourGetIsAimingState = 0;
    inline uintptr_t methodOwnerWeaponHolderFinishedPlayerAnimationStateSMBOnFinishedState = 0;
    inline uintptr_t methodOwnerWeaponHolderReadFirearmWeaponInput = 0;
    inline uintptr_t hookServerAzureBackendManagerAuthenticatePlayer = 0;
    inline uintptr_t hookClientConnectionManagerJoinServer = 0;
    inline uintptr_t hookPlayerBaseGetCanRun = 0;
    inline uintptr_t hookUnityDebugLogError = 0;
    inline uintptr_t hookOwnerPacketToServerWriteBitStream = 0;

    inline bool monoAttached = false;
    inline bool autoResolved = false;
    inline int requiredClassCount = 0;
    inline int requiredFieldCount = 0;
    inline int requiredMethodCount = 0;
    inline int resolvedClassCount = 0;
    inline int resolvedFieldCount = 0;
    inline int resolvedMethodCount = 0;
    inline int unresolvedRequiredCount = 0;
    inline std::string status = "Not started";
    inline std::string unresolvedSummary;
}

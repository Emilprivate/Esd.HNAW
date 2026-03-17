#include "core/mono/mono_resolver.h"

#include "core/hnaw_offsets.h"

#include <windows.h>

#include <initializer_list>
#include <cstring>
#include <string>
#include <vector>

struct MonoDomain;
struct MonoAssembly;
struct MonoImage;
struct MonoClass;
struct MonoClassField;
struct MonoMethod;

namespace {
    struct MonoApi {
        HMODULE module = nullptr;

        MonoDomain* (*mono_get_root_domain)() = nullptr;
        void* (*mono_thread_attach)(MonoDomain*) = nullptr;
        MonoAssembly* (*mono_domain_assembly_open)(MonoDomain*, const char*) = nullptr;
        MonoImage* (*mono_assembly_get_image)(MonoAssembly*) = nullptr;
        MonoClass* (*mono_class_from_name)(MonoImage*, const char*, const char*) = nullptr;
        MonoClassField* (*mono_class_get_field_from_name)(MonoClass*, const char*) = nullptr;
        int (*mono_field_get_offset)(MonoClassField*) = nullptr;
        MonoMethod* (*mono_class_get_method_from_name)(MonoClass*, const char*, int) = nullptr;
        void* (*mono_compile_method)(MonoMethod*) = nullptr;
    };

    bool LoadMonoApi(MonoApi& api) {
        api.module = GetModuleHandleA("mono-2.0-bdwgc.dll");
        if (!api.module) {
            api.module = GetModuleHandleA("mono-2.0-sgen.dll");
        }
        if (!api.module) {
            HnawOffsets::status = "Mono runtime module not loaded";
            return false;
        }

        api.mono_get_root_domain = reinterpret_cast<decltype(api.mono_get_root_domain)>(GetProcAddress(api.module, "mono_get_root_domain"));
        api.mono_thread_attach = reinterpret_cast<decltype(api.mono_thread_attach)>(GetProcAddress(api.module, "mono_thread_attach"));
        api.mono_domain_assembly_open = reinterpret_cast<decltype(api.mono_domain_assembly_open)>(GetProcAddress(api.module, "mono_domain_assembly_open"));
        api.mono_assembly_get_image = reinterpret_cast<decltype(api.mono_assembly_get_image)>(GetProcAddress(api.module, "mono_assembly_get_image"));
        api.mono_class_from_name = reinterpret_cast<decltype(api.mono_class_from_name)>(GetProcAddress(api.module, "mono_class_from_name"));
        api.mono_class_get_field_from_name = reinterpret_cast<decltype(api.mono_class_get_field_from_name)>(GetProcAddress(api.module, "mono_class_get_field_from_name"));
        api.mono_field_get_offset = reinterpret_cast<decltype(api.mono_field_get_offset)>(GetProcAddress(api.module, "mono_field_get_offset"));
        api.mono_class_get_method_from_name = reinterpret_cast<decltype(api.mono_class_get_method_from_name)>(GetProcAddress(api.module, "mono_class_get_method_from_name"));
        api.mono_compile_method = reinterpret_cast<decltype(api.mono_compile_method)>(GetProcAddress(api.module, "mono_compile_method"));

        if (!api.mono_get_root_domain || !api.mono_thread_attach || !api.mono_domain_assembly_open ||
            !api.mono_assembly_get_image || !api.mono_class_from_name || !api.mono_class_get_field_from_name ||
            !api.mono_field_get_offset || !api.mono_class_get_method_from_name || !api.mono_compile_method) {
            HnawOffsets::status = "Missing one or more Mono exports";
            return false;
        }

        return true;
    }

    std::string WideToUtf8(const std::wstring& wideText) {
        if (wideText.empty()) {
            return {};
        }

        const int sizeRequired = WideCharToMultiByte(CP_UTF8, 0, wideText.c_str(), static_cast<int>(wideText.size()), nullptr, 0, nullptr, nullptr);
        if (sizeRequired <= 0) {
            return {};
        }

        std::string utf8Text(static_cast<size_t>(sizeRequired), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wideText.c_str(), static_cast<int>(wideText.size()), utf8Text.data(), sizeRequired, nullptr, nullptr);
        return utf8Text;
    }

    std::string BuildManagedAssemblyPathUtf8(const wchar_t* assemblyName) {
        if (!assemblyName || !*assemblyName) {
            return {};
        }

        wchar_t modulePath[MAX_PATH]{};
        const DWORD written = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
        if (written == 0 || written >= MAX_PATH) {
            return {};
        }

        std::wstring path(modulePath, written);
        const size_t slash = path.find_last_of(L"\\/");
        if (slash == std::wstring::npos) {
            return {};
        }

        const std::wstring baseDir = path.substr(0, slash);
        const std::wstring managed = baseDir + L"\\Holdfast NaW_Data\\Managed\\" + assemblyName;
        return WideToUtf8(managed);
    }

    MonoImage* OpenAssemblyImage(const MonoApi& api, MonoDomain* domain, const char* fallbackAssemblyName, const wchar_t* managedAssemblyName) {
        if (!domain) {
            return nullptr;
        }

        const std::string assemblyPath = BuildManagedAssemblyPathUtf8(managedAssemblyName);
        if (!assemblyPath.empty()) {
            MonoAssembly* assembly = api.mono_domain_assembly_open(domain, assemblyPath.c_str());
            if (assembly) {
                return api.mono_assembly_get_image(assembly);
            }
        }

        MonoAssembly* fallbackAssembly = api.mono_domain_assembly_open(domain, fallbackAssemblyName);
        if (!fallbackAssembly) {
            return nullptr;
        }

        return api.mono_assembly_get_image(fallbackAssembly);
    }

    bool ResolveFieldOffset(
        const MonoApi& api,
        MonoImage* image,
        const char* namespaceName,
        const char* className,
        const char* fieldName,
        uintptr_t& outOffset,
        int& resolvedCounter) {

        MonoClass* klass = api.mono_class_from_name(image, namespaceName, className);
        if (!klass) {
            return false;
        }

        MonoClassField* field = api.mono_class_get_field_from_name(klass, fieldName);
        if (!field) {
            return false;
        }

        outOffset = static_cast<uintptr_t>(api.mono_field_get_offset(field));
        ++resolvedCounter;
        return true;
    }

    bool ResolveClassPointer(
        const MonoApi& api,
        MonoImage* image,
        const char* namespaceName,
        const char* className,
        uintptr_t& outClass,
        int& resolvedCounter) {

        MonoClass* klass = api.mono_class_from_name(image, namespaceName, className);
        if (!klass) {
            return false;
        }

        outClass = reinterpret_cast<uintptr_t>(klass);
        ++resolvedCounter;
        return true;
    }

    bool ResolveMethodAddress(
        const MonoApi& api,
        MonoImage* image,
        const char* namespaceName,
        const char* className,
        const char* methodName,
        int paramCount,
        uintptr_t& outAddress,
        int& resolvedCounter) {

        MonoClass* klass = api.mono_class_from_name(image, namespaceName, className);
        if (!klass) {
            return false;
        }

        MonoMethod* method = api.mono_class_get_method_from_name(klass, methodName, paramCount);
        if (!method) {
            return false;
        }

        void* native = api.mono_compile_method(method);
        if (!native) {
            return false;
        }

        outAddress = reinterpret_cast<uintptr_t>(native);
        ++resolvedCounter;
        return true;
    }

    bool ResolveMethodAddressAnyArity(
        const MonoApi& api,
        MonoImage* image,
        const char* namespaceName,
        const char* className,
        const char* methodName,
        std::initializer_list<int> candidateParamCounts,
        uintptr_t& outAddress,
        int& resolvedCounter) {

        for (int arity : candidateParamCounts) {
            if (ResolveMethodAddress(api, image, namespaceName, className, methodName, arity, outAddress, resolvedCounter)) {
                return true;
            }
        }
        return false;
    }

    void AppendMissing(std::vector<std::string>& missing, const std::string& symbolName) {
        missing.push_back(symbolName);
    }

    std::string JoinMissing(const std::vector<std::string>& missing) {
        if (missing.empty()) {
            return {};
        }

        std::string joined;
        for (size_t i = 0; i < missing.size(); ++i) {
            if (i > 0) {
                joined += ", ";
            }
            joined += missing[i];
        }
        return joined;
    }
}

bool MonoResolver::ResolveAll() {
    HnawOffsets::autoResolved = false;
    HnawOffsets::monoAttached = false;
    HnawOffsets::requiredClassCount = 0;
    HnawOffsets::requiredFieldCount = 0;
    HnawOffsets::requiredMethodCount = 0;
    HnawOffsets::resolvedClassCount = 0;
    HnawOffsets::resolvedFieldCount = 0;
    HnawOffsets::resolvedMethodCount = 0;
    HnawOffsets::unresolvedRequiredCount = 0;
    HnawOffsets::unresolvedSummary.clear();
    HnawOffsets::status = "Initializing mono resolver";

    MonoApi api{};
    if (!LoadMonoApi(api)) {
        return false;
    }

    MonoDomain* domain = api.mono_get_root_domain();
    if (!domain) {
        HnawOffsets::status = "mono_get_root_domain failed";
        return false;
    }

    if (!api.mono_thread_attach(domain)) {
        HnawOffsets::status = "mono_thread_attach failed";
        return false;
    }

    HnawOffsets::monoAttached = true;

    MonoImage* gameImage = OpenAssemblyImage(api, domain, "Assembly-CSharp.dll", L"Assembly-CSharp.dll");
    if (!gameImage) {
        HnawOffsets::status = "Unable to open Assembly-CSharp image";
        return false;
    }

    MonoImage* unityImage = OpenAssemblyImage(api, domain, "UnityEngine.CoreModule.dll", L"UnityEngine.CoreModule.dll");

    auto SelectImage = [&](const char* namespaceName) -> MonoImage* {
        if (namespaceName && std::strcmp(namespaceName, "UnityEngine") == 0) {
            return unityImage ? unityImage : gameImage;
        }
        return gameImage;
    };

    std::vector<std::string> missingRequired;

    auto resolveClassRequired = [&](const char* namespaceName, const char* className, uintptr_t& output, const char* label) {
        ++HnawOffsets::requiredClassCount;
        if (!ResolveClassPointer(api, SelectImage(namespaceName), namespaceName, className, output, HnawOffsets::resolvedClassCount)) {
            AppendMissing(missingRequired, label);
        }
    };

    auto resolveFieldRequired = [&](const char* namespaceName, const char* className, const char* fieldName, uintptr_t& output, const char* label) {
        ++HnawOffsets::requiredFieldCount;
        if (!ResolveFieldOffset(api, SelectImage(namespaceName), namespaceName, className, fieldName, output, HnawOffsets::resolvedFieldCount)) {
            AppendMissing(missingRequired, label);
        }
    };

    auto resolveFieldAnyNameRequired = [&](const char* namespaceName, const char* className, std::initializer_list<const char*> fieldNames, uintptr_t& output, const char* label) {
        ++HnawOffsets::requiredFieldCount;
        bool resolved = false;
        for (const char* fieldName : fieldNames) {
            if (ResolveFieldOffset(api, SelectImage(namespaceName), namespaceName, className, fieldName, output, HnawOffsets::resolvedFieldCount)) {
                resolved = true;
                break;
            }
        }
        if (!resolved) {
            AppendMissing(missingRequired, label);
        }
    };

    auto resolveMethodRequired = [&](const char* namespaceName, const char* className, const char* methodName, int paramCount, uintptr_t& output, const char* label) {
        ++HnawOffsets::requiredMethodCount;
        if (!ResolveMethodAddress(api, SelectImage(namespaceName), namespaceName, className, methodName, paramCount, output, HnawOffsets::resolvedMethodCount)) {
            AppendMissing(missingRequired, label);
        }
    };

    auto resolveMethodAnyRequired = [&](const char* namespaceName, const char* className, const char* methodName, std::initializer_list<int> arities, uintptr_t& output, const char* label) {
        ++HnawOffsets::requiredMethodCount;
        if (!ResolveMethodAddressAnyArity(api, SelectImage(namespaceName), namespaceName, className, methodName, arities, output, HnawOffsets::resolvedMethodCount)) {
            AppendMissing(missingRequired, label);
        }
    };

    resolveClassRequired("HoldfastGame", "ClientComponentReferenceManager", HnawOffsets::classClientComponentReferenceManager, "Class: HoldfastGame.ClientComponentReferenceManager");
    resolveClassRequired("HoldfastGame", "ServerAzureBackendManager", HnawOffsets::classServerAzureBackendManager, "Class: HoldfastGame.ServerAzureBackendManager");
    resolveClassRequired("HoldfastGame", "ClientRoundPlayerManager", HnawOffsets::classClientRoundPlayerManager, "Class: HoldfastGame.ClientRoundPlayerManager");
    resolveClassRequired("HoldfastGame", "RoundPlayer", HnawOffsets::classRoundPlayer, "Class: HoldfastGame.RoundPlayer");
    resolveClassRequired("HoldfastGame", "PlayerBase", HnawOffsets::classPlayerBase, "Class: HoldfastGame.PlayerBase");
    resolveClassRequired("HoldfastGame", "PlayerInitialDetails", HnawOffsets::classPlayerInitialDetails, "Class: HoldfastGame.PlayerInitialDetails");
    resolveClassRequired("HoldfastGame", "PlayerSpawnData", HnawOffsets::classPlayerSpawnData, "Class: HoldfastGame.PlayerSpawnData");
    resolveClassRequired("HoldfastGame", "ClientWeaponHolder", HnawOffsets::classClientWeaponHolder, "Class: HoldfastGame.ClientWeaponHolder");
    resolveClassRequired("HoldfastGame", "RoundPlayerInformation", HnawOffsets::classRoundPlayerInformation, "Class: HoldfastGame.RoundPlayerInformation");
    resolveClassRequired("HoldfastGame", "ClientSpectatorManager", HnawOffsets::classClientSpectatorManager, "Class: HoldfastGame.ClientSpectatorManager");
    resolveClassRequired("HoldfastGame", "ClientRemoteConsoleAccessManager", HnawOffsets::classClientRemoteConsoleAccessManager, "Class: HoldfastGame.ClientRemoteConsoleAccessManager");
    resolveClassRequired("UnityEngine", "Camera", HnawOffsets::classCamera, "Class: UnityEngine.Camera");
    resolveClassRequired("UnityEngine", "Transform", HnawOffsets::classTransform, "Class: UnityEngine.Transform");
    resolveClassRequired("UnityEngine", "RaycastHit", HnawOffsets::classRaycastHit, "Class: UnityEngine.RaycastHit");
    resolveClassRequired("UnityEngine", "Time", HnawOffsets::classTime, "Class: UnityEngine.Time");

    resolveFieldRequired("HoldfastGame", "ClientComponentReferenceManager", "clientRoundPlayerManager", HnawOffsets::clientRoundPlayerManager, "Field: ClientComponentReferenceManager.clientRoundPlayerManager");
    resolveFieldRequired("HoldfastGame", "RoundPlayer", "PlayerBase", HnawOffsets::roundPlayerPlayerBase, "Field: RoundPlayer.PlayerBase");
    resolveFieldRequired("HoldfastGame", "RoundPlayer", "PlayerTransform", HnawOffsets::roundPlayerPlayerTransform, "Field: RoundPlayer.PlayerTransform");
    resolveFieldRequired("HoldfastGame", "RoundPlayer", "NetworkPlayerID", HnawOffsets::roundPlayerNetworkPlayerID, "Field: RoundPlayer.NetworkPlayerID");
    resolveFieldRequired("HoldfastGame", "TransformData", "position", HnawOffsets::transformDataPosition, "Field: TransformData.position");
    resolveFieldRequired("HoldfastGame", "PlayerBase", "<Health>k__BackingField", HnawOffsets::playerBaseHealth, "Field: PlayerBase.<Health>k__BackingField");
    resolveFieldRequired("HoldfastGame", "PlayerSpawnData", "SquadID", HnawOffsets::playerSpawnDataSquadID, "Field: PlayerSpawnData.SquadID");
    resolveFieldRequired("HoldfastGame", "ClientRoundPlayerManager", "LocalPlayer", HnawOffsets::clientRoundPlayerManagerLocalPlayer, "Field: ClientRoundPlayerManager.LocalPlayer");
    resolveFieldRequired("HoldfastGame", "ClientRoundPlayerManager", "CurrentRoundPlayerInformation", HnawOffsets::clientRoundPlayerManagerCurrentRoundPlayerInformation, "Field: ClientRoundPlayerManager.CurrentRoundPlayerInformation");
    resolveFieldRequired("HoldfastGame", "RoundPlayerInformation", "InitialDetails", HnawOffsets::roundPlayerInformationInitialDetails, "Field: RoundPlayerInformation.InitialDetails");
    resolveFieldRequired("HoldfastGame", "PlayerInitialDetails", "displayname", HnawOffsets::playerInitialDetailsDisplayname, "Field: PlayerInitialDetails.displayname");
    resolveFieldRequired("HoldfastGame", "PlayerSpawnData", "ClassRank", HnawOffsets::playerSpawnDataClassRank, "Field: PlayerSpawnData.ClassRank");
    resolveFieldRequired("HoldfastGame", "PlayerBase", "<PlayerStartData>k__BackingField", HnawOffsets::playerBasePlayerStartData, "Field: PlayerBase.<PlayerStartData>k__BackingField");
    resolveFieldRequired("HoldfastGame", "PlayerSpawnData", "Faction", HnawOffsets::playerSpawnDataFaction, "Field: PlayerSpawnData.Faction");
    resolveFieldRequired("HoldfastGame", "RoundPlayer", "WeaponHolder", HnawOffsets::roundPlayerWeaponHolder, "Field: RoundPlayer.WeaponHolder");
    resolveFieldAnyNameRequired("HoldfastGame", "WeaponHolder", { "lastFiredTime", "<lastFiredTime>k__BackingField" }, HnawOffsets::weaponHolderLastFiredTime, "Field: WeaponHolder.lastFiredTime");
    resolveFieldAnyNameRequired("HoldfastGame", "WeaponHolder", { "playerFirearmAmmoHandler", "<playerFirearmAmmoHandler>k__BackingField" }, HnawOffsets::weaponHolderPlayerFirearmAmmoHandler, "Field: WeaponHolder.playerFirearmAmmoHandler");
    resolveFieldRequired("HoldfastGame", "ClientComponentReferenceManager", "clientAdminBroadcastMessageManager", HnawOffsets::clientAdminBroadcastMessageManager, "Field: ClientComponentReferenceManager.clientAdminBroadcastMessageManager");
    resolveFieldRequired("HoldfastGame", "ClientComponentReferenceManager", "clientSpectatorManager", HnawOffsets::clientSpectatorManager, "Field: ClientComponentReferenceManager.clientSpectatorManager");
    resolveFieldRequired("HoldfastGame", "ClientComponentReferenceManager", "clientRemoteConsoleAccessManager", HnawOffsets::clientRemoteConsoleAccessManager, "Field: ClientComponentReferenceManager.clientRemoteConsoleAccessManager");
    resolveFieldRequired("HoldfastGame", "ClientComponentReferenceManager", "clientRPCExecutionManager", HnawOffsets::clientRPCExecutionManager, "Field: ClientComponentReferenceManager.clientRPCExecutionManager");
    resolveFieldRequired("HoldfastGame", "ClientSpectatorManager", "currentlySpectatingPlayer", HnawOffsets::clientSpectatorManagerCurrentlySpectatingPlayer, "Field: ClientSpectatorManager.currentlySpectatingPlayer");
    ++HnawOffsets::requiredFieldCount;
    if (!ResolveFieldOffset(api, SelectImage("HoldfastGame"), "HoldfastGame", "PlayerSpawnData", "PlayerActorInitializer", HnawOffsets::playerSpawnDataPlayerActorInitializer, HnawOffsets::resolvedFieldCount) &&
        !ResolveFieldOffset(api, SelectImage("HoldfastGame"), "HoldfastGame", "PlayerSpawnData", "<PlayerActorInitializer>k__BackingField", HnawOffsets::playerSpawnDataPlayerActorInitializer, HnawOffsets::resolvedFieldCount) &&
        !ResolveFieldOffset(api, SelectImage("HoldfastGame"), "HoldfastGame", "PlayerStartData", "PlayerActorInitializer", HnawOffsets::playerSpawnDataPlayerActorInitializer, HnawOffsets::resolvedFieldCount) &&
        !ResolveFieldOffset(api, SelectImage("HoldfastGame"), "HoldfastGame", "PlayerStartData", "<PlayerActorInitializer>k__BackingField", HnawOffsets::playerSpawnDataPlayerActorInitializer, HnawOffsets::resolvedFieldCount)) {
        AppendMissing(missingRequired, "Field: PlayerSpawnData/PlayerStartData.PlayerActorInitializer");
    }

    resolveFieldAnyNameRequired("HoldfastGame", "ModelProperties", { "modelBonePositions", "<modelBonePositions>k__BackingField" }, HnawOffsets::modelPropertiesModelBonePositions, "Field: ModelProperties.modelBonePositions");
    resolveFieldRequired("HoldfastGame", "ModelBonePositions", "rootBone", HnawOffsets::modelBonePositionsRootBone, "Field: ModelBonePositions.rootBone");
    resolveFieldRequired("HoldfastGame", "ModelBonePositions", "hip", HnawOffsets::modelBonePositionsHip, "Field: ModelBonePositions.hip");
    resolveFieldRequired("HoldfastGame", "ModelBonePositions", "lowerSpine", HnawOffsets::modelBonePositionsLowerSpine, "Field: ModelBonePositions.lowerSpine");
    resolveFieldRequired("HoldfastGame", "ModelBonePositions", "middleSpine", HnawOffsets::modelBonePositionsMiddleSpine, "Field: ModelBonePositions.middleSpine");
    resolveFieldRequired("HoldfastGame", "ModelBonePositions", "chest", HnawOffsets::modelBonePositionsChest, "Field: ModelBonePositions.chest");
    resolveFieldRequired("HoldfastGame", "ModelBonePositions", "neck", HnawOffsets::modelBonePositionsNeck, "Field: ModelBonePositions.neck");
    resolveFieldRequired("HoldfastGame", "ModelBonePositions", "head", HnawOffsets::modelBonePositionsHead, "Field: ModelBonePositions.head");
    resolveFieldRequired("HoldfastGame", "ModelBonePositions", "leftHand", HnawOffsets::modelBonePositionsLeftHand, "Field: ModelBonePositions.leftHand");
    resolveFieldRequired("HoldfastGame", "ModelBonePositions", "rightHand", HnawOffsets::modelBonePositionsRightHand, "Field: ModelBonePositions.rightHand");
    resolveFieldAnyNameRequired("HoldfastGame", "OwnerWeaponHolder", { "queuedFireFirearm", "<queuedFireFirearm>k__BackingField" }, HnawOffsets::ownerWeaponHolderQueuedFireFirearm, "Field: OwnerWeaponHolder.queuedFireFirearm");
    resolveFieldAnyNameRequired("HoldfastGame", "OwnerWeaponHolder", { "queuedFirearmReload", "<queuedFirearmReload>k__BackingField" }, HnawOffsets::ownerWeaponHolderQueuedFirearmReload, "Field: OwnerWeaponHolder.queuedFirearmReload");

    resolveMethodRequired("HoldfastGame", "ClientRoundPlayerManager", "GetAllRoundPlayers", 0, HnawOffsets::methodGetAllRoundPlayers, "Method: ClientRoundPlayerManager.GetAllRoundPlayers");
    resolveMethodRequired("UnityEngine", "Camera", "get_main", 0, HnawOffsets::methodCameraGetMain, "Method: Camera.get_main");
    resolveMethodAnyRequired("UnityEngine", "Camera", "WorldToScreenPoint", { 1, 2 }, HnawOffsets::methodCameraWorldToScreenPoint, "Method: Camera.WorldToScreenPoint");
    resolveMethodRequired("UnityEngine", "Transform", "get_position", 0, HnawOffsets::methodTransformGetPosition, "Method: Transform.get_position");
    resolveMethodRequired("UnityEngine", "Transform", "set_localScale", 1, HnawOffsets::methodTransformSetLocalScale, "Method: Transform.set_localScale");
    resolveMethodRequired("HoldfastGame", "RoundPlayer", "get_PlayerStartData", 0, HnawOffsets::methodRoundPlayerGetPlayerStartData, "Method: RoundPlayer.get_PlayerStartData");
    resolveMethodAnyRequired("HoldfastGame", "RoundPlayer", "SetRotation", { 1, 2, 3 }, HnawOffsets::methodRoundPlayerSetRotation, "Method: RoundPlayer.SetRotation");
    resolveMethodRequired("HoldfastGame", "RoundPlayer", "ThrowPlayerToFloor_func", 0, HnawOffsets::methodRoundPlayerThrowPlayerToFloor, "Method: RoundPlayer.ThrowPlayerToFloor_func");
    resolveMethodAnyRequired("HoldfastGame", "RoundPlayer", "ExecutePlayerAction", { 1, 2, 3 }, HnawOffsets::methodRoundPlayerExecutePlayerAction, "Method: RoundPlayer.ExecutePlayerAction");
    resolveMethodRequired("HoldfastGame", "PlayerActorInitializer", "get_CurrentModel", 0, HnawOffsets::methodPlayerActorInitializerGetCurrentModel, "Method: PlayerActorInitializer.get_CurrentModel");
    resolveMethodAnyRequired("HoldfastGame", "ClientWeaponHolder", "get_ActiveWeaponDetails", { 0 }, HnawOffsets::methodWeaponHolderGetActiveWeaponDetails, "Method: ClientWeaponHolder.get_ActiveWeaponDetails");
    resolveMethodAnyRequired("HoldfastGame", "PlayerFirearmAmmoHandler", "get_EquippedFirearmLoadedAmmo", { 0, 1 }, HnawOffsets::methodPlayerFirearmAmmoHandlerGetEquippedFirearmLoadedAmmo, "Method: PlayerFirearmAmmoHandler.get_EquippedFirearmLoadedAmmo");
    resolveMethodAnyRequired("HoldfastGame", "PlayerFirearmAmmoHandler", "get_CanReloadFirearm", { 0, 1 }, HnawOffsets::methodPlayerFirearmAmmoHandlerGetCanReloadFirearm, "Method: PlayerFirearmAmmoHandler.get_CanReloadFirearm");
    resolveMethodAnyRequired("HoldfastGame", "PlayerFirearmAmmoHandler", "RefillCurrentFirearmAmmo", { 0, 1, 2 }, HnawOffsets::methodPlayerFirearmAmmoHandlerRefillCurrentFirearmAmmo, "Method: PlayerFirearmAmmoHandler.RefillCurrentFirearmAmmo");

    resolveMethodAnyRequired("HoldfastGame", "ClientWeaponHolder", "GetReloadDuration", { 0, 1, 2 }, HnawOffsets::hookClientWeaponHolderGetReloadDuration, "Hook: ClientWeaponHolder.GetReloadDuration");
    resolveMethodAnyRequired("HoldfastGame", "OwnerWeaponHolder", "ProcessFirearmWeaponInput", { 2, 3, 4 }, HnawOffsets::hookOwnerWeaponHolderProcessFirearmWeaponInput, "Hook: OwnerWeaponHolder.ProcessFirearmWeaponInput");
    resolveMethodAnyRequired("HoldfastGame", "OwnerWeaponHolder", "_ShootActiveFirearm", { 1, 2 }, HnawOffsets::hookOwnerWeaponHolderShootActiveFirearm, "Hook: OwnerWeaponHolder._ShootActiveFirearm");
    resolveMethodAnyRequired("HoldfastGame", "WeaponHolder", "get_CanShootFirearm", { 0, 1 }, HnawOffsets::hookWeaponHolderGetCanShootFirearm, "Hook: WeaponHolder.get_CanShootFirearm");
    resolveMethodAnyRequired("HoldfastGame", "PlayerAnimationHandler", "get_CanShootFirearm", { 0, 1 }, HnawOffsets::hookPlayerAnimationHandlerGetCanShootFirearm, "Hook: PlayerAnimationHandler.get_CanShootFirearm");
    resolveMethodAnyRequired("HoldfastGame", "OwnerWeaponRecoil", "OnFiringActiveWeapon", { 0, 1 }, HnawOffsets::hookOwnerWeaponRecoilOnFiringActiveWeapon, "Hook: OwnerWeaponRecoil.OnFiringActiveWeapon");
    resolveMethodAnyRequired("HoldfastGame", "ClientWeaponHolder", "CalculateFirearmShotTrajectory", { 7, 8 }, HnawOffsets::hookClientWeaponHolderCalculateFirearmShotTrajectory, "Hook: ClientWeaponHolder.CalculateFirearmShotTrajectory");
    resolveMethodAnyRequired("HoldfastGame", "ClientCannonInteractableObjectBehaviour", "get_IsAimingState", { 0, 1 }, HnawOffsets::hookClientCannonInteractableObjectBehaviourGetIsAimingState, "Hook: ClientCannonInteractableObjectBehaviour.get_IsAimingState");
    resolveMethodAnyRequired("HoldfastGame", "ClientMoveableCannonInteractableObjectBehaviour", "get_IsAimingState", { 0, 1 }, HnawOffsets::hookClientMoveableCannonInteractableObjectBehaviourGetIsAimingState, "Hook: ClientMoveableCannonInteractableObjectBehaviour.get_IsAimingState");
    resolveMethodAnyRequired("HoldfastGame", "OwnerWeaponHolder", "ReadFirearmWeaponInput", { 2, 3, 4 }, HnawOffsets::methodOwnerWeaponHolderReadFirearmWeaponInput, "Method: OwnerWeaponHolder.ReadFirearmWeaponInput");
    resolveMethodAnyRequired("HoldfastGame", "OwnerWeaponHolder", "FinishedPlayerAnimationStateSMB_OnFinishedState", { 2, 3 }, HnawOffsets::methodOwnerWeaponHolderFinishedPlayerAnimationStateSMBOnFinishedState, "Method: OwnerWeaponHolder.FinishedPlayerAnimationStateSMB_OnFinishedState");
    resolveMethodAnyRequired("HoldfastGame", "ServerAzureBackendManager", "AuthenticatePlayer", { 0, 1, 2, 3, 4, 5 }, HnawOffsets::hookServerAzureBackendManagerAuthenticatePlayer, "Hook: ServerAzureBackendManager.AuthenticatePlayer");
    resolveMethodAnyRequired("HoldfastGame", "ClientConnectionManager", "JoinServer", { 0, 1, 2, 3, 4, 5 }, HnawOffsets::hookClientConnectionManagerJoinServer, "Hook: ClientConnectionManager.JoinServer");
    resolveMethodAnyRequired("HoldfastGame", "PlayerBase", "get_CanRun", { 0 }, HnawOffsets::hookPlayerBaseGetCanRun, "Hook: PlayerBase.get_CanRun");
    resolveMethodAnyRequired("UnityEngine", "Debug", "LogError", { 1, 2, 3 }, HnawOffsets::hookUnityDebugLogError, "Hook: UnityEngine.Debug.LogError");
    resolveMethodAnyRequired("HoldfastGame", "OwnerPacketToServer", "WriteBitStream", { 0, 1, 2, 3, 4 }, HnawOffsets::hookOwnerPacketToServerWriteBitStream, "Hook: OwnerPacketToServer.WriteBitStream");

    HnawOffsets::unresolvedRequiredCount = static_cast<int>(missingRequired.size());
    HnawOffsets::unresolvedSummary = JoinMissing(missingRequired);

    const bool allClassesResolved = (HnawOffsets::resolvedClassCount == HnawOffsets::requiredClassCount);
    const bool allFieldsResolved = (HnawOffsets::resolvedFieldCount == HnawOffsets::requiredFieldCount);
    const bool allMethodsResolved = (HnawOffsets::resolvedMethodCount == HnawOffsets::requiredMethodCount);
    HnawOffsets::autoResolved = allClassesResolved && allFieldsResolved && allMethodsResolved;

    if (HnawOffsets::autoResolved) {
        HnawOffsets::status = "Mono resolver ready (all required symbols resolved)";
    } else {
        HnawOffsets::status = "Mono resolver partial (some required symbols missing)";
    }

    return HnawOffsets::autoResolved;
}

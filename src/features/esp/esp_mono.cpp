#include "features/esp/esp_internal.h"

#include "core/hnaw_offsets.h"

#include <cmath>
#include <string>
#include <vector>

namespace EspInternal {
    static bool IsValidVec3(const Vec3& value) {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }


    static std::string WideToUtf8(const std::wstring& wideText) {
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

    static std::string BuildManagedAssemblyPathUtf8(const wchar_t* assemblyName) {
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

    static MonoImage* OpenAssemblyImage(const char* fallbackAssemblyName, const wchar_t* managedAssemblyName) {
        if (!gDomain) {
            return nullptr;
        }

        const std::string assemblyPath = BuildManagedAssemblyPathUtf8(managedAssemblyName);
        if (!assemblyPath.empty()) {
            MonoAssembly* assembly = gMonoApi.monoDomainAssemblyOpen(gDomain, assemblyPath.c_str());
            if (assembly) {
                return gMonoApi.monoAssemblyGetImage(assembly);
            }
        }

        MonoAssembly* fallbackAssembly = gMonoApi.monoDomainAssemblyOpen(gDomain, fallbackAssemblyName);
        if (!fallbackAssembly) {
            return nullptr;
        }

        return gMonoApi.monoAssemblyGetImage(fallbackAssembly);
    }

    static bool LoadMonoApi() {
        if (gMonoApiLoaded) {
            return true;
        }
        if (gMonoApiFailed) {
            return false;
        }

        HMODULE monoModule = GetModuleHandleA("mono-2.0-bdwgc.dll");
        if (!monoModule) {
            monoModule = GetModuleHandleA("mono-2.0-sgen.dll");
        }
        if (!monoModule) {
            gMonoApiFailed = true;
            return false;
        }

        gMonoApi.monoGetRootDomain = reinterpret_cast<MonoGetRootDomainFn>(GetProcAddress(monoModule, "mono_get_root_domain"));
        gMonoApi.monoThreadAttach = reinterpret_cast<MonoThreadAttachFn>(GetProcAddress(monoModule, "mono_thread_attach"));
        gMonoApi.monoDomainAssemblyOpen = reinterpret_cast<MonoDomainAssemblyOpenFn>(GetProcAddress(monoModule, "mono_domain_assembly_open"));
        gMonoApi.monoAssemblyGetImage = reinterpret_cast<MonoAssemblyGetImageFn>(GetProcAddress(monoModule, "mono_assembly_get_image"));
        gMonoApi.monoClassFromName = reinterpret_cast<MonoClassFromNameFn>(GetProcAddress(monoModule, "mono_class_from_name"));
        gMonoApi.monoObjectGetClass = reinterpret_cast<MonoObjectGetClassFn>(GetProcAddress(monoModule, "mono_object_get_class"));
        gMonoApi.monoClassGetParent = reinterpret_cast<MonoClassGetParentFn>(GetProcAddress(monoModule, "mono_class_get_parent"));
        gMonoApi.monoClassGetFieldFromName = reinterpret_cast<MonoClassGetFieldFromNameFn>(GetProcAddress(monoModule, "mono_class_get_field_from_name"));
        gMonoApi.monoFieldGetValue = reinterpret_cast<MonoFieldGetValueFn>(GetProcAddress(monoModule, "mono_field_get_value"));
        gMonoApi.monoClassGetMethodFromName = reinterpret_cast<MonoClassGetMethodFromNameFn>(GetProcAddress(monoModule, "mono_class_get_method_from_name"));
        gMonoApi.monoClassVTable = reinterpret_cast<MonoClassVTableFn>(GetProcAddress(monoModule, "mono_class_vtable"));
        gMonoApi.monoFieldStaticGetValue = reinterpret_cast<MonoFieldStaticGetValueFn>(GetProcAddress(monoModule, "mono_field_static_get_value"));
        gMonoApi.monoRuntimeInvoke = reinterpret_cast<MonoRuntimeInvokeFn>(GetProcAddress(monoModule, "mono_runtime_invoke"));
        gMonoApi.monoObjectUnbox = reinterpret_cast<MonoObjectUnboxFn>(GetProcAddress(monoModule, "mono_object_unbox"));
        gMonoApi.monoStringNew = reinterpret_cast<MonoStringNewFn>(GetProcAddress(monoModule, "mono_string_new"));

        if (!gMonoApi.monoGetRootDomain || !gMonoApi.monoThreadAttach || !gMonoApi.monoDomainAssemblyOpen ||
            !gMonoApi.monoAssemblyGetImage || !gMonoApi.monoClassFromName || !gMonoApi.monoObjectGetClass || !gMonoApi.monoClassGetParent || !gMonoApi.monoClassGetFieldFromName || !gMonoApi.monoFieldGetValue ||
            !gMonoApi.monoClassGetMethodFromName || !gMonoApi.monoClassVTable || !gMonoApi.monoFieldStaticGetValue ||
            !gMonoApi.monoRuntimeInvoke || !gMonoApi.monoObjectUnbox) {
            gMonoApiFailed = true;
            return false;
        }

        gMonoApiLoaded = true;
        return true;
    }

    bool EnsureMonoSymbols() {
        if (gMonoSymbolsReady) {
            return true;
        }
        if (gMonoSymbolsAttempted) {
            return false;
        }
        gMonoSymbolsAttempted = true;

        if (!LoadMonoApi()) {
            return false;
        }

        gDomain = gMonoApi.monoGetRootDomain();
        if (!gDomain) {
            return false;
        }
        gMonoApi.monoThreadAttach(gDomain);

        gGameImage = OpenAssemblyImage("Assembly-CSharp.dll", L"Assembly-CSharp.dll");
        gUnityImage = OpenAssemblyImage("UnityEngine.CoreModule.dll", L"UnityEngine.CoreModule.dll");
        gUnityPhysicsImage = OpenAssemblyImage("UnityEngine.PhysicsModule.dll", L"UnityEngine.PhysicsModule.dll");
        if (!gGameImage) {
            return false;
        }

        gClientComponentReferenceManagerClass = gMonoApi.monoClassFromName(gGameImage, "HoldfastGame", "ClientComponentReferenceManager");
        gClientRoundPlayerManagerClass = gMonoApi.monoClassFromName(gGameImage, "HoldfastGame", "ClientRoundPlayerManager");
        gCameraClass = gMonoApi.monoClassFromName(gUnityImage ? gUnityImage : gGameImage, "UnityEngine", "Camera");
        gTransformClass = gMonoApi.monoClassFromName(gUnityImage ? gUnityImage : gGameImage, "UnityEngine", "Transform");
        gAnimatorClass = gMonoApi.monoClassFromName(gUnityImage ? gUnityImage : gGameImage, "UnityEngine", "Animator");
        gRendererClass = gMonoApi.monoClassFromName(gUnityImage ? gUnityImage : gGameImage, "UnityEngine", "Renderer");
        gMaterialClass = gMonoApi.monoClassFromName(gUnityImage ? gUnityImage : gGameImage, "UnityEngine", "Material");
        gShaderClass = gMonoApi.monoClassFromName(gUnityImage ? gUnityImage : gGameImage, "UnityEngine", "Shader");
        gPhysicsClass = gMonoApi.monoClassFromName(gUnityPhysicsImage ? gUnityPhysicsImage : (gUnityImage ? gUnityImage : gGameImage), "UnityEngine", "Physics");
        gPlayerActorInitializerClass = gMonoApi.monoClassFromName(gGameImage, "HoldfastGame", "PlayerActorInitializer");
        gPlayerSpawnDataClass = gMonoApi.monoClassFromName(gGameImage, "HoldfastGame", "PlayerSpawnData");
        gPlayerStartDataClass = gMonoApi.monoClassFromName(gGameImage, "HoldfastGame", "PlayerStartData");
        gModelBonePositionsClass = gMonoApi.monoClassFromName(gGameImage, "HoldfastGame", "ModelBonePositions");
        gRoundPlayerClass = gMonoApi.monoClassFromName(gGameImage, "HoldfastGame", "RoundPlayer");
        if (!gClientComponentReferenceManagerClass || !gClientRoundPlayerManagerClass || !gCameraClass || !gTransformClass) {
            return false;
        }

        gClientRoundPlayerManagerField = gMonoApi.monoClassGetFieldFromName(gClientComponentReferenceManagerClass, "clientRoundPlayerManager");
        gGetAllRoundPlayersMethod = gMonoApi.monoClassGetMethodFromName(gClientRoundPlayerManagerClass, "GetAllRoundPlayers", 0);
        gClientRoundPlayerManagerGetInstanceMethod = gMonoApi.monoClassGetMethodFromName(gClientRoundPlayerManagerClass, "get_Instance", 0);
        if (!gClientRoundPlayerManagerGetInstanceMethod) {
            gClientRoundPlayerManagerGetInstanceMethod = gMonoApi.monoClassGetMethodFromName(gClientRoundPlayerManagerClass, "GetInstance", 0);
        }
        if (!gClientRoundPlayerManagerGetInstanceMethod) {
            gClientRoundPlayerManagerGetInstanceMethod = gMonoApi.monoClassGetMethodFromName(gClientRoundPlayerManagerClass, "get_Current", 0);
        }
        gClientComponentReferenceManagerGetInstanceMethod = gMonoApi.monoClassGetMethodFromName(gClientComponentReferenceManagerClass, "get_Instance", 0);
        if (!gClientComponentReferenceManagerGetInstanceMethod) {
            gClientComponentReferenceManagerGetInstanceMethod = gMonoApi.monoClassGetMethodFromName(gClientComponentReferenceManagerClass, "GetInstance", 0);
        }
        if (!gClientComponentReferenceManagerGetInstanceMethod) {
            gClientComponentReferenceManagerGetInstanceMethod = gMonoApi.monoClassGetMethodFromName(gClientComponentReferenceManagerClass, "get_Current", 0);
        }
        gCameraGetMainMethod = gMonoApi.monoClassGetMethodFromName(gCameraClass, "get_main", 0);
        gCameraWorldToScreenPointMethod = gMonoApi.monoClassGetMethodFromName(gCameraClass, "WorldToScreenPoint", 1);
        gTransformGetPositionMethod = gMonoApi.monoClassGetMethodFromName(gTransformClass, "get_position", 0);
        if (gAnimatorClass) {
            gAnimatorGetBoneTransformMethod = gMonoApi.monoClassGetMethodFromName(gAnimatorClass, "GetBoneTransform", 1);
        }
        if (gRendererClass) {
            gRendererGetMaterialMethod = gMonoApi.monoClassGetMethodFromName(gRendererClass, "get_material", 0);
            gRendererGetSharedMaterialMethod = gMonoApi.monoClassGetMethodFromName(gRendererClass, "get_sharedMaterial", 0);
            gRendererGetMaterialsMethod = gMonoApi.monoClassGetMethodFromName(gRendererClass, "get_materials", 0);
            gRendererGetSharedMaterialsMethod = gMonoApi.monoClassGetMethodFromName(gRendererClass, "get_sharedMaterials", 0);
            gRendererSetShadowCastingModeMethod = gMonoApi.monoClassGetMethodFromName(gRendererClass, "set_shadowCastingMode", 1);
            gRendererSetReceiveShadowsMethod = gMonoApi.monoClassGetMethodFromName(gRendererClass, "set_receiveShadows", 1);
        }
        if (gMaterialClass) {
            gMaterialSetColorMethod = gMonoApi.monoClassGetMethodFromName(gMaterialClass, "set_color", 1);
            gMaterialSetColorByNameMethod = gMonoApi.monoClassGetMethodFromName(gMaterialClass, "SetColor", 2);
            gMaterialSetShaderMethod = gMonoApi.monoClassGetMethodFromName(gMaterialClass, "set_shader", 1);
            if (!gMaterialSetShaderMethod) {
                gMaterialSetShaderMethod = gMonoApi.monoClassGetMethodFromName(gMaterialClass, "SetShader", 1);
            }
            gMaterialSetFloatByNameMethod = gMonoApi.monoClassGetMethodFromName(gMaterialClass, "SetFloat", 2);
            gMaterialSetIntByNameMethod = gMonoApi.monoClassGetMethodFromName(gMaterialClass, "SetInt", 2);
        }
        if (gShaderClass) {
            gShaderFindMethod = gMonoApi.monoClassGetMethodFromName(gShaderClass, "Find", 1);
        }
        if (gPhysicsClass) {
            gPhysicsLinecastMethod = gMonoApi.monoClassGetMethodFromName(gPhysicsClass, "Linecast", 2);
        }
        if (gPlayerActorInitializerClass) {
            gPlayerActorInitializerGetCurrentModelMethod = gMonoApi.monoClassGetMethodFromName(gPlayerActorInitializerClass, "get_CurrentModel", 0);
        }
        if (gPlayerSpawnDataClass) {
            gPlayerSpawnDataGetPlayerActorInitializerMethod = gMonoApi.monoClassGetMethodFromName(gPlayerSpawnDataClass, "get_PlayerActorInitializer", 0);
            if (!gPlayerSpawnDataGetPlayerActorInitializerMethod) {
                gPlayerSpawnDataGetPlayerActorInitializerMethod = gMonoApi.monoClassGetMethodFromName(gPlayerSpawnDataClass, "GetPlayerActorInitializer", 0);
            }
        }
        if (gPlayerStartDataClass) {
            gPlayerStartDataGetPlayerActorInitializerMethod = gMonoApi.monoClassGetMethodFromName(gPlayerStartDataClass, "get_PlayerActorInitializer", 0);
            if (!gPlayerStartDataGetPlayerActorInitializerMethod) {
                gPlayerStartDataGetPlayerActorInitializerMethod = gMonoApi.monoClassGetMethodFromName(gPlayerStartDataClass, "GetPlayerActorInitializer", 0);
            }
        }
        if (gModelBonePositionsClass) {
            gModelBonePositionsResolveModeBoneMethod = gMonoApi.monoClassGetMethodFromName(gModelBonePositionsClass, "ResolveModeBone", 1);
        }
        if (gRoundPlayerClass) {
            gRoundPlayerGetPlayerStartDataMethod = gMonoApi.monoClassGetMethodFromName(gRoundPlayerClass, "get_PlayerStartData", 0);
        }


        if (!gClientRoundPlayerManagerField || !gGetAllRoundPlayersMethod || !gCameraGetMainMethod || !gCameraWorldToScreenPointMethod || !gTransformGetPositionMethod) {
            return false;
        }

        gMonoSymbolsReady = true;
        return true;
    }


    void AttachMonoThread() {
        if (gDomain && gMonoApi.monoThreadAttach) {
            gMonoApi.monoThreadAttach(gDomain);
        }
    }

    bool TryReadCollection(void* collectionObject, void*& outItems, int& outCount) {
        outItems = nullptr;
        outCount = 0;

        if (!collectionObject) {
            return false;
        }

        void* listItems = nullptr;
        int listSize = 0;
        if (SafeRead(reinterpret_cast<uintptr_t>(collectionObject) + 0x10, listItems) &&
            SafeRead(reinterpret_cast<uintptr_t>(collectionObject) + 0x18, listSize) &&
            listItems && listSize > 0 && listSize < 4096) {
            outItems = listItems;
            outCount = listSize;
            return true;
        }

        void* arrayBounds = nullptr;
        uintptr_t arrayLength = 0;
        if (SafeRead(reinterpret_cast<uintptr_t>(collectionObject) + 0x10, arrayBounds) &&
            SafeRead(reinterpret_cast<uintptr_t>(collectionObject) + 0x18, arrayLength) &&
            arrayBounds == nullptr && arrayLength > 0 && arrayLength < 4096) {
            outItems = collectionObject;
            outCount = static_cast<int>(arrayLength);
            return true;
        }

        return false;
    }

    MonoClassField* TryGetFieldByNames(MonoClass* klass, const char* const* names, size_t count) {
        if (!klass || !names || count == 0) {
            return nullptr;
        }

        MonoClass* currentClass = klass;
        while (currentClass) {
            for (size_t i = 0; i < count; ++i) {
                MonoClassField* field = gMonoApi.monoClassGetFieldFromName(currentClass, names[i]);
                if (field) {
                    return field;
                }
            }
            currentClass = gMonoApi.monoClassGetParent(currentClass);
        }

        return nullptr;
    }

    MonoObject* InvokeMethod(MonoMethod* method, void* instance, void** params) {
        if (!method) {
            return nullptr;
        }

        MonoObject* exception = nullptr;
        MonoObject* result = gMonoApi.monoRuntimeInvoke(method, instance, params, &exception);
        if (exception) {
            return nullptr;
        }
        return result;
    }

    bool InvokeMethodBool(MonoMethod* method, void* instance, void** params, bool& outValue) {
        MonoObject* boxedValue = InvokeMethod(method, instance, params);
        if (!boxedValue) {
            return false;
        }

        void* unboxed = gMonoApi.monoObjectUnbox(boxedValue);
        if (!unboxed) {
            return false;
        }

        outValue = (*reinterpret_cast<uint8_t*>(unboxed) != 0);
        return true;
    }

    static void* TryGetStaticObjectFromField(MonoClass* klass, const char* fieldName) {
        if (!klass || !fieldName || !*fieldName) {
            return nullptr;
        }

        MonoClassField* field = gMonoApi.monoClassGetFieldFromName(klass, fieldName);
        if (!field) {
            return nullptr;
        }

        MonoVTable* vtable = gMonoApi.monoClassVTable(gDomain, klass);
        if (!vtable) {
            return nullptr;
        }

        void* value = nullptr;
        gMonoApi.monoFieldStaticGetValue(vtable, field, &value);
        return value;
    }

    bool InvokeMethodVec3(MonoMethod* method, void* instance, void** params, Vec3& outValue) {
        MonoObject* boxedValue = InvokeMethod(method, instance, params);
        if (!boxedValue) {
            return false;
        }

        void* unboxed = gMonoApi.monoObjectUnbox(boxedValue);
        if (!unboxed) {
            return false;
        }

        outValue = *reinterpret_cast<Vec3*>(unboxed);
        return IsValidVec3(outValue);
    }

    static bool InvokeMethodInt(MonoMethod* method, void* instance, void** params, int& outValue) {
        MonoObject* boxedValue = InvokeMethod(method, instance, params);
        if (!boxedValue) {
            return false;
        }

        void* unboxed = gMonoApi.monoObjectUnbox(boxedValue);
        if (!unboxed) {
            return false;
        }

        outValue = *reinterpret_cast<int*>(unboxed);
        return true;
    }

    bool TryEnumerateCollectionByMethods(MonoObject* collectionObject, std::vector<void*>& outElements) {
        outElements.clear();
        if (!collectionObject) {
            return false;
        }

        MonoClass* collectionClass = gMonoApi.monoObjectGetClass(collectionObject);
        if (!collectionClass) {
            return false;
        }

        MonoMethod* getCountMethod = gMonoApi.monoClassGetMethodFromName(collectionClass, "get_Count", 0);
        if (!getCountMethod) {
            getCountMethod = gMonoApi.monoClassGetMethodFromName(collectionClass, "get_Length", 0);
        }
        MonoMethod* getItemMethod = gMonoApi.monoClassGetMethodFromName(collectionClass, "get_Item", 1);
        if (!getCountMethod || !getItemMethod) {
            return false;
        }

        int count = 0;
        if (!InvokeMethodInt(getCountMethod, collectionObject, nullptr, count) || count <= 0 || count > 4096) {
            return false;
        }

        outElements.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i) {
            void* args[1] = { &i };
            MonoObject* element = InvokeMethod(getItemMethod, collectionObject, args);
            if (element) {
                outElements.push_back(element);
            }
        }

        return !outElements.empty();
    }

    void* GetRoundPlayerManagerInstance() {
        if (!EnsureMonoSymbols()) {
            return nullptr;
        }

        gMonoApi.monoThreadAttach(gDomain);

        if (gClientRoundPlayerManagerGetInstanceMethod) {
            MonoObject* managerFromGetter = InvokeMethod(gClientRoundPlayerManagerGetInstanceMethod, nullptr, nullptr);
            if (managerFromGetter) {
                return managerFromGetter;
            }
        }

        const char* managerStaticFieldNames[] = {
            "Instance", "instance", "_instance", "current", "Current", "<Instance>k__BackingField"
        };
        for (const char* fieldName : managerStaticFieldNames) {
            void* managerFromField = TryGetStaticObjectFromField(gClientRoundPlayerManagerClass, fieldName);
            if (managerFromField) {
                return managerFromField;
            }
        }

        if (gClientComponentReferenceManagerGetInstanceMethod) {
            MonoObject* referenceManagerInstance = InvokeMethod(gClientComponentReferenceManagerGetInstanceMethod, nullptr, nullptr);
            if (referenceManagerInstance) {
                void* managerFromInstance = nullptr;
                if (SafeRead(reinterpret_cast<uintptr_t>(referenceManagerInstance) + HnawOffsets::clientRoundPlayerManager, managerFromInstance) && managerFromInstance) {
                    return managerFromInstance;
                }
            }
        }

        const char* referenceManagerStaticFieldNames[] = {
            "Instance", "instance", "_instance", "current", "Current", "<Instance>k__BackingField"
        };
        for (const char* fieldName : referenceManagerStaticFieldNames) {
            void* referenceManagerFromField = TryGetStaticObjectFromField(gClientComponentReferenceManagerClass, fieldName);
            if (!referenceManagerFromField) {
                continue;
            }

            void* managerFromReference = nullptr;
            if (SafeRead(reinterpret_cast<uintptr_t>(referenceManagerFromField) + HnawOffsets::clientRoundPlayerManager, managerFromReference) && managerFromReference) {
                return managerFromReference;
            }
        }

        MonoVTable* vtable = gMonoApi.monoClassVTable(gDomain, gClientComponentReferenceManagerClass);
        if (!vtable) {
            return nullptr;
        }

        void* managerInstance = nullptr;
        gMonoApi.monoFieldStaticGetValue(vtable, gClientRoundPlayerManagerField, &managerInstance);
        return managerInstance;
    }

    bool WorldToScreen(MonoObject* camera, const Vec3& world, ImVec2& screen) {
        if (!camera || !gCameraWorldToScreenPointMethod) {
            return false;
        }

        void* params[1] = { const_cast<Vec3*>(&world) };
        MonoObject* boxedProjected = InvokeMethod(gCameraWorldToScreenPointMethod, camera, params);
        if (!boxedProjected) {
            return false;
        }

        void* unboxed = gMonoApi.monoObjectUnbox(boxedProjected);
        if (!unboxed) {
            return false;
        }

        const Vec3 projected = *reinterpret_cast<Vec3*>(unboxed);
        if (!IsValidVec3(projected)) {
            return false;
        }

        if (projected.z <= 0.01f) {
            return false;
        }

        ImGuiIO& io = ImGui::GetIO();
        screen.x = projected.x;
        screen.y = io.DisplaySize.y - projected.y;

        if (screen.x < -250.0f || screen.x > io.DisplaySize.x + 250.0f ||
            screen.y < -250.0f || screen.y > io.DisplaySize.y + 250.0f) {
            return false;
        }

        return true;
    }


    void* GetManagedArrayElement(void* managedArrayObject, int index) {
        if (!managedArrayObject || index < 0) {
            return nullptr;
        }

        constexpr uintptr_t kArrayDataOffset = 0x20;
        void* value = nullptr;
        const uintptr_t entryAddress = reinterpret_cast<uintptr_t>(managedArrayObject) + kArrayDataOffset + (static_cast<uintptr_t>(index) * sizeof(void*));
        if (!SafeRead(entryAddress, value)) {
            return nullptr;
        }
        return value;
    }

    bool TryGetBonePositions(void* roundPlayer, void* playerBase, void* spawnData, std::vector<Vec3>& outBones) {
        outBones.clear();

        if (!gPlayerActorInitializerGetCurrentModelMethod) {
            return false;
        }

        if (!spawnData && roundPlayer && gRoundPlayerGetPlayerStartDataMethod) {
            MonoObject* startDataObj = InvokeMethod(gRoundPlayerGetPlayerStartDataMethod, roundPlayer, nullptr);
            if (startDataObj) {
                spawnData = startDataObj;
            }
        }

        void* actorInitializer = nullptr;

        auto tryResolveActorInitializer = [&](void* sourceObject) -> bool {
            if (!sourceObject) {
                return false;
            }

            MonoClass* sourceClass = gMonoApi.monoObjectGetClass(reinterpret_cast<MonoObject*>(sourceObject));
            if (!sourceClass) {
                return false;
            }

            const char* getterNames[] = {
                "get_PlayerActorInitializer",
                "GetPlayerActorInitializer",
                "get_ActorInitializer",
                "GetActorInitializer"
            };

            for (const char* getterName : getterNames) {
                MonoMethod* runtimeGetter = gMonoApi.monoClassGetMethodFromName(sourceClass, getterName, 0);
                if (!runtimeGetter) {
                    continue;
                }

                MonoObject* actorInitializerObj = InvokeMethod(runtimeGetter, sourceObject, nullptr);
                if (actorInitializerObj) {
                    actorInitializer = actorInitializerObj;
                    return true;
                }
            }

            return false;
        };

        if (spawnData) {
            tryResolveActorInitializer(spawnData);
        }

        if (!actorInitializer && spawnData && (gPlayerSpawnDataGetPlayerActorInitializerMethod || gPlayerStartDataGetPlayerActorInitializerMethod)) {
            MonoMethod* getter = gPlayerSpawnDataGetPlayerActorInitializerMethod ? gPlayerSpawnDataGetPlayerActorInitializerMethod : gPlayerStartDataGetPlayerActorInitializerMethod;
            MonoObject* actorInitializerObj = InvokeMethod(getter, spawnData, nullptr);
            if (actorInitializerObj) {
                actorInitializer = actorInitializerObj;
            }
        }

        if (!actorInitializer && spawnData && HnawOffsets::playerSpawnDataPlayerActorInitializer) {
            if (!SafeRead(reinterpret_cast<uintptr_t>(spawnData) + HnawOffsets::playerSpawnDataPlayerActorInitializer, actorInitializer)) {
                actorInitializer = nullptr;
            }
        }

        if (!actorInitializer) {
            tryResolveActorInitializer(playerBase);
        }

        if (!actorInitializer) {
            tryResolveActorInitializer(roundPlayer);
        }

        auto tryResolveModelDirect = [&](void* sourceObject) -> MonoObject* {
            if (!sourceObject) {
                return nullptr;
            }

            MonoClass* sourceClass = gMonoApi.monoObjectGetClass(reinterpret_cast<MonoObject*>(sourceObject));
            if (!sourceClass) {
                return nullptr;
            }

            const char* modelGetterNames[] = {
                "get_CurrentModel",
                "GetCurrentModel",
                "get_Model",
                "GetModel",
                "get_PlayerModel",
                "GetPlayerModel"
            };

            for (const char* getterName : modelGetterNames) {
                MonoMethod* getter = gMonoApi.monoClassGetMethodFromName(sourceClass, getterName, 0);
                if (!getter) {
                    continue;
                }

                MonoObject* modelObj = InvokeMethod(getter, sourceObject, nullptr);
                if (modelObj) {
                    return modelObj;
                }
            }

            const char* modelFieldNames[] = {
                "model",
                "Model",
                "_model",
                "currentModel",
                "CurrentModel",
                "<CurrentModel>k__BackingField",
                "modelProperties",
                "ModelProperties",
                "<modelProperties>k__BackingField"
            };

            for (const char* fieldName : modelFieldNames) {
                MonoClassField* field = gMonoApi.monoClassGetFieldFromName(sourceClass, fieldName);
                if (!field) {
                    continue;
                }

                MonoObject* modelObj = nullptr;
                gMonoApi.monoFieldGetValue(reinterpret_cast<MonoObject*>(sourceObject), field, &modelObj);
                if (modelObj) {
                    return modelObj;
                }
            }

            const char* actorInitFieldNames[] = {
                "playerActorInitializer",
                "PlayerActorInitializer",
                "_playerActorInitializer",
                "<PlayerActorInitializer>k__BackingField"
            };

            for (const char* fieldName : actorInitFieldNames) {
                MonoClassField* field = gMonoApi.monoClassGetFieldFromName(sourceClass, fieldName);
                if (!field) {
                    continue;
                }

                MonoObject* actorInitObj = nullptr;
                gMonoApi.monoFieldGetValue(reinterpret_cast<MonoObject*>(sourceObject), field, &actorInitObj);
                if (!actorInitObj) {
                    continue;
                }

                MonoClass* aiClass = gMonoApi.monoObjectGetClass(actorInitObj);
                if (!aiClass) {
                    continue;
                }

                MonoMethod* currentModelGetter = gMonoApi.monoClassGetMethodFromName(aiClass, "get_CurrentModel", 0);
                if (!currentModelGetter) {
                    currentModelGetter = gMonoApi.monoClassGetMethodFromName(aiClass, "GetCurrentModel", 0);
                }
                if (currentModelGetter) {
                    MonoObject* modelObj = InvokeMethod(currentModelGetter, actorInitObj, nullptr);
                    if (modelObj) {
                        return modelObj;
                    }
                }
            }

            return nullptr;
        };

        MonoObject* modelObject = nullptr;
        if (!actorInitializer) {
            modelObject = tryResolveModelDirect(spawnData);
            if (!modelObject) {
                modelObject = tryResolveModelDirect(playerBase);
            }
            if (!modelObject) {
                modelObject = tryResolveModelDirect(roundPlayer);
            }
            if (!modelObject) {
                ++gTrueBonesActorInitFail;
                return false;
            }
        }

        if (!modelObject) {
            modelObject = InvokeMethod(gPlayerActorInitializerGetCurrentModelMethod, actorInitializer, nullptr);
            if (!modelObject) {
                MonoClass* actorInitializerClass = gMonoApi.monoObjectGetClass(reinterpret_cast<MonoObject*>(actorInitializer));
                if (actorInitializerClass) {
                    MonoMethod* runtimeCurrentModelGetter = gMonoApi.monoClassGetMethodFromName(actorInitializerClass, "get_CurrentModel", 0);
                    if (!runtimeCurrentModelGetter) {
                        runtimeCurrentModelGetter = gMonoApi.monoClassGetMethodFromName(actorInitializerClass, "GetCurrentModel", 0);
                    }
                    if (runtimeCurrentModelGetter) {
                        modelObject = InvokeMethod(runtimeCurrentModelGetter, actorInitializer, nullptr);
                    }
                }
            }
        }

        if (!modelObject) {
            ++gTrueBonesModelFail;
            return false;
        }

        auto getTransformPosition = [&](void* transform, Vec3& outPos) -> bool {
            if (!transform) {
                return false;
            }
            return InvokeMethodVec3(gTransformGetPositionMethod, transform, nullptr, outPos);
        };

        auto tryFromAnimator = [&]() -> bool {
            if (!gAnimatorGetBoneTransformMethod || !gMonoApi.monoFieldGetValue) {
                return false;
            }

            MonoClass* modelClass = gMonoApi.monoObjectGetClass(modelObject);
            if (!modelClass) {
                return false;
            }

            MonoClassField* animatorField = gMonoApi.monoClassGetFieldFromName(modelClass, "animator");
            if (!animatorField) {
                animatorField = gMonoApi.monoClassGetFieldFromName(modelClass, "_animator");
            }
            if (!animatorField) {
                animatorField = gMonoApi.monoClassGetFieldFromName(modelClass, "<Animator>k__BackingField");
            }
            if (!animatorField) {
                return false;
            }

            MonoObject* animatorObject = nullptr;
            gMonoApi.monoFieldGetValue(modelObject, animatorField, &animatorObject);
            if (!animatorObject) {
                const char* animatorGetterNames[] = {
                    "get_animator",
                    "get_Animator",
                    "GetAnimator",
                    "get_CharacterAnimator",
                    "GetCharacterAnimator"
                };

                for (const char* getterName : animatorGetterNames) {
                    MonoMethod* animatorGetter = gMonoApi.monoClassGetMethodFromName(modelClass, getterName, 0);
                    if (!animatorGetter) {
                        continue;
                    }

                    animatorObject = InvokeMethod(animatorGetter, modelObject, nullptr);
                    if (animatorObject) {
                        break;
                    }
                }
            }
            if (!animatorObject) {
                return false;
            }

            struct BoneQuery {
                int enumValue;
                bool required;
            };

            constexpr BoneQuery kAnimatorBoneQueries[] = {
                { 0, true },
                { 7, true },
                { 8, true },
                { 9, false },
                { 10, true },
                { 11, true },
                { 12, false },
                { 14, false },
                { 16, false },
                { 18, false },
                { 13, false },
                { 15, false },
                { 17, false },
                { 19, false },
                { 1, false },
                { 3, false },
                { 5, false },
                { 2, false },
                { 4, false },
                { 6, false }
            };

            outBones.clear();
            outBones.reserve(std::size(kAnimatorBoneQueries));
            for (const BoneQuery& query : kAnimatorBoneQueries) {
                int valueArg = query.enumValue;
                void* args[1] = { &valueArg };
                MonoObject* transformObj = InvokeMethod(gAnimatorGetBoneTransformMethod, animatorObject, args);
                if (!transformObj) {
                    if (query.required) {
                        outBones.clear();
                        return false;
                    }
                    outBones.push_back(Vec3{});
                    continue;
                }

                Vec3 bonePos{};
                if (!getTransformPosition(transformObj, bonePos)) {
                    if (query.required) {
                        outBones.clear();
                        return false;
                    }
                    outBones.push_back(Vec3{});
                    continue;
                }

                outBones.push_back(bonePos);
            }

            return outBones.size() >= 8;
        };

        if (tryFromAnimator()) {
            return true;
        }

        if (HnawOffsets::modelPropertiesModelBonePositions &&
            HnawOffsets::modelBonePositionsHip &&
            HnawOffsets::modelBonePositionsLowerSpine &&
            HnawOffsets::modelBonePositionsMiddleSpine &&
            HnawOffsets::modelBonePositionsChest &&
            HnawOffsets::modelBonePositionsNeck &&
            HnawOffsets::modelBonePositionsHead &&
            HnawOffsets::modelBonePositionsLeftHand &&
            HnawOffsets::modelBonePositionsRightHand) {

            void* modelBonePositions = nullptr;
            if (SafeRead(reinterpret_cast<uintptr_t>(modelObject) + HnawOffsets::modelPropertiesModelBonePositions, modelBonePositions) && modelBonePositions) {
                const uintptr_t boneOffsets[] = {
                    HnawOffsets::modelBonePositionsHip,
                    HnawOffsets::modelBonePositionsLowerSpine,
                    HnawOffsets::modelBonePositionsMiddleSpine,
                    HnawOffsets::modelBonePositionsChest,
                    HnawOffsets::modelBonePositionsNeck,
                    HnawOffsets::modelBonePositionsHead,
                    HnawOffsets::modelBonePositionsLeftHand,
                    HnawOffsets::modelBonePositionsRightHand
                };

                outBones.clear();
                outBones.reserve(std::size(boneOffsets));
                for (const uintptr_t offset : boneOffsets) {
                    void* transform = nullptr;
                    if (!SafeRead(reinterpret_cast<uintptr_t>(modelBonePositions) + offset, transform) || !transform) {
                        outBones.clear();
                        break;
                    }

                    Vec3 bonePos{};
                    if (!getTransformPosition(transform, bonePos)) {
                        outBones.clear();
                        break;
                    }
                    outBones.push_back(bonePos);
                }

                if (outBones.size() >= 8) {
                    ++gTrueBonesSuccess;
                    return true;
                }

                if (gModelBonePositionsResolveModeBoneMethod) {
                    outBones.clear();
                    struct BoneQuery {
                        int enumValue;
                        bool required;
                    };

                    constexpr BoneQuery kBoneQueries[] = {
                        { 2, true },
                        { 3, true },
                        { 4, true },
                        { 5, true },
                        { 6, true },
                        { 7, true },
                        { 8, false },
                        { 9, false }
                    };

                    outBones.reserve(std::size(kBoneQueries));
                    for (const BoneQuery& query : kBoneQueries) {
                        int valueArg = query.enumValue;
                        void* args[1] = { &valueArg };
                        MonoObject* transformObj = InvokeMethod(gModelBonePositionsResolveModeBoneMethod, modelBonePositions, args);
                        if (!transformObj) {
                            if (query.required) {
                                outBones.clear();
                                break;
                            }
                            outBones.push_back(Vec3{});
                            continue;
                        }

                        Vec3 bonePos{};
                        if (!getTransformPosition(transformObj, bonePos)) {
                            if (query.required) {
                                outBones.clear();
                                break;
                            }
                            outBones.push_back(Vec3{});
                            continue;
                        }
                        outBones.push_back(bonePos);
                    }

                    if (outBones.size() >= 6) {
                        ++gTrueBonesSuccess;
                        return true;
                    }
                }
            }
        }

        ++gTrueBonesTransformFail;
        return false;
    }
}

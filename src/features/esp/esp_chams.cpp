#include "features/esp/esp_internal.h"

#include "core/hnaw_offsets.h"

#include <algorithm>

namespace EspInternal {
    static MonoObject* ResolveCurrentModelObject(void* roundPlayer, void* playerBase, void* spawnData) {
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
            MonoClassField* modelField = TryGetFieldByNames(sourceClass, modelFieldNames, std::size(modelFieldNames));
            if (modelField) {
                MonoObject* modelObj = nullptr;
                gMonoApi.monoFieldGetValue(reinterpret_cast<MonoObject*>(sourceObject), modelField, &modelObj);
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
            MonoClassField* actorInitField = TryGetFieldByNames(sourceClass, actorInitFieldNames, std::size(actorInitFieldNames));
            if (actorInitField) {
                MonoObject* actorInitObj = nullptr;
                gMonoApi.monoFieldGetValue(reinterpret_cast<MonoObject*>(sourceObject), actorInitField, &actorInitObj);
                if (actorInitObj) {
                    MonoClass* aiClass = gMonoApi.monoObjectGetClass(actorInitObj);
                    if (aiClass) {
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
            if (modelObject) {
                ++gChamsModelsResolved;
            }
            return modelObject;
        }

        if (gPlayerActorInitializerGetCurrentModelMethod) {
            modelObject = InvokeMethod(gPlayerActorInitializerGetCurrentModelMethod, actorInitializer, nullptr);
        }
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

        if (modelObject) {
            ++gChamsModelsResolved;
        }
        return modelObject;
    }

    static void ApplyChamsToClientItem(MonoObject* clientItemObject, const Color4& color) {
        if (!clientItemObject || !gRendererGetMaterialMethod || !gMaterialSetColorMethod) {
            return;
        }

        auto applyColorToMaterial = [&](MonoObject* materialObject) -> bool {
            if (!materialObject) {
                return false;
            }

            ++gChamsMaterialsResolved;

            static MonoObject* sUnlitColorShader = nullptr;

            Color4 finalColor = color;
            if (gChamsSolidMode) {
                finalColor.r = (std::min)(1.0f, finalColor.r * gChamsBrightness);
                finalColor.g = (std::min)(1.0f, finalColor.g * gChamsBrightness);
                finalColor.b = (std::min)(1.0f, finalColor.b * gChamsBrightness);
                finalColor.a = 1.0f;

                if (gMaterialSetShaderMethod && gShaderFindMethod && gMonoApi.monoStringNew && gDomain) {
                    if (!sUnlitColorShader) {
                        static const char* kShaderFallbacks[] = {
                            "Unlit/Color",
                            "Legacy Shaders/Unlit/Color",
                            "Sprites/Default",
                            "Unlit/Texture"
                        };
                        for (const char* shaderPath : kShaderFallbacks) {
                            MonoString* shaderName = gMonoApi.monoStringNew(gDomain, shaderPath);
                            if (!shaderName) {
                                continue;
                            }

                            void* findArgs[1] = { shaderName };
                            MonoObject* shaderObj = InvokeMethod(gShaderFindMethod, nullptr, findArgs);
                            if (shaderObj) {
                                sUnlitColorShader = shaderObj;
                                break;
                            }
                        }
                    }

                    if (sUnlitColorShader) {
                        void* shaderArgs[1] = { sUnlitColorShader };
                        InvokeMethod(gMaterialSetShaderMethod, materialObject, shaderArgs);
                    }
                }
            }

            void* colorArg = &finalColor;
            void* colorArgs[1] = { colorArg };
            InvokeMethod(gMaterialSetColorMethod, materialObject, colorArgs);
            ++gChamsColorCalls;

            if (gMaterialSetColorByNameMethod && gMonoApi.monoStringNew && gDomain) {
                static const char* kColorNames[] = { "_Color", "_BaseColor", "_TintColor", "_EmissionColor" };
                for (const char* colorName : kColorNames) {
                    MonoString* name = gMonoApi.monoStringNew(gDomain, colorName);
                    if (!name) {
                        continue;
                    }

                    void* namedArgs[2] = { name, colorArg };
                    InvokeMethod(gMaterialSetColorByNameMethod, materialObject, namedArgs);
                    ++gChamsColorCalls;
                }
            }

            if (gChamsSolidMode && gMonoApi.monoStringNew && gDomain) {
                if (gMaterialSetFloatByNameMethod) {
                    struct FloatProperty {
                        const char* name;
                        float value;
                    };
                    const FloatProperty kFloatProps[] = {
                        { "_Metallic", 0.0f },
                        { "_Glossiness", 0.0f },
                        { "_Smoothness", 0.0f },
                        { "_SpecularHighlights", 0.0f },
                        { "_GlossyReflections", 0.0f }
                    };
                    for (const auto& prop : kFloatProps) {
                        MonoString* propName = gMonoApi.monoStringNew(gDomain, prop.name);
                        if (!propName) {
                            continue;
                        }

                        float value = prop.value;
                        void* floatArgs[2] = { propName, &value };
                        InvokeMethod(gMaterialSetFloatByNameMethod, materialObject, floatArgs);
                    }
                }

                if (gMaterialSetIntByNameMethod) {
                    struct IntProperty {
                        const char* name;
                        int value;
                    };
                    const IntProperty kIntProps[] = {
                        { "_ZWrite", 1 },
                        { "_Cull", 0 },
                        { "_Mode", 1 }
                    };
                    for (const auto& prop : kIntProps) {
                        MonoString* propName = gMonoApi.monoStringNew(gDomain, prop.name);
                        if (!propName) {
                            continue;
                        }

                        int value = prop.value;
                        void* intArgs[2] = { propName, &value };
                        InvokeMethod(gMaterialSetIntByNameMethod, materialObject, intArgs);
                    }
                }
            }

            return true;
        };

        auto applyColorToMaterialArray = [&](MonoObject* materialsArrayObject) -> int {
            if (!materialsArrayObject) {
                return 0;
            }

            void* arrayBounds = nullptr;
            uintptr_t arrayLength = 0;
            if (!SafeRead(reinterpret_cast<uintptr_t>(materialsArrayObject) + 0x10, arrayBounds) ||
                !SafeRead(reinterpret_cast<uintptr_t>(materialsArrayObject) + 0x18, arrayLength) ||
                arrayBounds != nullptr ||
                arrayLength == 0 || arrayLength > 64) {
                return 0;
            }

            int applied = 0;
            for (int materialIndex = 0; materialIndex < static_cast<int>(arrayLength); ++materialIndex) {
                MonoObject* materialObject = reinterpret_cast<MonoObject*>(GetManagedArrayElement(materialsArrayObject, materialIndex));
                if (applyColorToMaterial(materialObject)) {
                    ++applied;
                }
            }

            return applied;
        };

        MonoClass* itemClass = gMonoApi.monoObjectGetClass(clientItemObject);
        if (!itemClass) {
            return;
        }

        const char* lodFieldNames[] = { "lodLevelRenderers", "<lodLevelRenderers>k__BackingField" };
        MonoClassField* lodField = TryGetFieldByNames(itemClass, lodFieldNames, std::size(lodFieldNames));
        if (!lodField) {
            return;
        }

        MonoObject* lodArrayObject = nullptr;
        gMonoApi.monoFieldGetValue(clientItemObject, lodField, &lodArrayObject);
        if (!lodArrayObject) {
            return;
        }

        void* arrayBounds = nullptr;
        uintptr_t arrayLength = 0;
        if (!SafeRead(reinterpret_cast<uintptr_t>(lodArrayObject) + 0x10, arrayBounds) ||
            !SafeRead(reinterpret_cast<uintptr_t>(lodArrayObject) + 0x18, arrayLength) ||
            arrayBounds != nullptr ||
            arrayLength == 0 || arrayLength > 64) {
            return;
        }

        for (int i = 0; i < static_cast<int>(arrayLength); ++i) {
            MonoObject* clientModelRendererObject = reinterpret_cast<MonoObject*>(GetManagedArrayElement(lodArrayObject, i));
            if (!clientModelRendererObject) {
                continue;
            }

            MonoClass* clientModelRendererClass = gMonoApi.monoObjectGetClass(clientModelRendererObject);
            if (!clientModelRendererClass) {
                continue;
            }

            const char* rendererFieldNames[] = { "renderer", "<renderer>k__BackingField" };
            MonoClassField* rendererField = TryGetFieldByNames(clientModelRendererClass, rendererFieldNames, std::size(rendererFieldNames));
            if (!rendererField) {
                continue;
            }

            MonoObject* rendererObject = nullptr;
            gMonoApi.monoFieldGetValue(clientModelRendererObject, rendererField, &rendererObject);
            if (!rendererObject) {
                continue;
            }
            ++gChamsRenderersResolved;

            if (gChamsSolidMode) {
                if (gRendererSetShadowCastingModeMethod) {
                    int disabled = 0;
                    void* shadowArgs[1] = { &disabled };
                    InvokeMethod(gRendererSetShadowCastingModeMethod, rendererObject, shadowArgs);
                }
                if (gRendererSetReceiveShadowsMethod) {
                    bool receiveShadows = false;
                    void* receiveArgs[1] = { &receiveShadows };
                    InvokeMethod(gRendererSetReceiveShadowsMethod, rendererObject, receiveArgs);
                }
            }

            int appliedMaterials = 0;

            if (gRendererGetMaterialsMethod) {
                MonoObject* materialsArrayObject = InvokeMethod(gRendererGetMaterialsMethod, rendererObject, nullptr);
                appliedMaterials += applyColorToMaterialArray(materialsArrayObject);
            }

            if (appliedMaterials == 0 && gRendererGetSharedMaterialsMethod) {
                MonoObject* sharedMaterialsArrayObject = InvokeMethod(gRendererGetSharedMaterialsMethod, rendererObject, nullptr);
                appliedMaterials += applyColorToMaterialArray(sharedMaterialsArrayObject);
            }

            if (appliedMaterials == 0) {
                MonoObject* materialObject = InvokeMethod(gRendererGetMaterialMethod, rendererObject, nullptr);
                if (!materialObject && gRendererGetSharedMaterialMethod) {
                    materialObject = InvokeMethod(gRendererGetSharedMaterialMethod, rendererObject, nullptr);
                }
                applyColorToMaterial(materialObject);
            }
        }
    }

    void ApplyPlayerModelChams(void* roundPlayer, void* playerBase, void* spawnData, const Color4& color) {
        MonoObject* modelObject = ResolveCurrentModelObject(roundPlayer, playerBase, spawnData);
        if (!modelObject) {
            return;
        }

        auto tryGetModelRenderableItems = [&](MonoObject* sourceObject) -> MonoObject* {
            if (!sourceObject) {
                return nullptr;
            }

            MonoClass* sourceClass = gMonoApi.monoObjectGetClass(sourceObject);
            if (!sourceClass) {
                return nullptr;
            }

            const char* modelRenderableItemsFieldNames[] = {
                "modelRenderableItems",
                "ModelRenderableItems",
                "_modelRenderableItems",
                "<modelRenderableItems>k__BackingField"
            };
            MonoClassField* modelRenderableItemsField = TryGetFieldByNames(sourceClass, modelRenderableItemsFieldNames, std::size(modelRenderableItemsFieldNames));
            if (modelRenderableItemsField) {
                MonoObject* modelRenderableItemsObject = nullptr;
                gMonoApi.monoFieldGetValue(sourceObject, modelRenderableItemsField, &modelRenderableItemsObject);
                if (modelRenderableItemsObject) {
                    return modelRenderableItemsObject;
                }
            }

            const char* modelRenderableItemsGetterNames[] = {
                "get_ModelRenderableItems",
                "GetModelRenderableItems",
                "get_modelRenderableItems"
            };
            for (const char* getterName : modelRenderableItemsGetterNames) {
                MonoMethod* getter = gMonoApi.monoClassGetMethodFromName(sourceClass, getterName, 0);
                if (!getter) {
                    continue;
                }

                MonoObject* modelRenderableItemsObject = InvokeMethod(getter, sourceObject, nullptr);
                if (modelRenderableItemsObject) {
                    return modelRenderableItemsObject;
                }
            }

            return nullptr;
        };

        auto tryGetModelProperties = [&](MonoObject* sourceObject) -> MonoObject* {
            if (!sourceObject) {
                return nullptr;
            }

            MonoClass* sourceClass = gMonoApi.monoObjectGetClass(sourceObject);
            if (!sourceClass) {
                return nullptr;
            }

            const char* getterNames[] = {
                "get_ModelProperties",
                "GetModelProperties",
                "get_modelProperties"
            };
            for (const char* getterName : getterNames) {
                MonoMethod* getter = gMonoApi.monoClassGetMethodFromName(sourceClass, getterName, 0);
                if (!getter) {
                    continue;
                }

                MonoObject* modelProperties = InvokeMethod(getter, sourceObject, nullptr);
                if (modelProperties) {
                    return modelProperties;
                }
            }

            const char* fieldNames[] = {
                "modelProperties",
                "ModelProperties",
                "_modelProperties",
                "<modelProperties>k__BackingField"
            };
            MonoClassField* modelPropertiesField = TryGetFieldByNames(sourceClass, fieldNames, std::size(fieldNames));
            if (!modelPropertiesField) {
                return nullptr;
            }

            MonoObject* modelProperties = nullptr;
            gMonoApi.monoFieldGetValue(sourceObject, modelPropertiesField, &modelProperties);
            return modelProperties;
        };

        MonoObject* modelRenderableItemsObject = tryGetModelRenderableItems(modelObject);
        if (!modelRenderableItemsObject) {
            MonoObject* modelProperties = tryGetModelProperties(modelObject);
            modelRenderableItemsObject = tryGetModelRenderableItems(modelProperties);
        }
        if (!modelRenderableItemsObject && roundPlayer) {
            modelRenderableItemsObject = tryGetModelRenderableItems(reinterpret_cast<MonoObject*>(roundPlayer));
        }
        if (!modelRenderableItemsObject && playerBase) {
            modelRenderableItemsObject = tryGetModelRenderableItems(reinterpret_cast<MonoObject*>(playerBase));
        }
        if (!modelRenderableItemsObject && spawnData) {
            modelRenderableItemsObject = tryGetModelRenderableItems(reinterpret_cast<MonoObject*>(spawnData));
        }
        if (!modelRenderableItemsObject) {
            return;
        }
        ++gChamsRenderableItemsResolved;

        MonoClass* modelRenderableItemsClass = gMonoApi.monoObjectGetClass(modelRenderableItemsObject);
        if (!modelRenderableItemsClass) {
            return;
        }

        const char* headItemFieldNames[] = { "clientHeadItem", "<clientHeadItem>k__BackingField" };
        const char* uniformItemFieldNames[] = { "clientUniformItem", "<clientUniformItem>k__BackingField" };
        const char* firstPersonItemFieldNames[] = { "clientFirstPersonItem", "<clientFirstPersonItem>k__BackingField" };

        MonoClassField* headItemField = TryGetFieldByNames(modelRenderableItemsClass, headItemFieldNames, std::size(headItemFieldNames));
        MonoClassField* uniformItemField = TryGetFieldByNames(modelRenderableItemsClass, uniformItemFieldNames, std::size(uniformItemFieldNames));
        MonoClassField* firstPersonItemField = TryGetFieldByNames(modelRenderableItemsClass, firstPersonItemFieldNames, std::size(firstPersonItemFieldNames));

        MonoObject* headItemObject = nullptr;
        MonoObject* uniformItemObject = nullptr;
        MonoObject* firstPersonItemObject = nullptr;
        if (headItemField) {
            gMonoApi.monoFieldGetValue(modelRenderableItemsObject, headItemField, &headItemObject);
        }
        if (uniformItemField) {
            gMonoApi.monoFieldGetValue(modelRenderableItemsObject, uniformItemField, &uniformItemObject);
        }
        if (firstPersonItemField) {
            gMonoApi.monoFieldGetValue(modelRenderableItemsObject, firstPersonItemField, &firstPersonItemObject);
        }

        if (uniformItemObject) {
            ++gChamsClientItemsResolved;
        }
        if (headItemObject) {
            ++gChamsClientItemsResolved;
        }
        if (firstPersonItemObject) {
            ++gChamsClientItemsResolved;
        }

        ApplyChamsToClientItem(uniformItemObject, color);
        ApplyChamsToClientItem(headItemObject, color);
        ApplyChamsToClientItem(firstPersonItemObject, color);
    }
}

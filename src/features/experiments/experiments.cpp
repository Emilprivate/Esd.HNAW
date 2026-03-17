#include "features/experiments/experiments.h"

#include "core/hnaw_offsets.h"
#include "features/esp/esp_internal.h"

#include "imgui.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <windows.h>

namespace {
    struct Snapshot {
        int playerCount = -1;
        int localPlayerId = -1;
        int spectatingPlayerId = -1;
        bool localAdminLoggedOn = false;
        bool adminStatusKnown = false;
        bool hasAdminBroadcastManager = false;
        bool hasSpectatorManager = false;
        bool hasRemoteConsoleManager = false;
        bool hasRpcExecutionManager = false;
    };

    Snapshot gSnapshot{};
    Snapshot gLastSnapshot{};
    std::string gLocalName;
    std::string gSpectatingName;
    std::vector<std::string> gEvents;
    std::string gReport;
    uint64_t gLastUpdateMs = 0;
    bool gLastMonoReady = true;

    constexpr size_t kMaxEvents = 200;
    constexpr uint64_t kUpdateIntervalMs = 300;

    void PushEvent(const std::string& text) {
        if (text.empty()) {
            return;
        }

        const uint64_t now = GetTickCount64();
        const std::string entry = "t=" + std::to_string(now) + "ms: " + text;
        gEvents.push_back(entry);
        if (gEvents.size() > kMaxEvents) {
            gEvents.erase(gEvents.begin(), gEvents.begin() + (gEvents.size() - kMaxEvents));
        }
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

    std::string ReadMonoStringUtf8(void* monoString) {
        if (!monoString) {
            return {};
        }

        int length = 0;
        if (!EspInternal::SafeRead(reinterpret_cast<uintptr_t>(monoString) + 0x10, length)) {
            return {};
        }

        if (length <= 0 || length > 128) {
            return {};
        }

        std::wstring wideText;
        wideText.resize(static_cast<size_t>(length));
        const uintptr_t charsBase = reinterpret_cast<uintptr_t>(monoString) + 0x14;
        for (int i = 0; i < length; ++i) {
            wchar_t ch = 0;
            if (!EspInternal::SafeRead(charsBase + static_cast<uintptr_t>(i * 2), ch)) {
                return {};
            }
            wideText[static_cast<size_t>(i)] = ch;
        }

        return WideToUtf8(wideText);
    }

    bool TryReadLocalName(void* roundPlayerManager, std::string& outName) {
        outName.clear();
        if (!roundPlayerManager || !HnawOffsets::clientRoundPlayerManagerCurrentRoundPlayerInformation ||
            !HnawOffsets::roundPlayerInformationInitialDetails || !HnawOffsets::playerInitialDetailsDisplayname) {
            return false;
        }

        void* roundInfo = nullptr;
        if (!EspInternal::SafeRead(reinterpret_cast<uintptr_t>(roundPlayerManager) + HnawOffsets::clientRoundPlayerManagerCurrentRoundPlayerInformation, roundInfo) || !roundInfo) {
            return false;
        }

        void* initialDetails = nullptr;
        if (!EspInternal::SafeRead(reinterpret_cast<uintptr_t>(roundInfo) + HnawOffsets::roundPlayerInformationInitialDetails, initialDetails) || !initialDetails) {
            return false;
        }

        void* displayName = nullptr;
        if (!EspInternal::SafeRead(reinterpret_cast<uintptr_t>(initialDetails) + HnawOffsets::playerInitialDetailsDisplayname, displayName) || !displayName) {
            return false;
        }

        outName = ReadMonoStringUtf8(displayName);
        return !outName.empty();
    }

    bool TryReadNetworkId(void* roundPlayer, int& outId) {
        outId = -1;
        if (!roundPlayer || !HnawOffsets::roundPlayerNetworkPlayerID) {
            return false;
        }

        int rawId = 0;
        if (!EspInternal::SafeRead(reinterpret_cast<uintptr_t>(roundPlayer) + HnawOffsets::roundPlayerNetworkPlayerID, rawId)) {
            return false;
        }
        outId = rawId;
        return true;
    }

    void* GetClientComponentReferenceManagerInstance() {
        if (!EspInternal::gClientComponentReferenceManagerGetInstanceMethod) {
            return nullptr;
        }

        MonoObject* instance = EspInternal::InvokeMethod(EspInternal::gClientComponentReferenceManagerGetInstanceMethod, nullptr, nullptr);
        return instance;
    }

    bool ReadComponentPointer(void* componentManager, uintptr_t offset, bool& present, void*& outPtr) {
        present = false;
        outPtr = nullptr;
        if (!componentManager || offset == 0) {
            return false;
        }

        if (!EspInternal::SafeRead(reinterpret_cast<uintptr_t>(componentManager) + offset, outPtr) || !outPtr) {
            return false;
        }

        present = true;
        return true;
    }

    bool TryReadLocalAdminLoggedOn(bool& outLoggedOn) {
        outLoggedOn = false;
        if (!EspInternal::gGameImage) {
            return false;
        }

        MonoClass* klass = EspInternal::gMonoApi.monoClassFromName(EspInternal::gGameImage, "HoldfastGame", "ClientRemoteConsoleAccessManager");
        if (!klass) {
            return false;
        }

        const char* fieldNames[] = { "loggedOn", "<loggedOn>k__BackingField" };
        MonoClassField* field = EspInternal::TryGetFieldByNames(klass, fieldNames, std::size(fieldNames));
        if (!field) {
            return false;
        }

        MonoVTable* vtable = EspInternal::gMonoApi.monoClassVTable(EspInternal::gDomain, klass);
        if (!vtable) {
            return false;
        }

        int rawValue = 0;
        EspInternal::gMonoApi.monoFieldStaticGetValue(vtable, field, &rawValue);
        outLoggedOn = (rawValue != 0);
        return true;
    }

    std::string FormatIdName(int id, const std::string& nameFallback) {
        if (id < 0) {
            return "unknown";
        }
        if (!nameFallback.empty()) {
            return nameFallback + " (" + std::to_string(id) + ")";
        }
        return "P" + std::to_string(id);
    }

    void BuildReportString() {
        gReport.clear();
        gReport += "[Experiments]\n";
        gReport += "Player count: " + std::to_string(gSnapshot.playerCount) + "\n";
        gReport += "Local player: " + FormatIdName(gSnapshot.localPlayerId, gLocalName) + "\n";
        gReport += "Spectating: " + FormatIdName(gSnapshot.spectatingPlayerId, gSpectatingName) + "\n";

        if (gSnapshot.adminStatusKnown) {
            gReport += std::string("Local admin logged on: ") + (gSnapshot.localAdminLoggedOn ? "yes" : "no") + "\n";
        } else {
            gReport += "Local admin logged on: unknown\n";
        }

        gReport += "Components: adminBroadcast=" + std::string(gSnapshot.hasAdminBroadcastManager ? "yes" : "no");
        gReport += ", spectator=" + std::string(gSnapshot.hasSpectatorManager ? "yes" : "no");
        gReport += ", remoteConsole=" + std::string(gSnapshot.hasRemoteConsoleManager ? "yes" : "no");
        gReport += ", rpcExecution=" + std::string(gSnapshot.hasRpcExecutionManager ? "yes" : "no");
        gReport += "\n\n[Events]\n";
        for (const std::string& entry : gEvents) {
            gReport += entry + "\n";
        }
    }
}

namespace Experiments {
    void Update() {
        const uint64_t now = GetTickCount64();
        const bool monoReady = EspInternal::EnsureMonoSymbols();
        if (!monoReady) {
            if (gLastMonoReady) {
                PushEvent("mono symbols unavailable");
            }
            gLastMonoReady = false;
            return;
        }
        gLastMonoReady = true;
        EspInternal::AttachMonoThread();

        if (now - gLastUpdateMs < kUpdateIntervalMs) {
            return;
        }
        gLastUpdateMs = now;

        Snapshot next{};

        void* componentManager = GetClientComponentReferenceManagerInstance();
        void* adminBroadcastManager = nullptr;
        void* spectatorManager = nullptr;
        void* remoteConsoleManager = nullptr;
        void* rpcExecutionManager = nullptr;

        ReadComponentPointer(componentManager, HnawOffsets::clientAdminBroadcastMessageManager, next.hasAdminBroadcastManager, adminBroadcastManager);
        ReadComponentPointer(componentManager, HnawOffsets::clientSpectatorManager, next.hasSpectatorManager, spectatorManager);
        ReadComponentPointer(componentManager, HnawOffsets::clientRemoteConsoleAccessManager, next.hasRemoteConsoleManager, remoteConsoleManager);
        ReadComponentPointer(componentManager, HnawOffsets::clientRPCExecutionManager, next.hasRpcExecutionManager, rpcExecutionManager);

        void* roundPlayerManager = EspInternal::GetRoundPlayerManagerInstance();
        MonoObject* playerListObject = nullptr;
        if (roundPlayerManager) {
            playerListObject = EspInternal::InvokeMethod(EspInternal::gGetAllRoundPlayersMethod, roundPlayerManager, nullptr);
        }
        if (!playerListObject) {
            playerListObject = EspInternal::InvokeMethod(EspInternal::gGetAllRoundPlayersMethod, nullptr, nullptr);
        }

        if (playerListObject) {
            void* items = nullptr;
            int size = 0;
            std::vector<void*> methodElements;
            const bool useRawCollection = EspInternal::TryReadCollection(playerListObject, items, size);
            const bool useMethodCollection = !useRawCollection && EspInternal::TryEnumerateCollectionByMethods(playerListObject, methodElements);
            if (useRawCollection) {
                next.playerCount = std::clamp(size, 0, 256);
            } else if (useMethodCollection) {
                next.playerCount = std::clamp(static_cast<int>(methodElements.size()), 0, 256);
            }
        }

        void* localRoundPlayer = nullptr;
        if (roundPlayerManager && HnawOffsets::clientRoundPlayerManagerLocalPlayer) {
            EspInternal::SafeRead(reinterpret_cast<uintptr_t>(roundPlayerManager) + HnawOffsets::clientRoundPlayerManagerLocalPlayer, localRoundPlayer);
        }
        if (localRoundPlayer) {
            TryReadNetworkId(localRoundPlayer, next.localPlayerId);
        }

        gLocalName.clear();
        TryReadLocalName(roundPlayerManager, gLocalName);

        if (spectatorManager && HnawOffsets::clientSpectatorManagerCurrentlySpectatingPlayer) {
            void* spectatingRoundPlayer = nullptr;
            if (EspInternal::SafeRead(reinterpret_cast<uintptr_t>(spectatorManager) + HnawOffsets::clientSpectatorManagerCurrentlySpectatingPlayer, spectatingRoundPlayer) && spectatingRoundPlayer) {
                TryReadNetworkId(spectatingRoundPlayer, next.spectatingPlayerId);
            }
        }

        gSpectatingName.clear();

        bool loggedOn = false;
        if (TryReadLocalAdminLoggedOn(loggedOn)) {
            next.localAdminLoggedOn = loggedOn;
            next.adminStatusKnown = true;
        }

        if (gLastSnapshot.playerCount != -1 && next.playerCount != gLastSnapshot.playerCount) {
            PushEvent("player count changed to " + std::to_string(next.playerCount));
        }
        if (gLastSnapshot.localPlayerId != -1 && next.localPlayerId != gLastSnapshot.localPlayerId) {
            PushEvent("local player id changed to " + std::to_string(next.localPlayerId));
        }
        if (gLastSnapshot.spectatingPlayerId != next.spectatingPlayerId) {
            if (next.spectatingPlayerId >= 0) {
                PushEvent("spectating player " + std::to_string(next.spectatingPlayerId));
            } else if (gLastSnapshot.spectatingPlayerId >= 0) {
                PushEvent("stopped spectating");
            }
        }
        if (next.adminStatusKnown && (!gLastSnapshot.adminStatusKnown || next.localAdminLoggedOn != gLastSnapshot.localAdminLoggedOn)) {
            PushEvent(std::string("local admin logged on: ") + (next.localAdminLoggedOn ? "yes" : "no"));
        }

        gSnapshot = next;
        gLastSnapshot = next;
        BuildReportString();
    }

    void DrawPanel() {
        ImGui::TextUnformatted("Read-only experiments (no RPCs sent). Use this for observation.");
        ImGui::Spacing();

        if (ImGui::BeginTabBar("ExperimentTabs", ImGuiTabBarFlags_None)) {
            if (ImGui::BeginTabItem("Overview")) {
                ImGui::Text("Player count: %d", gSnapshot.playerCount);
                ImGui::Text("Local player: %s", FormatIdName(gSnapshot.localPlayerId, gLocalName).c_str());
                ImGui::Text("Spectating: %s", FormatIdName(gSnapshot.spectatingPlayerId, gSpectatingName).c_str());

                ImGui::Spacing();
                if (gSnapshot.adminStatusKnown) {
                    ImGui::Text("Local admin logged on: %s", gSnapshot.localAdminLoggedOn ? "yes" : "no");
                } else {
                    ImGui::TextUnformatted("Local admin logged on: unknown");
                }

                ImGui::Spacing();
                if (ImGui::Button("Copy report", ImVec2(140.0f, 0.0f))) {
                    BuildReportString();
                    ImGui::SetClipboardText(gReport.c_str());
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Components")) {
                ImGui::Text("Admin broadcast manager: %s", gSnapshot.hasAdminBroadcastManager ? "present" : "missing");
                ImGui::Text("Spectator manager: %s", gSnapshot.hasSpectatorManager ? "present" : "missing");
                ImGui::Text("Remote console manager: %s", gSnapshot.hasRemoteConsoleManager ? "present" : "missing");
                ImGui::Text("RPC execution manager: %s", gSnapshot.hasRpcExecutionManager ? "present" : "missing");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Event Log")) {
                if (ImGui::Button("Copy log", ImVec2(120.0f, 0.0f))) {
                    std::string logText;
                    for (const std::string& entry : gEvents) {
                        logText += entry + "\n";
                    }
                    ImGui::SetClipboardText(logText.c_str());
                }

                ImGui::Spacing();
                if (ImGui::BeginChild("ExperimentLog", ImVec2(0.0f, 220.0f), true)) {
                    for (const std::string& entry : gEvents) {
                        ImGui::TextUnformatted(entry.c_str());
                    }
                    ImGui::EndChild();
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }

    const char* BuildReport() {
        BuildReportString();
        return gReport.c_str();
    }
}

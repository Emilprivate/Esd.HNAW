#include "config/config.h"

#include "features/aimbot/aimbot.h"
#include "features/esp/esp.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <windows.h>

namespace {
    struct EspProfile {
        bool enabled = true;
        bool cornerMode = false;
        bool filled = false;
        float fillAlpha = 0.20f;
        float thickness = 1.5f;
        float boxColor[3] = { 0.27f, 0.86f, 0.47f };
        bool useTeamColors = true;
        float teamColor[3] = { 0.30f, 0.70f, 1.00f };
        float enemyColor[3] = { 1.00f, 0.35f, 0.35f };
        int teamFilterMode = 0;
        int visibilityMode = 0;
        bool perFeatureDistanceLimitsEnabled = false;
        float maxBoxDistanceMeters = 200.0f;
        float maxSkeletonDistanceMeters = 200.0f;
        float maxInfoDistanceMeters = 200.0f;
        float maxChamsDistanceMeters = 200.0f;
        int infoPosition = 1;
        bool skeletonEnabled = false;
        float skeletonThickness = 1.2f;
        bool chamsEnabled = false;
        float chamsAlpha = 0.28f;
        bool chamsSolidMode = false;
        float chamsBrightness = 2.2f;
        bool showName = true;
        bool showDistance = true;
        bool showHealth = true;
        bool healthBarEnabled = true;
        bool showNetworkId = false;
        bool showClassRank = false;
        bool showFaction = false;
        bool cannonMapEnabled = false;
        bool cannonMapRequireContext = false;
        float cannonMapPosX = 26.0f;
        float cannonMapPosY = 0.0f;
        float cannonMapSizePx = 220.0f;
        float cannonMapRangeMeters = 300.0f;
        bool cannonMapShowTeammates = false;
        bool cannonImpactMarkerEnabled = true;
        float cannonImpactVelocity = 145.0f;
        float cannonImpactGravity = 9.81f;
        bool aimbotEnabled = false;
        bool aimbotRequireKey = true;
        int aimbotAimKey = 0x02;
        float aimbotFovPixels = 140.0f;
        float aimbotSmooth = 6.0f;
        int aimbotTargetBone = 0;
        int aimbotTeamFilterMode = 2;
        bool aimbotDrawFov = true;
        float aimbotFovColor[3] = { 0.20f, 0.85f, 0.35f };
        bool aimbotDropEnabled = true;
        bool aimbotUseWeaponBallistics = true;
        float aimbotDefaultVelocity = 100.0f;
        float aimbotDefaultGravity = 9.81f;
        bool aimbotVisibilityCheckEnabled = false;
        bool aimbotReloadSpeedEnabled = false;
        float aimbotReloadSpeedMultiplier = 1.0f;
        bool aimbotFireRateEnabled = false;
        float aimbotFireRateMultiplier = 1.0f;
        bool aimbotNoRecoilEnabled = false;
        bool aimbotNoSpreadEnabled = false;
    };

    struct ConfigDb {
        std::string activeProfile;
        std::map<std::string, EspProfile> profiles;
    };

    std::string gLastConfigError;
    std::string gActiveProfileName;

    void SetError(const std::string& msg) {
        gLastConfigError = msg;
    }

    std::filesystem::path GetConfigPath() {
        wchar_t modulePath[MAX_PATH]{};
        const DWORD written = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
        if (written == 0 || written >= MAX_PATH) {
            return std::filesystem::path(L"config.json");
        }

        std::filesystem::path exePath(modulePath);
        return exePath.parent_path() / "config.json";
    }

    std::string ReadTextFile(const std::filesystem::path& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open()) {
            return {};
        }
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    bool WriteTextFile(const std::filesystem::path& path, const std::string& content) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            return false;
        }
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        return out.good();
    }

    size_t SkipWs(const std::string& text, size_t pos) {
        while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) {
            ++pos;
        }
        return pos;
    }

    bool ParseJsonStringAt(const std::string& text, size_t& pos, std::string& out) {
        pos = SkipWs(text, pos);
        if (pos >= text.size() || text[pos] != '"') {
            return false;
        }
        ++pos;
        out.clear();
        while (pos < text.size()) {
            const char c = text[pos++];
            if (c == '"') {
                return true;
            }
            if (c == '\\' && pos < text.size()) {
                const char e = text[pos++];
                switch (e) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    default: out.push_back(e); break;
                }
                continue;
            }
            out.push_back(c);
        }
        return false;
    }

    size_t FindMatchingBrace(const std::string& text, size_t openPos) {
        if (openPos >= text.size() || text[openPos] != '{') {
            return std::string::npos;
        }

        int depth = 0;
        bool inString = false;
        bool escaped = false;
        for (size_t i = openPos; i < text.size(); ++i) {
            const char c = text[i];

            if (inString) {
                if (escaped) {
                    escaped = false;
                } else if (c == '\\') {
                    escaped = true;
                } else if (c == '"') {
                    inString = false;
                }
                continue;
            }

            if (c == '"') {
                inString = true;
                continue;
            }
            if (c == '{') {
                ++depth;
            } else if (c == '}') {
                --depth;
                if (depth == 0) {
                    return i;
                }
            }
        }
        return std::string::npos;
    }

    std::string EscapeJson(const std::string& value) {
        std::string out;
        out.reserve(value.size() + 8);
        for (const char c : value) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out.push_back(c); break;
            }
        }
        return out;
    }

    bool TryReadBool(const std::string& objectText, const char* key, bool& out) {
        const std::string marker = std::string("\"") + key + "\"";
        const size_t keyPos = objectText.find(marker);
        if (keyPos == std::string::npos) {
            return false;
        }

        size_t pos = objectText.find(':', keyPos + marker.size());
        if (pos == std::string::npos) {
            return false;
        }
        pos = SkipWs(objectText, pos + 1);
        if (objectText.compare(pos, 4, "true") == 0) {
            out = true;
            return true;
        }
        if (objectText.compare(pos, 5, "false") == 0) {
            out = false;
            return true;
        }
        return false;
    }

    bool TryReadInt(const std::string& objectText, const char* key, int& out) {
        const std::string marker = std::string("\"") + key + "\"";
        const size_t keyPos = objectText.find(marker);
        if (keyPos == std::string::npos) {
            return false;
        }

        size_t pos = objectText.find(':', keyPos + marker.size());
        if (pos == std::string::npos) {
            return false;
        }
        pos = SkipWs(objectText, pos + 1);
        size_t endPos = pos;
        if (endPos < objectText.size() && (objectText[endPos] == '-' || objectText[endPos] == '+')) {
            ++endPos;
        }
        while (endPos < objectText.size() && std::isdigit(static_cast<unsigned char>(objectText[endPos]))) {
            ++endPos;
        }
        if (endPos == pos) {
            return false;
        }

        try {
            out = std::stoi(objectText.substr(pos, endPos - pos));
            return true;
        } catch (...) {
            return false;
        }
    }

    bool TryReadFloat(const std::string& objectText, const char* key, float& out) {
        const std::string marker = std::string("\"") + key + "\"";
        const size_t keyPos = objectText.find(marker);
        if (keyPos == std::string::npos) {
            return false;
        }

        size_t pos = objectText.find(':', keyPos + marker.size());
        if (pos == std::string::npos) {
            return false;
        }
        pos = SkipWs(objectText, pos + 1);

        size_t endPos = pos;
        if (endPos < objectText.size() && (objectText[endPos] == '-' || objectText[endPos] == '+')) {
            ++endPos;
        }
        bool hasDigit = false;
        while (endPos < objectText.size() && std::isdigit(static_cast<unsigned char>(objectText[endPos]))) {
            hasDigit = true;
            ++endPos;
        }
        if (endPos < objectText.size() && objectText[endPos] == '.') {
            ++endPos;
            while (endPos < objectText.size() && std::isdigit(static_cast<unsigned char>(objectText[endPos]))) {
                hasDigit = true;
                ++endPos;
            }
        }
        if (endPos < objectText.size() && (objectText[endPos] == 'e' || objectText[endPos] == 'E')) {
            ++endPos;
            if (endPos < objectText.size() && (objectText[endPos] == '-' || objectText[endPos] == '+')) {
                ++endPos;
            }
            while (endPos < objectText.size() && std::isdigit(static_cast<unsigned char>(objectText[endPos]))) {
                hasDigit = true;
                ++endPos;
            }
        }
        if (!hasDigit) {
            return false;
        }

        try {
            out = std::stof(objectText.substr(pos, endPos - pos));
            return std::isfinite(out);
        } catch (...) {
            return false;
        }
    }

    bool TryReadColor3(const std::string& objectText, const char* key, float out[3]) {
        const std::string marker = std::string("\"") + key + "\"";
        const size_t keyPos = objectText.find(marker);
        if (keyPos == std::string::npos) {
            return false;
        }

        size_t pos = objectText.find(':', keyPos + marker.size());
        if (pos == std::string::npos) {
            return false;
        }
        pos = SkipWs(objectText, pos + 1);
        if (pos >= objectText.size() || objectText[pos] != '[') {
            return false;
        }

        ++pos;
        for (int i = 0; i < 3; ++i) {
            pos = SkipWs(objectText, pos);
            size_t endPos = pos;
            if (endPos < objectText.size() && (objectText[endPos] == '-' || objectText[endPos] == '+')) {
                ++endPos;
            }
            bool hasDigit = false;
            while (endPos < objectText.size() && std::isdigit(static_cast<unsigned char>(objectText[endPos]))) {
                hasDigit = true;
                ++endPos;
            }
            if (endPos < objectText.size() && objectText[endPos] == '.') {
                ++endPos;
                while (endPos < objectText.size() && std::isdigit(static_cast<unsigned char>(objectText[endPos]))) {
                    hasDigit = true;
                    ++endPos;
                }
            }
            if (!hasDigit) {
                return false;
            }

            try {
                out[i] = std::stof(objectText.substr(pos, endPos - pos));
            } catch (...) {
                return false;
            }

            pos = SkipWs(objectText, endPos);
            if (i < 2) {
                if (pos >= objectText.size() || objectText[pos] != ',') {
                    return false;
                }
                ++pos;
            }
        }

        pos = SkipWs(objectText, pos);
        return pos < objectText.size() && objectText[pos] == ']';
    }

    EspProfile CaptureCurrentProfile() {
        EspProfile p{};
        p.enabled = PlayerBoxes::Enabled();
        p.cornerMode = PlayerBoxes::CornerMode();
        p.filled = PlayerBoxes::Filled();
        p.fillAlpha = PlayerBoxes::FillAlpha();
        p.thickness = PlayerBoxes::Thickness();
        {
            const float* c = PlayerBoxes::ColorRgb();
            p.boxColor[0] = c[0];
            p.boxColor[1] = c[1];
            p.boxColor[2] = c[2];
        }
        p.useTeamColors = PlayerBoxes::UseTeamColors();
        {
            const float* c = PlayerBoxes::TeamColorRgb();
            p.teamColor[0] = c[0];
            p.teamColor[1] = c[1];
            p.teamColor[2] = c[2];
        }
        {
            const float* c = PlayerBoxes::EnemyColorRgb();
            p.enemyColor[0] = c[0];
            p.enemyColor[1] = c[1];
            p.enemyColor[2] = c[2];
        }
        p.teamFilterMode = PlayerBoxes::TeamFilterMode();
        p.visibilityMode = PlayerBoxes::VisibilityMode();
        p.perFeatureDistanceLimitsEnabled = PlayerBoxes::PerFeatureDistanceLimitsEnabled();
        p.maxBoxDistanceMeters = PlayerBoxes::MaxBoxDistanceMeters();
        p.maxSkeletonDistanceMeters = PlayerBoxes::MaxSkeletonDistanceMeters();
        p.maxInfoDistanceMeters = PlayerBoxes::MaxInfoDistanceMeters();
        p.maxChamsDistanceMeters = PlayerBoxes::MaxChamsDistanceMeters();
        p.infoPosition = PlayerBoxes::InfoPosition();
        p.skeletonEnabled = PlayerBoxes::SkeletonEnabled();
        p.skeletonThickness = PlayerBoxes::SkeletonThickness();
        p.chamsEnabled = PlayerBoxes::ChamsEnabled();
        p.chamsAlpha = PlayerBoxes::ChamsAlpha();
        p.chamsSolidMode = PlayerBoxes::ChamsSolidMode();
        p.chamsBrightness = PlayerBoxes::ChamsBrightness();
        p.showName = PlayerBoxes::ShowName();
        p.showDistance = PlayerBoxes::ShowDistance();
        p.showHealth = PlayerBoxes::ShowHealth();
        p.healthBarEnabled = PlayerBoxes::HealthBarEnabled();
        p.showNetworkId = PlayerBoxes::ShowNetworkId();
        p.showClassRank = PlayerBoxes::ShowClassRank();
        p.showFaction = PlayerBoxes::ShowFaction();
        p.cannonMapEnabled = PlayerBoxes::CannonMapEnabled();
        p.cannonMapRequireContext = PlayerBoxes::CannonMapRequireContext();
        p.cannonMapPosX = PlayerBoxes::CannonMapPosX();
        p.cannonMapPosY = PlayerBoxes::CannonMapPosY();
        p.cannonMapSizePx = PlayerBoxes::CannonMapSizePx();
        p.cannonMapRangeMeters = PlayerBoxes::CannonMapRangeMeters();
        p.cannonMapShowTeammates = PlayerBoxes::CannonMapShowTeammates();
        p.cannonImpactMarkerEnabled = PlayerBoxes::CannonImpactMarkerEnabled();
        p.cannonImpactVelocity = PlayerBoxes::CannonImpactVelocity();
        p.cannonImpactGravity = PlayerBoxes::CannonImpactGravity();
        p.aimbotEnabled = Aimbot::Enabled();
        p.aimbotRequireKey = Aimbot::RequireKey();
        p.aimbotAimKey = Aimbot::AimKey();
        p.aimbotFovPixels = Aimbot::FovPixels();
        p.aimbotSmooth = Aimbot::Smooth();
        p.aimbotTargetBone = Aimbot::TargetBone();
        p.aimbotTeamFilterMode = Aimbot::TeamFilterMode();
        p.aimbotDrawFov = Aimbot::DrawFovCircle();
        {
            const float* c = Aimbot::FovColorRgb();
            p.aimbotFovColor[0] = c[0];
            p.aimbotFovColor[1] = c[1];
            p.aimbotFovColor[2] = c[2];
        }
        p.aimbotDropEnabled = Aimbot::DropCompensationEnabled();
        p.aimbotUseWeaponBallistics = Aimbot::UseWeaponBallistics();
        p.aimbotDefaultVelocity = Aimbot::DefaultMuzzleVelocity();
        p.aimbotDefaultGravity = Aimbot::DefaultGravity();
        p.aimbotVisibilityCheckEnabled = Aimbot::VisibilityCheckEnabled();
        p.aimbotReloadSpeedEnabled = Aimbot::ReloadSpeedEnabled();
        p.aimbotReloadSpeedMultiplier = Aimbot::ReloadSpeedMultiplier();
        p.aimbotFireRateEnabled = Aimbot::FireRateEnabled();
        p.aimbotFireRateMultiplier = Aimbot::FireRateMultiplier();
        p.aimbotNoRecoilEnabled = Aimbot::NoRecoilEnabled();
        p.aimbotNoSpreadEnabled = Aimbot::NoSpreadEnabled();
        return p;
    }

    void ApplyProfile(const EspProfile& p) {
        PlayerBoxes::Enabled() = p.enabled;
        PlayerBoxes::CornerMode() = p.cornerMode;
        PlayerBoxes::Filled() = p.filled;
        PlayerBoxes::FillAlpha() = p.fillAlpha;
        PlayerBoxes::Thickness() = p.thickness;
        {
            float* c = PlayerBoxes::ColorRgb();
            c[0] = p.boxColor[0];
            c[1] = p.boxColor[1];
            c[2] = p.boxColor[2];
        }
        PlayerBoxes::UseTeamColors() = p.useTeamColors;
        {
            float* c = PlayerBoxes::TeamColorRgb();
            c[0] = p.teamColor[0];
            c[1] = p.teamColor[1];
            c[2] = p.teamColor[2];
        }
        {
            float* c = PlayerBoxes::EnemyColorRgb();
            c[0] = p.enemyColor[0];
            c[1] = p.enemyColor[1];
            c[2] = p.enemyColor[2];
        }
        PlayerBoxes::TeamFilterMode() = p.teamFilterMode;
        PlayerBoxes::VisibilityMode() = p.visibilityMode;
        PlayerBoxes::PerFeatureDistanceLimitsEnabled() = p.perFeatureDistanceLimitsEnabled;
        PlayerBoxes::MaxBoxDistanceMeters() = p.maxBoxDistanceMeters;
        PlayerBoxes::MaxSkeletonDistanceMeters() = p.maxSkeletonDistanceMeters;
        PlayerBoxes::MaxInfoDistanceMeters() = p.maxInfoDistanceMeters;
        PlayerBoxes::MaxChamsDistanceMeters() = p.maxChamsDistanceMeters;
        PlayerBoxes::InfoPosition() = p.infoPosition;
        PlayerBoxes::SkeletonEnabled() = p.skeletonEnabled;
        PlayerBoxes::SkeletonThickness() = p.skeletonThickness;
        PlayerBoxes::ChamsEnabled() = p.chamsEnabled;
        PlayerBoxes::ChamsAlpha() = p.chamsAlpha;
        PlayerBoxes::ChamsSolidMode() = p.chamsSolidMode;
        PlayerBoxes::ChamsBrightness() = p.chamsBrightness;
        PlayerBoxes::ShowName() = p.showName;
        PlayerBoxes::ShowDistance() = p.showDistance;
        PlayerBoxes::ShowHealth() = p.showHealth;
        PlayerBoxes::HealthBarEnabled() = p.healthBarEnabled;
        PlayerBoxes::ShowNetworkId() = p.showNetworkId;
        PlayerBoxes::ShowClassRank() = p.showClassRank;
        PlayerBoxes::ShowFaction() = p.showFaction;
        PlayerBoxes::CannonMapEnabled() = p.cannonMapEnabled;
        PlayerBoxes::CannonMapRequireContext() = p.cannonMapRequireContext;
        PlayerBoxes::CannonMapPosX() = p.cannonMapPosX;
        PlayerBoxes::CannonMapPosY() = p.cannonMapPosY;
        PlayerBoxes::CannonMapSizePx() = std::clamp(p.cannonMapSizePx, 140.0f, 420.0f);
        PlayerBoxes::CannonMapRangeMeters() = std::clamp(p.cannonMapRangeMeters, 50.0f, 1200.0f);
        PlayerBoxes::CannonMapShowTeammates() = p.cannonMapShowTeammates;
        PlayerBoxes::CannonImpactMarkerEnabled() = p.cannonImpactMarkerEnabled;
        PlayerBoxes::CannonImpactVelocity() = std::clamp(p.cannonImpactVelocity, 40.0f, 260.0f);
        PlayerBoxes::CannonImpactGravity() = std::clamp(p.cannonImpactGravity, 0.0f, 20.0f);
        Aimbot::Enabled() = p.aimbotEnabled;
        Aimbot::RequireKey() = p.aimbotRequireKey;
        Aimbot::AimKey() = p.aimbotAimKey;
        Aimbot::FovPixels() = p.aimbotFovPixels;
        Aimbot::Smooth() = p.aimbotSmooth;
        Aimbot::TargetBone() = p.aimbotTargetBone;
        Aimbot::TeamFilterMode() = p.aimbotTeamFilterMode;
        Aimbot::DrawFovCircle() = p.aimbotDrawFov;
        {
            float* c = Aimbot::FovColorRgb();
            c[0] = p.aimbotFovColor[0];
            c[1] = p.aimbotFovColor[1];
            c[2] = p.aimbotFovColor[2];
        }
        Aimbot::DropCompensationEnabled() = p.aimbotDropEnabled;
        Aimbot::UseWeaponBallistics() = p.aimbotUseWeaponBallistics;
        Aimbot::DefaultMuzzleVelocity() = p.aimbotDefaultVelocity;
        Aimbot::DefaultGravity() = p.aimbotDefaultGravity;
        Aimbot::VisibilityCheckEnabled() = p.aimbotVisibilityCheckEnabled;
        Aimbot::ReloadSpeedEnabled() = p.aimbotReloadSpeedEnabled;
        Aimbot::ReloadSpeedMultiplier() = std::clamp(p.aimbotReloadSpeedMultiplier, 1.0f, 5.0f);
        Aimbot::FireRateEnabled() = p.aimbotFireRateEnabled;
        Aimbot::FireRateMultiplier() = std::clamp(p.aimbotFireRateMultiplier, 1.0f, 5.0f);
        Aimbot::NoRecoilEnabled() = p.aimbotNoRecoilEnabled;
        Aimbot::NoSpreadEnabled() = p.aimbotNoSpreadEnabled;
    }

    void ReadProfileObject(const std::string& objectText, EspProfile& profile) {
        TryReadBool(objectText, "enabled", profile.enabled);
        TryReadBool(objectText, "cornerMode", profile.cornerMode);
        TryReadBool(objectText, "filled", profile.filled);
        TryReadFloat(objectText, "fillAlpha", profile.fillAlpha);
        TryReadFloat(objectText, "thickness", profile.thickness);
        TryReadColor3(objectText, "boxColor", profile.boxColor);
        TryReadBool(objectText, "useTeamColors", profile.useTeamColors);
        TryReadColor3(objectText, "teamColor", profile.teamColor);
        TryReadColor3(objectText, "enemyColor", profile.enemyColor);
        TryReadInt(objectText, "teamFilterMode", profile.teamFilterMode);
        TryReadInt(objectText, "visibilityMode", profile.visibilityMode);
        const bool hasPerFeatureEnabled = TryReadBool(objectText, "perFeatureDistanceLimitsEnabled", profile.perFeatureDistanceLimitsEnabled);
        const bool hasBoxDistance = TryReadFloat(objectText, "maxBoxDistanceMeters", profile.maxBoxDistanceMeters);
        const bool hasSkeletonDistance = TryReadFloat(objectText, "maxSkeletonDistanceMeters", profile.maxSkeletonDistanceMeters);
        const bool hasInfoDistance = TryReadFloat(objectText, "maxInfoDistanceMeters", profile.maxInfoDistanceMeters);
        const bool hasChamsDistance = TryReadFloat(objectText, "maxChamsDistanceMeters", profile.maxChamsDistanceMeters);

        bool legacyDistanceEnabled = false;
        float legacyMaxDistance = 200.0f;
        const bool hasLegacyEnabled = TryReadBool(objectText, "distanceLimitEnabled", legacyDistanceEnabled);
        const bool hasLegacyDistance = TryReadFloat(objectText, "maxDistanceMeters", legacyMaxDistance);

        if (!hasPerFeatureEnabled && hasLegacyEnabled) {
            profile.perFeatureDistanceLimitsEnabled = legacyDistanceEnabled;
        }
        if (hasLegacyDistance) {
            if (!hasBoxDistance) {
                profile.maxBoxDistanceMeters = legacyMaxDistance;
            }
            if (!hasSkeletonDistance) {
                profile.maxSkeletonDistanceMeters = legacyMaxDistance;
            }
            if (!hasInfoDistance) {
                profile.maxInfoDistanceMeters = legacyMaxDistance;
            }
            if (!hasChamsDistance) {
                profile.maxChamsDistanceMeters = legacyMaxDistance;
            }
        }
        TryReadInt(objectText, "infoPosition", profile.infoPosition);
        TryReadBool(objectText, "skeletonEnabled", profile.skeletonEnabled);
        TryReadFloat(objectText, "skeletonThickness", profile.skeletonThickness);
        TryReadBool(objectText, "chamsEnabled", profile.chamsEnabled);
        TryReadFloat(objectText, "chamsAlpha", profile.chamsAlpha);
        TryReadBool(objectText, "chamsSolidMode", profile.chamsSolidMode);
        TryReadFloat(objectText, "chamsBrightness", profile.chamsBrightness);
        TryReadBool(objectText, "showName", profile.showName);
        TryReadBool(objectText, "showDistance", profile.showDistance);
        TryReadBool(objectText, "showHealth", profile.showHealth);
        TryReadBool(objectText, "healthBarEnabled", profile.healthBarEnabled);
        TryReadBool(objectText, "showNetworkId", profile.showNetworkId);
        TryReadBool(objectText, "showClassRank", profile.showClassRank);
        TryReadBool(objectText, "showFaction", profile.showFaction);
        TryReadBool(objectText, "cannonMapEnabled", profile.cannonMapEnabled);
        TryReadBool(objectText, "cannonMapRequireContext", profile.cannonMapRequireContext);
        TryReadFloat(objectText, "cannonMapPosX", profile.cannonMapPosX);
        TryReadFloat(objectText, "cannonMapPosY", profile.cannonMapPosY);
        TryReadFloat(objectText, "cannonMapSizePx", profile.cannonMapSizePx);
        TryReadFloat(objectText, "cannonMapRangeMeters", profile.cannonMapRangeMeters);
        TryReadBool(objectText, "cannonMapShowTeammates", profile.cannonMapShowTeammates);
        TryReadBool(objectText, "cannonImpactMarkerEnabled", profile.cannonImpactMarkerEnabled);
        TryReadFloat(objectText, "cannonImpactVelocity", profile.cannonImpactVelocity);
        TryReadFloat(objectText, "cannonImpactGravity", profile.cannonImpactGravity);
        TryReadBool(objectText, "aimbotEnabled", profile.aimbotEnabled);
        TryReadBool(objectText, "aimbotRequireKey", profile.aimbotRequireKey);
        TryReadInt(objectText, "aimbotAimKey", profile.aimbotAimKey);
        TryReadFloat(objectText, "aimbotFovPixels", profile.aimbotFovPixels);
        TryReadFloat(objectText, "aimbotSmooth", profile.aimbotSmooth);
        TryReadInt(objectText, "aimbotTargetBone", profile.aimbotTargetBone);
        TryReadInt(objectText, "aimbotTeamFilterMode", profile.aimbotTeamFilterMode);
        TryReadBool(objectText, "aimbotDrawFov", profile.aimbotDrawFov);
        TryReadColor3(objectText, "aimbotFovColor", profile.aimbotFovColor);
        TryReadBool(objectText, "aimbotDropEnabled", profile.aimbotDropEnabled);
        TryReadBool(objectText, "aimbotUseWeaponBallistics", profile.aimbotUseWeaponBallistics);
        TryReadFloat(objectText, "aimbotDefaultVelocity", profile.aimbotDefaultVelocity);
        TryReadFloat(objectText, "aimbotDefaultGravity", profile.aimbotDefaultGravity);
        TryReadBool(objectText, "aimbotVisibilityCheckEnabled", profile.aimbotVisibilityCheckEnabled);
        TryReadBool(objectText, "aimbotReloadSpeedEnabled", profile.aimbotReloadSpeedEnabled);
        TryReadFloat(objectText, "aimbotReloadSpeedMultiplier", profile.aimbotReloadSpeedMultiplier);
        TryReadBool(objectText, "aimbotFireRateEnabled", profile.aimbotFireRateEnabled);
        TryReadFloat(objectText, "aimbotFireRateMultiplier", profile.aimbotFireRateMultiplier);
        TryReadBool(objectText, "aimbotNoRecoilEnabled", profile.aimbotNoRecoilEnabled);
        TryReadBool(objectText, "aimbotNoSpreadEnabled", profile.aimbotNoSpreadEnabled);
    }

    bool ParseConfigDb(const std::string& jsonText, ConfigDb& outDb) {
        outDb = ConfigDb{};

        const std::string activeMarker = "\"activeProfile\"";
        size_t activePos = jsonText.find(activeMarker);
        if (activePos != std::string::npos) {
            size_t colon = jsonText.find(':', activePos + activeMarker.size());
            if (colon != std::string::npos) {
                std::string active;
                size_t parsePos = colon + 1;
                if (ParseJsonStringAt(jsonText, parsePos, active)) {
                    outDb.activeProfile = active;
                }
            }
        }

        const std::string profilesMarker = "\"profiles\"";
        size_t profilesPos = jsonText.find(profilesMarker);
        if (profilesPos == std::string::npos) {
            return true;
        }

        size_t colon = jsonText.find(':', profilesPos + profilesMarker.size());
        if (colon == std::string::npos) {
            return false;
        }
        size_t objStart = SkipWs(jsonText, colon + 1);
        if (objStart >= jsonText.size() || jsonText[objStart] != '{') {
            return false;
        }
        const size_t objEnd = FindMatchingBrace(jsonText, objStart);
        if (objEnd == std::string::npos) {
            return false;
        }

        size_t pos = objStart + 1;
        while (pos < objEnd) {
            pos = SkipWs(jsonText, pos);
            if (pos >= objEnd) {
                break;
            }
            if (jsonText[pos] == ',') {
                ++pos;
                continue;
            }

            std::string profileName;
            if (!ParseJsonStringAt(jsonText, pos, profileName)) {
                return false;
            }

            pos = SkipWs(jsonText, pos);
            if (pos >= objEnd || jsonText[pos] != ':') {
                return false;
            }
            ++pos;

            pos = SkipWs(jsonText, pos);
            if (pos >= objEnd || jsonText[pos] != '{') {
                return false;
            }

            const size_t profileObjEnd = FindMatchingBrace(jsonText, pos);
            if (profileObjEnd == std::string::npos || profileObjEnd > objEnd) {
                return false;
            }

            EspProfile profile{};
            ReadProfileObject(jsonText.substr(pos, profileObjEnd - pos + 1), profile);
            outDb.profiles[profileName] = profile;

            pos = profileObjEnd + 1;
        }

        return true;
    }

    std::string BuildConfigDbJson(const ConfigDb& db) {
        std::ostringstream out;
        out.setf(std::ios::fixed);
        out.precision(3);

        out << "{\n";
        out << "  \"activeProfile\": \"" << EscapeJson(db.activeProfile) << "\",\n";
        out << "  \"profiles\": {\n";

        size_t index = 0;
        for (const auto& [name, p] : db.profiles) {
            out << "    \"" << EscapeJson(name) << "\": {\n";
            out << "      \"enabled\": " << (p.enabled ? "true" : "false") << ",\n";
            out << "      \"cornerMode\": " << (p.cornerMode ? "true" : "false") << ",\n";
            out << "      \"filled\": " << (p.filled ? "true" : "false") << ",\n";
            out << "      \"fillAlpha\": " << p.fillAlpha << ",\n";
            out << "      \"thickness\": " << p.thickness << ",\n";
            out << "      \"boxColor\": [" << p.boxColor[0] << ", " << p.boxColor[1] << ", " << p.boxColor[2] << "],\n";
            out << "      \"useTeamColors\": " << (p.useTeamColors ? "true" : "false") << ",\n";
            out << "      \"teamColor\": [" << p.teamColor[0] << ", " << p.teamColor[1] << ", " << p.teamColor[2] << "],\n";
            out << "      \"enemyColor\": [" << p.enemyColor[0] << ", " << p.enemyColor[1] << ", " << p.enemyColor[2] << "],\n";
            out << "      \"teamFilterMode\": " << p.teamFilterMode << ",\n";
            out << "      \"visibilityMode\": " << p.visibilityMode << ",\n";
            out << "      \"perFeatureDistanceLimitsEnabled\": " << (p.perFeatureDistanceLimitsEnabled ? "true" : "false") << ",\n";
            out << "      \"maxBoxDistanceMeters\": " << p.maxBoxDistanceMeters << ",\n";
            out << "      \"maxSkeletonDistanceMeters\": " << p.maxSkeletonDistanceMeters << ",\n";
            out << "      \"maxInfoDistanceMeters\": " << p.maxInfoDistanceMeters << ",\n";
            out << "      \"maxChamsDistanceMeters\": " << p.maxChamsDistanceMeters << ",\n";
            out << "      \"infoPosition\": " << p.infoPosition << ",\n";
            out << "      \"skeletonEnabled\": " << (p.skeletonEnabled ? "true" : "false") << ",\n";
            out << "      \"skeletonThickness\": " << p.skeletonThickness << ",\n";
            out << "      \"chamsEnabled\": " << (p.chamsEnabled ? "true" : "false") << ",\n";
            out << "      \"chamsAlpha\": " << p.chamsAlpha << ",\n";
            out << "      \"chamsSolidMode\": " << (p.chamsSolidMode ? "true" : "false") << ",\n";
            out << "      \"chamsBrightness\": " << p.chamsBrightness << ",\n";
            out << "      \"showName\": " << (p.showName ? "true" : "false") << ",\n";
            out << "      \"showDistance\": " << (p.showDistance ? "true" : "false") << ",\n";
            out << "      \"showHealth\": " << (p.showHealth ? "true" : "false") << ",\n";
            out << "      \"healthBarEnabled\": " << (p.healthBarEnabled ? "true" : "false") << ",\n";
            out << "      \"showNetworkId\": " << (p.showNetworkId ? "true" : "false") << ",\n";
            out << "      \"showClassRank\": " << (p.showClassRank ? "true" : "false") << ",\n";
            out << "      \"showFaction\": " << (p.showFaction ? "true" : "false") << ",\n";
            out << "      \"cannonMapEnabled\": " << (p.cannonMapEnabled ? "true" : "false") << ",\n";
            out << "      \"cannonMapRequireContext\": " << (p.cannonMapRequireContext ? "true" : "false") << ",\n";
            out << "      \"cannonMapPosX\": " << p.cannonMapPosX << ",\n";
            out << "      \"cannonMapPosY\": " << p.cannonMapPosY << ",\n";
            out << "      \"cannonMapSizePx\": " << p.cannonMapSizePx << ",\n";
            out << "      \"cannonMapRangeMeters\": " << p.cannonMapRangeMeters << ",\n";
            out << "      \"cannonMapShowTeammates\": " << (p.cannonMapShowTeammates ? "true" : "false") << ",\n";
            out << "      \"cannonImpactMarkerEnabled\": " << (p.cannonImpactMarkerEnabled ? "true" : "false") << ",\n";
            out << "      \"cannonImpactVelocity\": " << p.cannonImpactVelocity << ",\n";
            out << "      \"cannonImpactGravity\": " << p.cannonImpactGravity << ",\n";
            out << "      \"aimbotEnabled\": " << (p.aimbotEnabled ? "true" : "false") << ",\n";
            out << "      \"aimbotRequireKey\": " << (p.aimbotRequireKey ? "true" : "false") << ",\n";
            out << "      \"aimbotAimKey\": " << p.aimbotAimKey << ",\n";
            out << "      \"aimbotFovPixels\": " << p.aimbotFovPixels << ",\n";
            out << "      \"aimbotSmooth\": " << p.aimbotSmooth << ",\n";
            out << "      \"aimbotTargetBone\": " << p.aimbotTargetBone << ",\n";
            out << "      \"aimbotTeamFilterMode\": " << p.aimbotTeamFilterMode << ",\n";
            out << "      \"aimbotDrawFov\": " << (p.aimbotDrawFov ? "true" : "false") << ",\n";
            out << "      \"aimbotFovColor\": [" << p.aimbotFovColor[0] << ", " << p.aimbotFovColor[1] << ", " << p.aimbotFovColor[2] << "],\n";
            out << "      \"aimbotDropEnabled\": " << (p.aimbotDropEnabled ? "true" : "false") << ",\n";
            out << "      \"aimbotUseWeaponBallistics\": " << (p.aimbotUseWeaponBallistics ? "true" : "false") << ",\n";
            out << "      \"aimbotDefaultVelocity\": " << p.aimbotDefaultVelocity << ",\n";
            out << "      \"aimbotDefaultGravity\": " << p.aimbotDefaultGravity << ",\n";
            out << "      \"aimbotVisibilityCheckEnabled\": " << (p.aimbotVisibilityCheckEnabled ? "true" : "false") << ",\n";
            out << "      \"aimbotReloadSpeedEnabled\": " << (p.aimbotReloadSpeedEnabled ? "true" : "false") << ",\n";
            out << "      \"aimbotReloadSpeedMultiplier\": " << p.aimbotReloadSpeedMultiplier << ",\n";
            out << "      \"aimbotFireRateEnabled\": " << (p.aimbotFireRateEnabled ? "true" : "false") << ",\n";
            out << "      \"aimbotFireRateMultiplier\": " << p.aimbotFireRateMultiplier << ",\n";
            out << "      \"aimbotNoRecoilEnabled\": " << (p.aimbotNoRecoilEnabled ? "true" : "false") << ",\n";
            out << "      \"aimbotNoSpreadEnabled\": " << (p.aimbotNoSpreadEnabled ? "true" : "false") << "\n";
            out << "    }" << (++index < db.profiles.size() ? "," : "") << "\n";
        }

        out << "  }\n";
        out << "}\n";
        return out.str();
    }

    bool LoadDbFromDisk(ConfigDb& outDb, bool tolerateMissingFile) {
        const std::filesystem::path path = GetConfigPath();
        const std::string json = ReadTextFile(path);
        if (json.empty()) {
            if (tolerateMissingFile && !std::filesystem::exists(path)) {
                outDb = ConfigDb{};
                return true;
            }
            if (tolerateMissingFile) {
                outDb = ConfigDb{};
                return true;
            }
            SetError("Failed to read config.json");
            return false;
        }

        if (!ParseConfigDb(json, outDb)) {
            SetError("Invalid config.json format");
            return false;
        }
        return true;
    }

    bool SaveDbToDisk(const ConfigDb& db) {
        const std::filesystem::path path = GetConfigPath();
        if (!WriteTextFile(path, BuildConfigDbJson(db))) {
            SetError("Failed to write config.json");
            return false;
        }
        return true;
    }

    std::string TrimCopy(const std::string& value) {
        size_t start = 0;
        while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
            ++start;
        }
        size_t end = value.size();
        while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
            --end;
        }
        return value.substr(start, end - start);
    }
}

bool AppConfig::LoadConfigFromDisk() {
    ConfigDb db{};
    if (!LoadDbFromDisk(db, true)) {
        return false;
    }

    if (db.profiles.empty()) {
        gActiveProfileName.clear();
        return true;
    }

    if (!db.activeProfile.empty()) {
        auto it = db.profiles.find(db.activeProfile);
        if (it != db.profiles.end()) {
            ApplyProfile(it->second);
            gActiveProfileName = db.activeProfile;
            SetError({});
            return true;
        }
    }

    const auto it = db.profiles.begin();
    ApplyProfile(it->second);
    gActiveProfileName = it->first;
    db.activeProfile = it->first;
    SaveDbToDisk(db);
    SetError({});
    return true;
}

bool AppConfig::SaveProfileToDisk(const std::string& profileName) {
    const std::string cleanName = TrimCopy(profileName);
    if (cleanName.empty()) {
        SetError("Profile name cannot be empty");
        return false;
    }

    ConfigDb db{};
    if (!LoadDbFromDisk(db, true)) {
        return false;
    }

    if (db.profiles.find(cleanName) != db.profiles.end()) {
        SetError("Profile name already exists");
        return false;
    }

    db.profiles[cleanName] = CaptureCurrentProfile();
    db.activeProfile = cleanName;

    if (!SaveDbToDisk(db)) {
        return false;
    }

    gActiveProfileName = cleanName;
    SetError("Saved profile: " + cleanName);
    return true;
}

bool AppConfig::LoadProfileFromDisk(const std::string& profileName) {
    const std::string cleanName = TrimCopy(profileName);
    if (cleanName.empty()) {
        SetError("Profile name cannot be empty");
        return false;
    }

    ConfigDb db{};
    if (!LoadDbFromDisk(db, false)) {
        return false;
    }

    auto it = db.profiles.find(cleanName);
    if (it == db.profiles.end()) {
        SetError("Profile not found");
        return false;
    }

    ApplyProfile(it->second);
    db.activeProfile = cleanName;
    if (!SaveDbToDisk(db)) {
        return false;
    }

    gActiveProfileName = cleanName;
    SetError("Loaded profile: " + cleanName);
    return true;
}

bool AppConfig::OverwriteProfileToDisk(const std::string& profileName) {
    const std::string cleanName = TrimCopy(profileName);
    if (cleanName.empty()) {
        SetError("Profile name cannot be empty");
        return false;
    }

    ConfigDb db{};
    if (!LoadDbFromDisk(db, false)) {
        return false;
    }

    auto it = db.profiles.find(cleanName);
    if (it == db.profiles.end()) {
        SetError("Profile not found");
        return false;
    }

    it->second = CaptureCurrentProfile();
    db.activeProfile = cleanName;
    if (!SaveDbToDisk(db)) {
        return false;
    }

    gActiveProfileName = cleanName;
    SetError("Overwritten profile: " + cleanName);
    return true;
}

bool AppConfig::DeleteProfileFromDisk(const std::string& profileName) {
    const std::string cleanName = TrimCopy(profileName);
    if (cleanName.empty()) {
        SetError("Profile name cannot be empty");
        return false;
    }

    ConfigDb db{};
    if (!LoadDbFromDisk(db, false)) {
        return false;
    }

    auto it = db.profiles.find(cleanName);
    if (it == db.profiles.end()) {
        SetError("Profile not found");
        return false;
    }

    db.profiles.erase(it);
    if (db.activeProfile == cleanName) {
        db.activeProfile = db.profiles.empty() ? std::string{} : db.profiles.begin()->first;
    }

    if (!SaveDbToDisk(db)) {
        return false;
    }

    gActiveProfileName = db.activeProfile;
    SetError("Deleted profile: " + cleanName);
    return true;
}

bool AppConfig::RenameProfileOnDisk(const std::string& oldName, const std::string& newName) {
    const std::string cleanOldName = TrimCopy(oldName);
    const std::string cleanNewName = TrimCopy(newName);

    if (cleanOldName.empty() || cleanNewName.empty()) {
        SetError("Profile names cannot be empty");
        return false;
    }
    if (cleanOldName == cleanNewName) {
        SetError("New profile name must be different");
        return false;
    }

    ConfigDb db{};
    if (!LoadDbFromDisk(db, false)) {
        return false;
    }

    auto oldIt = db.profiles.find(cleanOldName);
    if (oldIt == db.profiles.end()) {
        SetError("Profile not found");
        return false;
    }
    if (db.profiles.find(cleanNewName) != db.profiles.end()) {
        SetError("Profile name already exists");
        return false;
    }

    db.profiles[cleanNewName] = oldIt->second;
    db.profiles.erase(oldIt);

    if (db.activeProfile == cleanOldName) {
        db.activeProfile = cleanNewName;
        gActiveProfileName = cleanNewName;
    }

    if (!SaveDbToDisk(db)) {
        return false;
    }

    SetError("Renamed profile: " + cleanOldName + " -> " + cleanNewName);
    return true;
}

std::vector<std::string> AppConfig::ListProfilesFromDisk() {
    ConfigDb db{};
    if (!LoadDbFromDisk(db, true)) {
        return {};
    }

    std::vector<std::string> names;
    names.reserve(db.profiles.size());
    for (const auto& [name, _] : db.profiles) {
        names.push_back(name);
    }
    return names;
}

std::string AppConfig::GetActiveProfileName() {
    return gActiveProfileName;
}

const std::string& AppConfig::LastConfigError() {
    return gLastConfigError;
}

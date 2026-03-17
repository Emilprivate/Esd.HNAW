#pragma once

#include <string>
#include <vector>

namespace AppConfig {
    inline std::string menuTitle = "Esd.HNAW";
    inline int menuToggleKey = 0x2D;      // VK_INSERT
    inline int unloadPrimaryKey = 0x2E;   // VK_DELETE
    inline int unloadSecondaryKey = 0x23; // VK_END

    bool LoadConfigFromDisk();
    bool SaveProfileToDisk(const std::string& profileName);
    bool OverwriteProfileToDisk(const std::string& profileName);
    bool LoadProfileFromDisk(const std::string& profileName);
    bool DeleteProfileFromDisk(const std::string& profileName);
    bool RenameProfileOnDisk(const std::string& oldName, const std::string& newName);
    std::vector<std::string> ListProfilesFromDisk();
    std::string GetActiveProfileName();
    const std::string& LastConfigError();
}

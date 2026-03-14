#include "ModManager.h"

using namespace geode::prelude;

std::vector<std::string> ModManager::LibraryMusics = {};
std::filesystem::path ModManager::SaveDirectory = dirs::getSaveDir();

bool ModManager::AutoSearch = Mod::get()->getSettingValue<bool>("auto-search");
#ifndef GEODE_IS_IOS
    bool ModManager::UseLocalLibrary = Mod::get()->getSettingValue<bool>("use-local-library"); 
#else
    bool ModManager::UseLocalLibrary = false;
#endif

$execute {
    listenForSettingChanges<bool>("auto-search", [](bool enabled) {
        ModManager::AutoSearch = enabled;
    });
    #ifndef GEODE_IS_IOS
        listenForSettingChanges<bool>("use-local-library", [](bool enabled) {
            ModManager::UseLocalLibrary = enabled;
        });
    #endif
};
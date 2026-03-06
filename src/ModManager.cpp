#include "ModManager.h"

using namespace geode::prelude;

std::vector<std::string> ModManager::LibraryMusics = {};
std::filesystem::path ModManager::SaveDirectory = dirs::getSaveDir();

bool ModManager::AutoSearch = Mod::get()->getSettingValue<bool>("auto-search");

$execute {
    listenForSettingChanges<bool>("auto-search", [](bool enabled) {
        ModManager::AutoSearch = enabled;
    });
};
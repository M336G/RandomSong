#pragma once

class ModManager {
public:
    static std::vector<std::string> LibraryMusics;
    static asp::fs::Path SaveDirectory;

    static bool AutoSearch;
    static bool UseLocalLibrary;
};
#include <Geode/modify/MusicBrowser.hpp>
#include "../ModManager.h"

class $modify(MusicBrowser) {
    // Currently don't really know how I could clear this when there's an actual
    // update and not just a click on the button
    void onUpdateLibrary(CCObject* sender) {
        MusicBrowser::onUpdateLibrary(sender);

        ModManager::LibraryMusics = {};
    };
};
#include <Geode/modify/CustomSongLayer.hpp>
#include <arc/time/Sleep.hpp>
#include <asp/iter.hpp>
#include "../ModManager.h"

using namespace geode::prelude;

class $modify(MyCustomSongLayer, CustomSongLayer) {
    struct Fields {
        bool m_isBeingFetched = false;
    };

    bool init(CustomSongDelegate* delegate) {
        if (!CustomSongLayer::init(delegate))
            return false;

        auto* menu = m_mainLayer->getChildByType<CCMenu>(0);
        if (!menu) return true;

        auto* randomLibrarySongButton = CCMenuItemSpriteExtra::create(
            CircleButtonSprite::createWithSpriteFrameName("RS_randomSprite_001.png"_spr),
            this,
            menu_selector(MyCustomSongLayer::onRandomLibrarySongButton)
        );
        randomLibrarySongButton->setID("random-library-song-button"_spr);
        randomLibrarySongButton->setPosition({ 25, -125 });

        auto* randomStoredSongButton = CCMenuItemSpriteExtra::create(
            CircleButtonSprite::createWithSpriteFrameName("RS_randomSprite_002.png"_spr),
            this,
            menu_selector(MyCustomSongLayer::onRandomStoredSongButton)
        );
        randomStoredSongButton->setID("random-stored-song-button"_spr);
        randomStoredSongButton->setPosition({ 25, -185 });

        menu->addChild(randomLibrarySongButton);
        menu->addChild(randomStoredSongButton);

        return true;
    };

    void onRandomLibrarySongButton(CCObject*) {
        if (ModManager::LibraryMusics.empty()) {
            Notification::create(
                "Getting library songs...",
                NotificationIcon::Loading,
                0.5f
            )->show();

            if (m_fields->m_isBeingFetched)
                return;

            m_fields->m_isBeingFetched = true;
            async::spawn(
                []() -> arc::Future<Result<void, std::string>> {
                    // It's funnier on apple
                    #ifdef GEODE_IS_MACOS
                        auto musicLibraryPath = std::filesystem::path(getEnvironmentVariable("HOME")) / "Library" / "Caches" / "musiclibrary.dat";
                    #else
                        auto musicLibraryPath = ModManager::SaveDirectory / "musiclibrary.dat";
                    #endif

                    // If musiclibrary.dat doesn't exist, try to call the music browser's
                    // function to fetch it (compatibility with GDPSs or just changes with
                    // the game in general). If it still doesn't exist after 5 seconds then
                    // just throw a timeout error
                    if (!std::filesystem::exists(musicLibraryPath)) {
                        co_await waitForMainThread<void>([]() {
                            auto* musicBrowser = MusicBrowser::create(0, GJSongType::Music);
                            musicBrowser->trySetupMusicBrowser();
                            musicBrowser->onClose(nullptr);
                        });

                        int attempts = 0;
                        while (!std::filesystem::exists(musicLibraryPath)) {
                            if (attempts++ > 5)
                                co_return Err("Failed downloading musiclibrary.dat: timed out");
                            co_await arc::sleep(asp::Duration::fromSecs(1));
                        }
                    }

                    auto read = utils::file::readString(musicLibraryPath);
                    if (!read)
                        co_return Err("Failed reading musiclibrary.dat: {}", read.unwrapErr());

                    auto& data = read.unwrap();
                    if (data.empty())
                        co_return Err("Failed reading musiclibrary.dat: empty file");

                    std::vector<std::string> ids;

                    // After getting the data, try decoding it from base64url and then try
                    // to ZLib inflate it to get the actual unencoded/uncompressed data
                    auto decoded = ZipUtils::base64URLDecode(data);
                    if (decoded.empty())
                        co_return Err("Failed decoding musiclibrary.dat");

                    unsigned char* out = nullptr;
                    auto outSize = ZipUtils::ccInflateMemory(
                        reinterpret_cast<unsigned char*>(decoded.data()),
                        decoded.size(),
                        &out
                    );

                    if (!out || outSize <= 0)
                        co_return Err("Failed decompressing musiclibrary.dat");
                    
                    std::string decompressed(reinterpret_cast<char*>(out), outSize);
                    free(out);

                    // Then parse every single ID into its own array in the ModManager so we
                    // we don't need to redo this behavior in this session
                    int counter = 1;
                    // https://boomlings.dev/resources/client/musiclibrary
                    asp::iter::split(decompressed, '|').forEach([&ids, &counter](std::string_view const part) {
                        if (counter++ == 3) { // "{version}|{artists}|{songs}|{tags}", we want songs
                            // {song};{song};{song};...
                            asp::iter::split(part, ';').forEach([&ids](std::string_view const song) {
                                // {id},{name},{artistID},{filesize},{duration},{tags},{musicPlatform},{extraArtists},{externalLink},{newButton},{priorityOrder},{songNumber}
                                auto id = asp::iter::split(song, ',').next();

                                if (id)
                                    ids.push_back(std::string(*id));
                            });
                        }
                    });

                    if (ids.empty())
                        co_return Err("Failed parsing musiclibrary.dat: no IDs");

                    ModManager::LibraryMusics = std::move(ids);
                    co_return Ok();
                },
                [self = Ref<MyCustomSongLayer>(this)](Result<void, std::string> result) {
                    if (!result.isOk()) {
                        Notification::create(
                            "Failed to get the songs from your library!",
                            NotificationIcon::Error,
                            1.5f
                        )->show();
                        log::error("{}", result.unwrapErr());

                        self->m_fields->m_isBeingFetched = false;
                        return;
                    }

                    self->m_fields->m_isBeingFetched = false;

                    auto id = utils::random::generate<int>(0, ModManager::LibraryMusics.size());
                    self->m_songIDInput->setString(ModManager::LibraryMusics.at(id));
                    if (ModManager::AutoSearch) self->onSearch(nullptr);
                }
            );

            return;
        }

        auto id = utils::random::generate<int>(0, ModManager::LibraryMusics.size());
        m_songIDInput->setString(ModManager::LibraryMusics.at(id));
        if (ModManager::AutoSearch) onSearch(nullptr);
    };

    void onRandomStoredSongButton(CCObject*) {
        // This is kind of harder to track so just do this every time the button is clicked
        // (shouldn't really consume that much anyways)

        #ifdef GEODE_IS_MACOS
            auto files = utils::file::readDirectory(std::filesystem::path(getEnvironmentVariable("HOME")) / "Library" / "Caches");
        #else
            auto files = utils::file::readDirectory(ModManager::SaveDirectory);
        #endif

        if (!files)
            return;

        std::vector<std::string> ids;
        asp::iter::from(files.unwrap()).forEach([&ids](std::filesystem::path const& path) {
            auto filename = path.filename();
            auto extension = filename.extension();
        
            if (utils::string::pathToString(filename)[0] != 's' && (extension == ".mp3" || extension == ".ogg"))
                ids.push_back(utils::string::pathToString(filename.stem()));
        });

        if (ids.empty()) {
            Notification::create(
                "No stored songs found!",
                NotificationIcon::Error,
                1.5f
            )->show();

            return;
        }

        m_songIDInput->setString(ids.at(utils::random::generate<int>(0, ids.size())));
        if (ModManager::AutoSearch) onSearch(nullptr);
    };
};
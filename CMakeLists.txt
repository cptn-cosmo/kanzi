cmake_minimum_required(VERSION 3.21)

project(Kanzi VERSION 0.5.4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS
    Core Gui Widgets Qml Quick QuickControls2 Network DBus WebEngineQuick WebChannel WebChannelQuick)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PULSE REQUIRED IMPORTED_TARGET libpulse)

# Optional: compositor-drawn shadows for the frameless mini player.
find_package(KF6WindowSystem 6.0 QUIET)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(kanzi
    src/main.cpp
    src/applemusic.h src/applemusic.cpp
    src/player.h src/player.cpp
    src/plex.h src/plex.cpp
    src/mpris.h src/mpris.cpp
    src/networkfactory.h src/networkfactory.cpp
    src/audiooutput.h src/audiooutput.cpp
    src/tray.h src/tray.cpp
    src/discordpresence.h src/discordpresence.cpp
    src/windowshadow.h src/windowshadow.cpp
)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(qml/Nav.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(kanzi
    URI Kanzi
    VERSION 1.0
    DEPENDENCIES QtWebEngine
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/Nav.qml
        qml/PlayerHost.qml
        qml/LoginWindow.qml
        qml/MiniPlayer.qml
        qml/components/Sidebar.qml
        qml/components/SidebarItem.qml
        qml/components/PlayerBar.qml
        qml/components/IconButton.qml
        qml/components/Icon.qml
        qml/components/Artwork.qml
        qml/components/MediaCard.qml
        qml/components/Shelf.qml
        qml/components/TrackRow.qml
        qml/components/SeekBar.qml
        qml/components/PillButton.qml
        qml/components/QueuePanel.qml
        qml/components/LyricsView.qml
        qml/components/NowPlaying.qml
        qml/components/PageHeader.qml
        qml/components/MediaGrid.qml
        qml/components/Spinner.qml
        qml/components/LinkText.qml
        qml/pages/HomePage.qml
        qml/pages/BrowsePage.qml
        qml/pages/RecentPage.qml
        qml/pages/SearchPage.qml
        qml/pages/CollectionPage.qml
        qml/pages/ArtistPage.qml
        qml/pages/LibraryGridPage.qml
        qml/pages/LibrarySongsPage.qml
        qml/pages/WelcomePage.qml
        qml/pages/SettingsPage.qml
    RESOURCES
        resources/player.html
        resources/plex.html
        resources/kanzi.svg
)

target_include_directories(kanzi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(kanzi PRIVATE KANZI_VERSION="${PROJECT_VERSION}")

target_link_libraries(kanzi PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Network
    Qt6::DBus Qt6::WebEngineQuick Qt6::WebChannel Qt6::WebChannelQuick
    PkgConfig::PULSE)

if(KF6WindowSystem_FOUND)
    target_link_libraries(kanzi PRIVATE KF6::WindowSystem)
    target_compile_definitions(kanzi PRIVATE KANZI_HAVE_KWINDOWSYSTEM)
    message(STATUS "KWindowSystem found: native window shadows enabled")
else()
    message(STATUS "KWindowSystem not found: mini player will have no compositor shadow")
endif()

include(GNUInstallDirs)
install(TARGETS kanzi RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/dev.kanzi.Kanzi.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES resources/kanzi.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        RENAME dev.kanzi.Kanzi.svg)

cmake_minimum_required(VERSION 3.16)
project(LanLauncherQt VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network Concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network Concurrent)
find_package(Qt${QT_VERSION_MAJOR} OPTIONAL_COMPONENTS Svg)

set(SOURCES
    src/main.cpp
    src/AppSettings.cpp
    src/Storage.cpp
    src/GameLauncher.cpp
    src/ConfigMaker.cpp
    src/ArchiveTool.cpp
    src/Downloader.cpp
    src/Dialogs.cpp
    src/Theme.cpp
    src/I18n.cpp
    src/ImportKitDialog.cpp
    src/MakeConfigDialog.cpp
    src/MainWindow.cpp
    src/pages/ModsPage.cpp
    src/pages/ServerPage.cpp
    src/SteamDetector.cpp
    src/SetupWizard.cpp
    src/ProgressDialog.cpp
    src/SmartModInstaller.cpp
    src/CllInstaller.cpp
    src/GameCatalog.cpp
    src/AnimatedLogo.cpp
    src/pages/PlayPage.cpp
    src/pages/SettingsPage.cpp
    src/pages/AboutPage.cpp
)

set(HEADERS
    src/AppSettings.h
    src/Storage.h
    src/GameLauncher.h
    src/ConfigMaker.h
    src/ArchiveTool.h
    src/Downloader.h
    src/Dialogs.h
    src/Theme.h
    src/I18n.h
    src/ImportKitDialog.h
    src/MakeConfigDialog.h
    src/MainWindow.h
    src/pages/ModsPage.h
    src/pages/ServerPage.h
    src/SteamDetector.h
    src/SetupWizard.h
    src/ProgressDialog.h
    src/SmartModInstaller.h
    src/CllInstaller.h
    src/GameCatalog.h
    src/AnimatedLogo.h
    src/pages/PlayPage.h
    src/pages/SettingsPage.h
    src/pages/AboutPage.h
)

qt_add_resources(QRC_SOURCES resources/resources.qrc)

add_executable(LanLauncherQt
    ${SOURCES}
    ${HEADERS}
    ${QRC_SOURCES}
)

target_include_directories(LanLauncherQt PRIVATE src)

target_link_libraries(LanLauncherQt PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Concurrent
)
if (TARGET Qt${QT_VERSION_MAJOR}::Svg)
    target_link_libraries(LanLauncherQt PRIVATE Qt${QT_VERSION_MAJOR}::Svg)
endif()

if (WIN32)
    set_target_properties(LanLauncherQt PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

if (MINGW)
    target_link_options(LanLauncherQt PRIVATE -static-libgcc -static-libstdc++)
endif()

# Static Qt: window, jpeg/png, svg and TLS plugins are baked into the exe
get_target_property(_qt_core_type Qt${QT_VERSION_MAJOR}::Core TYPE)
if (_qt_core_type STREQUAL "STATIC_LIBRARY")
    message(STATUS "Qt estatico detectado — importando plugins para o executavel")
    if (MINGW)
        target_link_options(LanLauncherQt PRIVATE -static)
    endif()
    if (QT_VERSION_MAJOR GREATER_EQUAL 6)
        set(_ll_static_plugins)
        foreach(_plugin IN ITEMS
            QWindowsIntegrationPlugin
            QModernWindowsStylePlugin
            QWindowsVistaStylePlugin
            QJpegPlugin
            QGifPlugin
            QICOPlugin
            QPngPlugin
            QWebpPlugin
            QSvgPlugin
            QSvgIconPlugin
            QTlsBackendSchannelPlugin
        )
            if (TARGET Qt${QT_VERSION_MAJOR}::${_plugin})
                list(APPEND _ll_static_plugins Qt${QT_VERSION_MAJOR}::${_plugin})
            endif()
        endforeach()
        if (_ll_static_plugins)
            qt_import_plugins(LanLauncherQt INCLUDE ${_ll_static_plugins})
        endif()
    endif()
endif()

install(TARGETS LanLauncherQt RUNTIME DESTINATION .)

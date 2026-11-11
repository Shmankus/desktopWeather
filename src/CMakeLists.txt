qt_add_executable(desktop-weather
    main.cpp
    WindowTracker.cpp
    WindowTracker.h
    particle.cpp
    particle.h
    raindrop.cpp
    raindrop.h
    snow.cpp
    snow.h
    pixel_overlay.cpp
    pixel_overlay.h
    GLOBAL_VARS.h
)

qt_add_qml_module(desktop-weather
    URI org.kde.desktopweather
    VERSION 1.0
    NO_IMPORT_SCAN
    QML_FILES
        Main.qml
)

target_link_libraries(desktop-weather
    PRIVATE
        nlohmann_json::nlohmann_json
        CURL::libcurl
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::DBus
        Qt6::Widgets
        KF6::CoreAddons
        KF6::I18n
        KF6::IconThemes
        KF6::StatusNotifierItem
        LayerShellQt::Interface
        
)

install(TARGETS desktop-weather ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

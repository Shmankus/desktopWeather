cmake_minimum_required(VERSION 3.20)
project(desktop-weather VERSION 0.1.0)

find_package(ECM 6.0.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMFindQmlModule)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    DBus
    Gui
    Quick
    QuickControls2
    Widgets
)

qt_standard_project_setup(REQUIRES 6.5)


find_package(KF6 REQUIRED COMPONENTS
    CoreAddons
    I18n
    IconThemes
    Kirigami
    QQC2DesktopStyle
    StatusNotifierItem
)

ecm_find_qmlmodule(org.kde.kirigami REQUIRED)

add_subdirectory(src)

install(FILES org.kde.desktopweather.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(DIRECTORY kwin/desktop-weather-window-tracker
    DESTINATION ${KDE_INSTALL_DATADIR}/kwin/scripts
)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
#pragma once

#include <vector>
#include <QApplication>

inline const std::vector<QString> SKIPPED_WINDOWS = {
    QStringLiteral("plasmashell"),
    QStringLiteral("desktop-weather"),
    QStringLiteral("xwaylandvideobridge")

};


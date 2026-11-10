#pragma once

#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QMenu>
#include <QPainter>
#include <QPaintEvent>
#include <QRandomGenerator>
#include <QScreen>
#include <QTimer>
#include <QtMath>
#include <KStatusNotifierItem>

#include <algorithm>
#include <cmath>
#include <vector>
#include "WindowTracker.h"
#include "particle.h"

class RainDrop : public Particle
{

public:

    

    using Particle::Particle; 
    void advance() override;

    explicit RainDrop(const QSize &areaSize, const WindowTracker &tracker, const QString &screenName);
};
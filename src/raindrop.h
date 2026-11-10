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

class RainDrop
{
    static constexpr qreal dropSize = 5;

    QPointF m_position;
    QSize m_areaSize;
    const WindowTracker &m_tracker;

    // The output this drop falls on. Window geometry from the KWin script is
    // reported per screen, so only windows on this screen can be collided with.
    QString m_screenName;

    qreal m_grav = 35;
    qreal m_default_grav = m_grav;

    qreal m_Xvel = 1000;
    qreal m_Yvel = 1000;

    qreal m_freeFall_Yvel;

    qreal m_default_m_Xvel = m_Xvel;
    qreal m_default_m_Yvel = m_Yvel;

    qreal m_angle;

    std::chrono::high_resolution_clock::time_point lastTime = std::chrono::high_resolution_clock::now();


    std::chrono::high_resolution_clock::time_point spawnTime = std::chrono::high_resolution_clock::now();

public:
    void startAtRandomEdge();
    void setAreaSize(const QSize &areaSize);
    void setDropAngle(double decAngle);
    qreal getDropAngle();
    void advance();
    QRectF bounds() const;
    void draw(QPainter &painter) const;
    explicit RainDrop(const QSize &areaSize, const WindowTracker &tracker, const QString &screenName);
};
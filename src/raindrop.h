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
#include <cmath>
#include <vector>
#include "WindowTracker.h"

class RainDrop
{
    static constexpr qreal dropSize = 5;

    QPointF m_position;
    QSize m_areaSize;
    const WindowTracker &m_tracker;

    qreal m_grav = 10;
    qreal m_default_grav = m_grav;

    qreal m_Xvel = 500;
    qreal m_Yvel = 100;

    qreal m_freeFall_Yvel;

    qreal m_default_m_Xvel = m_Xvel;
    qreal m_default_m_Yvel = m_Yvel;

    qreal m_angle;

    std::chrono::high_resolution_clock::time_point lastTime = std::chrono::high_resolution_clock::now();

public:
    void startAtRandomEdge();
    void setDropAngle(double decAngle);
    qreal getDropAngle();
    void advance();
    QRectF bounds() const;
    void draw(QPainter &painter) const;
    explicit RainDrop(const QSize &areaSize, const WindowTracker &tracker);
};
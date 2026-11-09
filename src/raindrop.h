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
    qreal m_Xvel;
    qreal m_Yvel;

    qreal m_grav = 5;
    qreal m_default_grav = 5;

    qreal m_default_m_Xvel;
    qreal m_default_m_Yvel;

    qreal m_angle;
public:
    void startAtRandomEdge();
     void setDropAngle(double decAngle);
     qreal getDropAngle();
     void advance();
     QRectF bounds() const;
     void draw(QPainter &painter) const;
     explicit RainDrop(const QSize &areaSize, const WindowTracker &tracker);
    
};
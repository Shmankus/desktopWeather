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
#include <cmath>;
#include <vector>;
#include "WindowTracker.h"
#include "raindrop.h"

// A single transparent window holds the pixel image and all the raindrops.
// It passes mouse input through so the user's other windows remain clickable.
class PixelOverlay : public QWidget
{
    QImage m_pixels;

    QTimer m_timer;

protected:
    // Qt calls this whenever the overlay needs to appear on screen again.
    void paintEvent(QPaintEvent *) override;
   
public:
    std::vector<RainDrop> m_drops;
    explicit PixelOverlay(const QRect &desktopGeometry, const WindowTracker &tracker);
  
};

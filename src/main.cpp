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
#include <QHash>
#include <QRegion>
#include <QScreen>
#include <QTimer>
#include <QtMath>
#include <KStatusNotifierItem>
#include <cmath>
#include <memory>
#include <vector>

#include "WindowTracker.h"
#include "raindrop.h"
#include "pixel_overlay.h"
#include "GLOBAL_VARS.h"

// Ask KWin to start its scripts, then check whether this app's tracker is loaded.
static void checkKWinWindowTracker(QApplication &app, const QDBusConnection &sessionBus)
{
    QTimer::singleShot(0, &app, [&app, sessionBus]()
                       {
        const QDBusMessage startScripts = QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.KWin"),
            QStringLiteral("/Scripting"),
            QStringLiteral("org.kde.kwin.Scripting"),
            QStringLiteral("start"));
        if (!sessionBus.send(startScripts))
        {
            qWarning() << "Could not ask KWin to start its enabled scripts:"
                       << sessionBus.lastError().message();
            return;
        }

        QTimer::singleShot(2000, &app, [&app, sessionBus]()
        {
            QDBusMessage checkScript = QDBusMessage::createMethodCall(
                QStringLiteral("org.kde.KWin"),
                QStringLiteral("/Scripting"),
                QStringLiteral("org.kde.kwin.Scripting"),
                QStringLiteral("isScriptLoaded"));
            checkScript << QStringLiteral("desktop-weather-window-tracker");

            auto *watcher = new QDBusPendingCallWatcher(sessionBus.asyncCall(checkScript), &app);
            QObject::connect(watcher, &QDBusPendingCallWatcher::finished, &app,
                             [](QDBusPendingCallWatcher *call)
            {
                const QDBusPendingReply<bool> reply = *call;
                if (reply.isError())
                {
                    qWarning() << "Could not check the KWin window tracker:"
                               << reply.error().message();
                }
                else if (!reply.value())
                {
                    qWarning() << "KWin window tracker is not enabled. Enable it in "
                                  "System Settings > Window Management > KWin Scripts.";
                }
                else
                {
                    qInfo() << "KWin window tracker is active.";
                }
                call->deleteLater();
            });
        }); });
}

int main(int argc, char *argv[])
{
    // The overlay is drawn as a wlr-layer-shell surface rather than an ordinary
    // top-level window, so the compositor owns its placement and it never
    // appears in the taskbar, the task switcher or the pager, and cannot take
    // focus. See PixelOverlay::configureAsLayerSurface(). Since Qt 6.5 the
    // shell integration is selected automatically, so nothing is needed here.
    // QApplication owns the event loop that runs timers, paints windows,
    // receives D-Bus calls, and handles tray-menu actions.
    QApplication app(argc, argv);
    qInfo() << "Desktop Weather started; Qt platform:" << QGuiApplication::platformName();
    if (QGuiApplication::platformName() != QStringLiteral("wayland"))
    {
        qWarning() << "Not running on Wayland: layer-shell is unavailable, so the "
                      "overlay stays an ordinary window and may appear in the taskbar.";
    }
    // Register the object that receives window information from the KWin script.
    WindowTracker tracker;
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (!sessionBus.registerService(QStringLiteral("org.kde.desktopweather")))
    {
        qCritical() << "Unable to register the window tracker D-Bus service:"
                    << sessionBus.lastError().message();
        return 1;
    }
    if (!sessionBus.registerObject(
            QStringLiteral("/DesktopWeather"), &tracker, QDBusConnection::ExportAllSlots))
    {
        qCritical() << "Unable to export the window tracker D-Bus object:"
                    << sessionBus.lastError().message();
        return 1;
    }
    checkKWinWindowTracker(app, sessionBus);

    // One transparent overlay per screen. A layer surface covers exactly one
    // output, so whole-desktop rain means one surface, and one set of drops,
    // for every connected screen.
    const QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.isEmpty())
    {
        qCritical() << "Unable to create the pixel overlay: no screens are available.";
        return 1;
    }

    std::vector<std::unique_ptr<PixelOverlay>> overlays;
    overlays.reserve(screens.size());
    for (QScreen *screen : screens)
    {
        auto overlay = std::make_unique<PixelOverlay>(screen, tracker);
        overlay->configureAsLayerSurface();
        overlay->startOverlay();
        overlay->show();
        qInfo() << "Raining on" << screen->name() << screen->geometry();
        overlays.push_back(std::move(overlay));
    }

    // The status notifier item is the app's tray icon and context menu.
    auto *trayItem = new KStatusNotifierItem(&app);
    trayItem->setIconByName(QStringLiteral("weather-showers"));
    trayItem->setStatus(KStatusNotifierItem::Active);
    trayItem->setTitle(QStringLiteral("Desktop Weather"));
    trayItem->setToolTip(
        QStringLiteral("weather-showers"), QStringLiteral("Desktop Weather"), QStringLiteral("Weather app"));
    trayItem->setIsMenu(true);

    QMenu *menu = trayItem->contextMenu();

    // Refresh droplet info whenever new window info is available
    QObject::connect(&tracker, &WindowTracker::windowsUpdated, &app,
                     [&overlays](const QJsonArray &windows)
                     {
                         // How much of each screen real windows actually cover.
                         // Rectangles are merged into a QRegion rather than
                         // summed, so where two windows overlap the shared area
                         // is counted once instead of twice. Minimised and
                         // hidden windows never reach us: the KWin script
                         // filters them out of the snapshot.
                         QHash<QString, QRegion> covered;
                         for (const QJsonValue &value : windows)
                         {
                             const QJsonObject window = value.toObject();

                             // plasmashell and friends are not real windows.
                             if (std::find(SKIPPED_WINDOWS.begin(), SKIPPED_WINDOWS.end(),
                                           window.value(QStringLiteral("appId")).toString()) != SKIPPED_WINDOWS.end())
                             {
                                 continue;
                             }

                             // x and y are relative to the window's own screen,
                             // so each screen is accumulated separately.
                             const QRect bounds(window.value(QStringLiteral("x")).toInt(),
                                                window.value(QStringLiteral("y")).toInt(),
                                                window.value(QStringLiteral("width")).toInt(),
                                                window.value(QStringLiteral("height")).toInt());

                             covered[window.value(QStringLiteral("screenName")).toString()] |= bounds;
                         }

                         for (const std::unique_ptr<PixelOverlay> &overlay : overlays)
                         {
                             // The overlay's own size is the coordinate space the
                             // drops live in, so coverage is measured against it.
                             const QRect screenRect(QPoint(0, 0), overlay->size());
                             const qint64 screenArea = qint64(screenRect.width()) * screenRect.height();
                             if (screenArea <= 0)
                             {
                                 continue;
                             }

                             // Clipping keeps a window hanging off the edge of
                             // the screen from inflating the total.
                             const QRegion onThisScreen = covered.value(overlay->screenName()) & screenRect;

                             // A QRegion's rectangles are disjoint, so adding
                             // them up gives the exact covered area.
                             qint64 coveredArea = 0;
                             for (const QRect &rect : onThisScreen)
                             {
                                 coveredArea += qint64(rect.width()) * rect.height();
                             }

                             // Stop rendering a screen that is almost entirely
                             // covered, so fullscreen video and games pay no CPU
                             // cost, and leave the other screens raining.
                             const bool tooCovered = coveredArea >= screenArea * 0.85;

                             if (tooCovered && overlay->isTimerActive())
                             {
                                 overlay->stopOverlay();
                                 overlay->hide();
                             }
                             else if (!tooCovered && !overlay->isTimerActive())
                             {
                                 overlay->startOverlay();
                                 overlay->show();
                             }
                         }
                     });

    // Let the user show or hide the animated overlay from the tray menu.

    auto *randomAngle = new QAction(QString::number(overlays.front()->m_drops[0].getDropAngle()), &app);
    menu->addAction(randomAngle);
    QObject::connect(randomAngle, &QAction::triggered, &app,
                     [randomAngle, &overlays]()
                     {
                         std::random_device rd;
                         std::mt19937 gen(rd());
                         std::uniform_int_distribution<> distrib(70, 90);
                         double random_num = distrib(gen);
                         for (const std::unique_ptr<PixelOverlay> &overlay : overlays)
                         {
                             for (RainDrop &drop : overlay->m_drops)
                             {
                                 drop.setDropAngle(random_num);
                             }
                         }
                         randomAngle->setText(QString::number(random_num));
                     });

    // Run until the user quits the application.
    return app.exec();
}

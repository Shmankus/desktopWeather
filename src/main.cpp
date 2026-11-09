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
#include "pixel_overlay.h"
#include "GLOBAL_VARS.h"

// Combine every monitor's rectangle into one rectangle for the overlay window.
static QRect desktopGeometry()
{
    QRect geometry;
    for (QScreen *screen : QGuiApplication::screens())
    {
        geometry = geometry.isNull()
                       ? screen->geometry()
                       : geometry.united(screen->geometry());
    }
    return geometry;
}

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

// Turn one KWin window description into a disabled item in the tray menu.
// static void addTrackedWindow(QMenu *windowsMenu, const QJsonObject &window)
// {
//     const QString title = window.value(QStringLiteral("title")).toString();
//     const QString appId = window.value(QStringLiteral("appId")).toString();
//     const int screen = window.value(QStringLiteral("screen")).toInt(-1);
//     const QString screenLabel = screen >= 0
//                                     ? QStringLiteral("Screen %1").arg(screen)
//                                     : QStringLiteral("Unknown screen");
//     const QString name = title.isEmpty() ? appId : title;

//     QString entryLabel = name;
//     if (!appId.isEmpty() && appId != name)
//     {
//         entryLabel += QStringLiteral(" — %1").arg(appId);
//     }
//     entryLabel += QStringLiteral(" (%1)").arg(screenLabel);

//     QAction *entry = windowsMenu->addAction(entryLabel);
//     entry->setEnabled(false);

//     QStringList states;
//     if (window.value(QStringLiteral("active")).toBool())
//     {
//         states.append(QStringLiteral("active"));
//     }
//     if (window.value(QStringLiteral("fullScreen")).toBool())
//     {
//         states.append(QStringLiteral("fullscreen"));
//     }
//     if (window.value(QStringLiteral("borderlessFullscreen")).toBool())
//     {
//         states.append(QStringLiteral("borderless fullscreen"));
//     }
//     if (window.value(QStringLiteral("minimized")).toBool())
//     {
//         states.append(QStringLiteral("minimized"));
//     }

//     const QString stateDetails = states.isEmpty()
//                                      ? QString()
//                                      : QStringLiteral(" — %1").arg(states.join(QStringLiteral(", ")));
//     entry->setToolTip(QStringLiteral("%1×%2 at (%3, %4) on %5%6")
//                           .arg(window.value(QStringLiteral("width")).toInt())
//                           .arg(window.value(QStringLiteral("height")).toInt())
//                           .arg(window.value(QStringLiteral("x")).toInt())
//                           .arg(window.value(QStringLiteral("y")).toInt())
//                           .arg(screenLabel)
//                           .arg(stateDetails));

//     qInfo().noquote() << QStringLiteral("%1 — %2, %3×%4 at (%5, %6) on %7%8")
//                              .arg(name, appId)
//                              .arg(window.value(QStringLiteral("width")).toInt())
//                              .arg(window.value(QStringLiteral("height")).toInt())
//                              .arg(window.value(QStringLiteral("x")).toInt())
//                              .arg(window.value(QStringLiteral("y")).toInt())
//                              .arg(screenLabel)
//                              .arg(states.isEmpty()
//                                       ? QString()
//                                       : QStringLiteral(" [%1]").arg(states.join(QStringLiteral(", "))));
// }

// Replace the tray's window list with the latest snapshot sent by KWin.
// static void updateTrackedWindowsMenu(QMenu *windowsMenu, const QJsonArray &windows)
// {
//     windowsMenu->clear();
//     windowsMenu->setTitle(QStringLiteral("Tracked windows (%1)").arg(windows.size()));

//     if (windows.isEmpty())
//     {
//         QAction *emptyAction = windowsMenu->addAction(QStringLiteral("No windows found"));
//         emptyAction->setEnabled(false);
//     }

//     // qInfo() << "KWin reported" << windows.size() << "windows.";
//     // for (const QJsonValue &value : windows)
//     // {
//     //     addTrackedWindow(windowsMenu, value.toObject());
//     // }
// }

int main(int argc, char *argv[])
{
    // Prefer XWayland when both display types are available, since Wayland
    // does not let ordinary apps choose exact top-level window positions.
    if (qEnvironmentVariableIsSet("WAYLAND_DISPLAY") && qEnvironmentVariableIsSet("DISPLAY") && !qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
    {
        qputenv("QT_QPA_PLATFORM", "xcb");
    }

    // QApplication owns the event loop that runs timers, paints windows,
    // receives D-Bus calls, and handles tray-menu actions.
    QApplication app(argc, argv);
    qInfo() << "Desktop Weather started; Qt platform:" << QGuiApplication::platformName();
    if (QGuiApplication::platformName() == QStringLiteral("wayland"))
    {
        qWarning() << "The Wayland compositor controls top-level window placement; "
                      "set QT_QPA_PLATFORM=xcb to use the requested box position via XWayland.";
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

    // Create one transparent overlay covering all connected screens.
    const QRect screenGeometry = desktopGeometry();
    if (screenGeometry.isNull())
    {
        qCritical() << "Unable to create the pixel overlay: no screen geometry is available.";
        return 1;
    }
    PixelOverlay overlay(screenGeometry, tracker);
    overlay.show();

    // The status notifier item is the app's tray icon and context menu.
    auto *trayItem = new KStatusNotifierItem(&app);
    trayItem->setIconByName(QStringLiteral("weather-clear"));
    trayItem->setStatus(KStatusNotifierItem::Active);
    trayItem->setTitle(QStringLiteral("Desktop Weather"));
    trayItem->setToolTip(
        QStringLiteral("weather-clear"), QStringLiteral("Desktop Weather"), QStringLiteral("Weather app"));
    trayItem->setIsMenu(true);

    QMenu *menu = trayItem->contextMenu();
    QMenu *windowsMenu = menu->addMenu(QStringLiteral("Tracked windows"));
    QAction *waitingAction = windowsMenu->addAction(QStringLiteral("Waiting for KWin script…"));
    waitingAction->setEnabled(false);
    // Refresh the tray's window list whenever the tracker receives new data.
    QObject::connect(&tracker, &WindowTracker::windowsUpdated, &app,
                     [windowsMenu, &overlay](const QJsonArray &windows)
                     {
                         // need to skip plasmashell
                         //  bool all_windowed = true;
                         bool too_big = false;
                         for (const QJsonValue &value : windows)
                         {
                             const QJsonObject window = value.toObject();
                             if (std::find(SKIPPED_WINDOWS.begin(), SKIPPED_WINDOWS.end(), window.value(QStringLiteral("appId")).toString()) == SKIPPED_WINDOWS.end())
                             {
                                 const double screenArea = (window.value(QStringLiteral("screenWidth")).toDouble() * window.value(QStringLiteral("screenHeight")).toDouble());
                                 //  qInfo()<< "screenArea: " << screenArea;
                                 const int windowArea = (window.value(QStringLiteral("width")).toInt() * window.value(QStringLiteral("height")).toInt());
                                 //  qInfo() << "windowArea: " << windowArea;
                                 const bool windowTooBig = (windowArea >= (screenArea * .75) && (window.value(QStringLiteral("screen")).toInt() == 1)); // check if x times y is < 80% of screens x times y (COULD ADD MONITOR x AND Y TO JSON)

                                 //  const bool fullscreen = window.value(QStringLiteral("fullScreen")).toBool();
                                 //  const bool borderlessFullscreen = window.value(QStringLiteral("borderlessFullscreen")).toBool();
                                 //  if (fullscreen || borderlessFullscreen)
                                 //  {
                                 //      all_windowed = false;
                                 //  }
                                 if (windowTooBig)
                                 {
                                     // qInfo() << window.value(QStringLiteral("appId")).toString() << "TOO BIG";
                                     too_big = true;
                                 }
                             }
                         }
                         if (!too_big)
                         {
                             overlay.show();
                         }
                         else
                         {

                             overlay.hide();
                         }

                        //  updateTrackedWindowsMenu(windowsMenu, windows);
                     });

    // Let the user show or hide the animated overlay from the tray menu.
    auto *toggleVisual = new QAction(QStringLiteral("Enabled"), &app);
    menu->addAction(toggleVisual);
    QObject::connect(toggleVisual, &QAction::triggered, &app,
                     [toggleVisual, &overlay]()
                     {
                         const bool shouldShow = toggleVisual->text() != QStringLiteral("Enabled");
                         toggleVisual->setText(shouldShow ? QStringLiteral("Enabled") : QStringLiteral("Disabled"));
                         shouldShow ? overlay.show() : overlay.hide();
                     });
    auto *randomAngle = new QAction(QString::number(overlay.m_drops[0].getDropAngle()), &app);
    menu->addAction(randomAngle);
    QObject::connect(randomAngle, &QAction::triggered, &app,
                     [randomAngle, &overlay]()
                     {
                         std::random_device rd;
                         std::mt19937 gen(rd());
                         std::uniform_int_distribution<> distrib(70, 90);
                         double random_num = distrib(gen);
                         for (RainDrop &drop : overlay.m_drops)
                         {
                             drop.setDropAngle(random_num);
                         }
                         randomAngle->setText(QString::number(random_num));
                     });

    // Run until the user quits the application.
    return app.exec();
}

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
#include <thread>
#include "WindowTracker.h"
#include "raindrop.h"
#include "snow.h"
#include "pixel_overlay.h"
#include "GLOBAL_VARS.h"
#include <fstream>

#include <QLineEdit>
#include <QInputDialog>
#include <QWidgetAction>
#include <QIntValidator> // Required if you are using the validator
// api needs
#include <curl/curl.h>
#include <nlohmann/json.hpp>
using nlohmann::json;

bool DEBUG = false;
std::string debug_json = "{\"location\":{\"name\":\"Fuquay Varina\",\"region\":\"North Carolina\",\"country\":\"USA\",\"lat\":35.5800018310547,\"lon\":-78.7908020019531,\"tz_id\":\"America/New_York\",\"localtime_epoch\":1794362971,\"localtime\":\"2026-11-10 21:09\"},\"current\":{\"last_updated_epoch\":1794362400,\"last_updated\":\"2026-11-10 21:00\",\"temp_c\":13.8,\"temp_f\":56.8,\"is_day\":0,\"condition\":{\"text\":\"Clear\",\"icon\":\"//cdn.weatherapi.com/weather/64x64/night/113.png\",\"code\":1205},\"wind_mph\":4.5,\"wind_kph\":7.2,\"wind_degree\":283,\"wind_dir\":\"WNW\",\"pressure_mb\":1018.0,\"pressure_in\":30.06,\"precip_mm\":0.0,\"precip_in\":0.0,\"humidity\":66,\"cloud\":22,\"feelslike_c\":11.9,\"feelslike_f\":53.4,\"windchill_c\":13.6,\"windchill_f\":56.5,\"heatindex_c\":13.8,\"heatindex_f\":56.8,\"dewpoint_c\":7.6,\"dewpoint_f\":45.6,\"vis_km\":10.0,\"vis_miles\":6.0,\"uv\":0.0,\"gust_mph\":6.3,\"gust_kph\":10.2,\"will_it_rain\":0,\"chance_of_rain\":6,\"will_it_snow\":0,\"chance_of_snow\":0,\"wetbulb_c\":10.5,\"wetbulb_f\":50.8}}";

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

// 1. Callback function to handle incoming data chunks
size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *userp)
{
    size_t totalSize = size * nmemb;
    if (userp)
    {
        userp->append((char *)contents, totalSize);
    }
    return totalSize;
}

// Maps ranges
template <typename T>
T mapRange(T value, T inMin, T inMax, T outMin, T outMax)
{
    return static_cast<T>(
        outMin + (static_cast<double>(value - inMin) / (inMax - inMin)) * (outMax - outMin));
}

// maps the east and west wind angles to usable angles for moving points
double mapEastWestWind(double windDegree)
{
    double rad = windDegree * M_PI / 180.0;
    double ewComponent = std::sin(rad);
    return 90.0 + (ewComponent * 45.0);
}
std::string getWeatherJson(std::string &API_KEY, std::string &zipcode)
{

    // if (DEBUG)
    // {
    //     return debug_json;
    // }
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        qInfo() << "Failed to initialize curl";
        return "";
    }

    std::string url = "https://api.weatherapi.com/v1/current.json?key=+" + API_KEY + "&q=" + zipcode;
    std::string responseString;

    // 2. Set curl options
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // Tell curl to use our callback function
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);

    // Pass the std::string pointer where curl should save the response data
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseString);

    // 3. Perform the request
    CURLcode res = curl_easy_perform(curl);

    // 4. Check for errors and print the result
    if (res != CURLE_OK)
    {
        qInfo() << "curl_easy_perform() failed: " << curl_easy_strerror(res);
    }
    else
    {
        qInfo() << "--- API Response ---";
        qInfo() << responseString;
        qInfo() << "--------------------";

        return responseString;
    }

    // Clean up
    curl_easy_cleanup(curl);

    return "";
}

void setParticleInfo(std::string jsonString, std::vector<std::unique_ptr<PixelOverlay>> &overlays, WindowTracker &tracker, std::string &location, double &particleWind, int &particleAmount)
{

    if (jsonString == "")
    {
        for (const std::unique_ptr<PixelOverlay> &overlay : overlays)
        {
            for (const auto &particle : overlay->m_particles)
            {
                particle->setDropAngle(45);
            }
            overlay->refreshPixels();
        }
        qInfo() << "JSON EMPTY, SETTING TO ANGLE 45" << "\n";
        return;
    }
    json data = json::parse(jsonString, nullptr, false);

    particleAmount = 500;
    std::string particleType = "rain";

    if (data.is_discarded() || !data.is_object())
    {
        qWarning() << "Invalid JSON response.\n";
        return;
    }
    if (data.contains("error") && data["error"].is_object())
    {
        int errCode = data["error"].value("code", -1);
        std::string errMsg = data["error"].value("message", "Unknown API error");
        qWarning() << "Weather API Error [" << errCode << "]: " << errMsg << '\n';
    }
    if (!data.contains("current") || !data["current"].is_object())
    {
        qWarning() << "Missing 'current' weather object.\n";
    }

    const auto &current = data["current"];

    double wind_mph = (current.contains("wind_mph") && current["wind_mph"].is_number())
                          ? current["wind_mph"].get<double>()
                          : 3.0;

    int wind_degree = (current.contains("wind_degree") && current["wind_degree"].is_number_integer())
                          ? current["wind_degree"].get<int>()
                          : 270;

    int condition_code = 1205; // Default clear sky code
    if (current.contains("condition") && current["condition"].is_object())
    {
        const auto &cond = current["condition"];
        if (cond.contains("code") && cond["code"].is_number_integer())
        {
            condition_code = cond["code"].get<int>();
        }
    }

    const auto &locData = data["location"];
    location = (locData.contains("name") && locData["name"].is_string())
                          ? locData["name"].get<std::string>()
                          : "Unknown";



    switch (condition_code)
    {
    case 1180 ... 1183: // light rain
        particleAmount = 250;
        particleType = "rain";
        break;
    case 1186 ... 1189: // medium rain
        particleAmount = 500;
        particleType = "rain";
        break;
    case 1192 ...1195: // heavy rain
        particleAmount = 1000;
        particleType = "rain";
        break;
    case 1213: // light snow
        particleAmount = 250;
        particleType = "snow";
        break;
    case 1215: // medium snow
        particleAmount = 500;
        particleType = "snow";
        break;
    case 1217: // heavy snow
        particleAmount = 1000;
        particleType = "snow";
        break;

    default:
        particleAmount = 0; // sunny / cloudy
        break;
    }

    double wind_dir_2d = mapEastWestWind(wind_degree);                          // map 0 north to 90 north and a 90 +- 45
    double wind_speed_bonus = mapRange(double(wind_mph), 0.0, 60.0, 0.0, 20.0); // map wind speed to alter angle
    double wind_dir_2d_bonus = wind_dir_2d;                                     // if directly down, dont add wind speed

    // add wind speed to either side
    if (wind_dir_2d != 90)
    {
        wind_dir_2d_bonus = (wind_dir_2d < 90) ? wind_dir_2d_bonus -= wind_speed_bonus : wind_dir_2d_bonus += wind_speed_bonus;
    }

    particleWind = wind_dir_2d_bonus;

    for (const std::unique_ptr<PixelOverlay> &overlay : overlays)
    {
        overlay->setParticleAmount(particleAmount, particleType, tracker);
        for (const auto &particle : overlay->m_particles)
        {
            particle->setDropAngle(wind_dir_2d_bonus);
        }
        overlay->refreshPixels();
    }

    qInfo() << "--- Particle Info ---";
    qInfo() << wind_mph << "mph @ " << wind_degree << "degrees";
    qInfo() << particleType;
    qInfo() << particleAmount << "Pcs";
    qInfo() << wind_dir_2d << "degrees";
    qInfo() << wind_dir_2d_bonus << "degrees with bonus";
    qInfo() << "--------------------";

    return;
}
void spawnMenuItems(QApplication &app, std::vector<std::unique_ptr<PixelOverlay>> &overlays, WindowTracker &tracker, std::string &location, std::string &api_key, std::string &zipcode, std::string &currType, double &particleWind, int &particleAmount)
{

    // The status notifier item is the app's tray icon and context menu.
    auto *trayItem = new KStatusNotifierItem(&app);
    trayItem->setIconByName(QStringLiteral("weather-showers"));
    trayItem->setStatus(KStatusNotifierItem::Active);
    trayItem->setTitle(QStringLiteral("Desktop Weather"));
    trayItem->setToolTip(
        QStringLiteral("weather-showers"), QStringLiteral("Desktop Weather"), QStringLiteral("Weather app"));
    trayItem->setIsMenu(true);

    QMenu *menu = trayItem->contextMenu();

    auto *locationMessageAction = new QAction(QString::fromStdString(location), menu);
    locationMessageAction->setEnabled(true);
    menu->addAction(locationMessageAction);

    auto *apiMessageAction = new QAction(QStringLiteral("Enter WeatherAPI key below"), menu);
    apiMessageAction->setEnabled(false);
    menu->addAction(apiMessageAction);

    auto *apiInputAction = new QAction(QStringLiteral("Enter Key"), menu);
    menu->addAction(apiInputAction);
    QObject::connect(apiInputAction, &QAction::triggered, &app,
                     [apiInputAction, &api_key, &overlays, &tracker, &zipcode, &location,locationMessageAction]()
                     {
                         bool accepted = false;
                         const QString text = QInputDialog::getText(
                             nullptr,
                             QStringLiteral("Desktop Weather"),
                             QStringLiteral("Type something:"),
                             QLineEdit::Normal,
                             apiInputAction->data().toString(),
                             &accepted);
                         if (accepted)
                         {
                             apiInputAction->setData(text);
                             apiInputAction->setText(text.isEmpty()
                                                         ? QStringLiteral("Enter Key")
                                                         : QStringLiteral("Key Set!"));
                             api_key = (text).toStdString();
                             qInfo() << api_key;
                             std::string jsonString = getWeatherJson(api_key, zipcode);
                             qInfo() << jsonString;
                             double particleWind;
                             int particleAmount;
                             setParticleInfo(jsonString, overlays, tracker, location, particleWind, particleAmount);
                             locationMessageAction->setText(QString::fromStdString(location));
                         }
                     });

    auto *zipMessageAction = new QAction(QStringLiteral("Enter zipcode below"), menu);
    zipMessageAction->setEnabled(false);
    menu->addAction(zipMessageAction);

    auto *zipcodeInputAction = new QAction(QStringLiteral("Enter Zip"), menu);
    menu->addAction(zipcodeInputAction);
    QObject::connect(zipcodeInputAction, &QAction::triggered, &app,
                     [zipcodeInputAction, &api_key, &overlays, &tracker, &zipcode, &location, locationMessageAction]()
                     {
                         bool accepted = false;
                         const QString text = QInputDialog::getText(
                             nullptr,
                             QStringLiteral("Desktop Weather"),
                             QStringLiteral("Type something:"),
                             QLineEdit::Normal,
                             zipcodeInputAction->data().toString(),
                             &accepted);
                         if (accepted)
                         {
                             zipcodeInputAction->setData(text);
                             zipcodeInputAction->setText(text.isEmpty()
                                                             ? QStringLiteral("Enter Zip")
                                                             : QStringLiteral("Zipcode Set!"));
                             zipcode = (text).toStdString();
                             qInfo() << zipcode;
                             std::string jsonString = getWeatherJson(api_key, zipcode);
                             qInfo() << jsonString;
                             double particleWind;
                             int particleAmount;
                             setParticleInfo(jsonString, overlays, tracker, location,particleWind, particleAmount);
                             locationMessageAction->setText(QString::fromStdString(location));
                         }
                     });

    if (DEBUG)
    {
        auto *randomAngle = new QAction(
            QString::number(particleWind), &app);
        menu->addAction(randomAngle);
        QObject::connect(randomAngle, &QAction::triggered, &app,
                         [randomAngle, &overlays]()
                         {
                             std::random_device rd;
                             std::mt19937 gen(rd());
                             std::uniform_int_distribution<> distrib(90 - 45, 90 + 45);
                             double random_num = distrib(gen);
                             for (const std::unique_ptr<PixelOverlay> &overlay : overlays)
                             {
                                 for (const auto &particle : overlay->m_particles)
                                 {
                                     particle->setDropAngle(random_num);
                                 }
                                 overlay->refreshPixels();
                             }
                             randomAngle->setText(QString::number(random_num));
                         });
        auto *randomParticleAmount = new QAction(
            QString::number(particleAmount), &app);
        menu->addAction(randomParticleAmount);
        QObject::connect(randomParticleAmount, &QAction::triggered, &app,
                         [randomParticleAmount, &overlays, &currType, &tracker, &particleWind]()
                         {
                             std::random_device rd;
                             std::mt19937 gen(rd());
                             std::uniform_int_distribution<> distrib(50, 1000);
                             double random_num = distrib(gen);
                             for (const std::unique_ptr<PixelOverlay> &overlay : overlays)
                             {
                                 overlay->setParticleAmount(random_num, currType, tracker);
                                 for (const auto &particle : overlay->m_particles)
                                 {
                                     particle->setDropAngle(particleWind);
                                 }
                                 overlay->refreshPixels();
                             }
                             randomParticleAmount->setText(QString::number(random_num));
                         });

        auto *weatherAction = new QAction(QStringLiteral("Rain"), &app);
        weatherAction->setCheckable(true);
        menu->addAction(weatherAction);

        QObject::connect(weatherAction, &QAction::toggled, &app,
                         [weatherAction, &overlays, &tracker, &currType, &particleWind](bool checked)
                         {
                             weatherAction->setText(
                                 checked ? QStringLiteral("Snow") : QStringLiteral("Rain"));

                             for (const std::unique_ptr<PixelOverlay> &overlay : overlays)
                             {
                                 overlay->changeParticleType(checked ? "snow" : "rain", tracker);
                                 currType = checked ? "snow" : "rain";

                                 for (const auto &particle : overlay->m_particles)
                                 {
                                     particle->setDropAngle(particleWind);
                                 }
                                 overlay->refreshPixels();
                             }
                         });
    }
}

void weatherPolling(QApplication &app, std::vector<std::unique_ptr<PixelOverlay>> &overlays, WindowTracker &tracker, std::string &currType, std::string &api_key, std::string &zipcode, std::string &location)
{

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(15 * 60));

        std::string jsonString = getWeatherJson(api_key, zipcode);
        double particleWind;
        int particleAmount;
        setParticleInfo(jsonString, overlays, tracker, location, particleWind, particleAmount);
    }
}

void load_dotenv(const std::string &filename = ".env")
{
    std::ifstream file(filename);
    std::string line;
    while (std::getline(file, line))
    {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#')
            continue;

        auto pos = line.find('=');
        if (pos == std::string::npos)
            continue;

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

#ifdef _WIN32
        _putenv_s(key.c_str(), value.c_str());
#else
        setenv(key.c_str(), value.c_str(), 1);
#endif
    }
}

int main(int argc, char *argv[])
{
    std::string api_key = "";
    std::string zipcode = "";
    std::string location = "Unknown";
    if (DEBUG)
    {
        qInfo() << "========= DEBUG MODE ENABLED ===========";
        load_dotenv("/home/shmank/Documents/code/desktopWeather/src/.env");
        const char *env_p = std::getenv("API_KEY");
        api_key = env_p ? env_p : "default_value";
    }

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
        qInfo() << "Overlay created on" << screen->name() << screen->geometry();
        overlays.push_back(std::move(overlay));
    }

    std::string currType = "rain";

    // try api (has fallback values)
    std::string jsonString = getWeatherJson(api_key, zipcode);
    double particleWind;
    int particleAmount;
    setParticleInfo(jsonString, overlays, tracker,location,  particleWind, particleAmount);

    // set api key + debug options
    spawnMenuItems(app, overlays, tracker,location, api_key, zipcode, currType, particleWind, particleAmount);

    // pulls weather json data every 15 minutes
    std::thread weatherPollingThread(weatherPolling, std::ref(app), std::ref(overlays), std::ref(tracker), std::ref(currType), std::ref(api_key), std::ref(zipcode),std::ref(location));
    weatherPollingThread.detach();

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

    qInfo() << "APP RUNNING";
    qInfo() << api_key;

    return app.exec();
}

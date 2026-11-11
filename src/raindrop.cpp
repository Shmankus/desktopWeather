
#include "raindrop.h"
#include "GLOBAL_VARS.h"

// Move the drop; when it leaves the screen, start it at a fresh edge.
void RainDrop::advance()
{

    auto currentTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> deltaDuration = currentTime - lastTime;
    lastTime = currentTime;
    float deltaTime = deltaDuration.count(); // Delta time in seconds

    m_Yvel += m_grav;
    const qreal angleRadians = qDegreesToRadians(m_angle);
    m_position += QPointF(m_Xvel * std::cos(angleRadians) * deltaTime, m_Yvel * std::sin(angleRadians) * deltaTime);

    const QRectF screenBounds(QPointF(0, 0), QSizeF(m_areaSize));
    if (!screenBounds.intersects(bounds()))
    {
        m_Yvel = m_Xvel;
        startAtRandomEdge();
    }

    for (const QJsonValue &value : m_tracker.windows())
    {
        if (!value.isObject())
        {
            continue;
        }

        const QJsonObject window = value.toObject();

        // Windows on another screen cannot be rained on, and must not reset the
        // gravity of a drop on this one. The KWin script reports x and y
        // relative to the window's own screen, which matches this overlay's
        // own coordinates because its layer surface covers exactly one output.
        if (window.value(QStringLiteral("screenName")).toString() != m_screenName)
        {
            continue;
        }

        const QString title = window.value(QStringLiteral("appId")).toString();
        if (std::find(SKIPPED_WINDOWS.begin(), SKIPPED_WINDOWS.end(), title) != SKIPPED_WINDOWS.end())
        {
            continue;
        }

        const int x = window.value(QStringLiteral("x")).toInt();
        const int y = window.value(QStringLiteral("y")).toInt();
        const int width = window.value(QStringLiteral("width")).toInt();
        const int height = window.value(QStringLiteral("height")).toInt();

        if (m_position.x() > x && m_position.x() < x + width && m_position.y() > y && m_position.y() < y + height)
        {

            float distToTop = m_position.y() - y;
            float distToBottom = (y + height) - m_position.y();
            float distToLeft = m_position.x() - x;
            float distToRight = (x + width) - m_position.x();

            float minDist = std::min({distToTop, distToBottom, distToLeft, distToRight});

            if (minDist == distToTop)
            {
                // Hit the Top surface
                m_position.setY(y);
                m_Yvel = 0;
                m_Xvel = m_default_m_Xvel * .75;
                m_grav = 0;
            }
            else if (minDist == distToBottom)
            {
                continue; // Bottom edge
            }
            else if (minDist == distToLeft)
            {
                // left edge
                m_position.setX(x+1);
                m_Yvel = m_grav * .85;
                m_Xvel = 0;
                
            }
            else if (minDist == distToRight)
            {
                // Right edge
                m_position.setX(x+width-1);
                m_Yvel = m_grav * .85;
                m_Xvel = 0;
            }
        }
        else
        {
            m_grav = m_default_grav;
            m_freeFall_Yvel = m_Yvel;
        }
    }

    // detect if water is stuck, either trickle down app or evaporate?
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - spawnTime;
    if (elapsed.count() > lifeSpan)
    {
        startAtRandomEdge();
    }
}

RainDrop::RainDrop(const QSize &areaSize, const WindowTracker &tracker, const QString &screenName)
    : Particle(areaSize, tracker, screenName)
{
    particleColor = QColor(0, 0, 255, 127);


    std::random_device rd; 
    std::mt19937 gen(rd()); 
    std::uniform_int_distribution<> distr(25000, 35000); 
    lifeSpan = distr(gen);

    startAtRandomEdge();
}


#include "raindrop.h"
#include "GLOBAL_VARS.h"
#include <algorithm>
#include <cmath>
// One raindrop stores its own position and movement settings.

void RainDrop::startAtRandomEdge()
{
    QRandomGenerator *random = QRandomGenerator::global();
    const int rand_edge = random->bounded(5); // only 1 in 6 get spawned on side of screen
    const int maxX = qMax(1, m_areaSize.width() - static_cast<int>(dropSize) + 1);
    const int maxY = qMax(1, m_areaSize.height() - static_cast<int>(dropSize) + 1);

    if (m_angle == 90 || m_Xvel == 0)

    {
        m_position = QPointF(random->bounded(maxX), 0);
    }
    else
    {
        if (rand_edge != 0)
        {
            m_position = QPointF(random->bounded(maxX), 0);
        }
        else
        {
            m_position = QPointF(0, random->bounded(maxY));
        }
    }
 
    m_Xvel = m_default_m_Xvel;
    m_Yvel = m_default_m_Yvel;
    lastTime = std::chrono::high_resolution_clock::now();
}
// sets new angle of attack for the droplets

void RainDrop::setDropAngle(double decAngle)
{

    m_angle = decAngle;
}

qreal RainDrop::getDropAngle()
{

    return m_angle;
}

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
        const QString title = window.value(QStringLiteral("appId")).toString();
        const int x = window.value(QStringLiteral("x")).toInt();
        const int y = window.value(QStringLiteral("y")).toInt();
        const int width = window.value(QStringLiteral("width")).toInt();
        const int height = window.value(QStringLiteral("height")).toInt();
        const bool fullscreen = window.value(QStringLiteral("fullScreen")).toBool();
        const bool screenIndex = window.value(QStringLiteral("screen")).toInt();

        if (m_position.x() > x && m_position.x() < x + width && m_position.y() > y && m_position.y() < y + height && screenIndex == 1 && std::find(SKIPPED_WINDOWS.begin(), SKIPPED_WINDOWS.end(), title) == SKIPPED_WINDOWS.end())
        {

            // fill in impact logic instead
            // qInfo() << "Stopped by: " << title;

            // bool hitTop = (m_position.y() >= y && m_position.y() <= y + height);

            // if (hitTop)
            // {

            //     m_position.setY(y);
            //     m_Yvel = 0;
            //     m_grav = 0;

            // }

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
                m_grav = 0;
            }
            else if (minDist == distToBottom)
            {
                continue; // Bottom edge
            }
            else if (minDist == distToLeft)
            {
                // left edge
                m_Yvel = m_freeFall_Yvel * .90;
                m_Xvel = 0;
            }
            else if (minDist == distToRight)
            {
                // Right edge
                m_Yvel = m_freeFall_Yvel * .90;
                m_Xvel = 0;
            }

            // if (hitSide)
            // {
            //     m_Yvel = 0; // only grav acts on it and it looks like a good sliding
            // }

            // m_Yvel = m_Xvel;
            // startAtRandomEdge();
        }
        else
        {

            m_grav = m_default_grav;
            m_freeFall_Yvel = m_Yvel;
        }
    }

    // detect if water is stuck, either trickle down app or evaporate?
}

// The overlay uses this rectangle both to draw and erase the drop.
QRectF RainDrop::bounds() const
{
    return QRectF(m_position, QSizeF(dropSize, dropSize));
}

// Paint this drop into the overlay's image.
void RainDrop::draw(QPainter &painter) const
{
    painter.fillRect(bounds(), QColor(0, 0, 255, 127));
}

RainDrop::RainDrop(const QSize &areaSize, const WindowTracker &tracker)
    : m_areaSize(areaSize),
      m_tracker(tracker)
{
    QRandomGenerator *random = QRandomGenerator::global();

    m_angle = 60;
    startAtRandomEdge();
}

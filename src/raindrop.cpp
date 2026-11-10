
#include "raindrop.h"
#include "GLOBAL_VARS.h"




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
    spawnTime = std::chrono::high_resolution_clock::now();
}
// The compositor can resize the overlay under us; keep drops inside the area.
void RainDrop::setAreaSize(const QSize &areaSize)
{
    m_areaSize = areaSize;

    const QRectF screenBounds(QPointF(0, 0), QSizeF(m_areaSize));
    if (!screenBounds.intersects(bounds()))
    {
        startAtRandomEdge();
    }
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
                m_Yvel = m_freeFall_Yvel * .92;
                m_Xvel = 0;
            }
            else if (minDist == distToRight)
            {
                // Right edge
                m_Yvel = m_freeFall_Yvel * .92;
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
    if(elapsed.count() > 30000){
        startAtRandomEdge();
    }
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

RainDrop::RainDrop(const QSize &areaSize, const WindowTracker &tracker, const QString &screenName)
    : m_areaSize(areaSize),
      m_tracker(tracker),
      m_screenName(screenName)
{
    m_angle = 60;
    startAtRandomEdge();
}

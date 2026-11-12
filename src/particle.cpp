
#include "raindrop.h"
#include "GLOBAL_VARS.h"

void Particle::startAtRandomEdge()
{
    QRandomGenerator *random = QRandomGenerator::global();
    const int rand_edge = random->bounded(5);
    const int maxX = qMax(1, m_areaSize.width() - static_cast<int>(dropSize) + 1);
    const int maxY = qMax(1, m_areaSize.height() - static_cast<int>(dropSize) + 1);

    if (rand_edge == 0 && !qFuzzyIsNull(qCos(qDegreesToRadians(m_angle))))
    {
        const qreal horizontalDirection = qCos(qDegreesToRadians(m_angle));
        const qreal x = horizontalDirection < 0 ? maxX - 1 : 0;
        m_position = QPointF(x, random->bounded(maxY));
    }
    else
    {
        m_position = QPointF(random->bounded(maxX), 0);
    }

    m_Xvel = m_default_m_Xvel;
    m_Yvel = m_default_m_Yvel;
    lastTime = std::chrono::high_resolution_clock::now();
    spawnTime = std::chrono::high_resolution_clock::now();
}
// The compositor can resize the overlay under us; keep particles inside the area.
void Particle::setAreaSize(const QSize &areaSize)
{
    m_areaSize = areaSize;

    const QRectF screenBounds(QPointF(0, 0), QSizeF(m_areaSize));
    if (!screenBounds.intersects(bounds()))
    {
        startAtRandomEdge();
    }
}

// sets new angle of attack for the droplets

void Particle::setDropAngle(double decAngle)
{

    m_angle = decAngle;
}

qreal Particle::getDropAngle()
{

    return m_angle;
}

// The overlay uses this rectangle both to draw and erase the drop.
QRectF Particle::bounds() const
{
    return QRectF(m_position, QSizeF(dropSize, dropSize));
}

// Paint this drop into the overlay's image.
void Particle::draw(QPainter &painter) const
{
    painter.fillRect(bounds(), particleColor);
}

Particle::Particle(const QSize &areaSize, const WindowTracker &tracker, const QString &screenName)
    : m_areaSize(areaSize),
      m_tracker(tracker),
      m_screenName(screenName)
{

    startAtRandomEdge();
}

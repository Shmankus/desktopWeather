#include "pixel_overlay.h"

#include <QResizeEvent>
#include <QWindow>
#include <LayerShellQt/Window>

// A single transparent window holds the pixel image and all the raindrops.
// It passes mouse input through so the user's other windows remain clickable.

// Qt calls this whenever the overlay needs to appear on screen again.
void PixelOverlay::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.drawImage(0, 0, m_pixels);
}

PixelOverlay::PixelOverlay(QScreen *screen, const WindowTracker &tracker)
    : m_pixels(screen->geometry().size(), QImage::Format_ARGB32_Premultiplied),
      m_screen(screen),
      m_screenName(screen->name())
{

    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                   Qt::WindowDoesNotAcceptFocus | Qt::WindowTransparentForInput);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setScreen(m_screen);
    setGeometry(m_screen->geometry());
    m_pixels.fill(Qt::transparent);

    // Create the drops once; each one chooses its own starting position.
    for (int i = 0; i < dropCount; ++i)
    {
        m_particles.push_back(std::make_unique<Snow>(m_pixels.size(), tracker, m_screenName));
    }

    // A timer advances and redraws every drop about 60 times per second.
    QObject::connect(&m_timer, &QTimer::timeout, this, [this]()
                     {
            QPainter painter(&m_pixels);

            // Erase the previous drop positions from the transparent image.
            painter.setCompositionMode(QPainter::CompositionMode_Clear);
            for (auto &particlePtr : m_particles)
            {
                Particle &particle = *particlePtr;

                painter.fillRect(particle.bounds(), Qt::transparent);
            }

            // Move each drop and draw it at its new position.
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            for (auto &particlePtr : m_particles)
            {
                Particle &particle = *particlePtr;
                particle.advance();
                particle.draw(painter);
            }
            update(); });
}

// Reconfigure this window as a layer-shell surface. Must run before show().
void PixelOverlay::configureAsLayerSurface()
{
    if (QGuiApplication::platformName() != QStringLiteral("wayland"))
    {
        // X11 and XWayland have no layer-shell; keep the old top-level path.
        return;
    }

    // The QWindow has to exist before it can be configured, and every property
    // below must be set before show() commits the surface.
    create();
    QWindow *handle = windowHandle();
    if (!handle)
    {
        qWarning() << "No window handle available; the overlay stays an ordinary window.";
        return;
    }

    using LayerWindow = LayerShellQt::Window;

    LayerWindow *layer = LayerWindow::get(handle);
    if (!layer)
    {
        qWarning() << "layer-shell is unavailable; the overlay stays an ordinary window.";
        return;
    }

    // LayerTop sits where panels sit: above ordinary windows, so drops stay
    // visible on title bars, but still underneath fullscreen windows.
    layer->setLayer(LayerWindow::LayerTop);

    // Anchoring to all four edges makes the compositor size the surface to
    // fill the whole output. The first flag is widened to Anchors explicitly
    // because LayerShellQt does not declare operator| for the enum.
    layer->setAnchors(LayerWindow::Anchors(LayerWindow::AnchorTop) | LayerWindow::AnchorBottom |
                      LayerWindow::AnchorLeft | LayerWindow::AnchorRight);

    // Reserve no space for ourselves, and ignore the space panels reserve, so
    // the rain covers the output edge to edge.
    layer->setExclusiveZone(-1);
    layer->setKeyboardInteractivity(LayerWindow::KeyboardInteractivityNone);

    // A desktop decoration must never ask to be activated on show. This is
    // already implied by KeyboardInteractivityNone, but stays correct if the
    // interactivity above is ever changed.
    layer->setActivateOnShow(false);

    layer->setScope(QStringLiteral("desktop-weather"));

    // Pin the surface to this overlay's own output. Qt's primaryScreen() is
    // not KDE's primary display on Wayland, so the screen is always explicit;
    // setScreenConfiguration() is deprecated since LayerShellQt 6.6.
    layer->setScreen(m_screen);
}

void PixelOverlay::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    if (m_pixels.size() == size())
    {
        return;
    }

    m_pixels = QImage(size(), QImage::Format_ARGB32_Premultiplied);
    m_pixels.fill(Qt::transparent);
    for (const std::unique_ptr<Particle> &particle : m_particles)
    {
        particle->setAreaSize(size());
    }
}

QString PixelOverlay::screenName() const
{
    return m_screenName;
}

// starts the render loop
void PixelOverlay::startOverlay()
{
    m_timer.start(16);
}
// stops the render loop
void PixelOverlay::stopOverlay()
{
    m_timer.stop();
}

bool PixelOverlay::isTimerActive()
{

    return m_timer.isActive();
}

void PixelOverlay::changeParticleType(std::string type, const WindowTracker &tracker)
{
    m_timer.stop();

    // The old particles are about to be destroyed without erasing themselves,
    // so wipe the buffer or their last frame would stay painted on screen.
    m_pixels.fill(Qt::transparent);

    m_particles.clear();

    for (int i = 0; i < dropCount; ++i)
    {
        if (type == "snow")
        {
            m_particles.push_back(std::make_unique<Snow>(m_pixels.size(), tracker, m_screenName));
        }
        else
        {
            m_particles.push_back(std::make_unique<RainDrop>(m_pixels.size(), tracker, m_screenName));
        }
    }

    m_timer.start(16);
}

int PixelOverlay::getParticleAmount()
{
    return dropCount;
}
void PixelOverlay::setParticleAmount(int num)
{
    dropCount = num;
}

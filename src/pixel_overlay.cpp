#include "pixel_overlay.h"

// A single transparent window holds the pixel image and all the raindrops.
// It passes mouse input through so the user's other windows remain clickable.

    // Qt calls this whenever the overlay needs to appear on screen again.
    void PixelOverlay::paintEvent(QPaintEvent *)
    {
        QPainter painter(this);
        painter.drawImage(0, 0, m_pixels);
    }


    std::vector<RainDrop> m_drops;

    PixelOverlay::PixelOverlay(const QRect &desktopGeometry, const WindowTracker &tracker)
        : m_pixels(desktopGeometry.size(), QImage::Format_ARGB32_Premultiplied)
    {
        setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                       Qt::WindowDoesNotAcceptFocus | Qt::WindowTransparentForInput);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setGeometry(desktopGeometry);
        m_pixels.fill(Qt::transparent);

        // Create the drops once; each one chooses its own starting position.
        constexpr int dropCount = 400;
        for (int i = 0; i < dropCount; ++i)
        {
            m_drops.emplace_back(m_pixels.size(), tracker);
        }

        // A timer advances and redraws every drop about 60 times per second.
        QObject::connect(&m_timer, &QTimer::timeout, this, [this]()
                         {
            QPainter painter(&m_pixels);

            // Erase the previous drop positions from the transparent image.
            painter.setCompositionMode(QPainter::CompositionMode_Clear);
            for (const RainDrop &drop : m_drops)
            {
                painter.fillRect(drop.bounds(), Qt::transparent);
            }

            // Move each drop and draw it at its new position.
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            for (RainDrop &drop : m_drops)
            {
                drop.advance();
                drop.draw(painter);
            }
            update(); });
        m_timer.start(16);
    }

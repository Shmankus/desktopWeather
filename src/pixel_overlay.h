#pragma once
#include "WindowTracker.h"
#include "raindrop.h"
#include "snow.h"
#include "particle.h"
#include <memory>
#include <string>
#include <vector>

// A single transparent window holds the pixel image and all the raindrops.
// It passes mouse input through so the user's other windows remain clickable.
class PixelOverlay : public QWidget
{
    QImage m_pixels;
    QTimer m_timer;

    // A layer surface belongs to exactly one output, so each overlay owns the
    // screen it covers.
    QScreen *m_screen;
    QString m_screenName;

    int dropCount = 1000;

protected:
    // Qt calls this whenever the overlay needs to appear on screen again.
    void paintEvent(QPaintEvent *) override;

    // A layer surface is sized by the compositor, not by us, so the pixel
    // buffer and the drops have to follow whatever size we are handed.
    void resizeEvent(QResizeEvent *event) override;

public:
    std::vector<std::unique_ptr<Particle>> m_particles;
    explicit PixelOverlay(QScreen *screen, const WindowTracker &tracker);

    // The name of the output this overlay covers, e.g. "DP-3".
    QString screenName() const;

    // Turn this window into a wlr-layer-shell surface. No-op off Wayland.
    void configureAsLayerSurface();

    void startOverlay();
    void stopOverlay();

    bool isTimerActive();

    void changeParticleType(std::string type, const WindowTracker &tracker);
    int getParticleAmount();
    void setParticleAmount(int num);
};

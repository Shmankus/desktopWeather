#pragma once

#include <QJsonArray>
#include <QObject>

class WindowTracker : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.desktopweather.WindowTracker")

public:
    using QObject::QObject;

    const QJsonArray &windows() const
    {
        return m_windows;
    }

public Q_SLOTS:
    void updateWindows(const QString &payload);

Q_SIGNALS:
    void windowsUpdated(const QJsonArray &windows);

private:
    QByteArray m_lastSnapshot;
    QJsonArray m_windows;
};

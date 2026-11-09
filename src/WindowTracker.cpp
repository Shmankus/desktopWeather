#include "WindowTracker.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

void WindowTracker::updateWindows(const QString &payload)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(payload.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isArray()) {
        qWarning() << "Ignoring invalid window snapshot from KWin:" << error.errorString();
        return;
    }

    const QJsonArray windows = document.array();
    for (const QJsonValue &value : windows) {
        if (!value.isObject()) {
            qWarning() << "Ignoring invalid window snapshot from KWin: expected an array of objects.";
            return;
        }
    }

    const QByteArray snapshot = document.toJson(QJsonDocument::Compact);
    if (snapshot == m_lastSnapshot) {
        return;
    }

    m_lastSnapshot = snapshot;
    m_windows = windows;
    Q_EMIT windowsUpdated(windows);
}

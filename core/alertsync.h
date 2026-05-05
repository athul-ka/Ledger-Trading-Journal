#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>

#include "pricealert.h"

// ── AlertSync ─────────────────────────────────────────────────────────────────
// Pushes the full local alert list to the Raspberry Pi server.
// Uses a simple bulk-replace endpoint so no per-ID tracking is needed.
//
// Pi server URL is stored in QSettings key "piServerUrl".
// Leave empty to skip syncing (app-only mode).

namespace AlertSync {

inline QString piBaseUrl()
{
    QSettings s("Ledger", "Ledger");
    return s.value("piServerUrl").toString().trimmed().remove(QRegularExpression("/$"));
}

// Push the complete alert list to the Pi (replaces Pi's list atomically).
// Triggered whenever the user adds or deletes an alert.
inline void syncAll(QNetworkAccessManager &nam, const QList<PriceAlert> &alerts)
{
    const QString base = piBaseUrl();
    if (base.isEmpty()) return;

    QJsonArray arr;
    for (const PriceAlert &a : alerts) {
        QJsonObject obj;
        obj["pair"]         = a.pair;
        obj["target_price"] = a.targetPrice;
        obj["near_pips"]    = a.nearPips;
        obj["alert_type"]   = a.alertType;
        obj["notes"]        = a.notes;
        arr.append(obj);
    }

    QNetworkRequest req(QUrl(base + "/alerts/sync"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setHeader(QNetworkRequest::UserAgentHeader, "Ledger-App");
    auto *reply = nam.post(req, QJsonDocument(arr).toJson(QJsonDocument::Compact));
    QObject::connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}

// Fetch Pi health status (returns true on success via callback).
inline void checkHealth(QNetworkAccessManager &nam,
                        std::function<void(bool, const QString &)> callback)
{
    const QString base = piBaseUrl();
    if (base.isEmpty()) {
        callback(false, "Pi URL not configured");
        return;
    }
    QNetworkRequest req(QUrl(base + "/health"));
    req.setHeader(QNetworkRequest::UserAgentHeader, "Ledger-App");
    req.setTransferTimeout(5000);
    auto *reply = nam.get(req);
    QObject::connect(reply, &QNetworkReply::finished, [reply, callback]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError)
            callback(true, "Connected");
        else
            callback(false, reply->errorString());
    });
}

} // namespace AlertSync

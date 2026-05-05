#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QSettings>
#include <QTimer>
#include <QtMath>

#include "lotcalculator.h"
#include "pricealert.h"

// ── Twelve Data symbol mapping ────────────────────────────────────────────────

inline QString toTwelveDataSymbol(const QString &pair)
{
    const QString p = pair.toUpper().trimmed();
    static const QMap<QString, QString> overrides = {
        {"NAS100",   "IXIC"},  {"US100",  "IXIC"}, {"NASDAQ100", "IXIC"},
        {"US30",     "DJI"},   {"DJ30",   "DJI"},
        {"SP500",    "SPX"},   {"US500",  "SPX"},
        {"BTCUSD",   "BTC/USD"}, {"ETHUSD", "ETH/USD"},
        {"XAUUSD",   "XAU/USD"}, {"XAGUSD", "XAG/USD"},
    };
    if (overrides.contains(p)) return overrides[p];
    if (p.length() == 6)       return p.left(3) + "/" + p.mid(3);
    return p;
}

// ── PriceFetcher ─────────────────────────────────────────────────────────────
// Polls Twelve Data every 60 s for all distinct pairs that have active alerts.
// Emits priceUpdated() for live display and alertTriggered() for desktop toast.

class PriceFetcher : public QObject
{
    Q_OBJECT

public:
    explicit PriceFetcher(QObject *parent = nullptr) : QObject(parent)
    {
        connect(&m_timer, &QTimer::timeout, this, &PriceFetcher::poll);
        m_timer.setInterval(60'000); // 60 s — well within free-tier 800 req/day
    }

    void start() { poll(); m_timer.start(); }
    void stop()  { m_timer.stop(); }

    // Force a price check immediately (e.g. after adding a new alert)
    void refresh() { poll(); }

    // Reload alert list from caller (called after any DB change)
    void setAlerts(const QList<PriceAlert> &alerts) { m_alerts = alerts; }

signals:
    void priceUpdated(const QString &pair, double price);
    void alertTriggered(const PriceAlert &alert, double price, bool isTouch);

public slots:
    void poll()
    {
        QSettings s("Ledger", "Ledger");
        const QString apiKey = s.value("twelveDataKey").toString().trimmed();
        if (apiKey.isEmpty()) return;

        QStringList symbols;
        for (const PriceAlert &a : std::as_const(m_alerts)) {
            if (!a.active) continue;
            const QString sym = toTwelveDataSymbol(a.pair);
            if (!symbols.contains(sym)) symbols.append(sym);
        }
        if (symbols.isEmpty()) return;

        // One HTTP request regardless of symbol count (1 API credit/call on free plan)
        const QUrl url(QString("https://api.twelvedata.com/price?symbol=%1&apikey=%2")
                           .arg(symbols.join(','), apiKey));
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::UserAgentHeader, "Ledger-App");

        auto *reply = m_nam.get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) return;
            parseResponse(QJsonDocument::fromJson(reply->readAll()).object());
        });
    }

private:
    void parseResponse(const QJsonObject &root)
    {
        // Single-symbol response: {"price": "1.17069"}
        // Multi-symbol response:  {"EUR/USD": {"price": "1.17069"}, ...}
        for (const PriceAlert &a : std::as_const(m_alerts)) {
            if (!a.active) continue;
            const QString sym = toTwelveDataSymbol(a.pair);

            double price = 0.0;
            if (root.contains(sym) && root[sym].isObject())
                price = root[sym].toObject()["price"].toString().toDouble();
            else if (root.contains("price"))
                price = root["price"].toString().toDouble();

            if (price <= 0.0) continue;
            emit priceUpdated(a.pair, price);

            const double pipSize     = LotCalculator::getPipInfo(a.pair).pipSize;
            const double distPips    = qAbs(price - a.targetPrice) / pipSize;
            const bool   isTouching  = distPips <= 1.0;
            const bool   isNearZone  = distPips <= a.nearPips;

            if (isTouching && !a.touchTriggered &&
                (a.alertType == "TOUCH" || a.alertType == "BOTH"))
            {
                emit alertTriggered(a, price, true);
            }
            if (isNearZone && !isTouching && !a.nearTriggered &&
                (a.alertType == "NEAR" || a.alertType == "BOTH"))
            {
                emit alertTriggered(a, price, false);
            }
        }
    }

    QNetworkAccessManager m_nam;
    QTimer                m_timer;
    QList<PriceAlert>     m_alerts;
};

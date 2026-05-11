#pragma once

#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QSettings>
#include <QTimer>
#include <QtMath>

#include "lotcalculator.h"
#include "pricealert.h"

// ── PriceFetcher ─────────────────────────────────────────────────────────────
// Polls a local MT5 EA JSONL tick file for active alert symbols.
// Emits priceUpdated() for live display and alertTriggered() for desktop toast.

class PriceFetcher : public QObject
{
    Q_OBJECT

public:
    explicit PriceFetcher(QObject *parent = nullptr) : QObject(parent)
    {
        connect(&m_timer, &QTimer::timeout, this, &PriceFetcher::poll);
        m_timer.setInterval(1'000);
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
    void feedHealthChanged(const QString &status, bool isStale);

public slots:
    void poll()
    {
        QSettings s("Ledger", "Ledger");
        const QString tickPath = s.value("mt5TickFilePath").toString().trimmed();
        if (tickPath.isEmpty()) {
            setFeedHealth("Feed: tick path not set", false);
            return;
        }

        const int configuredMs = s.value("mt5PollIntervalMs", 1000).toInt();
        const int pollIntervalMs = qBound(200, configuredMs, 5000);
        const int staleThresholdSec = qBound(5, s.value("mt5StaleThresholdSec", 15).toInt(), 300);
        if (m_timer.interval() != pollIntervalMs)
            m_timer.setInterval(pollIntervalMs);

        if (m_lastTickPath != tickPath) {
            m_lastTickPath = tickPath;
            m_lastOffset = 0;
        }

        QFile file(tickPath);
        if (!file.exists()) {
            setFeedHealth("Feed: waiting for tick file", false);
            return;
        }
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            setFeedHealth("Feed: cannot open tick file", false);
            return;
        }

        const QFileInfo info(file);
        if (m_lastOffset > info.size())
            m_lastOffset = 0;

        if (!file.seek(m_lastOffset))
            m_lastOffset = 0;

        QMap<QString, double> latestBySymbol;
        while (file.canReadLine()) {
            const QByteArray line = file.readLine().trimmed();
            m_lastOffset = file.pos();

            if (line.isEmpty())
                continue;

            const auto parsed = parseTickLine(line);
            if (parsed.first.isEmpty() || parsed.second <= 0.0)
                continue;

            latestBySymbol[parsed.first] = parsed.second;
        }

        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        if (!latestBySymbol.isEmpty())
            m_lastTickMs = nowMs;

        if (m_lastTickMs <= 0) {
            setFeedHealth("Feed: connected, waiting for first tick", false);
        } else {
            const qint64 ageMs = nowMs - m_lastTickMs;
            if (ageMs > (static_cast<qint64>(staleThresholdSec) * 1000)) {
                setFeedHealth(QString("Feed: stale (%1s old)").arg(ageMs / 1000), true);
            } else {
                setFeedHealth(QString("Feed: connected (%1s lag)").arg(ageMs / 1000), false);
            }
        }

        for (const PriceAlert &a : std::as_const(m_alerts)) {
            if (!a.active) continue;
            const QString pair = a.pair.toUpper().trimmed();
            const double price = latestBySymbol.value(pair, 0.0);

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

private:
    void setFeedHealth(const QString &status, bool isStale)
    {
        if (m_lastFeedStatus == status && m_lastFeedStale == isStale)
            return;
        m_lastFeedStatus = status;
        m_lastFeedStale = isStale;
        emit feedHealthChanged(status, isStale);
    }

    static double jsonValueToDouble(const QJsonValue &v)
    {
        if (v.isDouble()) return v.toDouble();
        if (v.isString()) return v.toString().toDouble();
        return 0.0;
    }

    static QPair<QString, double> parseTickLine(const QByteArray &line)
    {
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject())
            return qMakePair(QString(), 0.0);

        const QJsonObject obj = doc.object();
        const QString symbol = obj.value("symbol").toString().toUpper().trimmed();
        if (symbol.isEmpty())
            return qMakePair(QString(), 0.0);

        double mid = jsonValueToDouble(obj.value("mid"));
        const double bid = jsonValueToDouble(obj.value("bid"));
        const double ask = jsonValueToDouble(obj.value("ask"));

        if (mid <= 0.0 && bid > 0.0 && ask > 0.0)
            mid = (bid + ask) / 2.0;
        if (mid <= 0.0 && bid > 0.0)
            mid = bid;
        if (mid <= 0.0 && ask > 0.0)
            mid = ask;

        return qMakePair(symbol, mid);
    }

    QTimer                m_timer;
    QList<PriceAlert>     m_alerts;
    qint64                m_lastOffset = 0;
    qint64                m_lastTickMs = 0;
    QString               m_lastTickPath;
    QString               m_lastFeedStatus;
    bool                  m_lastFeedStale = false;
};

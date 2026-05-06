#pragma once

#include <QDateTime>
#include <QDebug>
#include <QList>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>

// ── Data model ───────────────────────────────────────────────────────────────

struct PriceAlert {
    int     id           = -1;
    QString pair;                  // e.g. "EURUSD"
    double  targetPrice  = 0.0;   // exact price level to watch
    double  nearPips     = 10.0;  // NEAR fires when within this many pips
    QString alertType    = "BOTH"; // "TOUCH" | "NEAR" | "BOTH"
    bool    active       = true;
    bool    touchTriggered = false;
    bool    nearTriggered  = false;
    QString createdAt;
    QString notes;
};

// ── SQLite CRUD helpers ──────────────────────────────────────────────────────

namespace AlertDB {

inline bool createTable(QSqlDatabase &db)
{
    QSqlQuery q(db);
    bool ok = q.exec(R"(
        CREATE TABLE IF NOT EXISTS alerts (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            pair            TEXT    NOT NULL,
            target_price    REAL    NOT NULL,
            near_pips       REAL    DEFAULT 10,
            alert_type      TEXT    DEFAULT 'BOTH',
            active          INTEGER DEFAULT 1,
            touch_triggered INTEGER DEFAULT 0,
            near_triggered  INTEGER DEFAULT 0,
            created_at      TEXT,
            notes           TEXT
        )
    )");
    if (!ok)
        qWarning() << "AlertDB::createTable:" << q.lastError().text();
    return ok;
}

inline QList<PriceAlert> loadAll(QSqlDatabase &db)
{
    QList<PriceAlert> list;
    QSqlQuery q(db);
    q.exec("SELECT id, pair, target_price, near_pips, alert_type, active, "
           "touch_triggered, near_triggered, created_at, notes "
           "FROM alerts ORDER BY id DESC");
    while (q.next()) {
        PriceAlert a;
        a.id             = q.value(0).toInt();
        a.pair           = q.value(1).toString();
        a.targetPrice    = q.value(2).toDouble();
        a.nearPips       = q.value(3).toDouble();
        a.alertType      = q.value(4).toString();
        a.active         = q.value(5).toBool();
        a.touchTriggered = q.value(6).toBool();
        a.nearTriggered  = q.value(7).toBool();
        a.createdAt      = q.value(8).toString();
        a.notes          = q.value(9).toString();
        list.append(a);
    }
    return list;
}

inline int insertAlert(QSqlDatabase &db, const PriceAlert &a)
{
    QSqlQuery q(db);
    q.prepare("INSERT INTO alerts "
              "(pair, target_price, near_pips, alert_type, active, created_at, notes) "
              "VALUES (?, ?, ?, ?, 1, ?, ?)");
    q.addBindValue(a.pair.toUpper());
    q.addBindValue(a.targetPrice);
    q.addBindValue(a.nearPips);
    q.addBindValue(a.alertType);
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    q.addBindValue(a.notes);
    if (!q.exec()) {
        qWarning() << "AlertDB::insertAlert:" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toInt();
}

inline void deleteAlert(QSqlDatabase &db, int id)
{
    QSqlQuery q(db);
    q.prepare("DELETE FROM alerts WHERE id = ?");
    q.addBindValue(id);
    q.exec();
}

inline void markTriggered(QSqlDatabase &db, int id, bool isTouch)
{
    QSqlQuery q(db);
    const QString col = isTouch ? "touch_triggered" : "near_triggered";
    q.prepare(QString("UPDATE alerts SET %1 = 1 WHERE id = ?").arg(col));
    q.addBindValue(id);
    q.exec();
}

inline void resetTriggered(QSqlDatabase &db, int id)
{
    QSqlQuery q(db);
    q.prepare("UPDATE alerts SET touch_triggered = 0, near_triggered = 0 WHERE id = ?");
    q.addBindValue(id);
    q.exec();
}

} // namespace AlertDB

#include "database.h"
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QtSql/QSqlRecord>
#include <QDir>
#include <QDebug>

namespace {

void ensureColumnExists(QSqlDatabase &db, const QString &columnName, const QString &definition)
{
    QSqlQuery infoQuery(db);
    if (!infoQuery.exec("PRAGMA table_info(trades)")) {
        qWarning() << "Failed to inspect trades schema:" << infoQuery.lastError().text();
        return;
    }

    while (infoQuery.next()) {
        if (infoQuery.value("name").toString() == columnName) {
            return;
        }
    }

    QSqlQuery alterQuery(db);
    if (!alterQuery.exec(QString("ALTER TABLE trades ADD COLUMN %1 %2").arg(columnName, definition))) {
        qWarning() << "Failed to add column" << columnName << ":" << alterQuery.lastError().text();
    }
}

}

Database& Database::instance() {
    static Database instance;
    return instance;
}

Database::Database() {}

bool Database::connect() {
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("ledger.db");

    if (!db.open()) {
        qWarning() << "Failed to open database:" << db.lastError().text();
        return false;
    }

    QSqlQuery query(db);
    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS trades (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            date TEXT,
            session TEXT,
            pair TEXT,
            direction TEXT,
            setup TEXT,
            entry REAL,
            sl REAL,
            tp REAL,
            rr REAL,
            risk_percent REAL,
            lot_size REAL,
            result_r REAL,
            result_usd REAL,
            win_loss TEXT,
            account TEXT,
            screenshot TEXT,
            notes TEXT
        )
    )")) {
        qWarning() << "Failed to create or verify trades table:" << query.lastError().text();
        return false;
    }

    ensureColumnExists(db, "lot_size", "REAL");
    ensureColumnExists(db, "account", "TEXT");
    ensureColumnExists(db, "screenshot", "TEXT");

    return true;
}

QSqlDatabase& Database::getDB() {
    return db;
}
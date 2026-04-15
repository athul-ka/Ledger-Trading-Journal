#include "database.h"
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlRecord>
#include <QDir>
#include <QDebug>

namespace {

void ensureColumnExists(QSqlDatabase &db, const QString &columnName, const QString &definition)
{
    QSqlQuery infoQuery(db);
    if (!infoQuery.exec("PRAGMA table_info(trades)")) {
        return;
    }

    while (infoQuery.next()) {
        if (infoQuery.value("name").toString() == columnName) {
            return;
        }
    }

    QSqlQuery alterQuery(db);
    alterQuery.exec(QString("ALTER TABLE trades ADD COLUMN %1 %2").arg(columnName, definition));
}

}

Database& Database::instance() {
    static Database instance;
    return instance;
}

Database::Database() {}

bool Database::connect() {
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("trades.db");

    if (!db.open()) return false;

    QSqlQuery query;
    query.exec(R"(
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
    )");

    ensureColumnExists(db, "lot_size", "REAL");
    ensureColumnExists(db, "account", "TEXT");
    ensureColumnExists(db, "screenshot", "TEXT");

    return true;
}

QSqlDatabase& Database::getDB() {
    return db;
}
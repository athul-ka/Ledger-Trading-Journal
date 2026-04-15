#pragma once
#include <QtSql/QSqlDatabase>

class Database {
public:
    static Database& instance();
    bool connect();
    QSqlDatabase& getDB();

private:
    Database();
    QSqlDatabase db;
};
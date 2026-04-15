#pragma once
#include <QtSql/QSqlDatabase>

class Stats {
public:
    static int totalTrades();
    static int wins();
    static int losses();
    static double totalR();
    static double winRate();
};
#include "stats.h"
#include "database.h"
#include <QtSql/QSqlQuery>

int Stats::totalTrades() {
    QSqlQuery q("SELECT COUNT(*) FROM trades");
    q.next(); return q.value(0).toInt();
}

int Stats::wins() {
    QSqlQuery q("SELECT COUNT(*) FROM trades WHERE win_loss IN ('W', 'Win')");
    q.next(); return q.value(0).toInt();
}

int Stats::losses() {
    QSqlQuery q("SELECT COUNT(*) FROM trades WHERE win_loss IN ('L', 'Loss')");
    q.next(); return q.value(0).toInt();
}

double Stats::totalR() {
    QSqlQuery q("SELECT SUM(result_r) FROM trades");
    q.next(); return q.value(0).toDouble();
}

double Stats::winRate() {
    int t = totalTrades();
    if (t == 0) return 0;
    return (double)wins() / t;
}
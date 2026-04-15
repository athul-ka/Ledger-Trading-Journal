#pragma once
#include <QString>

struct Trade {
    QString date;
    QString session;
    QString pair;
    QString direction;
    QString setup;

    double entry = 0.0;
    double sl = 0.0;
    double tp = 0.0;
    double rr = 0.0;
    double riskPercent = 0.0;
    double lotSize = 0.0;

    double resultR = 0.0;
    double resultUSD = 0.0;

    QString winLoss;
    QString account;
    QString screenshot;
    QString notes;
};
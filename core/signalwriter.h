#pragma once

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QVector>

// Writes a trade signal JSON file that the LedgerBridge MT5 EA polls.
// Place the file in your MT5 terminal's Common Files folder or MQL5/Files/.
namespace SignalWriter {

struct SignalItem {
    QString pair;
    QString direction;
    double entry = 0.0;
    double sl = 0.0;
    double tp = 0.0;
    double lotSize = 0.0;
    QString account;
};

inline bool writeSignal(const QString &pair,
                        const QString &direction,
                        double entry,
                        double sl,
                        double tp,
                        double lotSize,
                        const QString &filePath,
                        QString &errorMsg)
{
    QJsonObject obj;
    obj["pair"]      = pair;
    obj["direction"] = direction;
    obj["entry"]     = entry;
    obj["sl"]        = sl;
    obj["tp"]        = tp;
    obj["lotSize"]   = lotSize;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        errorMsg = file.errorString();
        return false;
    }

    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

// Writes one JSON object per line so MT5 can execute multiple account orders in one batch.
inline bool writeSignalQueue(const QVector<SignalItem> &items,
                             const QString &filePath,
                             QString &errorMsg)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        errorMsg = file.errorString();
        return false;
    }

    for (const SignalItem &item : items) {
        QJsonObject obj;
        obj["pair"] = item.pair;
        obj["direction"] = item.direction;
        obj["entry"] = item.entry;
        obj["sl"] = item.sl;
        obj["tp"] = item.tp;
        obj["lotSize"] = item.lotSize;
        obj["account"] = item.account;
        file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
        file.write("\n");
    }

    file.close();
    return true;
}

} // namespace SignalWriter

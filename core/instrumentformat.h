#pragma once

#include <QString>

namespace InstrumentFormat {

inline int priceDecimals(const QString &instrument)
{
    const QString symbol = instrument.toUpper().trimmed();

    if (symbol == "BTCUSD" || symbol == "ETHUSD") return 1;
    if (symbol == "XAUUSD") return 2;
    if (symbol == "XAGUSD") return 3;
    if (symbol == "US30" || symbol == "DJ30" || symbol == "DJIA") return 2;
    if (symbol == "NAS100" || symbol == "US100" || symbol == "NASDAQ100") return 1;
    if (symbol == "SP500" || symbol == "US500") return 1;
    if (symbol.endsWith("JPY")) return 3;
    return 5;
}

inline double priceStep(const QString &instrument)
{
    switch (priceDecimals(instrument)) {
    case 1:
        return 0.1;
    case 2:
        return 0.01;
    case 3:
        return 0.001;
    case 4:
        return 0.0001;
    default:
        return 0.00001;
    }
}

} // namespace InstrumentFormat
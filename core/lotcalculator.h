#pragma once

#include <QString>
#include <QtMath>

namespace LotCalculator {

struct PipInfo {
    double pipSize;           // price movement per 1 pip
    double pipValuePerLot;    // USD value per pip for 1 standard lot
};

// Returns pip size and USD pip value per standard lot for common instruments.
// Values are broker-approximate; user should verify with their broker.
inline PipInfo getPipInfo(const QString &pair)
{
    const QString p = pair.toUpper().trimmed();

    if (p == "XAUUSD")                              return {0.01,   1.0};
    if (p == "XAGUSD")                              return {0.001,  5.0};
    if (p == "NAS100" || p == "US100" || p == "NASDAQ100") return {0.01, 1.0};
    if (p == "US30"   || p == "DJ30"  || p == "DJIA")      return {1.0,  1.0};
    if (p == "SP500"  || p == "US500")              return {0.1,    1.0};
    if (p == "BTCUSD")                              return {1.0,    0.01};
    if (p == "ETHUSD")                              return {0.1,    0.01};
    if (p.endsWith("JPY"))                          return {0.01,   9.09}; // ~1/USDJPY × 1000
    if (p.endsWith("USD"))                          return {0.0001, 10.0}; // e.g. EURUSD, GBPUSD
    if (p.startsWith("USD"))                        return {0.0001, 10.0}; // e.g. USDJPY above, USDCHF
    return {0.0001, 10.0}; // fallback for cross pairs
}

// Risk-to-Reward ratio: reward / risk (both measured from entry).
inline double calculateRR(double entry, double sl, double tp)
{
    const double risk   = qAbs(entry - sl);
    const double reward = qAbs(tp - entry);
    if (risk < 1e-10) return 0.0;
    return reward / risk;
}

// Lot size based on fixed-dollar risk model.
// lotSize = (balance × riskPercent%) / (slPips × pipValuePerLot)
// Returns value rounded to 2 decimal places (standard broker precision).
inline double calculateLotSize(const QString &pair,
                               double entry,
                               double sl,
                               double riskPercent,
                               double balance)
{
    if (balance <= 0.0 || riskPercent <= 0.0) return 0.0;

    const double slDistance = qAbs(entry - sl);
    if (slDistance < 1e-10) return 0.0;

    const PipInfo info    = getPipInfo(pair);
    const double slPips   = slDistance / info.pipSize;
    const double riskAmt  = balance * riskPercent / 100.0;
    const double lotSize  = riskAmt / (slPips * info.pipValuePerLot);

    // Round to 2 decimal places
    return qRound(lotSize * 100.0) / 100.0;
}

// Lot size based directly on fixed-dollar risk.
// lotSize = riskAmountUsd / (slPips * pipValuePerLot)
inline double calculateLotSizeFromRiskAmount(const QString &pair,
                                             double entry,
                                             double sl,
                                             double riskAmountUsd)
{
    if (riskAmountUsd <= 0.0) return 0.0;

    const double slDistance = qAbs(entry - sl);
    if (slDistance < 1e-10) return 0.0;

    const PipInfo info   = getPipInfo(pair);
    const double slPips  = slDistance / info.pipSize;
    const double lotSize = riskAmountUsd / (slPips * info.pipValuePerLot);

    return qRound(lotSize * 100.0) / 100.0;
}

} // namespace LotCalculator

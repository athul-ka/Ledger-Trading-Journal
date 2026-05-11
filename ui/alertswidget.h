#pragma once

#include <QList>
#include <QWidget>

#include "pricealert.h"

class QLabel;
class QTableWidget;

class AlertsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AlertsWidget(QWidget *parent = nullptr);

    // Reload from DB and repaint table
    void refresh();

    // Called by PriceFetcher when a price update arrives (updates price column)
    void onPriceUpdated(const QString &pair, double price);

    // Called by PriceFetcher when an alert fires (marks row red/orange)
    void onAlertTriggered(const PriceAlert &alert, double price, bool isTouch);

    // Called by PriceFetcher to update feed health indicator
    void onFeedHealthChanged(const QString &status, bool isStale);

    const QList<PriceAlert> &alerts() const { return m_alerts; }

signals:
    void alertsChanged(); // emitted after any add / delete / reset

private slots:
    void addAlert();
    void deleteSelected();
    void resetSelected();

private:
    void rebuildTable();

    QTableWidget          *m_table       = nullptr;
    QLabel                *m_feedStatus  = nullptr;
    QList<PriceAlert>      m_alerts;
    QMap<QString, double>  m_lastPrices; // pair → last known price
};

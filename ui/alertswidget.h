#pragma once

#include <QList>
#include <QNetworkAccessManager>
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

    const QList<PriceAlert> &alerts() const { return m_alerts; }

signals:
    void alertsChanged(); // emitted after any add / delete / reset

private slots:
    void addAlert();
    void deleteSelected();
    void resetSelected();
    void syncToPi();

private:
    void rebuildTable();
    void updateStatusLabel();

    QTableWidget          *m_table       = nullptr;
    QLabel                *m_piStatus    = nullptr;
    QList<PriceAlert>      m_alerts;
    QNetworkAccessManager  m_nam;
    QMap<QString, double>  m_lastPrices; // pair → last known price
};

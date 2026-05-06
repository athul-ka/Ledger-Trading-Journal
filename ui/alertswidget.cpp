#include "alertswidget.h"
#include "alertsync.h"
#include "constants.h"
#include "database.h"
#include "instrumentformat.h"
#include "lotcalculator.h"

#include <QColor>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QDialog>
#include <QtMath>

namespace {

// Column indices
enum Col { ColPair = 0, ColTarget, ColType, ColNearPips, ColCurrentPrice, ColStatus, ColNotes, ColCount };

QStringList instrumentList()
{
    return {
        "EURUSD", "GBPUSD", "USDJPY", "USDCHF", "USDCAD", "AUDUSD", "NZDUSD",
        "EURGBP", "EURJPY", "EURCHF", "EURCAD", "EURAUD", "EURNZD",
        "GBPJPY", "GBPCHF", "GBPCAD", "GBPAUD", "GBPNZD",
        "AUDJPY", "AUDCHF", "AUDCAD", "AUDNZD",
        "NZDJPY", "NZDCHF", "NZDCAD",
        "CADJPY", "CADCHF", "CHFJPY",
        "NAS100", "US30", "XAUUSD", "XAGUSD", "BTCUSD", "ETHUSD"
    };
}

QString statusText(const PriceAlert &a)
{
    if (a.touchTriggered) return "Touched ✓";
    if (a.nearTriggered)  return "Near fired ●";
    return "Active";
}

QColor statusColor(const PriceAlert &a)
{
    if (a.touchTriggered) return QColor("#4CAF50");
    if (a.nearTriggered)  return QColor("#FF9800");
    return QColor("#00BCD4");
}

QTableWidgetItem *makeItem(const QString &text, Qt::Alignment align = Qt::AlignCenter)
{
    auto *item = new QTableWidgetItem(text);
    item->setTextAlignment(align);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

} // namespace

// ── Constructor ───────────────────────────────────────────────────────────────

AlertsWidget::AlertsWidget(QWidget *parent) : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(24, 18, 24, 18);
    outer->setSpacing(12);

    auto *titleRow = new QHBoxLayout;
    auto *title = new QLabel("Price Alerts", this);
    title->setObjectName("PageTitle");
    titleRow->addWidget(title);
    titleRow->addStretch();

    m_piStatus = new QLabel("Pi: not configured", this);
    m_piStatus->setStyleSheet("font-size: 11px; color: gray;");
    titleRow->addWidget(m_piStatus);

    outer->addLayout(titleRow);

    // Hint label
    auto *hint = new QLabel(
        "NEAR alert fires when price enters the pip zone.  "
        "TOUCH alert fires when price reaches the exact level (±1 pip).", this);
    hint->setWordWrap(true);
    hint->setStyleSheet("color: gray; font-size: 11px;");
    outer->addWidget(hint);

    // Table
    m_table = new QTableWidget(0, ColCount, this);
    m_table->setHorizontalHeaderLabels(
        {"Pair", "Target Price", "Type", "Near Pips", "Current Price", "Status", "Notes"});
    m_table->horizontalHeader()->setSectionResizeMode(ColNotes,  QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(ColStatus, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    outer->addWidget(m_table, 1);

    // Button row
    auto *btnRow = new QHBoxLayout;
    auto *addBtn    = new QPushButton("+ Add Alert",     this);
    auto *delBtn    = new QPushButton("Delete",          this);
    auto *resetBtn  = new QPushButton("Reset Selected",  this);
    auto *syncBtn   = new QPushButton("⟳ Sync to Pi",   this);
    addBtn->setObjectName("PrimaryButton");
    btnRow->addWidget(addBtn);
    btnRow->addWidget(delBtn);
    btnRow->addWidget(resetBtn);
    btnRow->addStretch();
    btnRow->addWidget(syncBtn);
    outer->addLayout(btnRow);

    connect(addBtn,   &QPushButton::clicked, this, &AlertsWidget::addAlert);
    connect(delBtn,   &QPushButton::clicked, this, &AlertsWidget::deleteSelected);
    connect(resetBtn, &QPushButton::clicked, this, &AlertsWidget::resetSelected);
    connect(syncBtn,  &QPushButton::clicked, this, &AlertsWidget::syncToPi);

    refresh();
    updateStatusLabel();
}

// ── Public API ────────────────────────────────────────────────────────────────

void AlertsWidget::refresh()
{
    m_alerts = AlertDB::loadAll(Database::instance().getDB());
    rebuildTable();
}

void AlertsWidget::onPriceUpdated(const QString &pair, double price)
{
    m_lastPrices[pair.toUpper()] = price;

    const int dec = InstrumentFormat::priceDecimals(pair);
    for (int row = 0; row < m_table->rowCount(); ++row) {
        if (m_table->item(row, ColPair)->text() == pair.toUpper()) {
            m_table->item(row, ColCurrentPrice)
                ->setText(QString::number(price, 'f', dec));

            // Update distance display in near-pips column
            if (row < m_alerts.size()) {
                const PriceAlert &a = m_alerts[row];
                const double pipSize  = LotCalculator::getPipInfo(a.pair).pipSize;
                const double distPips = qAbs(price - a.targetPrice) / pipSize;
                m_table->item(row, ColCurrentPrice)
                    ->setText(QString("%1  (%2 pips)")
                        .arg(QString::number(price, 'f', dec))
                        .arg(QString::number(distPips, 'f', 1)));
            }
        }
    }
}

void AlertsWidget::onAlertTriggered(const PriceAlert &alert, double price, bool isTouch)
{
    // Persist trigger to DB
    AlertDB::markTriggered(Database::instance().getDB(), alert.id, isTouch);

    // Refresh row colours in table
    for (int row = 0; row < m_table->rowCount(); ++row) {
        if (row < m_alerts.size() && m_alerts[row].id == alert.id) {
            if (isTouch) {
                m_alerts[row].touchTriggered = true;
                m_table->item(row, ColStatus)->setText("Touched ✓");
                m_table->item(row, ColStatus)->setForeground(QColor("#4CAF50"));
            } else {
                m_alerts[row].nearTriggered = true;
                m_table->item(row, ColStatus)->setText("Near fired ●");
                m_table->item(row, ColStatus)->setForeground(QColor("#FF9800"));
            }
        }
    }
    Q_UNUSED(price)
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void AlertsWidget::addAlert()
{
    // Inline dialog for adding an alert
    QDialog dlg(this);
    dlg.setWindowTitle("Add Price Alert");
    dlg.resize(360, 280);
    auto *form = new QFormLayout(&dlg);
    form->setSpacing(10);
    form->setContentsMargins(18, 16, 18, 16);

    auto *pairCombo = new QComboBox(&dlg);
    pairCombo->setEditable(true);
    pairCombo->addItems(instrumentList());

    auto *priceSpin = new QDoubleSpinBox(&dlg);
    priceSpin->setLocale(QLocale::c());
    priceSpin->setGroupSeparatorShown(false);
    priceSpin->setRange(0.0, 1'000'000.0);
    priceSpin->setDecimals(InstrumentFormat::priceDecimals(pairCombo->currentText()));
    priceSpin->setSingleStep(InstrumentFormat::priceStep(pairCombo->currentText()));

    connect(pairCombo, &QComboBox::currentTextChanged, this, [priceSpin](const QString &p) {
        priceSpin->setDecimals(InstrumentFormat::priceDecimals(p));
        priceSpin->setSingleStep(InstrumentFormat::priceStep(p));
    });

    auto *typeComboDlg = new QComboBox(&dlg);
    typeComboDlg->addItems({"BOTH", "NEAR", "TOUCH"});

    auto *nearSpin = new QSpinBox(&dlg);
    nearSpin->setRange(1, 500);
    nearSpin->setValue(10);
    nearSpin->setSuffix(" pips");

    auto *notesEdit = new QLineEdit(&dlg);
    notesEdit->setPlaceholderText("Optional label (e.g. Entry level)");

    form->addRow("Pair",       pairCombo);
    form->addRow("Price",      priceSpin);
    form->addRow("Alert Type", typeComboDlg);
    form->addRow("Near Zone",  nearSpin);
    form->addRow("Notes",      notesEdit);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    if (auto *ok = buttons->button(QDialogButtonBox::Ok))
        ok->setObjectName("PrimaryButton");
    form->addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    PriceAlert a;
    a.pair        = pairCombo->currentText().toUpper().trimmed();
    a.targetPrice = priceSpin->value();
    a.nearPips    = nearSpin->value();
    a.alertType   = typeComboDlg->currentText();
    a.notes       = notesEdit->text().trimmed();

    if (a.pair.isEmpty() || a.targetPrice <= 0.0) {
        QMessageBox::warning(this, "Invalid Alert", "Please enter a valid pair and price.");
        return;
    }

    AlertDB::insertAlert(Database::instance().getDB(), a);
    refresh();
    AlertSync::syncAll(m_nam, m_alerts);
    emit alertsChanged();
}

void AlertsWidget::deleteSelected()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_alerts.size()) return;

    if (QMessageBox::question(this, "Delete Alert",
            QString("Delete alert for %1 @ %2?")
                .arg(m_alerts[row].pair)
                .arg(m_alerts[row].targetPrice)) != QMessageBox::Yes)
        return;

    AlertDB::deleteAlert(Database::instance().getDB(), m_alerts[row].id);
    refresh();
    AlertSync::syncAll(m_nam, m_alerts);
    emit alertsChanged();
}

void AlertsWidget::resetSelected()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_alerts.size()) return;

    AlertDB::resetTriggered(Database::instance().getDB(), m_alerts[row].id);
    refresh();
    AlertSync::syncAll(m_nam, m_alerts);
    emit alertsChanged();
}

void AlertsWidget::syncToPi()
{
    const QString base = AlertSync::piBaseUrl();
    if (base.isEmpty()) {
        QMessageBox::information(this, "Sync to Pi",
            "Pi Server URL is not configured.\n\n"
            "Go to Settings → Price Alerts and enter your Raspberry Pi address.");
        return;
    }

    AlertSync::checkHealth(m_nam, [this](bool ok, const QString &msg) {
        if (!ok) {
            QMessageBox::warning(this, "Pi Unreachable",
                QString("Could not reach Pi server:\n%1\n\n"
                        "Check the URL in Settings and ensure the Pi is running.").arg(msg));
            return;
        }
        AlertSync::syncAll(m_nam, m_alerts);
        m_piStatus->setText(QString("Pi: Connected ● %1 alerts synced")
                                .arg(m_alerts.size()));
        m_piStatus->setStyleSheet("color: #4CAF50; font-size: 11px;");
    });
}

// ── Private helpers ───────────────────────────────────────────────────────────

void AlertsWidget::rebuildTable()
{
    m_table->setRowCount(0);
    m_table->setRowCount(m_alerts.size());

    for (int row = 0; row < m_alerts.size(); ++row) {
        const PriceAlert &a   = m_alerts[row];
        const int         dec = InstrumentFormat::priceDecimals(a.pair);

        m_table->setItem(row, ColPair,      makeItem(a.pair));
        m_table->setItem(row, ColTarget,    makeItem(QString::number(a.targetPrice, 'f', dec)));
        m_table->setItem(row, ColType,      makeItem(a.alertType));
        m_table->setItem(row, ColNearPips,  makeItem(QString::number(a.nearPips, 'f', 0)));
        m_table->setItem(row, ColNotes,     makeItem(a.notes, Qt::AlignLeft | Qt::AlignVCenter));

        // Current price — will be filled in by onPriceUpdated
        const QString cachedPrice = m_lastPrices.contains(a.pair)
            ? QString::number(m_lastPrices[a.pair], 'f', dec)
            : "—";
        m_table->setItem(row, ColCurrentPrice, makeItem(cachedPrice));

        auto *statusItem = makeItem(statusText(a));
        statusItem->setForeground(statusColor(a));
        m_table->setItem(row, ColStatus, statusItem);
    }

    m_table->resizeColumnsToContents();
}

void AlertsWidget::updateStatusLabel()
{
    AlertSync::checkHealth(m_nam, [this](bool ok, const QString &msg) {
        if (ok) {
            m_piStatus->setText("Pi: Connected ●");
            m_piStatus->setStyleSheet("color: #4CAF50; font-size: 11px;");
        } else {
            const QString url = AlertSync::piBaseUrl();
            m_piStatus->setText(url.isEmpty() ? "Pi: not configured"
                                              : QString("Pi: offline (%1)").arg(msg));
            m_piStatus->setStyleSheet("color: gray; font-size: 11px;");
        }
    });
}

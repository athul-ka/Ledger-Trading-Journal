#include "addtradedialog.h"
#include "database.h"

#include <QDate>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QComboBox>
#include <QPushButton>
#include <QSqlQuery>
#include <QSqlError>
#include <QTextEdit>

namespace {

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

QDoubleSpinBox *createPriceSpinBox(QWidget *parent)
{
    auto *spinBox = new QDoubleSpinBox(parent);
    spinBox->setDecimals(5);
    spinBox->setRange(0.0, 1000000000.0);
    spinBox->setSingleStep(0.0001);
    return spinBox;
}

QDoubleSpinBox *createMetricSpinBox(QWidget *parent)
{
    auto *spinBox = new QDoubleSpinBox(parent);
    spinBox->setDecimals(2);
    spinBox->setRange(-1000000.0, 1000000.0);
    spinBox->setSingleStep(0.25);
    return spinBox;
}

QComboBox *createEditableComboBox(QWidget *parent, const QStringList &items)
{
    auto *comboBox = new QComboBox(parent);
    comboBox->setEditable(true);
    comboBox->addItems(items);
    return comboBox;
}

}

AddTradeDialog::AddTradeDialog(int tradeId, QWidget *parent)
    : QDialog(parent)
    , tradeId(tradeId)
{
    setWindowTitle(tradeId >= 0 ? "Edit Trade" : "Add Trade");
    resize(520, 640);

    auto *layout = new QFormLayout(this);
    layout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    dateEdit = new QDateEdit(QDate::currentDate(), this);
    dateEdit->setCalendarPopup(true);
    dateEdit->setDisplayFormat("yyyy-MM-dd");

    sessionCombo = createEditableComboBox(this, {"London", "New York", "Asia"});
    pairCombo = createEditableComboBox(this, instrumentList());
    directionCombo = createEditableComboBox(this, {"BUY", "SELL"});
    setupCombo = createEditableComboBox(this, {"Breakout", "Pullback", "Liquidity Sweep"});

    entrySpin = createPriceSpinBox(this);
    slSpin = createPriceSpinBox(this);
    tpSpin = createPriceSpinBox(this);
    rrSpin = createMetricSpinBox(this);
    rrSpin->setRange(-1000.0, 1000.0);
    riskPercentSpin = createMetricSpinBox(this);
    riskPercentSpin->setRange(0.0, 100.0);
    riskPercentSpin->setSuffix(" %");
    lotSizeSpin = createMetricSpinBox(this);
    lotSizeSpin->setRange(0.0, 1000000.0);
    lotSizeSpin->setDecimals(3);
    lotSizeSpin->setSingleStep(0.01);
    resultRSpin = createMetricSpinBox(this);
    resultUsdSpin = createMetricSpinBox(this);
    resultUsdSpin->setPrefix("$");

    winLossCombo = new QComboBox(this);
    winLossCombo->addItems({"Win", "Loss", "Breakeven"});

    accountEdit = new QLineEdit(this);
    screenshotEdit = new QLineEdit(this);
    notesEdit = new QTextEdit(this);
    notesEdit->setMinimumHeight(120);

    auto *browseButton = new QPushButton("Browse", this);
    auto *screenshotLayout = new QHBoxLayout;
    screenshotLayout->setContentsMargins(0, 0, 0, 0);
    screenshotLayout->addWidget(screenshotEdit);
    screenshotLayout->addWidget(browseButton);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);

    layout->addRow("Date", dateEdit);
    layout->addRow("Session", sessionCombo);
    layout->addRow("Pair", pairCombo);
    layout->addRow("Direction", directionCombo);
    layout->addRow("Setup", setupCombo);
    layout->addRow("Entry", entrySpin);
    layout->addRow("SL", slSpin);
    layout->addRow("TP", tpSpin);
    layout->addRow("RR", rrSpin);
    layout->addRow("Risk %", riskPercentSpin);
    layout->addRow("Lot Size", lotSizeSpin);
    layout->addRow("Result (R)", resultRSpin);
    layout->addRow("Result ($)", resultUsdSpin);
    layout->addRow("Win/Loss", winLossCombo);
    layout->addRow("Account", accountEdit);
    layout->addRow("Screenshot", screenshotLayout);
    layout->addRow("Notes", notesEdit);
    layout->addWidget(buttonBox);

    connect(browseButton, &QPushButton::clicked, this, &AddTradeDialog::browseScreenshot);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &AddTradeDialog::saveTrade);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &AddTradeDialog::reject);

    if (tradeId >= 0) {
        buttonBox->button(QDialogButtonBox::Save)->setText("Update");
        loadTrade();
    }
}

void AddTradeDialog::browseScreenshot()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        "Select Screenshot",
        QString(),
        "Images (*.png *.jpg *.jpeg *.webp *.bmp);;All Files (*)"
    );

    if (!filePath.isEmpty()) {
        screenshotEdit->setText(filePath);
    }
}

void AddTradeDialog::loadTrade()
{
    QSqlQuery q(Database::instance().getDB());
    q.prepare(
        "SELECT date, session, pair, direction, setup, entry, sl, tp, rr, risk_percent, lot_size, "
        "result_r, result_usd, win_loss, account, screenshot, notes "
        "FROM trades WHERE id = ?"
    );
    q.addBindValue(tradeId);

    if (!q.exec() || !q.next()) {
        QMessageBox::critical(this, "Load Failed", q.lastError().text().isEmpty() ? "Trade not found." : q.lastError().text());
        reject();
        return;
    }

    const QDate tradeDate = QDate::fromString(q.value(0).toString(), "yyyy-MM-dd");
    if (tradeDate.isValid()) {
        dateEdit->setDate(tradeDate);
    }

    sessionCombo->setCurrentText(q.value(1).toString());
    pairCombo->setCurrentText(q.value(2).toString());
    directionCombo->setCurrentText(q.value(3).toString());
    setupCombo->setCurrentText(q.value(4).toString());
    entrySpin->setValue(q.value(5).toDouble());
    slSpin->setValue(q.value(6).toDouble());
    tpSpin->setValue(q.value(7).toDouble());
    rrSpin->setValue(q.value(8).toDouble());
    riskPercentSpin->setValue(q.value(9).toDouble());
    lotSizeSpin->setValue(q.value(10).toDouble());
    resultRSpin->setValue(q.value(11).toDouble());
    resultUsdSpin->setValue(q.value(12).toDouble());
    winLossCombo->setCurrentText(q.value(13).toString());
    accountEdit->setText(q.value(14).toString());
    screenshotEdit->setText(q.value(15).toString());
    notesEdit->setPlainText(q.value(16).toString());
}

void AddTradeDialog::saveTrade()
{
    QSqlQuery q(Database::instance().getDB());
    if (pairCombo->currentText().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Missing Pair", "Please enter a trading instrument.");
        return;
    }

    if (tradeId >= 0) {
        q.prepare(
            "UPDATE trades SET "
            "date = ?, session = ?, pair = ?, direction = ?, setup = ?, entry = ?, sl = ?, tp = ?, rr = ?, "
            "risk_percent = ?, lot_size = ?, result_r = ?, result_usd = ?, win_loss = ?, account = ?, screenshot = ?, notes = ? "
            "WHERE id = ?"
        );
    } else {
        q.prepare(
            "INSERT INTO trades ("
            "date, session, pair, direction, setup, entry, sl, tp, rr, risk_percent, lot_size, "
            "result_r, result_usd, win_loss, account, screenshot, notes"
            ") VALUES ("
            "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?"
            ")"
        );
    }

    q.addBindValue(dateEdit->date().toString("yyyy-MM-dd"));
    q.addBindValue(sessionCombo->currentText().trimmed());
    q.addBindValue(pairCombo->currentText().trimmed());
    q.addBindValue(directionCombo->currentText().trimmed());
    q.addBindValue(setupCombo->currentText().trimmed());
    q.addBindValue(entrySpin->value());
    q.addBindValue(slSpin->value());
    q.addBindValue(tpSpin->value());
    q.addBindValue(rrSpin->value());
    q.addBindValue(riskPercentSpin->value());
    q.addBindValue(lotSizeSpin->value());
    q.addBindValue(resultRSpin->value());
    q.addBindValue(resultUsdSpin->value());
    q.addBindValue(winLossCombo->currentText());
    q.addBindValue(accountEdit->text().trimmed());
    q.addBindValue(screenshotEdit->text().trimmed());
    q.addBindValue(notesEdit->toPlainText().trimmed());

    if (tradeId >= 0) {
        q.addBindValue(tradeId);
    }

    if (!q.exec()) {
        QMessageBox::critical(this, "Save Failed", q.lastError().text());
        return;
    }

    accept();
}
#include "addtradedialog.h"
#include "database.h"
#include "constants.h"
#include "instrumentformat.h"

#include <QDate>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGuiApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QComboBox>
#include <QPushButton>
#include <QRegularExpression>
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
    spinBox->setLocale(QLocale::c());
    spinBox->setGroupSeparatorShown(false);
    return spinBox;
}

void applyInstrumentPriceFormat(QDoubleSpinBox *spinBox, const QString &instrument)
{
    const double value = spinBox->value();
    spinBox->setDecimals(InstrumentFormat::priceDecimals(instrument));
    spinBox->setSingleStep(InstrumentFormat::priceStep(instrument));
    spinBox->setValue(value);
}

void applyInstrumentPriceFormat(const QString &instrument,
                                QDoubleSpinBox *entrySpin,
                                QDoubleSpinBox *slSpin,
                                QDoubleSpinBox *tpSpin)
{
    applyInstrumentPriceFormat(entrySpin, instrument);
    applyInstrumentPriceFormat(slSpin, instrument);
    applyInstrumentPriceFormat(tpSpin, instrument);
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

QStringList selectedAccounts(const QCheckBox *funded1Check, const QCheckBox *funded2Check, const QCheckBox *liveCheck)
{
    QStringList accounts;
    if (funded1Check->isChecked()) {
        accounts << "Funded 1";
    }
    if (funded2Check->isChecked()) {
        accounts << "Funded 2";
    }
    if (liveCheck->isChecked()) {
        accounts << "Live";
    }
    return accounts;
}

}

AddTradeDialog::AddTradeDialog(int tradeId, QWidget *parent)
    : QDialog(parent)
    , tradeId(tradeId)
{
    setObjectName("AddTradeDialog");
    setWindowTitle(tradeId >= 0 ? "Edit Trade" : "Add Trade");
    resize(520, 640);

    auto *layout = new QFormLayout(this);
    layout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    layout->setSpacing(10);
    layout->setContentsMargins(18, 16, 18, 16);

    dateEdit = new QDateEdit(QDate::currentDate(), this);
    dateEdit->setCalendarPopup(true);
    dateEdit->setDisplayFormat("yyyy-MM-dd");

    sessionCombo = createEditableComboBox(this, {"London", "New York", "Asia"});
    pairCombo = createEditableComboBox(this, instrumentList());
    directionCombo = createEditableComboBox(this, {"BUY", "SELL"});
    setupCombo = new QComboBox(this);
    setupCombo->addItems(Domain::setupOptions());

    entrySpin = createPriceSpinBox(this);
    slSpin = createPriceSpinBox(this);
    tpSpin = createPriceSpinBox(this);
    applyInstrumentPriceFormat(pairCombo->currentText(), entrySpin, slSpin, tpSpin);
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

    funded1Check = new QCheckBox("Funded 1", this);
    funded2Check = new QCheckBox("Funded 2", this);
    liveCheck = new QCheckBox("Live", this);
    auto *accountLayout = new QHBoxLayout;
    accountLayout->setContentsMargins(0, 0, 0, 0);
    accountLayout->addWidget(funded1Check);
    accountLayout->addWidget(funded2Check);
    accountLayout->addWidget(liveCheck);
    accountLayout->addStretch();

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

    auto *pasteTvButton = new QPushButton("⬇  Paste from TradingView", this);
    pasteTvButton->setToolTip("Paste clipboard text copied from the TradingView indicator.\n"
                              "Expected format:  EURUSD BUY E:1.2345 SL:1.2300 TP:1.2400");
    layout->addRow(pasteTvButton);

    layout->addRow("Entry", entrySpin);
    layout->addRow("SL", slSpin);
    layout->addRow("TP", tpSpin);
    layout->addRow("RR", rrSpin);
    layout->addRow("Risk %", riskPercentSpin);
    layout->addRow("Lot Size", lotSizeSpin);
    layout->addRow("Result (R)", resultRSpin);
    layout->addRow("Result ($)", resultUsdSpin);
    layout->addRow("Win/Loss", winLossCombo);
    layout->addRow("Accounts", accountLayout);
    layout->addRow("Screenshot", screenshotLayout);
    layout->addRow("Notes", notesEdit);

    layout->addWidget(buttonBox);

    // Mark save button as primary so the active app theme styles it
    if (QPushButton *saveButton = buttonBox->button(QDialogButtonBox::Save)) {
        saveButton->setObjectName("PrimaryButton");
    }

    connect(browseButton, &QPushButton::clicked, this, &AddTradeDialog::browseScreenshot);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &AddTradeDialog::saveTrade);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &AddTradeDialog::reject);
    connect(pasteTvButton, &QPushButton::clicked, this, &AddTradeDialog::pasteFromTradingView);
    connect(pairCombo, &QComboBox::currentTextChanged, this, [this](const QString &pair) {
        applyInstrumentPriceFormat(pair, entrySpin, slSpin, tpSpin);
    });

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
    const QStringList accounts = Domain::parseAccounts(q.value(14).toString());
    for (const QString &account : accounts) {
        if (account == "Funded 1") {
            funded1Check->setChecked(true);
        } else if (account == "Funded 2") {
            funded2Check->setChecked(true);
        } else if (account == "Live") {
            liveCheck->setChecked(true);
        }
    }
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

    const QStringList accounts = selectedAccounts(funded1Check, funded2Check, liveCheck);
    if (accounts.isEmpty()) {
        QMessageBox::warning(this, "Missing Account", "Please select at least one account.");
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
    q.addBindValue(Domain::joinAccounts(accounts));
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

// ---------------------------------------------------------------------------
// TradingView clipboard import
// ---------------------------------------------------------------------------
void AddTradeDialog::pasteFromTradingView()
{
    const QString text = QGuiApplication::clipboard()->text().trimmed();
    if (text.isEmpty()) {
        QMessageBox::information(this, "Paste from TradingView",
            "Clipboard is empty.\n\n"
            "Copy the formatted text from the TradingView indicator first.\n\n"
            "Expected format:\n"
            "  EURUSD BUY E:1.2345 SL:1.2300 TP:1.2400");
        return;
    }

    // Match:  PAIR  BUY|SELL  E(NTRY)?:value  S(L)?:value  T(P)?:value
    static const QRegularExpression pairDirRe(
        R"(^([A-Z0-9]+)\s+(BUY|SELL)\b)", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression entryRe(
        R"(\b(?:ENTRY|E):([\d.]+))", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression slRe(
        R"(\bSL?:([\d.]+))", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression tpRe(
        R"(\bTP?:([\d.]+))", QRegularExpression::CaseInsensitiveOption);

    bool allFound = true;

    const auto pairDirMatch = pairDirRe.match(text);
    if (pairDirMatch.hasMatch()) {
        pairCombo->setCurrentText(pairDirMatch.captured(1).toUpper());
        directionCombo->setCurrentText(pairDirMatch.captured(2).toUpper());
    }

    const auto entryMatch = entryRe.match(text);
    if (entryMatch.hasMatch()) {
        entrySpin->setValue(entryMatch.captured(1).toDouble());
    } else {
        allFound = false;
    }

    const auto slMatch = slRe.match(text);
    if (slMatch.hasMatch()) {
        slSpin->setValue(slMatch.captured(1).toDouble());
    } else {
        allFound = false;
    }

    const auto tpMatch = tpRe.match(text);
    if (tpMatch.hasMatch()) {
        tpSpin->setValue(tpMatch.captured(1).toDouble());
    } else {
        allFound = false;
    }

    if (!allFound) {
        QMessageBox::warning(this, "Paste from TradingView",
            "Could not fully parse the clipboard text.\n\n"
            "Expected format:\n"
            "  EURUSD BUY E:1.2345 SL:1.2300 TP:1.2400\n\n"
            "Clipboard contained:\n" + text.left(120));
    }
}
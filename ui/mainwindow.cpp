#include "mainwindow.h"
#include "database.h"
#include "addtradedialog.h"
#include "constants.h"
#include "instrumentformat.h"
#include "lotcalculator.h"
#include "signalwriter.h"
#include "theme.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QButtonGroup>
#include <QColor>
#include <QDate>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QMessageBox>
#include <QComboBox>
#include <QCheckBox>
#include <QClipboard>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QSortFilterProxyModel>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlTableModel>
#include <QRadioButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTableView>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>
#include <QLabel>
#include <QWidget>

#include <cmath>

namespace {

class TradeFilterModel : public QSortFilterProxyModel {
public:
    explicit TradeFilterModel(QObject *parent = nullptr)
        : QSortFilterProxyModel(parent)
    {
    }

    void setSearchText(const QString &text)
    {
        searchText = text.trimmed();
        invalidateFilter();
    }

    void setSessionFilter(const QString &value)
    {
        sessionFilter = value.trimmed();
        invalidateFilter();
    }

    void setResultFilter(const QString &value)
    {
        resultFilter = value.trimmed();
        invalidateFilter();
    }

    void setSetupFilter(const QString &value)
    {
        setupFilter = value.trimmed();
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override
    {
        const auto *sqlModel = qobject_cast<const QSqlTableModel *>(sourceModel());
        if (!sqlModel) {
            return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
        }

        const auto valueAt = [&](const QString &fieldName) {
            return sqlModel->data(sqlModel->index(sourceRow, sqlModel->fieldIndex(fieldName), sourceParent)).toString();
        };

        if (!sessionFilter.isEmpty() && sessionFilter != "All" && valueAt("session") != sessionFilter) {
            return false;
        }

        if (!resultFilter.isEmpty() && resultFilter != "All" && valueAt("win_loss") != resultFilter) {
            return false;
        }

        if (!setupFilter.isEmpty() && setupFilter != "All" && valueAt("setup") != setupFilter) {
            return false;
        }

        if (searchText.isEmpty()) {
            return true;
        }

        const QString needle = searchText.toLower();
        for (int column = 0; column < sqlModel->columnCount(sourceParent); ++column) {
            const QString haystack = sqlModel->data(sqlModel->index(sourceRow, column, sourceParent)).toString().toLower();
            if (haystack.contains(needle)) {
                return true;
            }
        }

        return false;
    }

private:
    QString searchText;
    QString sessionFilter;
    QString resultFilter;
    QString setupFilter;
};

QString csvEscape(const QString &value)
{
    QString escaped = value;
    escaped.replace('"', "\"\"");
    return QString("\"%1\"").arg(escaped);
}

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
    const QSignalBlocker blocker(spinBox);
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

QDoubleSpinBox *createMoneySpinBox(QWidget *parent)
{
    auto *spinBox = new QDoubleSpinBox(parent);
    spinBox->setDecimals(2);
    spinBox->setRange(0.0, 1000000000.0);
    spinBox->setSingleStep(1.0);
    spinBox->setPrefix("$");
    return spinBox;
}

bool parseTradingViewText(const QString &text,
                          QString &pair,
                          QString &direction,
                          double &entry,
                          double &sl,
                          double &tp)
{
    static const QRegularExpression pairDirRe(
        R"(^([A-Z0-9]+)\s+(BUY|SELL)\b)", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression entryRe(
        R"(\b(?:ENTRY|E):([\d.]+))", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression slRe(
        R"(\bSL?:([\d.]+))", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression tpRe(
        R"(\bTP?:([\d.]+))", QRegularExpression::CaseInsensitiveOption);

    const auto pairDirMatch = pairDirRe.match(text.trimmed());
    if (!pairDirMatch.hasMatch()) {
        return false;
    }

    const auto entryMatch = entryRe.match(text);
    const auto slMatch = slRe.match(text);
    const auto tpMatch = tpRe.match(text);
    if (!entryMatch.hasMatch() || !slMatch.hasMatch() || !tpMatch.hasMatch()) {
        return false;
    }

    pair = pairDirMatch.captured(1).toUpper();
    direction = pairDirMatch.captured(2).toUpper();
    entry = entryMatch.captured(1).toDouble();
    sl = slMatch.captured(1).toDouble();
    tp = tpMatch.captured(1).toDouble();
    return true;
}

double currentBalanceForAccount(const QString &account)
{
    const QString normalized = Domain::normalizeAccountName(account);
    double totalUsd = 0.0;

    QSqlQuery q(Database::instance().getDB());
    q.prepare("SELECT account, result_usd FROM trades");
    if (!q.exec()) {
        return Domain::startingBalanceForAccount(normalized);
    }

    while (q.next()) {
        const QStringList accounts = Domain::parseAccounts(q.value(0).toString());
        if (accounts.contains(normalized)) {
            totalUsd += q.value(1).toDouble();
        }
    }

    return Domain::startingBalanceForAccount(normalized) + totalUsd;
}

}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    Database::instance().connect();
    currentCalendarMonth = QDate::currentDate().addDays(1 - QDate::currentDate().day());
    selectedCalendarDate = QDate::currentDate();

    QSettings settings("Ledger", "Ledger");
    currentThemeId = static_cast<Theme::ThemeId>(
        settings.value("theme", static_cast<int>(Theme::Bloomberg)).toInt()
    );

    setupUI();
    setWindowTitle("Ledger");
}

void MainWindow::setupUI() {
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    viewTabs = new QTabWidget(this);
    auto *tradesPage = new QWidget(this);
    auto *tradesLayout = new QVBoxLayout(tradesPage);
    auto *filterLayout = new QHBoxLayout;
    auto *buttonLayout = new QHBoxLayout;

    auto *title = new QLabel("Ledger Dashboard");
    title->setObjectName("PageTitle");

    statsLabel = new QLabel;
    statsLabel->setObjectName("StatsCard");
    searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText("Search all trade fields...");
    sessionFilterCombo = new QComboBox(this);
    sessionFilterCombo->addItems({"All", "London", "New York", "Asia"});
    setupFilterCombo = new QComboBox(this);
    setupFilterCombo->addItem("All");
    setupFilterCombo->addItems(Domain::setupOptions());
    resultFilterCombo = new QComboBox(this);
    resultFilterCombo->addItems({"All", "Win", "Loss", "Breakeven"});

    auto *btnAdd = new QPushButton("Add Trade");
    btnAdd->setObjectName("PrimaryButton");
    editButton = new QPushButton("Edit Selected");
    deleteButton = new QPushButton("Delete Selected");
    exportButton = new QPushButton("Export CSV");
    tradeTable = new QTableView(this);

    filterLayout->addWidget(new QLabel("Search", this));
    filterLayout->addWidget(searchEdit, 1);
    filterLayout->addWidget(new QLabel("Session", this));
    filterLayout->addWidget(sessionFilterCombo);
    filterLayout->addWidget(new QLabel("Setup", this));
    filterLayout->addWidget(setupFilterCombo);
    filterLayout->addWidget(new QLabel("Result", this));
    filterLayout->addWidget(resultFilterCombo);

    buttonLayout->addWidget(btnAdd);
    buttonLayout->addWidget(editButton);
    buttonLayout->addWidget(deleteButton);
    buttonLayout->addWidget(exportButton);
    buttonLayout->addStretch();

    tradesLayout->addWidget(title);
    tradesLayout->addWidget(statsLabel);
    tradesLayout->addLayout(filterLayout);
    tradesLayout->addLayout(buttonLayout);
    tradesLayout->addWidget(tradeTable);

    viewTabs->addTab(tradesPage, "Trades");
    setupCalendarView();
    setupAccountsView();
    setupExecuteView();
    setupSettingsView();

    layout->addWidget(viewTabs);

    setCentralWidget(central);
    resize(1440, 820);

    applyTheme(currentThemeId);

    setupTradeTable();

    connect(btnAdd, &QPushButton::clicked, this, &MainWindow::addTrade);
    connect(editButton, &QPushButton::clicked, this, &MainWindow::editSelectedTrade);
    connect(deleteButton, &QPushButton::clicked, this, &MainWindow::deleteSelectedTrade);
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::exportTradesToCsv);
    connect(searchEdit, &QLineEdit::textChanged, this, [this]() { applyFilters(); });
    connect(sessionFilterCombo, &QComboBox::currentTextChanged, this, [this]() { applyFilters(); });
    connect(setupFilterCombo, &QComboBox::currentTextChanged, this, [this]() { applyFilters(); });
    connect(resultFilterCombo, &QComboBox::currentTextChanged, this, [this]() { applyFilters(); });

    refreshTrades();
    refreshCalendarView();
    loadStats();
    updateActionState();
}

void MainWindow::setupCalendarView()
{
    auto *calendarPage = new QWidget(this);
    auto *calendarLayout = new QVBoxLayout(calendarPage);
    auto *navigationLayout = new QHBoxLayout;
    auto *calendarTitle = new QLabel("Calendar View", this);
    calendarTitle->setObjectName("PageTitle");

    auto *previousMonthButton = new QPushButton("Previous", this);
    auto *nextMonthButton = new QPushButton("Next", this);
    calendarMonthLabel = new QLabel(this);
    calendarMonthLabel->setObjectName("PageTitle");
    calendarYearCombo = new QComboBox(this);
    for (int year = 2010; year <= 2035; ++year) {
        calendarYearCombo->addItem(QString::number(year), year);
    }

    calendarTable = new QTableWidget(6, 7, this);
    calendarTable->setHorizontalHeaderLabels({"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"});
    calendarTable->verticalHeader()->setVisible(false);
    calendarTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    calendarTable->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    calendarTable->setSelectionMode(QAbstractItemView::SingleSelection);
    calendarTable->setSelectionBehavior(QAbstractItemView::SelectItems);
    calendarTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    calendarTable->setFocusPolicy(Qt::NoFocus);
    calendarTable->setShowGrid(false);
    calendarTable->setWordWrap(true);
    calendarTable->setAlternatingRowColors(false);

    for (int row = 0; row < calendarTable->rowCount(); ++row) {
        calendarTable->setRowHeight(row, 110);
    }

    calendarSummaryLabel = new QLabel(this);
    calendarSummaryLabel->setObjectName("StatsCard");
    calendarSummaryLabel->setWordWrap(true);
    calendarSummaryLabel->setMinimumHeight(72);

    navigationLayout->addStretch();
    navigationLayout->addWidget(previousMonthButton);
    navigationLayout->addWidget(calendarMonthLabel);
    navigationLayout->addWidget(new QLabel("Year:", this));
    navigationLayout->addWidget(calendarYearCombo);
    navigationLayout->addWidget(nextMonthButton);
    navigationLayout->addStretch();

    calendarLayout->addWidget(calendarTitle);
    calendarLayout->addLayout(navigationLayout);
    calendarLayout->addWidget(calendarTable);
    calendarLayout->addWidget(calendarSummaryLabel);
    calendarLayout->addStretch();

    viewTabs->addTab(calendarPage, "Calendar");

    connect(previousMonthButton, &QPushButton::clicked, this, [this]() { changeCalendarMonth(-1); });
    connect(nextMonthButton, &QPushButton::clicked, this, [this]() { changeCalendarMonth(1); });
    connect(calendarYearCombo, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        setCalendarYear(text.toInt());
    });
    connect(calendarTable, &QTableWidget::cellClicked, this, &MainWindow::handleCalendarCellClicked);
}

void MainWindow::setupTradeTable()
{
    tradeModel = new QSqlTableModel(this, Database::instance().getDB());
    tradeModel->setTable("trades");
    tradeModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    tradeProxyModel = new TradeFilterModel(this);
    tradeProxyModel->setSourceModel(tradeModel);
    tradeProxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);

    tradeModel->setHeaderData(tradeModel->fieldIndex("date"), Qt::Horizontal, "Date");
    tradeModel->setHeaderData(tradeModel->fieldIndex("session"), Qt::Horizontal, "Session");
    tradeModel->setHeaderData(tradeModel->fieldIndex("pair"), Qt::Horizontal, "Pair");
    tradeModel->setHeaderData(tradeModel->fieldIndex("direction"), Qt::Horizontal, "Direction");
    tradeModel->setHeaderData(tradeModel->fieldIndex("setup"), Qt::Horizontal, "Setup");
    tradeModel->setHeaderData(tradeModel->fieldIndex("entry"), Qt::Horizontal, "Entry");
    tradeModel->setHeaderData(tradeModel->fieldIndex("sl"), Qt::Horizontal, "SL");
    tradeModel->setHeaderData(tradeModel->fieldIndex("tp"), Qt::Horizontal, "TP");
    tradeModel->setHeaderData(tradeModel->fieldIndex("rr"), Qt::Horizontal, "RR");
    tradeModel->setHeaderData(tradeModel->fieldIndex("risk_percent"), Qt::Horizontal, "Risk %");
    tradeModel->setHeaderData(tradeModel->fieldIndex("lot_size"), Qt::Horizontal, "Lot Size");
    tradeModel->setHeaderData(tradeModel->fieldIndex("result_r"), Qt::Horizontal, "Result (R)");
    tradeModel->setHeaderData(tradeModel->fieldIndex("result_usd"), Qt::Horizontal, "Result ($)");
    tradeModel->setHeaderData(tradeModel->fieldIndex("win_loss"), Qt::Horizontal, "Win/Loss");
    tradeModel->setHeaderData(tradeModel->fieldIndex("account"), Qt::Horizontal, "Account");
    tradeModel->setHeaderData(tradeModel->fieldIndex("screenshot"), Qt::Horizontal, "Screenshot");
    tradeModel->setHeaderData(tradeModel->fieldIndex("notes"), Qt::Horizontal, "Notes");

    tradeTable->setModel(tradeProxyModel);
    tradeTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    tradeTable->setSelectionMode(QAbstractItemView::SingleSelection);
    tradeTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tradeTable->setAlternatingRowColors(true);
    tradeTable->setSortingEnabled(true);
    tradeTable->verticalHeader()->setVisible(false);
    tradeTable->horizontalHeader()->setStretchLastSection(true);
    tradeTable->setWordWrap(false);
    tradeTable->setTextElideMode(Qt::ElideRight);
    tradeTable->hideColumn(tradeModel->fieldIndex("id"));

    connect(tradeTable->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
        updateActionState();
    });
    connect(tradeTable, &QTableView::doubleClicked, this, [this](const QModelIndex &) {
        handleTableActivated();
    });
}

void MainWindow::setupAccountsView()
{
    auto *accountsPage = new QWidget(this);
    auto *accountsLayout = new QVBoxLayout(accountsPage);
    auto *selectorLayout = new QHBoxLayout;
    auto *accountsTitle = new QLabel("Accounts Overview", this);
    accountsTitle->setObjectName("PageTitle");

    accountSelectorCombo = new QComboBox(this);
    accountSelectorCombo->addItems(Domain::accountOptions());

    accountStatsLabel = new QLabel(this);
    accountStatsLabel->setObjectName("StatsCard");
    accountStatsLabel->setWordWrap(true);
    accountStatsLabel->setMinimumHeight(110);

    accountTradesTable = new QTableWidget(this);
    accountTradesTable->setColumnCount(8);
    accountTradesTable->setHorizontalHeaderLabels(
        {"Date", "Pair", "Direction", "Setup", "Result", "Result (R)", "Result ($)", "Accounts"}
    );
    accountTradesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    accountTradesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    accountTradesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    accountTradesTable->setAlternatingRowColors(true);
    accountTradesTable->setWordWrap(false);
    accountTradesTable->setTextElideMode(Qt::ElideRight);
    accountTradesTable->verticalHeader()->setVisible(false);
    accountTradesTable->horizontalHeader()->setStretchLastSection(true);

    selectorLayout->addWidget(new QLabel("Account", this));
    selectorLayout->addWidget(accountSelectorCombo);
    selectorLayout->addStretch();

    accountsLayout->addWidget(accountsTitle);
    accountsLayout->addLayout(selectorLayout);
    accountsLayout->addWidget(accountStatsLabel);
    accountsLayout->addWidget(accountTradesTable);

    viewTabs->addTab(accountsPage, "Accounts");

    connect(accountSelectorCombo, &QComboBox::currentTextChanged, this, [this]() {
        refreshAccountsView();
    });
}

void MainWindow::setupExecuteView()
{
    auto *executePage = new QWidget(this);
    auto *outer = new QVBoxLayout(executePage);
    outer->setContentsMargins(24, 18, 24, 18);
    outer->setSpacing(12);

    auto *title = new QLabel("Execute", this);
    title->setObjectName("PageTitle");

    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    executePairCombo = new QComboBox(this);
    executePairCombo->setEditable(true);
    executePairCombo->addItems(instrumentList());

    executeDirectionCombo = new QComboBox(this);
    executeDirectionCombo->setEditable(false);
    executeDirectionCombo->addItems({"BUY", "SELL"});

    executeEntrySpin = createPriceSpinBox(this);
    executeSlSpin = createPriceSpinBox(this);
    executeTpSpin = createPriceSpinBox(this);

    auto *pasteButton = new QPushButton("Paste from TradingView", this);
    pasteButton->setObjectName("PrimaryButton");

    form->addRow("Pair", executePairCombo);
    form->addRow("Direction", executeDirectionCombo);
    form->addRow("Entry", executeEntrySpin);
    form->addRow("SL", executeSlSpin);
    form->addRow("TP", executeTpSpin);
    form->addRow("TradingView", pasteButton);

    auto *accountsLabel = new QLabel("Accounts", this);
    accountsLabel->setStyleSheet("font-size: 15px; font-weight: 700;");

    executeFunded1Check = new QCheckBox("Funded 1", this);
    executeFunded2Check = new QCheckBox("Funded 2", this);
    executeLiveCheck = new QCheckBox("Live", this);

    executeFunded1RiskSpin = createMoneySpinBox(this);
    executeFunded2RiskSpin = createMoneySpinBox(this);
    executeLiveRiskSpin = createMoneySpinBox(this);
    executeFunded1RiskSpin->setValue(50.0);
    executeFunded2RiskSpin->setValue(50.0);
    executeLiveRiskSpin->setValue(10.0);

    executeFunded1LotLabel = new QLabel("Lot: -", this);
    executeFunded2LotLabel = new QLabel("Lot: -", this);
    executeLiveLotLabel = new QLabel("Lot: -", this);

    auto *funded1Box = new QVBoxLayout;
    funded1Box->addWidget(executeFunded1Check);
    funded1Box->addWidget(new QLabel("Risk Dollar", this));
    funded1Box->addWidget(executeFunded1RiskSpin);
    funded1Box->addWidget(executeFunded1LotLabel);

    auto *funded2Box = new QVBoxLayout;
    funded2Box->addWidget(executeFunded2Check);
    funded2Box->addWidget(new QLabel("Risk Dollar", this));
    funded2Box->addWidget(executeFunded2RiskSpin);
    funded2Box->addWidget(executeFunded2LotLabel);

    auto *liveBox = new QVBoxLayout;
    liveBox->addWidget(executeLiveCheck);
    liveBox->addWidget(new QLabel("Risk Dollar", this));
    liveBox->addWidget(executeLiveRiskSpin);
    liveBox->addWidget(executeLiveLotLabel);

    auto *riskRow = new QHBoxLayout;
    riskRow->addLayout(funded1Box, 1);
    riskRow->addLayout(funded2Box, 1);
    riskRow->addLayout(liveBox, 1);

    executeLotsSummaryLabel = new QLabel("Lot sizes will appear here after calculation.", this);
    executeLotsSummaryLabel->setWordWrap(true);
    executeLotsSummaryLabel->setObjectName("StatsCard");

    auto *calculateBtn = new QPushButton("Calculate Lot Sizes", this);
    calculateBtn->setObjectName("PrimaryButton");
    auto *executeBtn = new QPushButton("Execute", this);
    executeBtn->setObjectName("PrimaryButton");

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addWidget(calculateBtn);
    buttonRow->addWidget(executeBtn);
    buttonRow->addStretch();

    outer->addWidget(title);
    outer->addLayout(form);
    outer->addWidget(accountsLabel);
    outer->addLayout(riskRow);
    outer->addWidget(executeLotsSummaryLabel);
    outer->addLayout(buttonRow);
    outer->addStretch();

    viewTabs->addTab(executePage, "Execute");

    connect(pasteButton, &QPushButton::clicked, this, &MainWindow::pasteExecuteFromTradingView);
    connect(executeFunded1Check, &QCheckBox::toggled, this, &MainWindow::updateExecuteAccountInputs);
    connect(executeFunded2Check, &QCheckBox::toggled, this, &MainWindow::updateExecuteAccountInputs);
    connect(executeLiveCheck, &QCheckBox::toggled, this, &MainWindow::updateExecuteAccountInputs);
    connect(calculateBtn, &QPushButton::clicked, this, &MainWindow::calculateExecuteLotSizes);
    connect(executeBtn, &QPushButton::clicked, this, &MainWindow::executeTradesToMt5);
    connect(executeEntrySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::calculateExecuteLotSizes);
    connect(executeSlSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::calculateExecuteLotSizes);
    connect(executeTpSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::calculateExecuteLotSizes);
    connect(executeFunded1RiskSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::calculateExecuteLotSizes);
    connect(executeFunded2RiskSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::calculateExecuteLotSizes);
    connect(executeLiveRiskSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::calculateExecuteLotSizes);
    connect(executePairCombo, &QComboBox::currentTextChanged, this, [this](const QString &pair) {
        applyInstrumentPriceFormat(pair, executeEntrySpin, executeSlSpin, executeTpSpin);
        calculateExecuteLotSizes();
    });

    applyInstrumentPriceFormat(executePairCombo->currentText(), executeEntrySpin, executeSlSpin, executeTpSpin);
    updateExecuteAccountInputs();
}

void MainWindow::loadStats() {
    if (!tradeModel || !tradeProxyModel) {
        return;
    }

    int totalTrades = 0;
    int wins = 0;
    int losses = 0;
    double totalR = 0.0;

    const int winLossColumn = tradeModel->fieldIndex("win_loss");
    const int resultRColumn = tradeModel->fieldIndex("result_r");

    for (int row = 0; row < tradeProxyModel->rowCount(); ++row) {
        ++totalTrades;
        const QModelIndex winLossIndex = tradeProxyModel->index(row, winLossColumn);
        const QString result = tradeProxyModel->data(winLossIndex).toString().trimmed().toLower();
        if (result == "w" || result == "win") {
            ++wins;
        } else if (result == "l" || result == "loss") {
            ++losses;
        }

        const QModelIndex resultRIndex = tradeProxyModel->index(row, resultRColumn);
        totalR += tradeProxyModel->data(resultRIndex).toDouble();
    }

    const double winRate = totalTrades == 0 ? 0.0 : (static_cast<double>(wins) / static_cast<double>(totalTrades)) * 100.0;
    const QString setupContext = setupFilterCombo && setupFilterCombo->currentText() != "All"
        ? QString(" (%1)").arg(setupFilterCombo->currentText())
        : QString();

    QString text = QString(
        "Total Trades%1: %2\n"
        "Wins: %3\n"
        "Losses: %4\n"
        "Win Rate: %5%\n"
        "Total R: %6"
    ).arg(setupContext)
     .arg(totalTrades)
     .arg(wins)
     .arg(losses)
     .arg(winRate, 0, 'f', 1)
     .arg(totalR, 0, 'f', 2);

    statsLabel->setText(text);
}

void MainWindow::refreshTrades()
{
    tradeModel->select();
    const int dateColumn = tradeModel->fieldIndex("date");
    if (dateColumn >= 0) {
        tradeTable->sortByColumn(dateColumn, Qt::DescendingOrder);
    }
    applyFilters();
    tradeTable->resizeColumnsToContents();
    refreshCalendarView();
    refreshAccountsView();
    updateActionState();
}

void MainWindow::refreshAccountsView()
{
    if (!accountSelectorCombo || !accountStatsLabel || !accountTradesTable) {
        return;
    }

    const QString selectedAccount = Domain::normalizeAccountName(accountSelectorCombo->currentText());
    const double initialBalance = Domain::startingBalanceForAccount(selectedAccount);

    int totalTrades = 0;
    int wins = 0;
    int losses = 0;
    double totalR = 0.0;
    double totalUsd = 0.0;

    accountTradesTable->setRowCount(0);

    QSqlQuery q(Database::instance().getDB());
    q.prepare(
        "SELECT date, pair, direction, setup, win_loss, result_r, result_usd, account "
        "FROM trades ORDER BY date DESC, id DESC"
    );

    if (!q.exec()) {
        accountStatsLabel->setText("Could not load account analytics.");
        return;
    }

    while (q.next()) {
        const QStringList accounts = Domain::parseAccounts(q.value(7).toString());
        if (!accounts.contains(selectedAccount)) {
            continue;
        }

        ++totalTrades;
        const QString result = q.value(4).toString().trimmed().toLower();
        if (result == "w" || result == "win") {
            ++wins;
        } else if (result == "l" || result == "loss") {
            ++losses;
        }

        const double resultR = q.value(5).toDouble();
        const double resultUsd = q.value(6).toDouble();
        totalR += resultR;
        totalUsd += resultUsd;

        const int row = accountTradesTable->rowCount();
        accountTradesTable->insertRow(row);
        accountTradesTable->setItem(row, 0, new QTableWidgetItem(q.value(0).toString()));
        accountTradesTable->setItem(row, 1, new QTableWidgetItem(q.value(1).toString()));
        accountTradesTable->setItem(row, 2, new QTableWidgetItem(q.value(2).toString()));
        accountTradesTable->setItem(row, 3, new QTableWidgetItem(q.value(3).toString()));
        accountTradesTable->setItem(row, 4, new QTableWidgetItem(q.value(4).toString()));
        accountTradesTable->setItem(row, 5, new QTableWidgetItem(QString::number(resultR, 'f', 2)));
        accountTradesTable->setItem(row, 6, new QTableWidgetItem(QString::number(resultUsd, 'f', 2)));
        accountTradesTable->setItem(row, 7, new QTableWidgetItem(q.value(7).toString()));
    }

    accountTradesTable->resizeColumnsToContents();

    const double currentBalance = initialBalance + totalUsd;
    const double winRate = totalTrades == 0 ? 0.0 : (static_cast<double>(wins) / static_cast<double>(totalTrades)) * 100.0;
    const QString pnlPrefix = totalUsd >= 0.0 ? "+$" : "-$";
    const QString balancePrefix = currentBalance >= 0.0 ? "$" : "-$";

    const QString analyticsText = QString(
        "%1\n"
        "Starting Balance: $%2\n"
        "Current Balance: %3%4\n"
        "P/L: %5%6\n"
        "Trades: %7 | Wins: %8 | Losses: %9 | Win Rate: %10%\n"
        "Total R: %11"
    )
        .arg(selectedAccount)
        .arg(QString::number(initialBalance, 'f', 2))
        .arg(balancePrefix)
        .arg(QString::number(std::abs(currentBalance), 'f', 2))
        .arg(pnlPrefix)
        .arg(QString::number(std::abs(totalUsd), 'f', 2))
        .arg(totalTrades)
        .arg(wins)
        .arg(losses)
        .arg(QString::number(winRate, 'f', 1))
        .arg(QString::number(totalR, 'f', 2));

    accountStatsLabel->setText(analyticsText);
}

void MainWindow::refreshCalendarView()
{
    if (!calendarTable || !calendarMonthLabel || !calendarYearCombo) {
        return;
    }

    const QDate firstOfMonth(currentCalendarMonth.year(), currentCalendarMonth.month(), 1);
    const int offset = firstOfMonth.dayOfWeek() % 7;
    const QDate firstVisibleDate = firstOfMonth.addDays(-offset);
    const QDate lastVisibleDate = firstVisibleDate.addDays(41);

    calendarMonthLabel->setText(firstOfMonth.toString("MMMM"));
    {
        const QSignalBlocker blocker(calendarYearCombo);
        const int yearIndex = calendarYearCombo->findData(firstOfMonth.year());
        if (yearIndex >= 0) {
            calendarYearCombo->setCurrentIndex(yearIndex);
        }
    }

    QMap<QDate, DailyTradeSummary> summaries;

    QSqlQuery q(Database::instance().getDB());
    q.prepare(
        "SELECT date, COUNT(*), COALESCE(SUM(result_r), 0), COALESCE(SUM(result_usd), 0) "
        "FROM trades WHERE date BETWEEN ? AND ? GROUP BY date"
    );
    q.addBindValue(firstVisibleDate.toString("yyyy-MM-dd"));
    q.addBindValue(lastVisibleDate.toString("yyyy-MM-dd"));

    if (!q.exec()) {
        loadSelectedDaySummary();
        return;
    }

    while (q.next()) {
        const QDate date = QDate::fromString(q.value(0).toString(), "yyyy-MM-dd");
        if (!date.isValid()) {
            continue;
        }

        DailyTradeSummary summary;
        summary.tradeCount = q.value(1).toInt();
        summary.totalR = q.value(2).toDouble();
        summary.totalUsd = q.value(3).toDouble();
        summaries.insert(date, summary);
    }

    const QSignalBlocker blocker(calendarTable);
    calendarTable->clearContents();

    for (int index = 0; index < 42; ++index) {
        const int row = index / 7;
        const int column = index % 7;
        const QDate date = firstVisibleDate.addDays(index);
        const bool inCurrentMonth = date.month() == firstOfMonth.month() && date.year() == firstOfMonth.year();
        const DailyTradeSummary summary = summaries.value(date);

        QStringList lines;
        lines << QString::number(date.day());
        if (summary.tradeCount > 0) {
            lines << QString();
            lines << QString("$%1").arg(QString::number(summary.totalUsd, 'f', 2));
            lines << QString("%1 %2").arg(summary.tradeCount).arg(summary.tradeCount == 1 ? "trade" : "trades");
        }

        auto *item = new QTableWidgetItem(lines.join('\n'));
        item->setData(Qt::UserRole, date);
        item->setTextAlignment(Qt::AlignTop | Qt::AlignLeft);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

        if (!inCurrentMonth) {
            item->setBackground(QColor("#e5e7eb"));
            item->setForeground(QColor("#a1a1aa"));
        } else if (summary.tradeCount == 0) {
            item->setBackground(QColor("#e5e7eb"));
            item->setForeground(QColor("#111827"));
        } else if (summary.totalUsd > 0.0) {
            item->setBackground(QColor("#c7f9cc"));
            item->setForeground(QColor("#111827"));
        } else if (summary.totalUsd < 0.0) {
            item->setBackground(QColor("#f8b4b4"));
            item->setForeground(QColor("#111827"));
        } else {
            item->setBackground(QColor("#dbeafe"));
            item->setForeground(QColor("#111827"));
        }

        QFont font = item->font();
        font.setPointSize(11);
        item->setFont(font);
        calendarTable->setItem(row, column, item);
    }

    QDate desiredSelection = selectedCalendarDate;
    if (!desiredSelection.isValid()) {
        desiredSelection = QDate::currentDate();
    }
    if (desiredSelection.year() != firstOfMonth.year() || desiredSelection.month() != firstOfMonth.month()) {
        desiredSelection = firstOfMonth;
    }
    selectedCalendarDate = desiredSelection;

    for (int row = 0; row < calendarTable->rowCount(); ++row) {
        for (int column = 0; column < calendarTable->columnCount(); ++column) {
            QTableWidgetItem *item = calendarTable->item(row, column);
            if (!item) {
                continue;
            }
            if (item->data(Qt::UserRole).toDate() == selectedCalendarDate) {
                calendarTable->setCurrentCell(row, column);
                loadSelectedDaySummary();
                return;
            }
        }
    }

    loadSelectedDaySummary();
}

void MainWindow::loadSelectedDaySummary()
{
    if (!calendarSummaryLabel) {
        return;
    }

    const QDate selectedDate = selectedCalendarDate;
    const DailyTradeSummary summary = dailySummaryForDate(selectedDate);
    const QString usdPrefix = summary.totalUsd >= 0.0 ? "+$" : "-$";
    const QString rPrefix = summary.totalR >= 0.0 ? "+" : "";

    QString text;
    if (summary.tradeCount == 0) {
        text = QString(
            "%1\nNo trades recorded for this day."
        ).arg(selectedDate.toString("dddd, dd MMM yyyy"));
    } else {
        text = QString(
            "%1\nTrades: %2 | Cumulative R: %3%4 | Cumulative P/L: %5%6"
        )
            .arg(selectedDate.toString("dddd, dd MMM yyyy"))
            .arg(summary.tradeCount)
            .arg(rPrefix)
            .arg(QString::number(summary.totalR, 'f', 2))
            .arg(usdPrefix)
            .arg(QString::number(std::abs(summary.totalUsd), 'f', 2));
    }

    calendarSummaryLabel->setText(text);
}

void MainWindow::changeCalendarMonth(int monthOffset)
{
    currentCalendarMonth = currentCalendarMonth.addMonths(monthOffset);
    currentCalendarMonth = QDate(currentCalendarMonth.year(), currentCalendarMonth.month(), 1);
    selectedCalendarDate = currentCalendarMonth;
    refreshCalendarView();
}

void MainWindow::setCalendarYear(int year)
{
    if (year <= 0 || currentCalendarMonth.year() == year) {
        return;
    }

    currentCalendarMonth = QDate(year, currentCalendarMonth.month(), 1);
    selectedCalendarDate = currentCalendarMonth;
    refreshCalendarView();
}

void MainWindow::handleCalendarCellClicked(int row, int column)
{
    if (!calendarTable) {
        return;
    }

    QTableWidgetItem *item = calendarTable->item(row, column);
    if (!item) {
        return;
    }

    const QDate clickedDate = item->data(Qt::UserRole).toDate();
    if (!clickedDate.isValid()) {
        return;
    }

    selectedCalendarDate = clickedDate;
    if (clickedDate.year() != currentCalendarMonth.year() || clickedDate.month() != currentCalendarMonth.month()) {
        currentCalendarMonth = QDate(clickedDate.year(), clickedDate.month(), 1);
        refreshCalendarView();
        return;
    }

    loadSelectedDaySummary();
}

void MainWindow::updateActionState()
{
    const bool hasSelection = selectedTradeId() >= 0;
    editButton->setEnabled(hasSelection);
    deleteButton->setEnabled(hasSelection);
}

int MainWindow::selectedTradeId() const
{
    if (!tradeTable || !tradeModel || !tradeProxyModel) {
        return -1;
    }

    const QModelIndex currentIndex = tradeTable->currentIndex();
    if (!currentIndex.isValid()) {
        return -1;
    }

    const QModelIndex sourceIndex = tradeProxyModel->mapToSource(currentIndex);
    if (!sourceIndex.isValid()) {
        return -1;
    }

    const int idColumn = tradeModel->fieldIndex("id");
    return tradeModel->data(tradeModel->index(sourceIndex.row(), idColumn)).toInt();
}

void MainWindow::applyFilters()
{
    auto *filterModel = static_cast<TradeFilterModel *>(tradeProxyModel);
    if (!filterModel) {
        return;
    }

    filterModel->setSearchText(searchEdit->text());
    filterModel->setSessionFilter(sessionFilterCombo->currentText());
    filterModel->setSetupFilter(setupFilterCombo->currentText());
    filterModel->setResultFilter(resultFilterCombo->currentText());
    tradeTable->resizeColumnsToContents();
    loadStats();
}

void MainWindow::addTrade()
{
    AddTradeDialog dialog(-1, this);
    if (dialog.exec() == QDialog::Accepted) {
        refreshTrades();
        loadStats();
    }
}

void MainWindow::editSelectedTrade()
{
    const int tradeId = selectedTradeId();
    if (tradeId < 0) {
        return;
    }

    AddTradeDialog dialog(tradeId, this);
    if (dialog.exec() == QDialog::Accepted) {
        refreshTrades();
        loadStats();
    }
}

void MainWindow::deleteSelectedTrade()
{
    const int tradeId = selectedTradeId();
    if (tradeId < 0) {
        return;
    }

    if (QMessageBox::question(this, "Delete Trade", "Delete the selected trade entry?") != QMessageBox::Yes) {
        return;
    }

    QSqlQuery q(Database::instance().getDB());
    q.prepare("DELETE FROM trades WHERE id = ?");
    q.addBindValue(tradeId);

    if (!q.exec()) {
        QMessageBox::critical(this, "Delete Failed", q.lastError().text());
        return;
    }

    refreshTrades();
    loadStats();
}

void MainWindow::exportTradesToCsv()
{
    const QString filePath = QFileDialog::getSaveFileName(
        this,
        "Export Trades",
        "trades.csv",
        "CSV Files (*.csv)"
    );

    if (filePath.isEmpty()) {
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QMessageBox::critical(this, "Export Failed", "Could not write the selected CSV file.");
        return;
    }

    QTextStream stream(&file);
    QStringList headers;
    for (int column = 0; column < tradeProxyModel->columnCount(); ++column) {
        if (tradeTable->isColumnHidden(column)) {
            continue;
        }

        headers << csvEscape(tradeProxyModel->headerData(column, Qt::Horizontal).toString());
    }
    stream << headers.join(',') << '\n';

    for (int row = 0; row < tradeProxyModel->rowCount(); ++row) {
        QStringList values;
        for (int column = 0; column < tradeProxyModel->columnCount(); ++column) {
            if (tradeTable->isColumnHidden(column)) {
                continue;
            }

            values << csvEscape(tradeProxyModel->data(tradeProxyModel->index(row, column)).toString());
        }
        stream << values.join(',') << '\n';
    }
}

void MainWindow::handleTableActivated()
{
    if (!tradeModel || !tradeProxyModel) {
        return;
    }

    const QModelIndex currentIndex = tradeTable->currentIndex();
    if (!currentIndex.isValid()) {
        return;
    }

    const int screenshotColumn = tradeModel->fieldIndex("screenshot");
    if (currentIndex.column() == screenshotColumn) {
        const QString filePath = tradeProxyModel->data(currentIndex).toString().trimmed();
        if (filePath.isEmpty()) {
            QMessageBox::information(this, "No Screenshot", "No screenshot path is saved for this trade.");
            return;
        }

        if (!QFileInfo::exists(filePath)) {
            QMessageBox::warning(this, "Missing Screenshot", "The saved screenshot file could not be found.");
            return;
        }

        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(filePath))) {
            QMessageBox::warning(this, "Open Failed", "The screenshot could not be opened.");
        }
        return;
    }

    editSelectedTrade();
}

void MainWindow::pasteExecuteFromTradingView()
{
    const QString text = QGuiApplication::clipboard()->text().trimmed();
    if (text.isEmpty()) {
        QMessageBox::information(this, "Paste from TradingView",
                                 "Clipboard is empty.");
        return;
    }

    QString pair;
    QString direction;
    double entry = 0.0;
    double sl = 0.0;
    double tp = 0.0;
    if (!parseTradingViewText(text, pair, direction, entry, sl, tp)) {
        QMessageBox::warning(this,
                             "Paste from TradingView",
                             "Could not parse text. Expected format: EURUSD BUY E:1.2345 SL:1.2300 TP:1.2400");
        return;
    }

    executePairCombo->setCurrentText(pair);
    executeDirectionCombo->setCurrentText(direction);
    executeEntrySpin->setValue(entry);
    executeSlSpin->setValue(sl);
    executeTpSpin->setValue(tp);
    calculateExecuteLotSizes();
}

void MainWindow::updateExecuteAccountInputs()
{
    const bool funded1 = executeFunded1Check->isChecked();
    const bool funded2 = executeFunded2Check->isChecked();
    const bool live = executeLiveCheck->isChecked();

    executeFunded1RiskSpin->setEnabled(funded1);
    executeFunded2RiskSpin->setEnabled(funded2);
    executeLiveRiskSpin->setEnabled(live);
    calculateExecuteLotSizes();
}

void MainWindow::calculateExecuteLotSizes()
{
    const double entry = executeEntrySpin->value();
    const double sl = executeSlSpin->value();
    const double tp = executeTpSpin->value();
    const QString pair = executePairCombo->currentText().trimmed();

    if (entry <= 0.0 || sl <= 0.0 || tp <= 0.0 || pair.isEmpty()) {
        executeFunded1LotLabel->setText("Lot: -");
        executeFunded2LotLabel->setText("Lot: -");
        executeLiveLotLabel->setText("Lot: -");
        executeLotsSummaryLabel->setText("Enter Pair, Entry, SL and TP.");
        return;
    }

    QStringList lines;
    if (executeFunded1Check->isChecked()) {
        const double lot = LotCalculator::calculateLotSizeFromRiskAmount(
            pair, entry, sl, executeFunded1RiskSpin->value());
        executeFunded1LotLabel->setText(QString("Lot: %1").arg(QString::number(lot, 'f', 2)));
        lines << QString("Funded 1  | Risk: $%1 | Lot: %2")
                 .arg(QString::number(executeFunded1RiskSpin->value(), 'f', 2))
                 .arg(QString::number(lot, 'f', 2));
    } else {
        executeFunded1LotLabel->setText("Lot: -");
    }
    if (executeFunded2Check->isChecked()) {
        const double lot = LotCalculator::calculateLotSizeFromRiskAmount(
            pair, entry, sl, executeFunded2RiskSpin->value());
        executeFunded2LotLabel->setText(QString("Lot: %1").arg(QString::number(lot, 'f', 2)));
        lines << QString("Funded 2  | Risk: $%1 | Lot: %2")
                 .arg(QString::number(executeFunded2RiskSpin->value(), 'f', 2))
                 .arg(QString::number(lot, 'f', 2));
    } else {
        executeFunded2LotLabel->setText("Lot: -");
    }
    if (executeLiveCheck->isChecked()) {
        const double lot = LotCalculator::calculateLotSizeFromRiskAmount(
            pair, entry, sl, executeLiveRiskSpin->value());
        executeLiveLotLabel->setText(QString("Lot: %1").arg(QString::number(lot, 'f', 2)));
        lines << QString("Live      | Risk: $%1 | Lot: %2")
                 .arg(QString::number(executeLiveRiskSpin->value(), 'f', 2))
                 .arg(QString::number(lot, 'f', 2));
    } else {
        executeLiveLotLabel->setText("Lot: -");
    }

    if (lines.isEmpty()) {
        executeLotsSummaryLabel->setText("Select at least one account to calculate lot size.");
        return;
    }

    executeLotsSummaryLabel->setText(lines.join("\n"));
}

void MainWindow::executeTradesToMt5()
{
    const QString pair = executePairCombo->currentText().trimmed().toUpper();
    const QString direction = executeDirectionCombo->currentText().trimmed().toUpper();
    const double entry = executeEntrySpin->value();
    const double sl = executeSlSpin->value();
    const double tp = executeTpSpin->value();

    if (pair.isEmpty() || (direction != "BUY" && direction != "SELL") ||
        entry <= 0.0 || sl <= 0.0 || tp <= 0.0)
    {
        QMessageBox::warning(this,
                             "Execute",
                             "Please provide valid pair, direction, entry, SL and TP.");
        return;
    }

    QVector<SignalWriter::SignalItem> items;
    struct AccountPlan { QString name; double risk; double lot; };
    QVector<AccountPlan> plans;

    const auto addPlan = [&](bool enabled, const QString &name, double risk) {
        if (!enabled) return;
        const double lot = LotCalculator::calculateLotSizeFromRiskAmount(pair, entry, sl, risk);
        if (risk <= 0.0 || lot <= 0.0) return;
        SignalWriter::SignalItem item;
        item.pair = pair;
        item.direction = direction;
        item.entry = entry;
        item.sl = sl;
        item.tp = tp;
        item.lotSize = lot;
        item.account = name;
        items.push_back(item);
        plans.push_back({name, risk, lot});
    };

    addPlan(executeFunded1Check->isChecked(), "Funded 1", executeFunded1RiskSpin->value());
    addPlan(executeFunded2Check->isChecked(), "Funded 2", executeFunded2RiskSpin->value());
    addPlan(executeLiveCheck->isChecked(), "Live", executeLiveRiskSpin->value());

    if (items.isEmpty()) {
        QMessageBox::warning(this,
                             "Execute",
                             "Select at least one account and set a valid risk amount.");
        return;
    }

    QSettings settings("Ledger", "Ledger");
    const QString signalPath = settings.value("mt5SignalPath", "ledger_signal.json").toString();

    QString errorMsg;
    if (!SignalWriter::writeSignalQueue(items, signalPath, errorMsg)) {
        QMessageBox::critical(this,
                              "Execute Failed",
                              QString("Could not write signal queue to:\n%1\n\nError: %2")
                                  .arg(signalPath, errorMsg));
        return;
    }

    const double rr = LotCalculator::calculateRR(entry, sl, tp);
    int journaled = 0;
    for (const AccountPlan &plan : plans) {
        if (insertJournalEntryForExecution(plan.name, pair, direction, entry, sl, tp, rr, plan.lot, plan.risk)) {
            ++journaled;
        }
    }

    refreshTrades();
    loadStats();

    QMessageBox::information(
        this,
        "Executed",
        QString("Signals queued for MT5: %1\nJournal entries added: %2\n\nSignal file:\n%3")
            .arg(items.size())
            .arg(journaled)
            .arg(signalPath));
}

bool MainWindow::insertJournalEntryForExecution(const QString &account,
                                                const QString &pair,
                                                const QString &direction,
                                                double entry,
                                                double sl,
                                                double tp,
                                                double rr,
                                                double lotSize,
                                                double riskAmount)
{
    QSqlQuery q(Database::instance().getDB());
    q.prepare(
        "INSERT INTO trades ("
        "date, session, pair, direction, setup, entry, sl, tp, rr, risk_percent, lot_size, "
        "result_r, result_usd, win_loss, account, screenshot, notes"
        ") VALUES ("
        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?"
        ")"
    );

    const double accountSize = currentBalanceForAccount(account);
    const double riskPercent = accountSize > 0.0 ? (riskAmount / accountSize) * 100.0 : 0.0;
    const QString note = QString("Auto-added from Execute. Risk $%1")
                            .arg(QString::number(riskAmount, 'f', 2));

    q.addBindValue(QDate::currentDate().toString("yyyy-MM-dd"));
    q.addBindValue("-");
    q.addBindValue(pair);
    q.addBindValue(direction);
    q.addBindValue("Simple");
    q.addBindValue(entry);
    q.addBindValue(sl);
    q.addBindValue(tp);
    q.addBindValue(rr);
    q.addBindValue(riskPercent);
    q.addBindValue(lotSize);
    q.addBindValue(0.0);
    q.addBindValue(0.0);
    q.addBindValue("Breakeven");
    q.addBindValue(account);
    q.addBindValue("");
    q.addBindValue(note);

    if (!q.exec()) {
        return false;
    }
    return true;
}

DailyTradeSummary MainWindow::dailySummaryForDate(const QDate &date) const
{
    DailyTradeSummary summary;

    if (!date.isValid()) {
        return summary;
    }

    QSqlQuery q(Database::instance().getDB());
    q.prepare(
        "SELECT COUNT(*), COALESCE(SUM(result_r), 0), COALESCE(SUM(result_usd), 0) "
        "FROM trades WHERE date = ?"
    );
    q.addBindValue(date.toString("yyyy-MM-dd"));

    if (!q.exec() || !q.next()) {
        return summary;
    }

    summary.tradeCount = q.value(0).toInt();
    summary.totalR = q.value(1).toDouble();
    summary.totalUsd = q.value(2).toDouble();
    return summary;
}

void MainWindow::setupSettingsView()
{
    auto *settingsPage = new QWidget(this);
    auto *outerLayout = new QVBoxLayout(settingsPage);
    outerLayout->setContentsMargins(32, 24, 32, 24);
    outerLayout->setSpacing(20);

    auto *settingsTitle = new QLabel("Settings", this);
    settingsTitle->setObjectName("PageTitle");

    auto *themeGroupLabel = new QLabel("Theme", this);
    themeGroupLabel->setStyleSheet("font-size: 15px; font-weight: 700; margin-top: 8px;");

    themeButtonGroup = new QButtonGroup(this);

    const struct {
        Theme::ThemeId id;
        QString icon;
    } themes[] = {
        { Theme::Bloomberg,     "■" },
        { Theme::Glassmorphism, "◈" },
        { Theme::HighContrast,  "◉" },
    };

    auto *cardsLayout = new QHBoxLayout;
    cardsLayout->setSpacing(16);

    for (const auto &t : themes) {
        auto *card = new QWidget(settingsPage);
        card->setObjectName("ThemeCard");
        card->setFixedWidth(280);

        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(16, 14, 16, 14);
        cardLayout->setSpacing(2);

        auto *radioRow = new QHBoxLayout;
        auto *radio = new QRadioButton(t.icon + "  " + Theme::themeName(t.id), card);
        radio->setObjectName("ThemeRadio");
        radio->setChecked(t.id == currentThemeId);

        radioRow->addWidget(radio);
        radioRow->addStretch();

        cardLayout->addLayout(radioRow);

        themeButtonGroup->addButton(radio, static_cast<int>(t.id));
        cardsLayout->addWidget(card);
    }
    cardsLayout->addStretch();

    outerLayout->addWidget(settingsTitle);
    outerLayout->addWidget(themeGroupLabel);
    outerLayout->addLayout(cardsLayout);
    outerLayout->addSpacing(8);

    // ── MT5 Integration ──────────────────────────────────────────────────────
    auto *mt5GroupLabel = new QLabel("MT5 Integration", this);
    mt5GroupLabel->setStyleSheet("font-size: 15px; font-weight: 700; margin-top: 8px;");

    auto *mt5HintLabel = new QLabel(
        "Path where the LedgerBridge.mq5 EA reads the signal file. "
        "Point this to your MT5 terminal's MQL5\\Files\\ folder.", this);
    mt5HintLabel->setWordWrap(true);
    mt5HintLabel->setStyleSheet("color: gray; font-size: 11px;");

    auto *mt5PathEdit  = new QLineEdit(this);
    mt5PathEdit->setPlaceholderText("e.g. C:\\Users\\You\\AppData\\Roaming\\MetaQuotes\\Terminal\\...\\MQL5\\Files\\ledger_signal.json");
    {
        QSettings s("Ledger", "Ledger");
        mt5PathEdit->setText(s.value("mt5SignalPath", "ledger_signal.json").toString());
    }

    auto *mt5BrowseBtn = new QPushButton("Browse", this);
    mt5BrowseBtn->setFixedWidth(80);

    auto *mt5Row = new QHBoxLayout;
    mt5Row->addWidget(mt5PathEdit);
    mt5Row->addWidget(mt5BrowseBtn);

    outerLayout->addWidget(mt5GroupLabel);
    outerLayout->addWidget(mt5HintLabel);
    outerLayout->addLayout(mt5Row);

    // ── Updates section ──────────────────────────────────────────────────────
    auto *updateGroupLabel = new QLabel("Updates", this);
    updateGroupLabel->setStyleSheet("font-size: 15px; font-weight: 700; margin-top: 8px;");

    auto *updateRow = new QHBoxLayout;
    auto *versionLabel = new QLabel(QString("Current version: <b>v%1</b>").arg(APP_VERSION), this);
    auto *updateBtn = new QPushButton("Check for Updates", this);
    updateBtn->setObjectName("PrimaryButton");
    updateBtn->setFixedWidth(180);
    updateRow->addWidget(versionLabel);
    updateRow->addStretch();
    updateRow->addWidget(updateBtn);

    outerLayout->addWidget(updateGroupLabel);
    outerLayout->addLayout(updateRow);

    connect(mt5PathEdit, &QLineEdit::textChanged, this, [](const QString &text) {
        QSettings s("Ledger", "Ledger");
        s.setValue("mt5SignalPath", text);
    });

    connect(mt5BrowseBtn, &QPushButton::clicked, this, [mt5PathEdit]() {
        const QString path = QFileDialog::getSaveFileName(
            nullptr,
            "Select Signal File Location",
            mt5PathEdit->text(),
            "JSON Files (*.json);;All Files (*)");
        if (!path.isEmpty())
            mt5PathEdit->setText(path);
    });

    connect(updateBtn, &QPushButton::clicked, this, [this]() {
        m_updater.checkForUpdates(this);
    });

    outerLayout->addStretch();

    viewTabs->addTab(settingsPage, "Settings");

    connect(themeButtonGroup, &QButtonGroup::idClicked, this, [this](int id) {
        applyTheme(static_cast<Theme::ThemeId>(id));
    });
}

void MainWindow::applyTheme(Theme::ThemeId id)
{
    currentThemeId = id;
    qApp->setStyleSheet(Theme::stylesheetForTheme(id));

    QSettings settings("Ledger", "Ledger");
    settings.setValue("theme", static_cast<int>(id));

    // Sync radio buttons if Settings tab already exists
    if (themeButtonGroup) {
        QAbstractButton *btn = themeButtonGroup->button(static_cast<int>(id));
        if (btn) {
            btn->setChecked(true);
        }
    }
}
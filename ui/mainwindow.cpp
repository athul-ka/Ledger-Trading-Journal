#include "mainwindow.h"
#include "database.h"
#include "stats.h"
#include "addtradedialog.h"

#include <QAbstractItemView>
#include <QColor>
#include <QDate>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QComboBox>
#include <QRegularExpression>
#include <QSortFilterProxyModel>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlTableModel>
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
};

QString csvEscape(const QString &value)
{
    QString escaped = value;
    escaped.replace('"', "\"\"");
    return QString("\"%1\"").arg(escaped);
}

}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    Database::instance().connect();
    currentCalendarMonth = QDate::currentDate().addDays(1 - QDate::currentDate().day());
    selectedCalendarDate = QDate::currentDate();
    setupUI();
}

void MainWindow::setupUI() {
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    viewTabs = new QTabWidget(this);
    auto *tradesPage = new QWidget(this);
    auto *tradesLayout = new QVBoxLayout(tradesPage);
    auto *filterLayout = new QHBoxLayout;
    auto *buttonLayout = new QHBoxLayout;

    auto *title = new QLabel("Trading Journal Dashboard");
    title->setStyleSheet("font-size:20px;font-weight:bold;");

    statsLabel = new QLabel;
    searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText("Search all trade fields...");
    sessionFilterCombo = new QComboBox(this);
    sessionFilterCombo->addItems({"All", "London", "New York", "Asia"});
    resultFilterCombo = new QComboBox(this);
    resultFilterCombo->addItems({"All", "Win", "Loss", "Breakeven"});

    auto *btnAdd = new QPushButton("Add Trade");
    editButton = new QPushButton("Edit Selected");
    deleteButton = new QPushButton("Delete Selected");
    exportButton = new QPushButton("Export CSV");
    tradeTable = new QTableView(this);

    filterLayout->addWidget(new QLabel("Search", this));
    filterLayout->addWidget(searchEdit, 1);
    filterLayout->addWidget(new QLabel("Session", this));
    filterLayout->addWidget(sessionFilterCombo);
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

    layout->addWidget(viewTabs);

    setCentralWidget(central);
    resize(1440, 820);

    setupTradeTable();

    connect(btnAdd, &QPushButton::clicked, this, &MainWindow::addTrade);
    connect(editButton, &QPushButton::clicked, this, &MainWindow::editSelectedTrade);
    connect(deleteButton, &QPushButton::clicked, this, &MainWindow::deleteSelectedTrade);
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::exportTradesToCsv);
    connect(searchEdit, &QLineEdit::textChanged, this, [this]() { applyFilters(); });
    connect(sessionFilterCombo, &QComboBox::currentTextChanged, this, [this]() { applyFilters(); });
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
    calendarTitle->setStyleSheet("font-size:20px;font-weight:bold;");

    auto *previousMonthButton = new QPushButton("Previous", this);
    auto *nextMonthButton = new QPushButton("Next", this);
    calendarMonthLabel = new QLabel(this);
    calendarMonthLabel->setStyleSheet("font-size:18px;font-weight:bold;");
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
    calendarTable->setStyleSheet(
        "QTableWidget { background: white; border: none; }"
        "QHeaderView::section { border: none; padding: 8px 0; font-weight: 600; }"
        "QTableWidget::item { margin: 4px; padding: 10px; border: 1px solid #ffffff; }"
        "QTableWidget::item:selected { border: 2px solid #111827; }"
    );

    for (int row = 0; row < calendarTable->rowCount(); ++row) {
        calendarTable->setRowHeight(row, 110);
    }

    calendarSummaryLabel = new QLabel(this);
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

void MainWindow::loadStats() {
    QString text = QString(
        "Total Trades: %1\n"
        "Wins: %2\n"
        "Losses: %3\n"
        "Win Rate: %4\n"
        "Total R: %5"
    ).arg(Stats::totalTrades())
     .arg(Stats::wins())
     .arg(Stats::losses())
     .arg(Stats::winRate() * 100.0, 0, 'f', 1)
     .arg(Stats::totalR());

    statsLabel->setText(text + "%");
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
    updateActionState();
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
    filterModel->setResultFilter(resultFilterCombo->currentText());
    tradeTable->resizeColumnsToContents();
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
#pragma once
#include <QMainWindow>
#include <QDate>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSortFilterProxyModel;
class QSqlTableModel;
class QTableWidget;
class QTableView;
class QTabWidget;

struct DailyTradeSummary {
    int tradeCount = 0;
    double totalR = 0.0;
    double totalUsd = 0.0;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);

private:
    void setupUI();
    void setupTradeTable();
    void setupCalendarView();
    void loadStats();
    void refreshTrades();
    void refreshCalendarView();
    void loadSelectedDaySummary();
    void changeCalendarMonth(int monthOffset);
    void setCalendarYear(int year);
    void handleCalendarCellClicked(int row, int column);
    void updateActionState();
    int selectedTradeId() const;
    void applyFilters();
    void addTrade();
    void editSelectedTrade();
    void deleteSelectedTrade();
    void exportTradesToCsv();
    void handleTableActivated();
    DailyTradeSummary dailySummaryForDate(const QDate &date) const;

    QLabel *statsLabel = nullptr;
    QLabel *calendarSummaryLabel = nullptr;
    QLabel *calendarMonthLabel = nullptr;
    QLineEdit *searchEdit = nullptr;
    QComboBox *sessionFilterCombo = nullptr;
    QComboBox *resultFilterCombo = nullptr;
    QComboBox *calendarYearCombo = nullptr;
    QTableWidget *calendarTable = nullptr;
    QTabWidget *viewTabs = nullptr;
    QTableView *tradeTable = nullptr;
    QSqlTableModel *tradeModel = nullptr;
    QSortFilterProxyModel *tradeProxyModel = nullptr;
    QPushButton *editButton = nullptr;
    QPushButton *deleteButton = nullptr;
    QPushButton *exportButton = nullptr;
    QDate currentCalendarMonth;
    QDate selectedCalendarDate;
};
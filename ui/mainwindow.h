#pragma once
#include <QMainWindow>
#include <QDate>
#include "theme.h"
#include "updater.h"

class QButtonGroup;
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
    void setupAccountsView();
    void setupSettingsView();
    void applyTheme(Theme::ThemeId id);
    void loadStats();
    void refreshTrades();
    void refreshCalendarView();
    void refreshAccountsView();
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
    QComboBox *setupFilterCombo = nullptr;
    QComboBox *resultFilterCombo = nullptr;
    QComboBox *accountSelectorCombo = nullptr;
    QComboBox *calendarYearCombo = nullptr;
    QTableWidget *calendarTable = nullptr;
    QTableWidget *accountTradesTable = nullptr;
    QTabWidget *viewTabs = nullptr;
    QTableView *tradeTable = nullptr;
    QLabel *accountStatsLabel = nullptr;
    QSqlTableModel *tradeModel = nullptr;
    QSortFilterProxyModel *tradeProxyModel = nullptr;
    QPushButton *editButton = nullptr;
    QPushButton *deleteButton = nullptr;
    QPushButton *exportButton = nullptr;
    QButtonGroup *themeButtonGroup = nullptr;
    Theme::ThemeId currentThemeId = Theme::Bloomberg;    Updater        m_updater;    QDate currentCalendarMonth;
    QDate selectedCalendarDate;
};
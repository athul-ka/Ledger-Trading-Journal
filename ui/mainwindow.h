#pragma once
#include <QMainWindow>
#include <QDate>
#include "theme.h"
#include "updater.h"

class QButtonGroup;
class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
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
    void setupExecuteView();
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
    void pasteExecuteFromTradingView();
    void updateExecuteAccountInputs();
    void calculateExecuteLotSizes();
    void executeTradesToMt5();
    bool insertJournalEntryForExecution(const QString &account,
                                        const QString &pair,
                                        const QString &direction,
                                        double entry,
                                        double sl,
                                        double tp,
                                        double rr,
                                        double lotSize,
                                        double riskAmount);
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

    // Execute tab widgets
    QComboBox *executePairCombo = nullptr;
    QComboBox *executeDirectionCombo = nullptr;
    QDoubleSpinBox *executeEntrySpin = nullptr;
    QDoubleSpinBox *executeSlSpin = nullptr;
    QDoubleSpinBox *executeTpSpin = nullptr;
    QCheckBox *executeFunded1Check = nullptr;
    QCheckBox *executeFunded2Check = nullptr;
    QCheckBox *executeLiveCheck = nullptr;
    QDoubleSpinBox *executeFunded1RiskSpin = nullptr;
    QDoubleSpinBox *executeFunded2RiskSpin = nullptr;
    QDoubleSpinBox *executeLiveRiskSpin = nullptr;
    QLabel *executeFunded1LotLabel = nullptr;
    QLabel *executeFunded2LotLabel = nullptr;
    QLabel *executeLiveLotLabel = nullptr;
    QLabel *executeLotsSummaryLabel = nullptr;

    QButtonGroup *themeButtonGroup = nullptr;
    Theme::ThemeId currentThemeId = Theme::Bloomberg;
    Updater m_updater;
    QDate currentCalendarMonth;
    QDate selectedCalendarDate;
};
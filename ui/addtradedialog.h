#pragma once
#include <QDialog>

class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QCheckBox;
class QLineEdit;
class QTextEdit;

class AddTradeDialog : public QDialog {
    Q_OBJECT

public:
    explicit AddTradeDialog(int tradeId = -1, QWidget *parent = nullptr);

private:
    void browseScreenshot();
    void loadTrade();
    void saveTrade();
    void pasteFromTradingView();

    QDateEdit *dateEdit = nullptr;
    QComboBox *sessionCombo = nullptr;
    QComboBox *pairCombo = nullptr;
    QComboBox *directionCombo = nullptr;
    QComboBox *setupCombo = nullptr;
    QDoubleSpinBox *entrySpin = nullptr;
    QDoubleSpinBox *slSpin = nullptr;
    QDoubleSpinBox *tpSpin = nullptr;
    QDoubleSpinBox *rrSpin = nullptr;
    QDoubleSpinBox *riskPercentSpin = nullptr;
    QDoubleSpinBox *lotSizeSpin = nullptr;
    QDoubleSpinBox *resultRSpin = nullptr;
    QDoubleSpinBox *resultUsdSpin = nullptr;
    QComboBox *winLossCombo = nullptr;
    QCheckBox *funded1Check = nullptr;
    QCheckBox *funded2Check = nullptr;
    QCheckBox *liveCheck = nullptr;
    QLineEdit *screenshotEdit = nullptr;
    QTextEdit *notesEdit = nullptr;
    int tradeId = -1;
};
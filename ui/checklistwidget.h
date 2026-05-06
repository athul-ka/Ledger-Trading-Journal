#pragma once

#include <QWidget>

class QCheckBox;
class QLabel;
class QProgressBar;

class ChecklistWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChecklistWidget(QWidget *parent = nullptr);

private slots:
    void onCheckChanged();
    void resetAll();

private:
    void updateScore();

    // 3 HTF conditions
    QCheckBox *m_htf[3] = {};
    // 3 MTF conditions
    QCheckBox *m_mtf[3] = {};
    // 3 LTF conditions
    QCheckBox *m_ltf[3] = {};

    QLabel       *m_scoreLabel  = nullptr;
    QLabel       *m_setupLabel  = nullptr;
    QProgressBar *m_progressBar = nullptr;
};

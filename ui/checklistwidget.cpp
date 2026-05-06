#include "checklistwidget.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtMath>

// ── Scoring weights ──────────────────────────────────────────────────────────
//  HTF  3 items × (50/3) pts each  = 50 pts total  → Simple Setup
//  MTF  3 items × (25/3) pts each  = 25 pts total  → + 2-Step Setup
//  LTF  3 items × (25/3) pts each  = 25 pts total  → = Perfect Setup
// ─────────────────────────────────────────────────────────────────────────────

ChecklistWidget::ChecklistWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(32, 24, 32, 24);
    root->setSpacing(20);

    // ── Page title ────────────────────────────────────────────────────────────
    auto *title = new QLabel("Trade Checklist", this);
    title->setObjectName("PageTitle");
    root->addWidget(title);

    // ── Score card ────────────────────────────────────────────────────────────
    auto *scoreCard = new QWidget(this);
    scoreCard->setObjectName("ScoreCard");
    scoreCard->setStyleSheet(
        "QWidget#ScoreCard { border: 1px solid rgba(255,255,255,0.12); "
        "border-radius: 10px; background: rgba(255,255,255,0.04); }");

    auto *cardLayout = new QHBoxLayout(scoreCard);
    cardLayout->setContentsMargins(28, 18, 28, 18);
    cardLayout->setSpacing(24);

    // Left: big score number + setup name + progress bar
    auto *scoreColumn = new QVBoxLayout();
    scoreColumn->setSpacing(6);

    m_scoreLabel = new QLabel("0 pts", this);
    m_scoreLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_setupLabel = new QLabel("Not Ready", this);
    m_setupLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(8);

    scoreColumn->addWidget(m_scoreLabel);
    scoreColumn->addWidget(m_setupLabel);
    scoreColumn->addWidget(m_progressBar);

    cardLayout->addLayout(scoreColumn, 1);

    // Right: legend
    auto *legendLayout = new QVBoxLayout();
    legendLayout->setSpacing(4);

    auto addLegend = [&](const QString &color, const QString &text) {
        auto *row = new QHBoxLayout();
        auto *dot = new QLabel("●", this);
        dot->setStyleSheet(QString("color: %1; font-size: 10px;").arg(color));
        auto *lbl = new QLabel(text, this);
        lbl->setStyleSheet("font-size: 12px; opacity: 0.75;");
        row->addWidget(dot);
        row->addWidget(lbl);
        row->addStretch();
        legendLayout->addLayout(row);
    };

    addLegend("#ef5350", "< 50 pts  —  Not ready");
    addLegend("#ffab00", "50 pts  —  Simple Setup (HTF)");
    addLegend("#448aff", "75 pts  —  2-Step Setup (HTF + MTF)");
    addLegend("#00c853", "100 pts  —  Perfect Setup (all TFs)");

    cardLayout->addLayout(legendLayout, 1);

    // Reset button
    auto *resetBtn = new QPushButton("Reset All", this);
    resetBtn->setFixedWidth(110);
    connect(resetBtn, &QPushButton::clicked, this, &ChecklistWidget::resetAll);
    cardLayout->addWidget(resetBtn, 0, Qt::AlignVCenter);

    root->addWidget(scoreCard);

    // ── Timeframe group boxes ─────────────────────────────────────────────────
    struct TFDef {
        const char  *groupTitle;
        const char  *pts;
        const char  *items[3];
        QCheckBox  **checks;
    } tfs[] = {
        {
            "HTF",
            "50 pts",
            {
                "1.  BOS in HTF",
                "2.  OB with FVG as demand zone",
                "3.  Liquidity sweep before BOS\n     or left / right leg liquidity"
            },
            m_htf
        },
        {
            "MTF",
            "25 pts",
            {
                "4.  CHOCH in MTF from HTF demand zone",
                "5.  OB with FVG as demand zone",
                "6.  Liquidity sweep before CHOCH\n     or left / right leg liquidity"
            },
            m_mtf
        },
        {
            "LTF",
            "25 pts",
            {
                "7.  CHOCH in LTF from MTF demand zone",
                "8.  OB with FVG as demand zone",
                "9.  Liquidity sweep before CHOCH\n     or left / right leg liquidity"
            },
            m_ltf
        }
    };

    auto *tfRow = new QHBoxLayout();
    tfRow->setSpacing(16);

    for (auto &tf : tfs) {
        auto *box = new QGroupBox(
            QString("%1  ·  %2").arg(tf.groupTitle, tf.pts), this);
        box->setStyleSheet(
            "QGroupBox { font-size: 13px; font-weight: 700; "
            "border: 1px solid rgba(255,255,255,0.15); border-radius: 8px; "
            "margin-top: 14px; padding-top: 8px; } "
            "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; }");

        auto *vbox = new QVBoxLayout(box);
        vbox->setSpacing(14);
        vbox->setContentsMargins(16, 16, 16, 16);

        for (int i = 0; i < 3; ++i) {
            tf.checks[i] = new QCheckBox(tf.items[i], this);
            tf.checks[i]->setStyleSheet("font-size: 13px;");
            connect(tf.checks[i], &QCheckBox::toggled,
                    this, &ChecklistWidget::onCheckChanged);
            vbox->addWidget(tf.checks[i]);
        }
        vbox->addStretch();
        tfRow->addWidget(box, 1);
    }

    root->addLayout(tfRow, 1);

    // ── Entry note ────────────────────────────────────────────────────────────
    auto *note = new QLabel(
        "Minimum entry requirement: all 3 HTF conditions must be satisfied (Simple Setup · 50 pts).",
        this);
    note->setStyleSheet("font-size: 12px; opacity: 0.6;");
    note->setWordWrap(true);
    root->addWidget(note);

    updateScore();
}

void ChecklistWidget::onCheckChanged()
{
    updateScore();
}

void ChecklistWidget::resetAll()
{
    for (auto *c : m_htf) c->setChecked(false);
    for (auto *c : m_mtf) c->setChecked(false);
    for (auto *c : m_ltf) c->setChecked(false);
}

void ChecklistWidget::updateScore()
{
    int htfCount = 0, mtfCount = 0, ltfCount = 0;
    for (auto *c : m_htf) htfCount += c->isChecked() ? 1 : 0;
    for (auto *c : m_mtf) mtfCount += c->isChecked() ? 1 : 0;
    for (auto *c : m_ltf) ltfCount += c->isChecked() ? 1 : 0;

    const double score = htfCount * (50.0 / 3.0)
                       + mtfCount * (25.0 / 3.0)
                       + ltfCount * (25.0 / 3.0);
    const int scoreInt = qRound(score);

    m_scoreLabel->setText(QString::number(scoreInt) + " pts");
    m_progressBar->setValue(scoreInt);

    const bool htfDone = (htfCount == 3);
    const bool mtfDone = (mtfCount == 3);
    const bool ltfDone = (ltfCount == 3);

    QString setupName;
    QString color;

    if (htfDone && mtfDone && ltfDone) {
        setupName = "Perfect Setup  ✓";
        color     = "#00c853";
    } else if (htfDone && mtfDone) {
        setupName = "2-Step Setup  ✓";
        color     = "#448aff";
    } else if (htfDone) {
        setupName = "Simple Setup  ✓";
        color     = "#ffab00";
    } else {
        setupName = "Not Ready";
        color     = "#ef5350";
    }

    m_setupLabel->setText(setupName);
    m_setupLabel->setStyleSheet(
        QString("color: %1; font-size: 15px; font-weight: 700;").arg(color));
    m_scoreLabel->setStyleSheet(
        QString("color: %1; font-size: 38px; font-weight: 800;").arg(color));
    m_progressBar->setStyleSheet(
        QString("QProgressBar { background: rgba(255,255,255,0.08); border-radius: 4px; } "
                "QProgressBar::chunk { background: %1; border-radius: 4px; }").arg(color));
}

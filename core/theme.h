#pragma once
#include <QString>

namespace Theme {

enum ThemeId {
    Bloomberg    = 0,   // Minimal professional, dark terminal
    Glassmorphism = 1,  // Premium fintech, translucent cards
    HighContrast  = 2   // Ultra-clear for long trading sessions
};

inline QString themeName(ThemeId id)
{
    switch (id) {
    case Bloomberg:     return "Bloomberg Terminal";
    case Glassmorphism: return "Premium Fintech";
    case HighContrast:  return "High-Contrast Performance";
    }
    return {};
}

inline QString themeDescription(ThemeId id)
{
    switch (id) {
    case Bloomberg:     return "Dark terminal aesthetic inspired by professional trading platforms. Amber accents on near-black.";
    case Glassmorphism: return "Deep violet gradient with translucent frosted-glass cards and purple accents.";
    case HighContrast:  return "Pure white background, maximum contrast for long trading sessions. No visual noise.";
    }
    return {};
}

inline QString stylesheetForTheme(ThemeId id)
{
    switch (id) {

    // ─────────────────────────────────────────────────────────────
    //  Bloomberg Terminal
    // ─────────────────────────────────────────────────────────────
    case Bloomberg:
        return R"(
QMainWindow {
    background: #0d1117;
}
QWidget {
    background: transparent;
    color: #e6edf3;
    font-family: "Courier New", monospace;
}
QDialog {
    background: #0d1117;
}
QLabel#PageTitle {
    color: #f0b429;
    font-size: 18px;
    font-weight: 800;
    letter-spacing: 2px;
    padding: 4px 0 8px 0;
    background: transparent;
}
QLabel#StatsCard {
    background: #161b22;
    color: #c9d1d9;
    border: 1px solid #30363d;
    border-left: 3px solid #f0b429;
    border-radius: 4px;
    padding: 10px 14px;
    font-size: 12px;
}
QLineEdit, QComboBox, QDateEdit, QDoubleSpinBox, QTextEdit {
    background: #161b22;
    color: #e6edf3;
    border: 1px solid #30363d;
    border-radius: 3px;
    min-height: 28px;
    padding: 4px 8px;
    selection-background-color: #f0b429;
    selection-color: #000000;
}
QLineEdit:focus, QComboBox:focus, QDateEdit:focus,
QDoubleSpinBox:focus, QTextEdit:focus {
    border: 1px solid #f0b429;
}
QComboBox::drop-down { border: none; width: 20px; }
QCheckBox { color: #c9d1d9; spacing: 8px; background: transparent; }
QCheckBox::indicator {
    width: 14px; height: 14px;
    border: 1px solid #30363d;
    background: #161b22;
    border-radius: 2px;
}
QCheckBox::indicator:checked {
    background: #f0b429;
    border-color: #f0b429;
}
QPushButton {
    background: #21262d;
    color: #c9d1d9;
    border: 1px solid #30363d;
    border-radius: 3px;
    padding: 7px 14px;
    font-weight: 700;
}
QPushButton:hover {
    background: #30363d;
    border-color: #f0b429;
    color: #f0b429;
}
QPushButton:pressed { background: #3d444d; }
QPushButton#PrimaryButton {
    background: #f0b429;
    color: #000000;
    border: 1px solid #d9a020;
    font-weight: 900;
}
QPushButton#PrimaryButton:hover { background: #d9a020; }
QPushButton#PrimaryButton:pressed { background: #b8860b; }
QTabWidget::pane {
    border: 1px solid #30363d;
    border-radius: 4px;
    background: #161b22;
    top: -1px;
}
QTabBar::tab {
    background: #0d1117;
    color: #8b949e;
    border: 1px solid #30363d;
    border-bottom: none;
    border-top-left-radius: 3px;
    border-top-right-radius: 3px;
    padding: 7px 16px;
    margin-right: 2px;
    font-weight: 700;
    letter-spacing: 1px;
}
QTabBar::tab:selected {
    background: #161b22;
    color: #f0b429;
    border-bottom: 2px solid #f0b429;
}
QTableView, QTableWidget {
    background: #0d1117;
    alternate-background-color: #161b22;
    color: #e6edf3;
    border: 1px solid #30363d;
    border-radius: 4px;
    gridline-color: #21262d;
}
QHeaderView::section {
    background: #161b22;
    color: #f0b429;
    border: none;
    border-bottom: 1px solid #30363d;
    border-right: 1px solid #21262d;
    padding: 7px;
    font-weight: 800;
    letter-spacing: 1px;
}
QScrollBar:vertical {
    background: #0d1117;
    width: 8px;
    border: none;
    border-radius: 4px;
}
QScrollBar::handle:vertical {
    background: #30363d;
    border-radius: 4px;
    min-height: 24px;
}
QScrollBar::handle:vertical:hover { background: #f0b429; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; }
QCalendarWidget { background: #0d1117; }
)";

    // ─────────────────────────────────────────────────────────────
    //  Premium Fintech / Glassmorphism
    // ─────────────────────────────────────────────────────────────
    case Glassmorphism:
        return R"(
QMainWindow {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
        stop:0 #0f0c29, stop:0.45 #302b63, stop:1 #1a1a40);
}
QDialog {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
        stop:0 #1a1035, stop:1 #2d2660);
}
QWidget {
    background: transparent;
    color: #e2e8f0;
}
QLabel#PageTitle {
    color: #c4b5fd;
    font-size: 22px;
    font-weight: 800;
    padding: 4px 0 8px 0;
    background: transparent;
}
QLabel#StatsCard {
    background: rgba(255,255,255,0.07);
    color: #e2e8f0;
    border: 1px solid rgba(167,139,250,0.35);
    border-radius: 16px;
    padding: 14px 16px;
    font-size: 13px;
}
QLineEdit, QComboBox, QDateEdit, QDoubleSpinBox, QTextEdit {
    background: rgba(255,255,255,0.09);
    color: #f1f5f9;
    border: 1px solid rgba(167,139,250,0.4);
    border-radius: 10px;
    min-height: 30px;
    padding: 4px 10px;
    selection-background-color: #7c3aed;
    selection-color: #ffffff;
}
QLineEdit:focus, QComboBox:focus, QDateEdit:focus,
QDoubleSpinBox:focus, QTextEdit:focus {
    border: 2px solid #a78bfa;
    background: rgba(255,255,255,0.13);
}
QComboBox::drop-down { border: none; width: 20px; }
QCheckBox { color: #c4b5fd; spacing: 8px; background: transparent; }
QCheckBox::indicator {
    width: 15px; height: 15px;
    border: 1px solid rgba(167,139,250,0.5);
    background: rgba(255,255,255,0.06);
    border-radius: 4px;
}
QCheckBox::indicator:checked {
    background: #7c3aed;
    border-color: #a78bfa;
}
QPushButton {
    background: rgba(255,255,255,0.07);
    color: #c4b5fd;
    border: 1px solid rgba(167,139,250,0.35);
    border-radius: 10px;
    padding: 8px 16px;
    font-weight: 700;
}
QPushButton:hover {
    background: rgba(167,139,250,0.18);
    border-color: #a78bfa;
    color: #ffffff;
}
QPushButton:pressed { background: rgba(167,139,250,0.3); }
QPushButton#PrimaryButton {
    background: qlineargradient(x1:0,y1:0,x2:1,y2:0,
        stop:0 #7c3aed, stop:1 #4f46e5);
    color: #ffffff;
    border: 1px solid #6d28d9;
    font-weight: 800;
}
QPushButton#PrimaryButton:hover {
    background: qlineargradient(x1:0,y1:0,x2:1,y2:0,
        stop:0 #6d28d9, stop:1 #4338ca);
}
QPushButton#PrimaryButton:pressed {
    background: #4338ca;
}
QTabWidget::pane {
    border: 1px solid rgba(167,139,250,0.25);
    border-radius: 14px;
    background: rgba(255,255,255,0.04);
    top: -1px;
}
QTabBar::tab {
    background: rgba(255,255,255,0.04);
    color: #94a3b8;
    border: 1px solid rgba(167,139,250,0.2);
    border-bottom: none;
    border-top-left-radius: 10px;
    border-top-right-radius: 10px;
    padding: 8px 18px;
    margin-right: 4px;
    font-weight: 700;
}
QTabBar::tab:selected {
    background: rgba(167,139,250,0.14);
    color: #a78bfa;
    border-color: rgba(167,139,250,0.5);
}
QTableView, QTableWidget {
    background: rgba(255,255,255,0.04);
    alternate-background-color: rgba(255,255,255,0.07);
    color: #e2e8f0;
    border: 1px solid rgba(167,139,250,0.2);
    border-radius: 12px;
    gridline-color: rgba(255,255,255,0.06);
}
QHeaderView::section {
    background: rgba(167,139,250,0.12);
    color: #c4b5fd;
    border: none;
    border-bottom: 1px solid rgba(167,139,250,0.25);
    border-right: 1px solid rgba(167,139,250,0.08);
    padding: 8px;
    font-weight: 800;
}
QScrollBar:vertical {
    background: transparent;
    width: 6px;
    border: none;
}
QScrollBar::handle:vertical {
    background: rgba(167,139,250,0.4);
    border-radius: 3px;
    min-height: 24px;
}
QScrollBar::handle:vertical:hover { background: #a78bfa; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; }
)";

    // ─────────────────────────────────────────────────────────────
    //  High-Contrast Performance
    // ─────────────────────────────────────────────────────────────
    case HighContrast:
        return R"(
QMainWindow {
    background: #ffffff;
}
QDialog {
    background: #ffffff;
}
QWidget {
    background: #ffffff;
    color: #000000;
}
QLabel#PageTitle {
    color: #000000;
    font-size: 22px;
    font-weight: 900;
    padding: 4px 0 8px 0;
    border-bottom: 3px solid #0047ff;
    background: transparent;
}
QLabel#StatsCard {
    background: #f4f7ff;
    color: #000000;
    border: 2px solid #0047ff;
    border-radius: 8px;
    padding: 10px 14px;
    font-size: 14px;
    font-weight: 600;
}
QLineEdit, QComboBox, QDateEdit, QDoubleSpinBox, QTextEdit {
    background: #ffffff;
    color: #000000;
    border: 2px solid #000000;
    border-radius: 4px;
    min-height: 30px;
    padding: 4px 8px;
    font-weight: 600;
    selection-background-color: #0047ff;
    selection-color: #ffffff;
}
QLineEdit:focus, QComboBox:focus, QDateEdit:focus,
QDoubleSpinBox:focus, QTextEdit:focus {
    border: 2px solid #0047ff;
    background: #eef4ff;
}
QComboBox::drop-down { border: none; width: 20px; }
QCheckBox { color: #000000; spacing: 8px; background: transparent; }
QCheckBox::indicator {
    width: 16px; height: 16px;
    border: 2px solid #000000;
    background: #ffffff;
    border-radius: 2px;
}
QCheckBox::indicator:checked {
    background: #0047ff;
    border-color: #0047ff;
}
QPushButton {
    background: #f0f0f0;
    color: #000000;
    border: 2px solid #000000;
    border-radius: 4px;
    padding: 8px 16px;
    font-weight: 800;
}
QPushButton:hover {
    background: #0047ff;
    color: #ffffff;
    border-color: #0047ff;
}
QPushButton:pressed { background: #0035cc; color: #ffffff; }
QPushButton#PrimaryButton {
    background: #0047ff;
    color: #ffffff;
    border: 2px solid #0035cc;
    font-weight: 900;
}
QPushButton#PrimaryButton:hover { background: #0035cc; }
QPushButton#PrimaryButton:pressed { background: #0022aa; }
QTabWidget::pane {
    border: 2px solid #000000;
    border-radius: 6px;
    background: #ffffff;
    top: -1px;
}
QTabBar::tab {
    background: #f0f0f0;
    color: #444444;
    border: 2px solid #000000;
    border-bottom: none;
    border-top-left-radius: 4px;
    border-top-right-radius: 4px;
    padding: 8px 18px;
    margin-right: 3px;
    font-weight: 800;
}
QTabBar::tab:selected {
    background: #0047ff;
    color: #ffffff;
    border-color: #0035cc;
}
QTableView, QTableWidget {
    background: #ffffff;
    alternate-background-color: #f4f7ff;
    color: #000000;
    border: 2px solid #000000;
    border-radius: 6px;
    gridline-color: #cccccc;
}
QHeaderView::section {
    background: #000000;
    color: #ffffff;
    border: none;
    border-bottom: 2px solid #0047ff;
    border-right: 1px solid #333333;
    padding: 8px;
    font-weight: 900;
    letter-spacing: 1px;
}
QScrollBar:vertical {
    background: #f0f0f0;
    width: 10px;
    border: 1px solid #cccccc;
    border-radius: 5px;
}
QScrollBar::handle:vertical {
    background: #0047ff;
    border-radius: 4px;
    min-height: 24px;
}
QScrollBar::handle:vertical:hover { background: #0035cc; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; }
)";

    } // switch
    return {};
}

} // namespace Theme

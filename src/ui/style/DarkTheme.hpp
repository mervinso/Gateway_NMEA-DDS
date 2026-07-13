// src/ui/style/DarkTheme.hpp
#pragma once

namespace nmea::ui {

// Qt Style Sheet: tema oscuro.
// Paleta: bg=#0f172a panels=#1e293b borders=#334155
//         text=#e2e8f0 accent=#0284c7 ok=#4ade80 warn=#fbbf24 err=#ef4444
inline constexpr const char* kDarkThemeQss = R"(
QMainWindow, QDialog {
    background-color: #0f172a;
}
QWidget {
    background-color: #0f172a;
    color: #e2e8f0;
    font-family: "Segoe UI", "Ubuntu", sans-serif;
    font-size: 12px;
}
QGroupBox {
    background-color: #1e293b;
    border: 1px solid #334155;
    border-radius: 4px;
    margin-top: 44px;
    padding-top: 18px;
}
QGroupBox::title {
    color: #f59e0b;
    font-weight: bold;
    font-size: 11px;
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 10px;
    top: 0px;
    padding: 10px 10px 10px 10px;
    background-color: #1e293b;
}
QComboBox, QLineEdit, QSpinBox, QTextEdit, QPlainTextEdit {
    background-color: #0f172a;
    border: 1px solid #475569;
    border-radius: 3px;
    color: #e2e8f0;
    padding: 3px 6px;
    selection-background-color: #0284c7;
}
QComboBox:focus, QLineEdit:focus, QSpinBox:focus {
    border-color: #0284c7;
}
QPushButton {
    background-color: #334155;
    border: 1px solid #475569;
    border-radius: 3px;
    color: #e2e8f0;
    padding: 4px 12px;
}
QPushButton:hover  { background-color: #475569; }
QPushButton:pressed { background-color: #0284c7; }
QPushButton#btn_connect, QPushButton#btn_launch {
    background-color: #0284c7;
    border-color: #0369a1;
    font-weight: bold;
}
QPushButton#btn_connect:hover, QPushButton#btn_launch:hover {
    background-color: #0369a1;
}
QPushButton#btn_stop { background-color: #7f1d1d; border-color: #991b1b; }
QPushButton#btn_stop:hover { background-color: #991b1b; }
QTableView, QTreeWidget {
    background-color: #0f172a;
    alternate-background-color: #1e293b;
    gridline-color: #334155;
    border: 1px solid #334155;
    selection-background-color: #1e40af;
}
QHeaderView::section {
    background-color: #1e293b;
    border: 1px solid #334155;
    color: #94a3b8;
    font-weight: bold;
    padding: 4px;
}
QScrollBar:vertical {
    background: #1e293b;
    width: 8px;
    border-radius: 4px;
}
QScrollBar::handle:vertical {
    background: #475569;
    border-radius: 4px;
    min-height: 20px;
}
QStatusBar { background-color: #1e293b; color: #94a3b8; }
QSplitter::handle { background-color: #334155; width: 2px; }
QLabel#lbl_ok    { color: #4ade80; }
QLabel#lbl_warn  { color: #fbbf24; }
QLabel#lbl_error { color: #ef4444; }
)";

}  // namespace nmea::ui

#pragma once

#include <QColor>
#include <QString>

namespace eqmax::ui::theme {

// eqMac-inspired dark palette
inline const QColor Window{0x1C, 0x1C, 0x1E};
inline const QColor Card{0x26, 0x26, 0x29};
inline const QColor CardBorder{0x32, 0x32, 0x36};
inline const QColor Control{0x34, 0x34, 0x38};
inline const QColor ControlHover{0x3E, 0x3E, 0x43};
inline const QColor Track{0x3A, 0x3A, 0x3F};
inline const QColor TextPrimary{0xEC, 0xEC, 0xEE};
inline const QColor TextSecondary{0x8E, 0x8E, 0x94};
inline const QColor TextTertiary{0x5E, 0x5E, 0x64};
inline const QColor Accent{0x4F, 0x9C, 0xF9};
inline const QColor AccentDim{0x2E, 0x5C, 0x94};
inline const QColor Danger{0xFF, 0x5F, 0x57};
inline const QColor Success{0x32, 0xD7, 0x4B};

inline QString css(const QColor& color) { return color.name(QColor::HexRgb); }

inline QString globalStyleSheet() {
    return QString(R"(
        QMainWindow, QWidget#central { background: %1; }
        QWidget { color: %2; font-size: 12px; }
        QLabel { background: transparent; }
        QToolTip { background: %3; color: %2; border: 1px solid %4; padding: 4px; }

        QComboBox {
            background: %5; border: 1px solid %4; border-radius: 6px;
            padding: 4px 10px; min-height: 20px; color: %2;
        }
        QComboBox:hover { background: %6; }
        QComboBox::drop-down { border: none; width: 18px; }
        QComboBox QAbstractItemView {
            background: %3; border: 1px solid %4; selection-background-color: %7; color: %2; outline: none;
        }

        QPushButton {
            background: %5; border: 1px solid %4; border-radius: 6px;
            padding: 4px 12px; min-height: 20px; color: %2;
        }
        QPushButton:hover { background: %6; }
        QPushButton:pressed { background: %8; }
        QPushButton:disabled { color: %9; }

        QPushButton[segment="true"] {
            background: transparent; border: none; border-radius: 5px;
            padding: 3px 12px; min-height: 18px; color: %10; font-weight: 600;
        }
        QPushButton[segment="true"]:hover { color: %2; }
        QPushButton[segment="true"]:checked { background: %6; color: %2; }

        QPushButton[flat="true"] { background: transparent; border: none; padding: 2px 6px; color: %10; }
        QPushButton[flat="true"]:hover { color: %2; }

        QSlider::groove:horizontal { background: %11; height: 4px; border-radius: 2px; }
        QSlider::sub-page:horizontal { background: %7; height: 4px; border-radius: 2px; }
        QSlider::handle:horizontal {
            background: #E6E6EA; width: 14px; height: 14px; margin: -5px 0; border-radius: 7px;
        }
        QSlider::handle:horizontal:hover { background: #FFFFFF; }
        QSlider[bipolar="true"]::sub-page:horizontal { background: %11; }

        QScrollArea { background: transparent; border: none; }
        QScrollArea > QWidget > QWidget { background: transparent; }
        QScrollBar:vertical { background: transparent; width: 8px; margin: 2px; }
        QScrollBar::handle:vertical { background: %4; border-radius: 3px; min-height: 30px; }
        QScrollBar:horizontal { background: transparent; height: 8px; margin: 2px; }
        QScrollBar::handle:horizontal { background: %4; border-radius: 3px; min-width: 30px; }
        QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
        QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
    )")
        .arg(css(Window), css(TextPrimary), css(Card), css(CardBorder), css(Control), css(ControlHover),
             css(Accent), css(Track), css(TextTertiary))
        .arg(css(TextSecondary), css(Track));
}

} // namespace eqmax::ui::theme

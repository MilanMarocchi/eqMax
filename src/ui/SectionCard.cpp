#include "SectionCard.hpp"
#include "Theme.hpp"

namespace eqmax::ui {

SectionCard::SectionCard(const QString& title, QWidget* parent)
    : QFrame(parent) {
    setObjectName("sectionCard");
    setStyleSheet(QString("QFrame#sectionCard { background: %1; border: 1px solid %2; border-radius: 10px; }")
                      .arg(theme::css(theme::Card), theme::css(theme::CardBorder)));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 10, 14, 12);
    layout->setSpacing(10);

    headerLayout_ = new QHBoxLayout();
    headerLayout_->setSpacing(6);

    chevron_ = new QToolButton(this);
    chevron_->setArrowType(Qt::DownArrow);
    chevron_->setAutoRaise(true);
    chevron_->setFixedSize(16, 16);
    chevron_->setAccessibleName("Expand or collapse " + title);
    chevron_->setStyleSheet("QToolButton { border: none; background: transparent; }");
    connect(chevron_, &QToolButton::clicked, this, [this]() { setCollapsed(!collapsed_); });
    headerLayout_->addWidget(chevron_);

    auto* titleLabel = new QLabel(title.toUpper(), this);
    titleLabel->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: 700; letter-spacing: 1px;")
                                  .arg(theme::css(theme::TextSecondary)));
    headerLayout_->addWidget(titleLabel);
    headerLayout_->addStretch(1);
    layout->addLayout(headerLayout_);

    content_ = new QWidget(this);
    contentLayout_ = new QVBoxLayout(content_);
    contentLayout_->setContentsMargins(0, 0, 0, 0);
    contentLayout_->setSpacing(10);
    layout->addWidget(content_);
}

void SectionCard::addHeaderWidget(QWidget* widget) {
    headerLayout_->addWidget(widget);
}

void SectionCard::setCollapsed(bool collapsed) {
    if (collapsed_ == collapsed) return;
    collapsed_ = collapsed;
    content_->setVisible(!collapsed);
    chevron_->setArrowType(collapsed ? Qt::RightArrow : Qt::DownArrow);
    chevron_->setToolTip(collapsed ? "Expand section" : "Collapse section");
    emit collapsedChanged(collapsed);
}

} // namespace eqmax::ui

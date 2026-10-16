#pragma once

#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QToolButton>

namespace eqmax::ui {

/**
 * @brief eqMac-style collapsible module card: title row with optional accessory widgets and a content area.
 */
class SectionCard : public QFrame {
    Q_OBJECT
public:
    explicit SectionCard(const QString& title, QWidget* parent = nullptr);
    ~SectionCard() override = default;

    [[nodiscard]] QVBoxLayout* contentLayout() const noexcept { return contentLayout_; }
    void addHeaderWidget(QWidget* widget);
    void setCollapsed(bool collapsed);
    [[nodiscard]] bool isCollapsed() const noexcept { return collapsed_; }

signals:
    void collapsedChanged(bool collapsed);

private:
    QHBoxLayout* headerLayout_{nullptr};
    QWidget* content_{nullptr};
    QVBoxLayout* contentLayout_{nullptr};
    QToolButton* chevron_{nullptr};
    bool collapsed_{false};
};

} // namespace eqmax::ui

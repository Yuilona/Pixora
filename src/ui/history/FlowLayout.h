#pragma once

#include <QLayout>
#include <QList>

namespace pixora {

// 流式布局:子项从左到右排,放不下自动换行,随容器宽度重排。
// 子项一律按 sizeHint 摆放(卡片网格用,项尺寸一致)。
class FlowLayout : public QLayout {
public:
    explicit FlowLayout(QWidget* parent = nullptr, int spacing = 12);
    ~FlowLayout() override;

    void addItem(QLayoutItem* item) override;
    int count() const override;
    QLayoutItem* itemAt(int index) const override;
    QLayoutItem* takeAt(int index) override;
    Qt::Orientations expandingDirections() const override { return {}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override;
    QSize minimumSize() const override;
    QSize sizeHint() const override { return minimumSize(); }
    void setGeometry(const QRect& rect) override;

private:
    int doLayout(const QRect& rect, bool testOnly) const;

    QList<QLayoutItem*> items_;
    int spacing_;
};

} // namespace pixora

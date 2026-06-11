#pragma once

#include "core/annotate/AnnotationItem.h"

#include <QObject>
#include <QUndoStack>

#include <memory>
#include <vector>

namespace pixora {

// 标注文档:持有矢量条目,所有编辑经 QUndoStack 命令执行,
// 天然获得撤销/重做(见 ARCHITECTURE §5.2)。
class AnnotationDocument : public QObject {
    Q_OBJECT
public:
    explicit AnnotationDocument(QObject* parent = nullptr);

    const std::vector<std::unique_ptr<AnnotationItem>>& items() const { return items_; }
    bool isEmpty() const { return items_.empty(); }

    // 所有编辑经撤销栈
    void pushAddItem(std::unique_ptr<AnnotationItem> item);
    void pushRemoveItem(int index);
    // delta 已由 UI 在拖动中实时应用,命令首个 redo 跳过(只记账)
    void pushMoveItem(int index, const QPoint& delta);

    // 几何编辑:新值已由 UI 实时写入条目(同 Move 的首 redo 跳过模式),
    // 提交时只需传"拖拽开始前"的旧值,新值从条目现值捕获。
    void pushSetShapeRect(int index, const QRect& oldRect);
    void pushSetArrow(int index, const QPoint& oldFrom, const QPoint& oldTo);

    // 样式/文字:点击即生效(无实时预览阶段),redo 正常应用
    void pushRestyleItem(int index, const StrokeStyle& style);
    void pushEditText(int index, const QString& text);

    QUndoStack& undoStack() { return undoStack_; }

signals:
    void changed();

private:
    friend class AddItemCommand;
    friend class RemoveItemCommand;
    friend class MoveItemCommand;
    friend class SetShapeRectCommand;
    friend class SetArrowCommand;
    friend class RestyleItemCommand;
    friend class EditTextCommand;

    std::vector<std::unique_ptr<AnnotationItem>> items_;
    QUndoStack undoStack_;
};

} // namespace pixora

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

    // 经撤销栈追加条目
    void pushAddItem(std::unique_ptr<AnnotationItem> item);

    QUndoStack& undoStack() { return undoStack_; }

signals:
    void changed();

private:
    friend class AddItemCommand;

    std::vector<std::unique_ptr<AnnotationItem>> items_;
    QUndoStack undoStack_;
};

} // namespace pixora

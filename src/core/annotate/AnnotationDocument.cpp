#include "core/annotate/AnnotationDocument.h"

#include <utility>

namespace pixora {

namespace {
// 所有权在命令与文档之间往返:redo 时移交文档,undo 时收回。
} // namespace

class AddItemCommand : public QUndoCommand {
public:
    AddItemCommand(AnnotationDocument* doc, std::unique_ptr<AnnotationItem> item)
        : doc_(doc), item_(std::move(item)) {}

    void redo() override {
        doc_->items_.push_back(std::move(item_));
        emit doc_->changed();
    }

    void undo() override {
        item_ = std::move(doc_->items_.back());
        doc_->items_.pop_back();
        emit doc_->changed();
    }

private:
    AnnotationDocument* doc_;
    std::unique_ptr<AnnotationItem> item_;
};

class RemoveItemCommand : public QUndoCommand {
public:
    RemoveItemCommand(AnnotationDocument* doc, int index) : doc_(doc), index_(index) {}

    void redo() override {
        item_ = std::move(doc_->items_[static_cast<size_t>(index_)]);
        doc_->items_.erase(doc_->items_.begin() + index_);
        emit doc_->changed();
    }

    void undo() override {
        doc_->items_.insert(doc_->items_.begin() + index_, std::move(item_));
        emit doc_->changed();
    }

private:
    AnnotationDocument* doc_;
    int index_;
    std::unique_ptr<AnnotationItem> item_;
};

class MoveItemCommand : public QUndoCommand {
public:
    MoveItemCommand(AnnotationDocument* doc, int index, QPoint delta)
        : doc_(doc), index_(index), delta_(delta) {}

    void redo() override {
        if (firstRedo_) {
            firstRedo_ = false; // 拖动中 UI 已实时应用位移
            return;
        }
        doc_->items_[static_cast<size_t>(index_)]->translate(delta_);
        emit doc_->changed();
    }

    void undo() override {
        doc_->items_[static_cast<size_t>(index_)]->translate(-delta_);
        emit doc_->changed();
    }

private:
    AnnotationDocument* doc_;
    int index_;
    QPoint delta_;
    bool firstRedo_ = true;
};

// 矩形/椭圆/马赛克/模糊的几何编辑;拖拽中 UI 已实时写入 rect
class SetShapeRectCommand : public QUndoCommand {
public:
    SetShapeRectCommand(AnnotationDocument* doc, int index, const QRect& oldRect,
                        const QRect& newRect)
        : doc_(doc), index_(index), oldRect_(oldRect), newRect_(newRect) {}

    void redo() override {
        if (firstRedo_) {
            firstRedo_ = false;
            return;
        }
        shape().rect = newRect_;
        emit doc_->changed();
    }

    void undo() override {
        shape().rect = oldRect_;
        emit doc_->changed();
    }

private:
    ShapeItem& shape() {
        return static_cast<ShapeItem&>(*doc_->items_[static_cast<size_t>(index_)]);
    }

    AnnotationDocument* doc_;
    int index_;
    QRect oldRect_;
    QRect newRect_;
    bool firstRedo_ = true;
};

// 箭头端点编辑;拖拽中 UI 已实时写入 from/to
class SetArrowCommand : public QUndoCommand {
public:
    SetArrowCommand(AnnotationDocument* doc, int index, QPoint oldFrom, QPoint oldTo,
                    QPoint newFrom, QPoint newTo)
        : doc_(doc), index_(index), oldFrom_(oldFrom), oldTo_(oldTo), newFrom_(newFrom),
          newTo_(newTo) {}

    void redo() override {
        if (firstRedo_) {
            firstRedo_ = false;
            return;
        }
        apply(newFrom_, newTo_);
    }

    void undo() override { apply(oldFrom_, oldTo_); }

private:
    void apply(const QPoint& from, const QPoint& to) {
        auto& arrow =
            static_cast<ArrowItem&>(*doc_->items_[static_cast<size_t>(index_)]);
        arrow.from = from;
        arrow.to = to;
        emit doc_->changed();
    }

    AnnotationDocument* doc_;
    int index_;
    QPoint oldFrom_;
    QPoint oldTo_;
    QPoint newFrom_;
    QPoint newTo_;
    bool firstRedo_ = true;
};

class RestyleItemCommand : public QUndoCommand {
public:
    RestyleItemCommand(AnnotationDocument* doc, int index, const StrokeStyle& oldStyle,
                       const StrokeStyle& newStyle)
        : doc_(doc), index_(index), oldStyle_(oldStyle), newStyle_(newStyle) {}

    void redo() override { apply(newStyle_); }
    void undo() override { apply(oldStyle_); }

private:
    void apply(const StrokeStyle& style) {
        doc_->items_[static_cast<size_t>(index_)]->setStyle(style);
        emit doc_->changed();
    }

    AnnotationDocument* doc_;
    int index_;
    StrokeStyle oldStyle_;
    StrokeStyle newStyle_;
};

class EditTextCommand : public QUndoCommand {
public:
    EditTextCommand(AnnotationDocument* doc, int index, QString oldText, QString newText)
        : doc_(doc), index_(index), oldText_(std::move(oldText)),
          newText_(std::move(newText)) {}

    void redo() override { apply(newText_); }
    void undo() override { apply(oldText_); }

private:
    void apply(const QString& text) {
        static_cast<TextItem&>(*doc_->items_[static_cast<size_t>(index_)]).text = text;
        emit doc_->changed();
    }

    AnnotationDocument* doc_;
    int index_;
    QString oldText_;
    QString newText_;
};

AnnotationDocument::AnnotationDocument(QObject* parent) : QObject(parent) {}

void AnnotationDocument::pushAddItem(std::unique_ptr<AnnotationItem> item) {
    undoStack_.push(new AddItemCommand(this, std::move(item)));
}

void AnnotationDocument::pushRemoveItem(int index) {
    if (index < 0 || index >= static_cast<int>(items_.size())) {
        return;
    }
    undoStack_.push(new RemoveItemCommand(this, index));
}

void AnnotationDocument::pushMoveItem(int index, const QPoint& delta) {
    if (index < 0 || index >= static_cast<int>(items_.size()) || delta.isNull()) {
        return;
    }
    undoStack_.push(new MoveItemCommand(this, index, delta));
}

namespace {
bool isShapeTool(AnnotationTool tool) {
    return tool == AnnotationTool::Rect || tool == AnnotationTool::Ellipse ||
           tool == AnnotationTool::Mosaic || tool == AnnotationTool::Blur;
}
} // namespace

void AnnotationDocument::pushSetShapeRect(int index, const QRect& oldRect) {
    if (index < 0 || index >= static_cast<int>(items_.size()) ||
        !isShapeTool(items_[static_cast<size_t>(index)]->tool())) {
        return;
    }
    const QRect newRect = static_cast<ShapeItem&>(*items_[static_cast<size_t>(index)]).rect;
    if (newRect == oldRect) {
        return;
    }
    undoStack_.push(new SetShapeRectCommand(this, index, oldRect, newRect));
}

void AnnotationDocument::pushSetArrow(int index, const QPoint& oldFrom,
                                      const QPoint& oldTo) {
    if (index < 0 || index >= static_cast<int>(items_.size()) ||
        items_[static_cast<size_t>(index)]->tool() != AnnotationTool::Arrow) {
        return;
    }
    const auto& arrow = static_cast<ArrowItem&>(*items_[static_cast<size_t>(index)]);
    if (arrow.from == oldFrom && arrow.to == oldTo) {
        return;
    }
    undoStack_.push(new SetArrowCommand(this, index, oldFrom, oldTo, arrow.from, arrow.to));
}

void AnnotationDocument::pushRestyleItem(int index, const StrokeStyle& style) {
    if (index < 0 || index >= static_cast<int>(items_.size())) {
        return;
    }
    const StrokeStyle old = items_[static_cast<size_t>(index)]->style();
    if (old.color == style.color && old.width == style.width) {
        return;
    }
    undoStack_.push(new RestyleItemCommand(this, index, old, style));
}

void AnnotationDocument::pushEditText(int index, const QString& text) {
    if (index < 0 || index >= static_cast<int>(items_.size()) ||
        items_[static_cast<size_t>(index)]->tool() != AnnotationTool::Text) {
        return;
    }
    const QString old = static_cast<TextItem&>(*items_[static_cast<size_t>(index)]).text;
    if (old == text || text.trimmed().isEmpty()) {
        return;
    }
    undoStack_.push(new EditTextCommand(this, index, old, text));
}

} // namespace pixora

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

} // namespace pixora

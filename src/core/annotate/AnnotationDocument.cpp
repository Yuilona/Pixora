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

AnnotationDocument::AnnotationDocument(QObject* parent) : QObject(parent) {}

void AnnotationDocument::pushAddItem(std::unique_ptr<AnnotationItem> item) {
    undoStack_.push(new AddItemCommand(this, std::move(item)));
}

} // namespace pixora

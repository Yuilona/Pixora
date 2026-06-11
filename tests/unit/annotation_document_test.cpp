#include "core/annotate/AnnotationDocument.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>

using namespace pixora;

namespace {

std::unique_ptr<ShapeItem> makeRect(int x) {
    return std::make_unique<ShapeItem>(AnnotationTool::Rect, StrokeStyle{},
                                       QRect(x, 0, 10, 10));
}

} // namespace

TEST_CASE("document add/undo/redo through undo stack", "[annotate]") {
    AnnotationDocument doc;
    CHECK(doc.isEmpty());

    doc.pushAddItem(makeRect(0));
    doc.pushAddItem(makeRect(100));
    REQUIRE(doc.items().size() == 2);

    doc.undoStack().undo();
    REQUIRE(doc.items().size() == 1);
    CHECK(doc.items().front()->bounds() == QRect(0, 0, 10, 10));

    doc.undoStack().redo();
    REQUIRE(doc.items().size() == 2);
    CHECK(doc.items().back()->bounds() == QRect(100, 0, 10, 10));

    doc.undoStack().undo();
    doc.undoStack().undo();
    CHECK(doc.isEmpty());
}

TEST_CASE("document emits changed for every mutation", "[annotate]") {
    AnnotationDocument doc;
    int notified = 0;
    QObject::connect(&doc, &AnnotationDocument::changed, [&notified] { ++notified; });

    doc.pushAddItem(makeRect(0)); // push 触发 redo
    doc.undoStack().undo();
    doc.undoStack().redo();
    CHECK(notified == 3);
}

TEST_CASE("undo branch is discarded after new edit", "[annotate]") {
    AnnotationDocument doc;
    doc.pushAddItem(makeRect(0));
    doc.pushAddItem(makeRect(100));
    doc.undoStack().undo();
    doc.pushAddItem(makeRect(200)); // 丢弃被撤销的分支

    REQUIRE(doc.items().size() == 2);
    CHECK(doc.items().back()->bounds() == QRect(200, 0, 10, 10));
    CHECK_FALSE(doc.undoStack().canRedo());
}

TEST_CASE("item bounds reflect geometry", "[annotate]") {
    const ArrowItem arrow(StrokeStyle{}, QPoint(50, 50), QPoint(10, 20));
    CHECK(arrow.bounds() == QRect(QPoint(10, 20), QPoint(50, 50)).normalized());

    QPolygon pts;
    pts << QPoint(5, 5) << QPoint(30, 8) << QPoint(12, 40);
    const PenItem pen(AnnotationTool::Pen, StrokeStyle{}, pts);
    CHECK(pen.bounds() == QRect(5, 5, 26, 36));
}

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

TEST_CASE("remove item is undoable and restores position in z-order", "[annotate]") {
    AnnotationDocument doc;
    doc.pushAddItem(makeRect(0));
    doc.pushAddItem(makeRect(100));
    doc.pushAddItem(makeRect(200));

    doc.pushRemoveItem(1);
    REQUIRE(doc.items().size() == 2);
    CHECK(doc.items()[1]->bounds() == QRect(200, 0, 10, 10));

    doc.undoStack().undo();
    REQUIRE(doc.items().size() == 3);
    CHECK(doc.items()[1]->bounds() == QRect(100, 0, 10, 10));
}

TEST_CASE("move command: first redo is a no-op, undo/redo apply delta", "[annotate]") {
    AnnotationDocument doc;
    doc.pushAddItem(makeRect(0));

    // UI 在拖动中已实时位移
    doc.items()[0]->translate(QPoint(30, 40));
    doc.pushMoveItem(0, QPoint(30, 40));
    CHECK(doc.items()[0]->bounds() == QRect(30, 40, 10, 10)); // push 不重复应用

    doc.undoStack().undo();
    CHECK(doc.items()[0]->bounds() == QRect(0, 0, 10, 10));
    doc.undoStack().redo();
    CHECK(doc.items()[0]->bounds() == QRect(30, 40, 10, 10));
}

TEST_CASE("shape rect command: first redo no-op, undo/redo swap rects", "[annotate]") {
    AnnotationDocument doc;
    doc.pushAddItem(makeRect(0));

    // UI 在拖拽中已实时写入新几何
    static_cast<ShapeItem&>(*doc.items()[0]).rect = QRect(5, 5, 40, 30);
    doc.pushSetShapeRect(0, QRect(0, 0, 10, 10));
    CHECK(doc.items()[0]->bounds() == QRect(5, 5, 40, 30)); // push 不重复应用

    doc.undoStack().undo();
    CHECK(doc.items()[0]->bounds() == QRect(0, 0, 10, 10));
    doc.undoStack().redo();
    CHECK(doc.items()[0]->bounds() == QRect(5, 5, 40, 30));
}

TEST_CASE("shape rect command: unchanged rect pushes nothing", "[annotate]") {
    AnnotationDocument doc;
    doc.pushAddItem(makeRect(0));
    const int before = doc.undoStack().count();
    doc.pushSetShapeRect(0, QRect(0, 0, 10, 10)); // 新旧一致
    CHECK(doc.undoStack().count() == before);
}

TEST_CASE("arrow command: undo/redo swap endpoints", "[annotate]") {
    AnnotationDocument doc;
    doc.pushAddItem(
        std::make_unique<ArrowItem>(StrokeStyle{}, QPoint(0, 0), QPoint(50, 50)));

    auto& arrow = static_cast<ArrowItem&>(*doc.items()[0]);
    arrow.to = QPoint(80, 20); // UI 已实时拖到新端点
    doc.pushSetArrow(0, QPoint(0, 0), QPoint(50, 50));

    doc.undoStack().undo();
    CHECK(arrow.from == QPoint(0, 0));
    CHECK(arrow.to == QPoint(50, 50));
    doc.undoStack().redo();
    CHECK(arrow.to == QPoint(80, 20));
}

TEST_CASE("restyle command applies immediately and is undoable", "[annotate]") {
    AnnotationDocument doc;
    doc.pushAddItem(makeRect(0));

    StrokeStyle restyled;
    restyled.color = QColor(Qt::blue);
    restyled.width = 8;
    doc.pushRestyleItem(0, restyled); // 点击即生效,redo 正常应用
    CHECK(doc.items()[0]->style().color == QColor(Qt::blue));
    CHECK(doc.items()[0]->style().width == 8);

    doc.undoStack().undo();
    CHECK(doc.items()[0]->style().color == StrokeStyle{}.color);
    CHECK(doc.items()[0]->style().width == StrokeStyle{}.width);
}

TEST_CASE("edit text command swaps text; empty/no-op rejected", "[annotate]") {
    AnnotationDocument doc;
    doc.pushAddItem(std::make_unique<TextItem>(StrokeStyle{}, QPoint(0, 0),
                                               QStringLiteral("old")));

    doc.pushEditText(0, QStringLiteral("new"));
    CHECK(static_cast<TextItem&>(*doc.items()[0]).text == QStringLiteral("new"));
    doc.undoStack().undo();
    CHECK(static_cast<TextItem&>(*doc.items()[0]).text == QStringLiteral("old"));

    const int before = doc.undoStack().count();
    doc.pushEditText(0, QStringLiteral("old")); // 与现值相同
    doc.pushEditText(0, QStringLiteral("   ")); // 空白
    CHECK(doc.undoStack().count() == before);
}

TEST_CASE("item bounds reflect geometry", "[annotate]") {
    const ArrowItem arrow(StrokeStyle{}, QPoint(50, 50), QPoint(10, 20));
    CHECK(arrow.bounds() == QRect(QPoint(10, 20), QPoint(50, 50)).normalized());

    QPolygon pts;
    pts << QPoint(5, 5) << QPoint(30, 8) << QPoint(12, 40);
    const PenItem pen(AnnotationTool::Pen, StrokeStyle{}, pts);
    CHECK(pen.bounds() == QRect(5, 5, 26, 36));
}

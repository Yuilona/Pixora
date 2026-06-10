#include "ui/overlay/SelectionHandles.h"

#include <catch2/catch_test_macros.hpp>

using namespace pixora::SelectionHandles;

namespace {
const QRect kSel(100, 100, 200, 100); // 右 299, 下 199
}

TEST_CASE("handle hit test distinguishes corners, edges, inside, outside", "[handles]") {
    CHECK(hitTest(kSel, {100, 100}) == Hit::TopLeft);
    CHECK(hitTest(kSel, {299, 199}) == Hit::BottomRight);
    CHECK(hitTest(kSel, {200, 100}) == Hit::Top);
    CHECK(hitTest(kSel, {100, 150}) == Hit::Left);
    CHECK(hitTest(kSel, {299, 150}) == Hit::Right);
    CHECK(hitTest(kSel, {200, 199}) == Hit::Bottom);
    CHECK(hitTest(kSel, {200, 150}) == Hit::Inside);
    CHECK(hitTest(kSel, {50, 50}) == Hit::None);
    // 边缘容差(margin 6)
    CHECK(hitTest(kSel, {305, 150}) == Hit::Right);
    CHECK(hitTest(kSel, {306, 150}) == Hit::None);
}

TEST_CASE("empty selection never hits", "[handles]") {
    CHECK(hitTest(QRect(), {0, 0}) == Hit::None);
}

TEST_CASE("resize math applies delta to the dragged edge only", "[handles]") {
    CHECK(resized(kSel, Hit::Right, {10, 99}) == QRect(100, 100, 210, 100));
    CHECK(resized(kSel, Hit::Top, {99, -10}) == QRect(100, 90, 200, 110));
    CHECK(resized(kSel, Hit::BottomRight, {10, 10}) == QRect(100, 100, 210, 110));
    // 拖过对边自动翻转;Qt6 normalized() 保持尺寸语义:|宽| 不变,起点 +1
    const QRect flipped = resized(kSel, Hit::Left, {250, 0});
    CHECK(flipped.left() == 300);
    CHECK(flipped.width() == 50);
}

TEST_CASE("cursor shapes match handle direction", "[handles]") {
    CHECK(cursorFor(Hit::TopLeft) == Qt::SizeFDiagCursor);
    CHECK(cursorFor(Hit::TopRight) == Qt::SizeBDiagCursor);
    CHECK(cursorFor(Hit::Top) == Qt::SizeVerCursor);
    CHECK(cursorFor(Hit::Left) == Qt::SizeHorCursor);
    CHECK(cursorFor(Hit::Inside) == Qt::SizeAllCursor);
    CHECK(cursorFor(Hit::None) == Qt::CrossCursor);
}

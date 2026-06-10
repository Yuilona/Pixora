#include "core/capture/SnipSession.h"

#include <catch2/catch_test_macros.hpp>

using pixora::SnipSession;
using pixora::WindowInfo;

TEST_CASE("hover picks topmost window in z-order", "[session]") {
    SnipSession session(pixora::DesktopSnapshot{});
    // 列表按 Z 序:前者在顶
    session.setWindowCandidates({
        WindowInfo{1, QStringLiteral("front"), QRect(100, 100, 200, 200)},
        WindowInfo{2, QStringLiteral("back"), QRect(0, 0, 800, 600)},
    });

    session.updateHover(QPoint(150, 150)); // 两窗重叠区
    CHECK(session.hoverRect() == QRect(100, 100, 200, 200));

    session.updateHover(QPoint(700, 500)); // 仅命中底层大窗
    CHECK(session.hoverRect() == QRect(0, 0, 800, 600));

    session.updateHover(QPoint(900, 700)); // 命中空白
    CHECK(session.hoverRect().isEmpty());
}

TEST_CASE("hoverChanged emits only on real change", "[session]") {
    SnipSession session(pixora::DesktopSnapshot{});
    session.setWindowCandidates(
        {WindowInfo{1, QStringLiteral("w"), QRect(0, 0, 100, 100)}});

    int notified = 0;
    QObject::connect(&session, &SnipSession::hoverChanged,
                     [&notified](const QRect&) { ++notified; });

    session.updateHover(QPoint(10, 10));
    session.updateHover(QPoint(20, 20)); // 同一窗口,悬停矩形不变
    CHECK(notified == 1);
    session.updateHover(QPoint(500, 500)); // 移出 → 置空
    CHECK(notified == 2);
}

TEST_CASE("confirm and save emit only with a valid selection", "[session]") {
    SnipSession session(pixora::DesktopSnapshot{});
    int confirmedCount = 0;
    int saveCount = 0;
    QObject::connect(&session, &SnipSession::confirmed,
                     [&confirmedCount](const QRect&) { ++confirmedCount; });
    QObject::connect(&session, &SnipSession::saveRequested,
                     [&saveCount](const QRect&) { ++saveCount; });

    session.confirm();
    session.requestSave();
    CHECK(confirmedCount == 0);
    CHECK(saveCount == 0);

    session.setSelection(QRect(10, 10, 50, 50));
    session.confirm();
    session.requestSave();
    CHECK(confirmedCount == 1);
    CHECK(saveCount == 1);
}

TEST_CASE("setSelection normalizes inverted drag rects", "[session]") {
    SnipSession session(pixora::DesktopSnapshot{});
    session.setSelection(QRect(QPoint(100, 100), QPoint(50, 60)));
    CHECK(session.selection() == QRect(QPoint(50, 60), QPoint(100, 100)));
}

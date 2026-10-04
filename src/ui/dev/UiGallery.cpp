#include "ui/dev/UiGallery.h"

#include "core/capture/DesktopSnapshot.h"
#include "core/capture/SnipSession.h"
#include "ui/Theme.h"
#include "ui/editor/AnnotationToolbar.h"
#include "ui/notify/ToastWindow.h"
#include "ui/overlay/Magnifier.h"
#include "ui/overlay/OverlayWindow.h"
#include "ui/scroll/ScrollCaptureBar.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QToolButton>

#include <spdlog/spdlog.h>

#include <memory>

namespace pixora {

namespace {

constexpr QSize kScene(1120, 640); // 截图场景(逻辑像素)
constexpr int kStripH = 200;       // 下方长截图/通知条带

// 示例桌面:左半浅色文档窗,右半深色代码编辑器——HUD 两种典型底色都要好看
QImage sampleDesktop(qreal dpr) {
    QImage img(kScene * dpr, QImage::Format_ARGB32_Premultiplied);
    QPainter p(&img);
    p.scale(dpr, dpr);
    p.setRenderHint(QPainter::Antialiasing);

    const int half = kScene.width() / 2;
    p.fillRect(QRect(0, 0, half, kScene.height()), QColor(0xE9, 0xEC, 0xF1));
    p.fillRect(QRect(30, 30, half - 50, kScene.height() - 60), Qt::white);
    p.fillRect(QRect(30, 30, half - 50, 36), QColor(0xF3, 0xF4, 0xF6));
    p.setFont(theme::font(14, true));
    p.setPen(QColor(0x1F, 0x23, 0x29));
    p.drawText(QRect(48, 84, 400, 28), Qt::AlignVCenter, QStringLiteral("Quarterly report"));
    for (int i = 0; i < 14; ++i) {
        const int w = (i % 4 == 3) ? 260 : 440 - (i * 37) % 90;
        p.fillRect(QRect(48, 130 + i * 26, w, 9), QColor(0xD5, 0xD9, 0xE0));
    }
    p.setBrush(theme::accent());
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(QRect(48, 520, 120, 34), 6, 6);

    p.fillRect(QRect(half, 0, half, kScene.height()), QColor(0x1E, 0x1F, 0x24));
    const QColor code[] = {QColor(0xC6, 0x78, 0xDD), QColor(0x61, 0xAF, 0xEF),
                           QColor(0x98, 0xC3, 0x79), QColor(0xAB, 0xB2, 0xBF)};
    for (int i = 0; i < 22; ++i) {
        int x = half + 40 + (i % 5) * 18;
        for (int t = 0; t < 4; ++t) {
            const int w = 30 + ((i * 7 + t * 13) % 70);
            p.fillRect(QRect(x, 40 + i * 26, w, 9), code[(i + t) % 4]);
            x += w + 10;
        }
    }
    return img;
}

QPixmap grabHidden(QWidget* w) {
    w->setAttribute(Qt::WA_DontShowOnScreen);
    if (!w->isVisible()) {
        w->show();
    }
    return w->grab();
}

} // namespace

bool renderUiGallery(const QString& outPath) {
    const qreal dpr = QGuiApplication::primaryScreen()->devicePixelRatio();
    const QImage desktop = sampleDesktop(dpr);
    const QRect sceneRect(QPoint(0, 0), kScene);

    DesktopSnapshot snapshot({ScreenSnap{desktop, sceneRect, dpr}});
    SnipSession session(snapshot);

    // 截图场景:跨浅/深两区的选区 + 一个矩形标注
    session.setSelection(QRect(300, 150, 520, 300));
    session.setActiveTool(AnnotationTool::Rect);
    session.beginAnnotation(QPoint(360, 200));
    session.updateAnnotation(QPoint(520, 280));
    session.endAnnotation();
    session.setActiveTool(std::nullopt);

    auto overlay = std::make_unique<OverlayWindow>(snapshot.screens().front(), session,
                                                   nullptr, false);
    overlay->setAttribute(Qt::WA_DeleteOnClose, false);
    auto toolbar = std::make_unique<AnnotationToolbar>(session);
    toolbar->setAttribute(Qt::WA_DontShowOnScreen);
    session.notifyInteractionFinished(); // 工具栏定位并"显示"
    if (const auto buttons = toolbar->findChildren<QToolButton*>(); buttons.size() > 2) {
        buttons[2]->click(); // 选中箭头工具,展示 On 态
    }

    const QRect stripRect(0, kScene.height(), kScene.width(), kStripH);
    auto scrollBar = std::make_unique<ScrollCaptureBar>(
        QRect(40, stripRect.top() - 40, 420, 40), QRect(0, 0, kScene.width(), 2000),
        /*autoModeAvailable=*/true);
    auto toast = std::make_unique<ToastWindow>();
    toast->setAttribute(Qt::WA_DontShowOnScreen);
    toast->popup(QStringLiteral("Pixora"),
                 QCoreApplication::translate("main", "Screenshot copied to clipboard"));

    QImage out(QSize(kScene.width(), kScene.height() + kStripH) * dpr,
               QImage::Format_ARGB32_Premultiplied);
    out.setDevicePixelRatio(dpr);
    QPainter p(&out);
    p.drawPixmap(0, 0, grabHidden(overlay.get()));
    p.drawPixmap(toolbar->pos(), grabHidden(toolbar.get()));

    // 放大镜:画在选区外深色区域的一个光标点上
    Magnifier::Context ctx;
    ctx.physicalImage = &desktop;
    ctx.dpr = dpr;
    ctx.cursorLocalLogical = QPoint(880, 90);
    ctx.cursorGlobalLogical = ctx.cursorLocalLogical;
    ctx.widgetSize = kScene;
    Magnifier::draw(p, ctx);

    // 下方条带:中灰底,左长截图控制条、右通知卡
    p.fillRect(stripRect, QColor(0x8A, 0x90, 0x9A));
    p.drawPixmap(scrollBar->pos(), grabHidden(scrollBar.get()));
    const QPixmap toastPm = grabHidden(toast.get());
    p.drawPixmap(stripRect.right() - toastPm.deviceIndependentSize().toSize().width() - 20,
                 stripRect.top() + 20, toastPm);
    p.end();

    if (!out.save(outPath)) {
        spdlog::error("ui gallery: failed to write {}", outPath.toStdString());
        return false;
    }
    spdlog::info("ui gallery written to {}", outPath.toStdString());
    return true;
}

} // namespace pixora

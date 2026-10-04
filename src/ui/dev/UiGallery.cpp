#include "ui/dev/UiGallery.h"

#include "app/HistoryService.h"
#include "app/SettingsService.h"
#include "app/TrayService.h"
#include "core/capture/DesktopSnapshot.h"
#include "core/capture/SnipSession.h"
#include "ui/Theme.h"
#include "ui/editor/AnnotationToolbar.h"
#include "ui/history/HistoryWindow.h"
#include "ui/notify/ToastWindow.h"
#include "ui/overlay/Magnifier.h"
#include "ui/overlay/OverlayWindow.h"
#include "ui/pin/PinWindow.h"
#include "ui/scroll/RegionIndicator.h"
#include "ui/scroll/ScrollCaptureBar.h"
#include "ui/scroll/ScrollPreview.h"
#include "ui/settings/SettingsDialog.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QEnterEvent>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidget>
#include <QMenu>
#include <QTemporaryDir>
#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QUrl>

#include <spdlog/spdlog.h>

#include <memory>

namespace pixora {

namespace {

constexpr QSize kScene(1120, 640); // 截图场景(逻辑像素)
constexpr int kStripH = 560;       // 下方条带:工具激活态工具栏 / 长截图 / 贴图 / 通知

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

// 文档型窗口(设置各页 / 历史 / 托盘菜单)各存一张:<out>-settings-N.png 等。
// 数据全部来自临时目录,不读写用户真实设置与历史。
void renderWindows(const QString& outPath, const QImage& desktop, qreal dpr) {
    const QString base = outPath.endsWith(QLatin1String(".png"), Qt::CaseInsensitive)
                             ? outPath.left(outPath.size() - 4)
                             : outPath;
    QTemporaryDir temp;
    SettingsService settings(temp.filePath(QStringLiteral("gallery.ini")));

    auto dialog = std::make_unique<SettingsDialog>(settings, nullptr);
    dialog->setAttribute(Qt::WA_DeleteOnClose, false);
    dialog->setAttribute(Qt::WA_DontShowOnScreen);
    dialog->show();
    auto* nav = dialog->findChild<QListWidget*>(QStringLiteral("settingsNav"));
    for (int i = 0; nav && i < nav->count(); ++i) {
        nav->setCurrentRow(i);
        dialog->grab().save(QStringLiteral("%1-settings-%2.png").arg(base).arg(i + 1));
    }

    // 历史:今天 3 张、昨天 2 张、三天前 1 张(手写清单,时间可控)
    const QString historyDir = temp.filePath(QStringLiteral("history"));
    QDir().mkpath(historyDir);
    const QDateTime now = QDateTime::currentDateTime();
    const struct {
        int daysAgo;
        QRect crop;
    } samples[] = {
        {0, QRect(300, 150, 520, 300)}, {0, QRect(30, 30, 500, 260)},
        {0, QRect(600, 30, 400, 400)},  {1, QRect(40, 80, 300, 500)},
        {1, QRect(560, 200, 520, 160)}, {3, QRect(100, 300, 700, 300)},
    };
    QJsonArray manifest;
    int seq = 0;
    for (const auto& s : samples) {
        const QString id = QStringLiteral("sample_%1").arg(seq);
        QImage img = desktop.copy(QRect(s.crop.topLeft() * dpr, s.crop.size() * dpr));
        img.save(QStringLiteral("%1/%2.png").arg(historyDir, id));
        QJsonObject o;
        o[QStringLiteral("id")] = id;
        o[QStringLiteral("time")] =
            now.addDays(-s.daysAgo).addSecs(-600 * seq).toString(Qt::ISODate);
        o[QStringLiteral("dpr")] = dpr;
        manifest.append(o);
        ++seq;
    }
    QFile file(historyDir + QStringLiteral("/history.json"));
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(manifest).toJson());
        file.close();
    }
    HistoryService history(&settings, historyDir);
    auto historyWindow = std::make_unique<HistoryWindow>(history, &settings);
    historyWindow->setAttribute(Qt::WA_DeleteOnClose, false);
    historyWindow->setAttribute(Qt::WA_DontShowOnScreen);
    historyWindow->show();
    historyWindow->grab().save(base + QStringLiteral("-history.png"));

    TrayService tray;
    QMenu* menu = tray.menuForPreview();
    menu->setAttribute(Qt::WA_DontShowOnScreen);
    menu->show();
    menu->grab().save(base + QStringLiteral("-tray.png"));
    menu->hide();
}

} // namespace

bool renderUiGallery(const QString& outPath) {
    const qreal dpr = QGuiApplication::primaryScreen()->devicePixelRatio();
    const QImage desktop = sampleDesktop(dpr);
    const QRect sceneRect(QPoint(0, 0), kScene);

    DesktopSnapshot snapshot({ScreenSnap{desktop, sceneRect, dpr}});
    SnipSession session(snapshot);

    // 截图场景:跨浅/深两区的选区 + 一个已选中的矩形标注(展示手柄与情境栏)
    session.setSelection(QRect(300, 150, 520, 300));
    session.setActiveTool(AnnotationTool::Rect);
    session.beginAnnotation(QPoint(360, 200));
    session.updateAnnotation(QPoint(520, 280));
    session.endAnnotation();
    session.setActiveTool(std::nullopt);
    session.selectAnnotationAt(QPoint(360, 240));

    auto overlay = std::make_unique<OverlayWindow>(snapshot.screens().front(), session,
                                                   nullptr, false);
    overlay->setAttribute(Qt::WA_DeleteOnClose, false);
    auto toolbar = std::make_unique<AnnotationToolbar>(session);
    toolbar->setAttribute(Qt::WA_DontShowOnScreen);
    session.notifyInteractionFinished(); // 工具栏定位并"显示"

    // 下方条带的第二个会话:马赛克工具激活(工具 On 态 + 情境栏只剩大小)
    const QRect stripRect(0, kScene.height(), kScene.width(), kStripH);
    const QRect fullRect(0, 0, kScene.width(), kScene.height() + kStripH);
    QImage blank(fullRect.size() * dpr, QImage::Format_ARGB32_Premultiplied);
    blank.fill(QColor(0x8A, 0x90, 0x9A));
    const DesktopSnapshot stripSnapshot({ScreenSnap{blank, fullRect, dpr}});
    SnipSession toolSession(stripSnapshot);
    toolSession.setSelection(QRect(40, stripRect.top() + 10, 760, 20));
    toolSession.setActiveTool(AnnotationTool::Mosaic);
    auto toolToolbar = std::make_unique<AnnotationToolbar>(toolSession);
    toolToolbar->setAttribute(Qt::WA_DontShowOnScreen);
    toolSession.notifyInteractionFinished();

    // 长截图场景:捕获区域(示例内容)+ 区域框 + 右侧预览 + 下方控制条
    const auto physical = [dpr](const QRect& logical) {
        return QRect(logical.topLeft() * dpr, logical.size() * dpr);
    };
    const QRect region(40, stripRect.top() + 170, 360, 230);
    auto indicator = std::make_unique<RegionIndicator>(region);
    indicator->setAttribute(Qt::WA_DeleteOnClose, false);
    auto preview = std::make_unique<ScrollPreview>(region, fullRect);
    preview->setAttribute(Qt::WA_DontShowOnScreen);
    preview->show(); // 先布局,尺寸确定后再喂内容
    QImage tail = desktop.copy(physical(QRect(30, 30, 500, 600)));
    preview->updateContent(tail, 3840);
    auto scrollBar = std::make_unique<ScrollCaptureBar>(region, fullRect,
                                                        /*autoModeAvailable=*/true);
    scrollBar->setProgress(3840);
    scrollBar->setStatus(QCoreApplication::translate(
                             "pixora::ScrollCaptureService",
                             "Keep scrolling; press F1 or Copy to finish"),
                         ScrollCaptureBar::Tone::Good);

    // 贴图:悬停态(关闭按钮 + 主题蓝细边)+ 翻译中角标
    QImage pinImage = desktop.copy(physical(QRect(600, 30, 300, 180)));
    pinImage.setDevicePixelRatio(dpr);
    auto pin = std::make_unique<PinWindow>(pinImage, QPoint(620, stripRect.top() + 170),
                                           nullptr);
    pin->setAttribute(Qt::WA_DeleteOnClose, false);
    pin->setStatusBadge(
        QCoreApplication::translate("pixora::ScreenTextService", "Translating..."));
    QEnterEvent enter(QPointF(20, 20), QPointF(20, 20), QPointF(20, 20));
    QCoreApplication::sendEvent(pin.get(), &enter);

    // 通知:复制(缩略图)+ 保存(点击打开文件夹)
    auto copiedToast = std::make_unique<ToastWindow>();
    copiedToast->setAttribute(Qt::WA_DontShowOnScreen);
    copiedToast->popup(QStringLiteral("Pixora"),
                       QCoreApplication::translate("main", "Screenshot copied to clipboard"),
                       {},
                       desktop.copy(physical(QRect(300, 150, 520, 300))));
    auto savedToast = std::make_unique<ToastWindow>();
    savedToast->setAttribute(Qt::WA_DontShowOnScreen);
    savedToast->popup(QStringLiteral("Pixora"),
                      QCoreApplication::translate("main", "Screenshot saved: %1")
                          .arg(QStringLiteral("Pixora_20261004_153012.png")),
                      QUrl::fromLocalFile(QStringLiteral("C:/Users/Public/Pictures")).toString());

    QImage out(fullRect.size() * dpr, QImage::Format_ARGB32_Premultiplied);
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

    // 下方条带:中灰底
    p.drawImage(stripRect, blank, physical(stripRect));
    p.drawPixmap(toolToolbar->pos(), grabHidden(toolToolbar.get()));
    p.drawImage(region, desktop, physical(QRect(30, 30, region.width(), region.height())));
    p.drawPixmap(indicator->pos(), grabHidden(indicator.get()));
    p.drawPixmap(preview->pos(), grabHidden(preview.get()));
    p.drawPixmap(scrollBar->pos(), grabHidden(scrollBar.get()));
    p.drawPixmap(pin->pos(), grabHidden(pin.get()));

    const QPixmap savedPm = grabHidden(savedToast.get());
    const QPixmap copiedPm = grabHidden(copiedToast.get());
    const QSize savedSize = savedPm.deviceIndependentSize().toSize();
    const QSize copiedSize = copiedPm.deviceIndependentSize().toSize();
    p.drawPixmap(stripRect.right() - savedSize.width() - 10,
                 stripRect.bottom() - savedSize.height() - 10, savedPm);
    p.drawPixmap(stripRect.right() - copiedSize.width() - 10,
                 stripRect.bottom() - savedSize.height() - copiedSize.height() - 4, copiedPm);
    p.end();

    if (!out.save(outPath)) {
        spdlog::error("ui gallery: failed to write {}", outPath.toStdString());
        return false;
    }
    renderWindows(outPath, desktop, dpr);
    spdlog::info("ui gallery written to {}", outPath.toStdString());
    return true;
}

} // namespace pixora

#include "ui/ToolIcons.h"

#include "ui/Theme.h"

#include <QHash>
#include <QIconEngine>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

#include <algorithm>
#include <functional>

namespace pixora::icons {

namespace {

// SVG 着色图标引擎:Fluent SVG 为单色路径,渲染后以 SourceIn 整体染色,
// 一份矢量源即可覆盖常态/禁用/选中各色,且按请求尺寸 × DPR 现渲,
// 不存在固定倍率位图在 125%/150% 缩放下的发虚。
class TintedSvgEngine : public QIconEngine {
public:
    TintedSvgEngine(QString offPath, QString onPath, QColor color)
        : offPath_(std::move(offPath)), onPath_(std::move(onPath)), color_(color) {}

    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode,
               QIcon::State state) override {
        const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
        painter->drawPixmap(rect, scaledPixmap(rect.size(), mode, state, dpr));
    }

    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override {
        return scaledPixmap(size, mode, state, 1.0);
    }

    QPixmap scaledPixmap(const QSize& size, QIcon::Mode mode, QIcon::State state,
                         qreal scale) override {
        const QSize px = size * scale;
        const bool on = state == QIcon::On && !onPath_.isEmpty();
        const QString key = QStringLiteral("%1x%2/%3/%4")
                                .arg(px.width())
                                .arg(px.height())
                                .arg(static_cast<int>(mode))
                                .arg(on);
        if (const auto it = cache_.constFind(key); it != cache_.constEnd()) {
            return *it;
        }

        QPixmap pm(px);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        QSvgRenderer(on ? onPath_ : offPath_).render(&p, QRectF(QPointF(0, 0), px));
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(pm.rect(), colorFor(mode, state));
        p.end();
        pm.setDevicePixelRatio(scale);
        cache_.insert(key, pm);
        return pm;
    }

    QIconEngine* clone() const override { return new TintedSvgEngine(*this); }
    QString key() const override { return QStringLiteral("pixora.tinted-svg"); }

private:
    QColor colorFor(QIcon::Mode mode, QIcon::State state) const {
        if (mode == QIcon::Disabled) {
            // Qt 自动生成的灰化版在深色底上几乎不可辨,改为同色 35% 透明
            QColor c = color_;
            c.setAlphaF(0.35f);
            return c;
        }
        if (state == QIcon::On || mode == QIcon::Selected) {
            return Qt::white; // 选中态落在主题蓝底上
        }
        return color_;
    }

    QString offPath_;
    QString onPath_;
    QColor color_;
    QHash<QString, QPixmap> cache_;
};

QString fluent(const char* name, const char* style = "regular") {
    return QStringLiteral(":/icons/fluent/%1_20_%2.svg")
        .arg(QLatin1String(name), QLatin1String(style));
}

QIcon svgIcon(const char* name, QColor color = theme::hudIcon()) {
    return QIcon(new TintedSvgEngine(fluent(name), QString(), color));
}

// 标注工具:未选中描边造型,选中换实心造型
QIcon toolSvgIcon(const char* name) {
    return QIcon(new TintedSvgEngine(fluent(name), fluent(name, "filled"),
                                     theme::hudIcon()));
}

QIcon paintedIcon(const std::function<void(QPainter&)>& draw) {
    // 逻辑 20x20、2x 渲染;绘制代码用 16 单位坐标系,整体缩放适配
    QPixmap pm(40, 40);
    pm.setDevicePixelRatio(2.0);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(20.0 / 16.0, 20.0 / 16.0);
    draw(p);
    p.end();

    QIcon icon(pm);
    QPixmap dim(40, 40);
    dim.setDevicePixelRatio(2.0);
    dim.fill(Qt::transparent);
    QPainter dp(&dim);
    dp.setOpacity(0.35);
    dp.drawPixmap(0, 0, pm);
    dp.end();
    icon.addPixmap(dim, QIcon::Disabled);
    return icon;
}

} // namespace

QIcon toolIcon(AnnotationTool tool) {
    switch (tool) {
    case AnnotationTool::Rect:
        return toolSvgIcon("rectangle_landscape");
    case AnnotationTool::Ellipse:
        return toolSvgIcon("oval");
    case AnnotationTool::Arrow:
        return toolSvgIcon("arrow_up_right");
    case AnnotationTool::Pen:
        return toolSvgIcon("pen");
    case AnnotationTool::Marker:
        return toolSvgIcon("highlight");
    case AnnotationTool::Text:
        return toolSvgIcon("text_t");
    case AnnotationTool::Badge:
        return toolSvgIcon("number_circle_1");
    case AnnotationTool::Mosaic:
        return toolSvgIcon("mosaic"); // Fluent 无棋盘格造型,按同规格自绘
    case AnnotationTool::Blur:
        return toolSvgIcon("blur");
    }
    return {};
}

QIcon widthIcon(int width) {
    return paintedIcon([width](QPainter& p) {
        p.setPen(QPen(theme::hudIcon(), std::clamp(width * 0.55, 1.2, 4.5),
                      Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(3, 8), QPointF(13, 8));
    });
}

QIcon undoIcon() { return svgIcon("arrow_undo"); }
QIcon redoIcon() { return svgIcon("arrow_redo"); }
QIcon ocrIcon() { return svgIcon("scan_text"); }
QIcon translateIcon() { return svgIcon("translate"); }
QIcon scrollIcon() { return svgIcon("arrow_autofit_down"); }
QIcon pinIcon() { return svgIcon("pin"); }
QIcon saveIcon() { return svgIcon("save"); }
QIcon confirmIcon() { return svgIcon("checkmark", theme::accentHover()); }
QIcon cancelIcon() { return svgIcon("dismiss", theme::danger().lighter(118)); }
QIcon primaryConfirmIcon() { return svgIcon("checkmark", Qt::white); }

} // namespace pixora::icons

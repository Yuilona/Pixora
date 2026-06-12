#include "core/text/ReplaceRenderer.h"

#include <QFont>
#include <QFontMetrics>
#include <QPainter>

#include <algorithm>
#include <vector>

namespace pixora {

namespace {

// 框外 2px 环带采样背景色(各通道取中位数,抗文字像素混入)
QColor sampleBackground(const QImage& image, const QRect& box) {
    std::vector<int> rs, gs, bs;
    auto sample = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= image.width() || y >= image.height()) {
            return;
        }
        const QColor c = image.pixelColor(x, y);
        rs.push_back(c.red());
        gs.push_back(c.green());
        bs.push_back(c.blue());
    };
    const int stepX = std::max(1, box.width() / 24);
    const int stepY = std::max(1, box.height() / 8);
    for (int x = box.left(); x <= box.right(); x += stepX) {
        sample(x, box.top() - 2);
        sample(x, box.bottom() + 2);
    }
    for (int y = box.top(); y <= box.bottom(); y += stepY) {
        sample(box.left() - 2, y);
        sample(box.right() + 2, y);
    }
    if (rs.empty()) {
        return Qt::white;
    }
    auto median = [](std::vector<int>& v) {
        std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
        return v[v.size() / 2];
    };
    return QColor(median(rs), median(gs), median(bs));
}

// 框内网格采样,取与背景反差最大的颜色作为文字色;
// 反差不足(纯色区)时按背景亮度退化为黑/白。
QColor sampleTextColor(const QImage& image, const QRect& box, const QColor& bg) {
    QColor best = bg;
    int bestDist = 0;
    const int stepX = std::max(1, box.width() / 24);
    const int stepY = std::max(1, box.height() / 6);
    for (int y = box.top(); y <= box.bottom(); y += stepY) {
        for (int x = box.left(); x <= box.right(); x += stepX) {
            if (x < 0 || y < 0 || x >= image.width() || y >= image.height()) {
                continue;
            }
            const QColor c = image.pixelColor(x, y);
            const int dr = c.red() - bg.red();
            const int dg = c.green() - bg.green();
            const int db = c.blue() - bg.blue();
            const int dist = dr * dr + dg * dg + db * db;
            if (dist > bestDist) {
                bestDist = dist;
                best = c;
            }
        }
    }
    if (bestDist < 3 * 48 * 48) {
        return bg.lightness() > 128 ? QColor(Qt::black) : QColor(Qt::white);
    }
    return best;
}

} // namespace

QImage renderReplacedText(QImage base, const QList<OcrLine>& lines,
                          const QStringList& translations) {
    if (base.isNull()) {
        return base;
    }
    const qreal dpr = base.devicePixelRatio();
    QImage canvas = base.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    canvas.setDevicePixelRatio(1.0); // 框坐标是设备像素,绘制按 1:1 进行

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    const QRect imageRect(QPoint(0, 0), canvas.size());

    const qsizetype count = std::min(lines.size(), qsizetype(translations.size()));
    for (qsizetype i = 0; i < count; ++i) {
        const QString translation = translations[i].trimmed();
        QRect box = lines[i].box.intersected(imageRect);
        if (translation.isEmpty() || box.width() < 6 || box.height() < 6) {
            continue; // 原样保留
        }
        const QColor bg = sampleBackground(canvas, box);
        const QColor fg = sampleTextColor(canvas, box, bg);
        // 多盖 2px,吃掉抗锯齿残边
        painter.fillRect(box.adjusted(-2, -2, 2, 2).intersected(imageRect), bg);

        QFont font = painter.font();
        int pixelSize = std::clamp(qRound(box.height() * 0.72), 8, 200);
        font.setPixelSize(pixelSize);
        // 字号自适应:放不下逐级缩小,到下限仍超宽则省略号截断
        while (pixelSize > 8) {
            font.setPixelSize(pixelSize);
            if (QFontMetrics(font).horizontalAdvance(translation) <= box.width()) {
                break;
            }
            --pixelSize;
        }
        QString display = translation;
        const QFontMetrics metrics(font);
        if (metrics.horizontalAdvance(display) > box.width()) {
            display = metrics.elidedText(display, Qt::ElideRight, box.width());
        }
        painter.setFont(font);
        painter.setPen(fg);
        painter.drawText(box, Qt::AlignLeft | Qt::AlignVCenter, display);
    }
    painter.end();

    canvas.setDevicePixelRatio(dpr);
    return canvas;
}

} // namespace pixora

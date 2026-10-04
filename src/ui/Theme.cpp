#include "ui/Theme.h"

#include <QHash>
#include <QImage>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace pixora::theme {

namespace {

// 一层投影:blur = 模糊半径,dy = 下移量,alpha = 不透明度峰值
struct ShadowLayer {
    int blur;
    int dy;
    int alpha;
};

// 两层叠加:大范围的淡环境影托起卡片,贴边的短影勾出轮廓(Win11 浮层的做法)。
// 总外扩 = blur + |dy| 须 ≤ kShadowMargin,否则被窗口边缘截断
constexpr ShadowLayer kLayers[] = {{10, 3, 90}, {2, 1, 70}};
static_assert(10 + 3 <= kShadowMargin, "shadow exceeds reserved margin");

// 一维盒式模糊(边界外视为透明),原地处理 stride 间隔的 n 个样本
void boxBlur(std::vector<int>& buf, int offset, int stride, int n, int r,
             std::vector<int>& tmp) {
    tmp.resize(static_cast<size_t>(n));
    const int div = 2 * r + 1;
    int sum = 0;
    for (int i = 0; i <= std::min(r, n - 1); ++i) {
        sum += buf[static_cast<size_t>(offset + i * stride)];
    }
    for (int i = 0; i < n; ++i) {
        tmp[static_cast<size_t>(i)] = sum / div;
        const int add = i + r + 1;
        const int sub = i - r;
        if (add < n) {
            sum += buf[static_cast<size_t>(offset + add * stride)];
        }
        if (sub >= 0) {
            sum -= buf[static_cast<size_t>(offset + sub * stride)];
        }
    }
    for (int i = 0; i < n; ++i) {
        buf[static_cast<size_t>(offset + i * stride)] = tmp[static_cast<size_t>(i)];
    }
}

// 生成一层投影图:圆角矩形的 alpha 蒙版,三遍盒式模糊近似高斯。
// 投影纯黑,只需模糊 alpha;返回图四周各外扩 blur 像素。
QImage makeShadow(const QSize& card, qreal radius, int blur, int alpha) {
    const int w = card.width() + 2 * blur;
    const int h = card.height() + 2 * blur;
    QImage mask(w, h, QImage::Format_ARGB32_Premultiplied);
    mask.fill(Qt::transparent);
    {
        QPainter p(&mask);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, alpha));
        p.drawRoundedRect(QRectF(blur, blur, card.width(), card.height()), radius, radius);
    }

    std::vector<int> a(static_cast<size_t>(w) * static_cast<size_t>(h));
    for (int y = 0; y < h; ++y) {
        const auto* row = reinterpret_cast<const quint32*>(mask.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            a[static_cast<size_t>(y * w + x)] = qAlpha(row[x]);
        }
    }
    const int r = std::max(1, blur / 3);
    std::vector<int> tmp;
    for (int pass = 0; pass < 3; ++pass) {
        for (int y = 0; y < h; ++y) {
            boxBlur(a, y * w, 1, w, r, tmp);
        }
        for (int x = 0; x < w; ++x) {
            boxBlur(a, x, w, h, r, tmp);
        }
    }

    QImage out(w, h, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < h; ++y) {
        auto* row = reinterpret_cast<quint32*>(out.scanLine(y));
        for (int x = 0; x < w; ++x) {
            row[x] = qPremultiply(qRgba(0, 0, 0, a[static_cast<size_t>(y * w + x)]));
        }
    }
    return out;
}

} // namespace

void paintShadow(QPainter& p, const QRect& card, qreal radius) {
    // 九宫格:按"能容纳完整圆角 + 模糊衰减"的最小卡片生成一张投影,
    // 四角原样贴、四边拉伸、中心由卡片覆盖。成本与卡片尺寸无关——
    // 贴出几千像素高的长截图也只缓存几十像素见方的小图。
    // 卡片小于最小切片时退化为按实际尺寸整张生成(小图本身就便宜)。
    static QHash<QString, QImage> cache;
    if (cache.size() > 64) {
        cache.clear();
    }
    for (const ShadowLayer& layer : kLayers) {
        const int b = layer.blur;
        const int c = static_cast<int>(std::ceil(radius)) + b + 1; // 角区(卡片内)
        const bool sliced = card.width() > 2 * c && card.height() > 2 * c;
        const QSize tileCard = sliced ? QSize(2 * c + 1, 2 * c + 1) : card.size();
        const QString key = QStringLiteral("%1x%2/%3/%4/%5")
                                .arg(tileCard.width())
                                .arg(tileCard.height())
                                .arg(radius)
                                .arg(b)
                                .arg(layer.alpha);
        auto it = cache.find(key);
        if (it == cache.end()) {
            it = cache.insert(key, makeShadow(tileCard, radius, b, layer.alpha));
        }
        const QImage& img = *it;
        const QRect target = card.adjusted(-b, -b, b, b).translated(0, layer.dy);
        if (!sliced) {
            p.drawImage(target.topLeft(), img);
            continue;
        }
        const int k = b + c;                // 切片角边长(含外扩)
        const int tw = target.width() - 2 * k;  // 拉伸段长度
        const int th = target.height() - 2 * k;
        const int x[] = {target.left(), target.left() + k, target.right() - k + 1};
        const int y[] = {target.top(), target.top() + k, target.bottom() - k + 1};
        const int w[] = {k, tw, k};
        const int h[] = {k, th, k};
        const int sx[] = {0, k, k + 1};
        const int sw[] = {k, 1, k};
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                if (row == 1 && col == 1) {
                    continue; // 中心被卡片盖住
                }
                p.drawImage(QRect(x[col], y[row], w[col], h[row]), img,
                            QRect(sx[col], sx[row], sw[col], sw[row]));
            }
        }
    }
}

} // namespace pixora::theme

#include "app/PinService.h"

#include "ui/pin/PinWindow.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QCursor>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QScreen>
#include <QStandardPaths>
#include <QUuid>

#include <spdlog/spdlog.h>

#include <algorithm>

namespace pixora {

namespace {
constexpr int kSaveDebounceMs = 500;

QString manifestPath(const QString& dir) {
    return dir + QStringLiteral("/pins.json");
}
} // namespace

PinService::PinService(ISystemIntegration* system, QObject* parent)
    : QObject(parent), system_(system) {
    saveTimer_.setSingleShot(true);
    saveTimer_.setInterval(kSaveDebounceMs);
    connect(&saveTimer_, &QTimer::timeout, this, &PinService::saveManifest);
    // 退出时把防抖中的最后一次状态变化落盘
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this] {
        if (saveTimer_.isActive()) {
            saveTimer_.stop();
            saveManifest();
        }
    });
}

QString PinService::storageDir() const {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
           QStringLiteral("/pins");
}

void PinService::pinImage(const QImage& image, const QPoint& topLeftLogical) {
    if (image.isNull()) {
        return;
    }
    auto* pin = new PinWindow(image, topLeftLogical, system_); // WA_DeleteOnClose 自管理
    const QString id =
        QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);

    const QString dir = storageDir();
    if (QDir().mkpath(dir) && image.save(dir + QStringLiteral("/%1.png").arg(id))) {
        attach(id, pin);
    } else {
        spdlog::warn("pin persistence unavailable, pin will not survive restart");
        pins_.push_back(Tracked{QString(), pin}); // 仍跟踪以支持"关闭全部"
    }
    pin->show();
    saveManifest();
    spdlog::info("pinned {}x{} image at ({}, {})", image.width(), image.height(),
                 topLeftLogical.x(), topLeftLogical.y());
}

void PinService::attach(const QString& id, PinWindow* pin) {
    pins_.push_back(Tracked{id, pin});
    connect(pin, &PinWindow::stateChanged, this, [this] { saveTimer_.start(); });
    // 旋转/翻转改变图像本体 → 立刻重存 PNG,清单无需记录变换
    connect(pin, &PinWindow::imageChanged, this, [this, id, pin] {
        pin->image().save(storageDir() + QStringLiteral("/%1.png").arg(id));
    });
    connect(pin, &PinWindow::closedByUser, this, [this](PinWindow* closed) {
        const auto it = std::find_if(pins_.begin(), pins_.end(), [closed](const Tracked& t) {
            return t.win.data() == closed;
        });
        if (it != pins_.end()) {
            if (!it->id.isEmpty()) {
                QFile::remove(storageDir() + QStringLiteral("/%1.png").arg(it->id));
            }
            pins_.erase(it);
        }
        saveManifest();
    });
}

bool PinService::pinFromClipboard() {
    const QImage image = QGuiApplication::clipboard()->image();
    if (image.isNull()) {
        spdlog::info("pin from clipboard: no image in clipboard");
        return false;
    }
    pinImage(image, QCursor::pos() + QPoint(20, 20));
    return true;
}

void PinService::closeAll() {
    // close() 触发 closedByUser → 从 pins_ 与清单移除
    const auto snapshot = pins_;
    int closed = 0;
    for (const Tracked& t : snapshot) {
        if (t.win) {
            t.win->close();
            ++closed;
        }
    }
    spdlog::info("closed {} pin window(s)", closed);
}

void PinService::saveManifest() {
    const QString dir = storageDir();
    if (!QDir().mkpath(dir)) {
        return;
    }
    QJsonArray arr;
    for (const Tracked& t : pins_) {
        if (!t.win || t.id.isEmpty()) {
            continue;
        }
        QJsonObject o;
        o[QStringLiteral("id")] = t.id;
        o[QStringLiteral("x")] = t.win->pos().x();
        o[QStringLiteral("y")] = t.win->pos().y();
        o[QStringLiteral("scale")] = t.win->scale();
        o[QStringLiteral("opacity")] = t.win->windowOpacity();
        o[QStringLiteral("dpr")] = t.win->image().devicePixelRatio();
        o[QStringLiteral("folded")] = t.win->isFolded();
        arr.append(o);
    }
    QSaveFile file(manifestPath(dir));
    if (!file.open(QIODevice::WriteOnly)) {
        spdlog::warn("cannot write pin manifest: {}",
                     file.fileName().toStdString());
        return;
    }
    file.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    file.commit();
}

int PinService::restorePins() {
    const QString dir = storageDir();
    QFile file(manifestPath(dir));
    if (!file.open(QIODevice::ReadOnly)) {
        return 0;
    }
    const QJsonArray arr = QJsonDocument::fromJson(file.readAll()).array();
    const QRect desktop = QGuiApplication::primaryScreen()->virtualGeometry();

    int restored = 0;
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        const QString id = o[QStringLiteral("id")].toString();
        QImage image(dir + QStringLiteral("/%1.png").arg(id));
        if (id.isEmpty() || image.isNull()) {
            continue;
        }
        image.setDevicePixelRatio(o[QStringLiteral("dpr")].toDouble(1.0));

        // 显示器布局可能变了:至少把左上角拉回桌面内
        QPoint pos(o[QStringLiteral("x")].toInt(), o[QStringLiteral("y")].toInt());
        pos.setX(std::clamp(pos.x(), desktop.left(), desktop.right() - 48));
        pos.setY(std::clamp(pos.y(), desktop.top(), desktop.bottom() - 48));

        auto* pin = new PinWindow(image, pos, system_);
        pin->restoreState(o[QStringLiteral("scale")].toDouble(1.0),
                          o[QStringLiteral("opacity")].toDouble(1.0),
                          o[QStringLiteral("folded")].toBool(false));
        attach(id, pin);
        pin->show();
        ++restored;
    }
    saveManifest(); // 丢弃清单中已失效的条目
    if (restored > 0) {
        spdlog::info("restored {} pin(s) from previous session", restored);
    }
    return restored;
}

} // namespace pixora

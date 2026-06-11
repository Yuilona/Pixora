#include "platform/windows/WinElementLocator.h"

#include "platform/windows/WinCoordinates.h"

#include <QDateTime>

#include <spdlog/spdlog.h>

// 不定义 WIN32_LEAN_AND_MEAN:UIAutomation 头依赖 ole2.h 的
// COM 基础声明(interface 宏等),裁剪后报上百个语法错误
#define NOMINMAX
#include <windows.h>

#include <UIAutomation.h>

namespace pixora {

namespace {
constexpr int kMaxDepth = 16;       // 下钻层数上限
constexpr int kMaxSiblings = 128;   // 每层兄弟遍历上限(超大列表防卡)
constexpr qint64 kCacheMs = 200;    // 微缓存有效期
constexpr int kCacheDist = 4;       // 微缓存命中距离(逻辑像素)

bool rectContains(const RECT& r, const POINT& p) {
    return p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom;
}
} // namespace

WinElementLocator::WinElementLocator() {
    // Qt GUI 线程已 OleInitialize(STA),直接建 COM 对象
    const HRESULT hr =
        CoCreateInstance(__uuidof(CUIAutomation), nullptr, CLSCTX_INPROC_SERVER,
                         __uuidof(IUIAutomation), reinterpret_cast<void**>(&automation_));
    if (FAILED(hr) || !automation_) {
        spdlog::warn("UIAutomation unavailable (hr={:#x}), element snap disabled",
                     static_cast<unsigned>(hr));
        automation_ = nullptr;
        return;
    }
    if (FAILED(automation_->get_ControlViewWalker(&walker_)) || !walker_) {
        automation_->Release();
        automation_ = nullptr;
    }
}

WinElementLocator::~WinElementLocator() {
    if (walker_) {
        walker_->Release();
    }
    if (automation_) {
        automation_->Release();
    }
}

QRect WinElementLocator::elementAt(quintptr windowId, const QPoint& globalLogical) {
    if (!automation_ || !windowId) {
        return {};
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (windowId == cacheWindow_ && now - cacheTimeMs_ < kCacheMs &&
        (globalLogical - cachePoint_).manhattanLength() <= kCacheDist) {
        return cacheRect_;
    }

    const QPoint phys = wincoord::logicalToPhysical(globalLogical);
    const POINT pt{phys.x(), phys.y()};

    IUIAutomationElement* current = nullptr;
    if (FAILED(automation_->ElementFromHandle(reinterpret_cast<UIA_HWND>(windowId),
                                              &current)) ||
        !current) {
        return {};
    }

    RECT best{};
    bool hasBest = false;
    {
        RECT r{};
        if (SUCCEEDED(current->get_CurrentBoundingRectangle(&r)) &&
            rectContains(r, pt)) {
            best = r;
            hasBest = true;
        }
    }

    // 控件视图按几何包含关系下钻:每层取首个含点的子元素
    for (int depth = 0; depth < kMaxDepth; ++depth) {
        IUIAutomationElement* child = nullptr;
        if (FAILED(walker_->GetFirstChildElement(current, &child)) || !child) {
            break;
        }
        IUIAutomationElement* match = nullptr;
        int visited = 0;
        while (child && visited++ < kMaxSiblings) {
            RECT r{};
            if (SUCCEEDED(child->get_CurrentBoundingRectangle(&r)) &&
                rectContains(r, pt) && (r.right - r.left) > 4 &&
                (r.bottom - r.top) > 4) {
                match = child;
                best = r;
                hasBest = true;
                break;
            }
            IUIAutomationElement* next = nullptr;
            walker_->GetNextSiblingElement(child, &next);
            child->Release();
            child = next;
        }
        if (!match) {
            if (child) {
                child->Release();
            }
            break;
        }
        current->Release();
        current = match;
    }
    current->Release();

    QRect logical;
    if (hasBest) {
        logical = wincoord::physicalToLogical(
            QRect(best.left, best.top, best.right - best.left, best.bottom - best.top));
    }
    cacheWindow_ = windowId;
    cachePoint_ = globalLogical;
    cacheRect_ = logical;
    cacheTimeMs_ = now;
    return logical;
}

} // namespace pixora

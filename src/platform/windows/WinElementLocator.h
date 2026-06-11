#pragma once

#include "platform/interface/ElementLocator.h"

struct IUIAutomation;
struct IUIAutomationTreeWalker;

namespace pixora {

// UIAutomation 实现:ElementFromHandle 取窗口根元素,
// 沿控件视图按"包围盒含光标点"逐层下钻,取最深元素。
// 创建 COM 失败时 available() 为假,elementAt 恒返回空。
class WinElementLocator : public IElementLocator {
public:
    WinElementLocator();
    ~WinElementLocator() override;

    bool available() const { return automation_ != nullptr; }
    QRect elementAt(quintptr windowId, const QPoint& globalLogical) override;

private:
    IUIAutomation* automation_ = nullptr;
    IUIAutomationTreeWalker* walker_ = nullptr;

    // 微缓存:同窗口近距离点位短时间内直接复用,避免重复树遍历
    quintptr cacheWindow_ = 0;
    QPoint cachePoint_;
    QRect cacheRect_;
    qint64 cacheTimeMs_ = 0;
};

} // namespace pixora

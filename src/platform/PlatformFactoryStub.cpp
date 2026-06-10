// 非 Windows 平台占位(M4 跨平台里程碑补齐):
// 热键暂无实现返回 nullptr,上层降级为"无全局热键";抓屏用跨平台 Qt 实现。
#include "platform/interface/GlobalHotkey.h"
#include "platform/interface/PlatformFactory.h"
#include "platform/shared/QtScreenCapturer.h"

namespace pixora {

std::unique_ptr<IGlobalHotkey> createGlobalHotkey() {
    return nullptr;
}

std::unique_ptr<IScreenCapturer> createScreenCapturer() {
    return std::make_unique<QtScreenCapturer>();
}

} // namespace pixora

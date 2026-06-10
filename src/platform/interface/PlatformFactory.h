#pragma once

#include <memory>

namespace pixora {

class IGlobalHotkey;
class IScreenCapturer;

// 按里程碑逐步扩展为完整 PlatformBundle(见 ARCHITECTURE §4.1)。
// 当前平台无实现时返回 nullptr,上层须降级处理(隐藏入口/提示)。
std::unique_ptr<IGlobalHotkey> createGlobalHotkey();
std::unique_ptr<IScreenCapturer> createScreenCapturer();

} // namespace pixora

#pragma once

#include <memory>

namespace pixora {

class IGlobalHotkey;
class IScreenCapturer;
class ISystemIntegration;
class IWindowEnumerator;

// 按里程碑逐步扩展为完整 PlatformBundle(见 ARCHITECTURE §4.1)。
// 当前平台无实现时返回 nullptr,上层须降级处理(隐藏入口/提示)。
std::unique_ptr<IGlobalHotkey> createGlobalHotkey();
std::unique_ptr<IScreenCapturer> createScreenCapturer();
std::unique_ptr<IWindowEnumerator> createWindowEnumerator();
std::unique_ptr<ISystemIntegration> createSystemIntegration();

} // namespace pixora

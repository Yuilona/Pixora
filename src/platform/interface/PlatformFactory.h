#pragma once

#include <memory>

namespace pixora {

class IGlobalHotkey;

// M0 仅提供热键;后续里程碑扩展为完整 PlatformBundle(见 ARCHITECTURE §4.1)。
// 当前平台无实现时返回 nullptr,上层须降级处理(隐藏入口/提示)。
std::unique_ptr<IGlobalHotkey> createGlobalHotkey();

} // namespace pixora

// M0 占位:非 Windows 平台暂无热键实现(M4 跨平台里程碑补齐),
// 返回 nullptr,上层降级为"无全局热键"。
#include "platform/interface/GlobalHotkey.h"
#include "platform/interface/PlatformFactory.h"

namespace pixora {

std::unique_ptr<IGlobalHotkey> createGlobalHotkey() {
    return nullptr;
}

} // namespace pixora

#include "platform/interface/PlatformFactory.h"

#include "platform/windows/WinGlobalHotkey.h"

namespace pixora {

std::unique_ptr<IGlobalHotkey> createGlobalHotkey() {
    return std::make_unique<WinGlobalHotkey>();
}

} // namespace pixora

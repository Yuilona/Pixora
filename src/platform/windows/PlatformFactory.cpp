#include "platform/interface/PlatformFactory.h"

#include "platform/shared/QtScreenCapturer.h"
#include "platform/windows/WinGlobalHotkey.h"

namespace pixora {

std::unique_ptr<IGlobalHotkey> createGlobalHotkey() {
    return std::make_unique<WinGlobalHotkey>();
}

std::unique_ptr<IScreenCapturer> createScreenCapturer() {
    return std::make_unique<QtScreenCapturer>();
}

} // namespace pixora

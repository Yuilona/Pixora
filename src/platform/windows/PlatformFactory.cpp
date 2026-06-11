#include "platform/interface/PlatformFactory.h"

#include "platform/shared/QtScreenCapturer.h"
#include "platform/windows/WinGlobalHotkey.h"
#include "platform/windows/WinInputInjector.h"
#include "platform/windows/WinSystemIntegration.h"
#include "platform/windows/WinWindowEnumerator.h"

namespace pixora {

std::unique_ptr<IGlobalHotkey> createGlobalHotkey() {
    return std::make_unique<WinGlobalHotkey>();
}

std::unique_ptr<IScreenCapturer> createScreenCapturer() {
    return std::make_unique<QtScreenCapturer>();
}

std::unique_ptr<IWindowEnumerator> createWindowEnumerator() {
    return std::make_unique<WinWindowEnumerator>();
}

std::unique_ptr<ISystemIntegration> createSystemIntegration() {
    return std::make_unique<WinSystemIntegration>();
}

std::unique_ptr<IInputInjector> createInputInjector() {
    return std::make_unique<WinInputInjector>();
}

} // namespace pixora

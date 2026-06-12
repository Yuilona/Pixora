#include "app/HotkeyService.h"
#include "app/SettingsService.h"
#include "platform/interface/GlobalHotkey.h"

#include <QStringList>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

namespace {

// 可编程假后端:按预设结果应答注册
class FakeHotkeyBackend : public pixora::IGlobalHotkey {
public:
    bool acceptCapture = true;
    bool acceptPin = true;
    int unregisterCalls = 0;

    bool registerHotkey(pixora::HotkeyId id, const QKeySequence&) override {
        return id == pixora::HotkeyId::CaptureRegion ? acceptCapture : acceptPin;
    }
    void unregisterAll() override { ++unregisterCalls; }
};

} // namespace

TEST_CASE("hotkey registration failure is tracked and signalled", "[hotkey]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    pixora::SettingsService settings(dir.filePath("test.ini"));
    FakeHotkeyBackend backend;
    backend.acceptCapture = false;
    pixora::HotkeyService service(settings, &backend);

    QStringList failedActions;
    QObject::connect(&service, &pixora::HotkeyService::registrationFailed,
                     [&failedActions](const QString& action, const QKeySequence&) {
                         failedActions << action;
                     });

    service.registerAll();
    CHECK(service.failed(pixora::HotkeyId::CaptureRegion));
    CHECK_FALSE(service.failed(pixora::HotkeyId::PinFromClipboard));
    // 动作名经 tr() 翻译;测试无翻译器,得到英文源文
    REQUIRE(failedActions == QStringList{QStringLiteral("capture")});

    // 改键成功后重注册,失败状态清除
    backend.acceptCapture = true;
    service.reregisterAll();
    CHECK_FALSE(service.failed(pixora::HotkeyId::CaptureRegion));
    CHECK(backend.unregisterCalls == 1);
}

TEST_CASE("hotkey service without backend reports no failures", "[hotkey]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    pixora::SettingsService settings(dir.filePath("test.ini"));
    pixora::HotkeyService service(settings, nullptr);

    service.registerAll();
    CHECK_FALSE(service.failed(pixora::HotkeyId::CaptureRegion));
    CHECK_FALSE(service.failed(pixora::HotkeyId::PinFromClipboard));
}

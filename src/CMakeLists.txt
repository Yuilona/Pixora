qt_add_executable(pixora WIN32 MACOSX_BUNDLE
    main.cpp
    common/Log.h
    common/Log.cpp
    app/CaptureService.h
    app/CaptureService.cpp
    app/HotkeyService.h
    app/HotkeyService.cpp
    app/OutputService.h
    app/OutputService.cpp
    app/SettingsService.h
    app/SettingsService.cpp
    app/SingleInstanceGuard.h
    app/SingleInstanceGuard.cpp
    app/TrayService.h
    app/TrayService.cpp
    core/capture/DesktopSnapshot.h
    core/capture/DesktopSnapshot.cpp
    core/capture/SnipSession.h
    core/capture/SnipSession.cpp
    ui/overlay/OverlayWindow.h
    ui/overlay/OverlayWindow.cpp
    platform/interface/GlobalHotkey.h
    platform/interface/PlatformFactory.h
    platform/interface/ScreenCapturer.h
    platform/shared/QtScreenCapturer.h
    platform/shared/QtScreenCapturer.cpp
)

if(WIN32)
    target_sources(pixora PRIVATE
        platform/windows/WinGlobalHotkey.h
        platform/windows/WinGlobalHotkey.cpp
        platform/windows/PlatformFactory.cpp
    )
else()
    target_sources(pixora PRIVATE
        platform/PlatformFactoryStub.cpp
    )
endif()

target_include_directories(pixora PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(pixora PRIVATE PIXORA_VERSION="${PROJECT_VERSION}")

target_link_libraries(pixora PRIVATE
    Qt6::Widgets
    Qt6::Network
    spdlog::spdlog
)

if(WIN32)
    target_link_libraries(pixora PRIVATE user32)
endif()

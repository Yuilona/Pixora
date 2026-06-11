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
    app/PinService.h
    app/PinService.cpp
    app/ScrollCaptureService.h
    app/ScrollCaptureService.cpp
    app/SettingsService.h
    app/SettingsService.cpp
    app/SingleInstanceGuard.h
    app/SingleInstanceGuard.cpp
    app/TrayService.h
    app/TrayService.cpp
    core/annotate/AnnotationTypes.h
    core/annotate/AnnotationItem.h
    core/annotate/AnnotationDocument.h
    core/annotate/AnnotationDocument.cpp
    core/capture/DesktopSnapshot.h
    core/capture/DesktopSnapshot.cpp
    core/capture/SnipSession.h
    core/capture/SnipSession.cpp
    core/stitch/FixedRegionDetector.h
    core/stitch/FixedRegionDetector.cpp
    core/stitch/Stitcher.h
    core/stitch/Stitcher.cpp
    ui/editor/AnnotationRenderer.h
    ui/editor/AnnotationRenderer.cpp
    ui/editor/AnnotationToolbar.h
    ui/editor/AnnotationToolbar.cpp
    ui/pin/PinWindow.h
    ui/pin/PinWindow.cpp
    ui/scroll/RegionIndicator.h
    ui/scroll/RegionIndicator.cpp
    ui/scroll/ScrollCaptureBar.h
    ui/scroll/ScrollCaptureBar.cpp
    ui/overlay/Magnifier.h
    ui/overlay/Magnifier.cpp
    ui/overlay/OverlayWindow.h
    ui/overlay/OverlayWindow.cpp
    ui/overlay/SelectionHandles.h
    ui/overlay/SelectionHandles.cpp
    platform/interface/GlobalHotkey.h
    platform/interface/InputInjector.h
    platform/interface/PlatformFactory.h
    platform/interface/PlatformTypes.h
    platform/interface/ScreenCapturer.h
    platform/interface/SystemIntegration.h
    platform/interface/WindowEnumerator.h
    platform/shared/QtScreenCapturer.h
    platform/shared/QtScreenCapturer.cpp
)

if(WIN32)
    target_sources(pixora PRIVATE
        platform/windows/WinCoordinates.h
        platform/windows/WinCoordinates.cpp
        platform/windows/WinGlobalHotkey.h
        platform/windows/WinGlobalHotkey.cpp
        platform/windows/WinInputInjector.h
        platform/windows/WinInputInjector.cpp
        platform/windows/WinSystemIntegration.h
        platform/windows/WinSystemIntegration.cpp
        platform/windows/WinWindowEnumerator.h
        platform/windows/WinWindowEnumerator.cpp
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
    ${OpenCV_LIBS}
)

if(WIN32)
    target_link_libraries(pixora PRIVATE user32 dwmapi)
endif()

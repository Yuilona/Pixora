find_package(Catch2 3 CONFIG REQUIRED)

add_executable(pixora_tests
    unit/smoke_test.cpp
    unit/settings_test.cpp
    unit/history_test.cpp
    unit/filename_template_test.cpp
    unit/desktop_snapshot_test.cpp
    unit/selection_handles_test.cpp
    unit/snip_session_test.cpp
    unit/annotation_document_test.cpp
    unit/stitcher_test.cpp
    unit/stitcher_fixture_test.cpp
    ${CMAKE_SOURCE_DIR}/src/app/SettingsService.h
    ${CMAKE_SOURCE_DIR}/src/app/SettingsService.cpp
    ${CMAKE_SOURCE_DIR}/src/app/HistoryService.h
    ${CMAKE_SOURCE_DIR}/src/app/HistoryService.cpp
    ${CMAKE_SOURCE_DIR}/src/core/stitch/FixedRegionDetector.h
    ${CMAKE_SOURCE_DIR}/src/core/stitch/FixedRegionDetector.cpp
    ${CMAKE_SOURCE_DIR}/src/core/stitch/Stitcher.h
    ${CMAKE_SOURCE_DIR}/src/core/stitch/Stitcher.cpp
    ${CMAKE_SOURCE_DIR}/src/core/annotate/AnnotationDocument.h
    ${CMAKE_SOURCE_DIR}/src/core/annotate/AnnotationDocument.cpp
    ${CMAKE_SOURCE_DIR}/src/core/capture/DesktopSnapshot.h
    ${CMAKE_SOURCE_DIR}/src/core/capture/DesktopSnapshot.cpp
    ${CMAKE_SOURCE_DIR}/src/core/capture/SnipSession.h
    ${CMAKE_SOURCE_DIR}/src/core/capture/SnipSession.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/overlay/SelectionHandles.h
    ${CMAKE_SOURCE_DIR}/src/ui/overlay/SelectionHandles.cpp
)

target_include_directories(pixora_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_compile_definitions(pixora_tests PRIVATE
    PIXORA_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures"
)

target_link_libraries(pixora_tests PRIVATE
    Catch2::Catch2WithMain
    Qt6::Gui
    spdlog::spdlog
    ${OpenCV_LIBS}
)

include(Catch)
# DL_PATHS: Windows 上测试枚举/运行时需能找到 Qt DLL(Qt 不在系统 PATH)
catch_discover_tests(pixora_tests
    DL_PATHS "$<TARGET_FILE_DIR:Qt6::Core>"
)

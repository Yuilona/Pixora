qt_add_executable(pixora WIN32 MACOSX_BUNDLE
    main.cpp
    common/Log.h
    common/Log.cpp
    app/SingleInstanceGuard.h
    app/SingleInstanceGuard.cpp
    app/TrayService.h
    app/TrayService.cpp
)

target_include_directories(pixora PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(pixora PRIVATE PIXORA_VERSION="${PROJECT_VERSION}")

target_link_libraries(pixora PRIVATE
    Qt6::Widgets
    Qt6::Network
    spdlog::spdlog
)

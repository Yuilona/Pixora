find_package(Catch2 3 CONFIG REQUIRED)

add_executable(pixora_tests
    unit/smoke_test.cpp
)

target_link_libraries(pixora_tests PRIVATE Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(pixora_tests)

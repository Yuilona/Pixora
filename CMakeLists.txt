cmake_minimum_required(VERSION 3.24)

project(Pixora VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PIXORA_BUILD_TESTS "Build unit tests" ON)

if(MSVC)
  add_compile_options(/utf-8 /permissive- /W4)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Network)
find_package(spdlog CONFIG REQUIRED)
find_package(OpenCV CONFIG REQUIRED) # core + imgproc(拼接模板匹配用)

qt_standard_project_setup()

add_subdirectory(src)

if(PIXORA_BUILD_TESTS)
  include(CTest)
  add_subdirectory(tests)
endif()

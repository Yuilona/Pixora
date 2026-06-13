cmake_minimum_required(VERSION 3.24)

project(Pixora VERSION 0.6.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PIXORA_BUILD_TESTS "Build unit tests" ON)

if(MSVC)
  add_compile_options(/utf-8 /permissive- /W4)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Network LinguistTools)
find_package(spdlog CONFIG REQUIRED)
find_package(OpenCV CONFIG REQUIRED) # core + imgproc(拼接模板匹配用)

# 源码字符串为英文,i18n/pixora_zh_CN.ts 提供中文;qm 嵌入 :/i18n
qt_standard_project_setup(I18N_TRANSLATED_LANGUAGES zh_CN)
# qt_standard_project_setup 只开 AUTOMOC/AUTOUIC;不开 AUTORCC 的话
# 源列表里的 .qrc 会被静默忽略,资源(托盘/窗口图标)加载失败
set(CMAKE_AUTORCC ON)

add_subdirectory(src)

# update_translations 目标跑 lupdate 刷新 ts;构建时 lrelease 自动出 qm
qt_add_translations(pixora
    TS_FILE_BASE pixora
    TS_OUTPUT_DIRECTORY i18n
    NO_GENERATE_PLURALS_TS_FILE
)

if(PIXORA_BUILD_TESTS)
  include(CTest)
  add_subdirectory(tests)
endif()

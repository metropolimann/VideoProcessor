cmake_minimum_required(VERSION 3.16)
project(VideoProcessor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find dependencies
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavformat libavcodec libavutil libswscale
)

find_package(Qt6 COMPONENTS Core Quick QuickControls2 REQUIRED PATHS "/usr/lib64/cmake/Qt6" NO_DEFAULT_PATH)

# Core Library
add_library(video_core STATIC
    src/video_utils.cpp
    src/registry.cpp
    src/fire_crop.cpp
    src/video_variance.cpp
)
target_link_libraries(video_core PRIVATE
    PkgConfig::FFMPEG
    Threads::Threads
    Qt6::Core  # <--- Add Qt6::Core here
)
target_include_directories(video_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
# Enable AUTOMOC for video_core so it parses Q_INVOKABLE / Q_OBJECT macros
set_target_properties(video_core PROPERTIES
    AUTOMOC ON
)

# CLI Executable (main)
add_executable(video_processor_cli src/main.cpp)
target_link_libraries(video_processor_cli PRIVATE video_core)
set_target_properties(video_processor_cli PROPERTIES OUTPUT_NAME "video_processor")

# Variance CLI Executable
add_executable(video_processor_variance src/main_variance.cpp)
target_link_libraries(video_processor_variance PRIVATE video_core)

# GUI Executable
add_executable(video_processor_gui
    src/main_gui.cpp
    src/qml.qrc
)
target_link_libraries(video_processor_gui PRIVATE
    video_core
    Qt6::Core
    Qt6::Quick
    Qt6::Qml
    Qt6::QuickControls2
)
set_target_properties(video_processor_gui PROPERTIES
    AUTOMOC ON
    AUTORCC ON
)
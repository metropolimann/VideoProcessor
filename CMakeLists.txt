cmake_minimum_required(VERSION 3.16)
project(VideoProcessor VERSION 1.0.0 LANGUAGES CXX)

# 1. Enforce Modern C++ Standards
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 2. Find general dependencies
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

# 3. Find FFmpeg
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET 
    libavformat libavcodec libavutil libswscale
)

# 4. Find Qt 6 (Fedora Specific Fix)

find_package(Qt6 COMPONENTS Core Quick REQUIRED PATHS "/usr/lib64/cmake/Qt6" NO_DEFAULT_PATH)

# 5. Core Library (Your existing backend)
add_library(video_core STATIC
    src/video_utils.cpp
    src/registry.cpp
    # src/fire_crop.cpp
    # src/video_variance.cpp
)
target_link_libraries(video_core PRIVATE 
    PkgConfig::FFMPEG 
    Threads::Threads
)
target_include_directories(video_core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# 6. CLI Executable (Command Line Tool)
add_executable(video_processor_cli 
    src/main.cpp
)
target_link_libraries(video_processor_cli PRIVATE video_core)
set_target_properties(video_processor_cli PROPERTIES OUTPUT_NAME "video_processor")

# 7. GUI Executable (The Qt 6 App)
add_executable(video_processor_gui 
    src/main_gui.cpp
    src/qml.qrc
)

# Link GUI to Core and Qt 6
target_link_libraries(video_processor_gui PRIVATE 
    video_core 
    Qt6::Core 
    Qt6::Quick
    Qt6::Qml
)

# Enable Qt MOC (Meta-Object Compiler) and Resource Compiler
set_target_properties(video_processor_gui PROPERTIES
    AUTOMOC ON
    AUTORCC ON
)
# 喷印瑕疵检测系统 - CMake构建配置
cmake_minimum_required(VERSION 3.16)
project(defect_detection VERSION 1.0.0 LANGUAGES CXX)

# C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 编译选项
if(MSVC)
    # /utf-8 forces source and execution character set to UTF-8
    add_compile_options(/W4 /O2 /openmp /utf-8)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -O3 -fopenmp)
endif()

# 查找OpenCV (vcpkg安装时需要设置相关路径)
# 优先从环境变量获取，否则使用默认值
if(DEFINED ENV{VCPKG_ROOT})
    set(VCPKG_INSTALLED_PATH "$ENV{VCPKG_ROOT}/installed/x64-windows")
elseif(DEFINED VCPKG_INSTALLED_PATH)
    # 使用用户通过-D传递的路径
else()
    set(VCPKG_INSTALLED_PATH "E:/OpenSource/vcpkg/installed/x64-windows")
    message(WARNING "VCPKG_ROOT not set, using default path: ${VCPKG_INSTALLED_PATH}")
endif()

if(EXISTS "${VCPKG_INSTALLED_PATH}/share/opencv4")
    set(OpenCV_DIR "${VCPKG_INSTALLED_PATH}/share/opencv4")
    set(Protobuf_DIR "${VCPKG_INSTALLED_PATH}/share/protobuf")
    set(absl_DIR "${VCPKG_INSTALLED_PATH}/share/absl")
    set(utf8_range_DIR "${VCPKG_INSTALLED_PATH}/share/utf8_range")
    list(APPEND CMAKE_PREFIX_PATH "${VCPKG_INSTALLED_PATH}")
endif()
find_package(OpenCV 4.0 REQUIRED)
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")

# 查找OpenMP
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP found")
else()
    message(WARNING "OpenMP not found, multi-threading will be disabled")
endif()

# 可选：查找CUDA
option(USE_CUDA "Enable CUDA acceleration" OFF)
if(USE_CUDA)
    find_package(CUDA)
    if(CUDA_FOUND)
        message(STATUS "CUDA found: ${CUDA_VERSION}")
        add_definitions(-DUSE_CUDA)
    endif()
endif()

# 包含目录
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

# 核心库源文件
set(CORE_SOURCES
    src/roi_manager.cpp
    src/template_matcher.cpp
    src/defect_analyzer.cpp
    src/localization.cpp
    src/template_learner.cpp
    src/defect_detector.cpp
    src/visualization_config.cpp
    src/visualizer.cpp
)

# 创建静态库
add_library(defect_detection_core STATIC ${CORE_SOURCES})
target_link_libraries(defect_detection_core 
    ${OpenCV_LIBS}
)
if(OpenMP_CXX_FOUND)
    target_link_libraries(defect_detection_core OpenMP::OpenMP_CXX)
endif()

# C API 共享库
add_library(defect_detection SHARED
    api/c_api.cpp
)
target_link_libraries(defect_detection
    defect_detection_core
    ${OpenCV_LIBS}
)

# 测试程序
add_executable(test_detection
    tests/test_detection.cpp
)
target_link_libraries(test_detection
    defect_detection_core
    ${OpenCV_LIBS}
)

# 性能测试程序
add_executable(test_performance
    tests/test_performance.cpp
)
target_link_libraries(test_performance
    defect_detection_core
    ${OpenCV_LIBS}
)

# 演示程序
add_executable(demo
    tests/demo.cpp
)
target_link_libraries(demo
    defect_detection_core
    ${OpenCV_LIBS}
)

# 调试程序
add_executable(debug_detection
    tests/debug_detection.cpp
)
target_link_libraries(debug_detection
    defect_detection_core
    ${OpenCV_LIBS}
)

# API v2.0 测试程序
add_executable(test_api_v2
    tests/test_api_v2.cpp
)
target_link_libraries(test_api_v2
    defect_detection
    ${OpenCV_LIBS}
)

# API Logger 测试程序
add_executable(test_api_logger
    tests/test_api_logger.cpp
)
target_link_libraries(test_api_logger
    defect_detection
    ${OpenCV_LIBS}
)

# C#风格演示程序（带CSV定位数据）
add_executable(demo_cpp
    tests/demo_cpp.cpp
)
target_link_libraries(demo_cpp
    defect_detection_core
    ${OpenCV_LIBS}
)

# 安装配置
install(TARGETS defect_detection defect_detection_core
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include)

# 输出构建信息
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")


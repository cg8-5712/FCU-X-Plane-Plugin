cmake_minimum_required(VERSION 3.15)
project(ToLissFCUMonitor)

# C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 平台标准输出名（X-Plane 要求）
if (WIN32)
    set(PLUGIN_OUTPUT_NAME "win.xpl")
elseif(APPLE)
    set(PLUGIN_OUTPUT_NAME "mac.xpl")
elseif(UNIX)
    set(PLUGIN_OUTPUT_NAME "lin.xpl")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# SDK 头文件路径 (可通过命令行 -D XPLM_SDK_PATH 传入)
if(NOT DEFINED XPLM_SDK_PATH)
    set(XPLM_SDK_PATH "${CMAKE_SOURCE_DIR}/extern/XPLM-SDK" CACHE PATH "Path to X-Plane SDK")
endif()

# 包含目录
include_directories(${XPLM_SDK_PATH}/CHeaders/XPLM)
include_directories(${XPLM_SDK_PATH}/CHeaders/Widgets)
include_directories(${XPLM_SDK_PATH}/CHeaders/Wrappers)

# 源文件目录
file(GLOB SRC_FILES "${CMAKE_SOURCE_DIR}/src/*.cpp")

# 生成共享库
add_library(${PROJECT_NAME} SHARED ${SRC_FILES})

# XPLM 版本定义（可通过 -D XPLM_VERSION 覆盖）
# XPLM301 = XP11.20+, XPLM303 = XP11.50+, XPLM400 = XP12.04+
if(NOT DEFINED XPLM_VERSION)
    set(XPLM_VERSION "301" CACHE STRING "XPLM API version")
endif()

# 平台定义和 XPLM 版本定义
# SDK 的 API 是累积的，需要定义所有较低版本的宏
if (WIN32)
    if(XPLM_VERSION STREQUAL "400" OR XPLM_VERSION STREQUAL "410")
        target_compile_definitions(${PROJECT_NAME} PRIVATE
            IBM=1
            XPLM200=1 XPLM210=1 XPLM300=1 XPLM301=1 XPLM303=1 XPLM${XPLM_VERSION}=1
        )
    elseif(XPLM_VERSION STREQUAL "303")
        target_compile_definitions(${PROJECT_NAME} PRIVATE
            IBM=1
            XPLM200=1 XPLM210=1 XPLM300=1 XPLM301=1 XPLM303=1
        )
    elseif(XPLM_VERSION STREQUAL "301")
        target_compile_definitions(${PROJECT_NAME} PRIVATE
            IBM=1
            XPLM200=1 XPLM210=1 XPLM300=1 XPLM301=1
        )
    elseif(XPLM_VERSION STREQUAL "300")
        target_compile_definitions(${PROJECT_NAME} PRIVATE
            IBM=1
            XPLM200=1 XPLM210=1 XPLM300=1
        )
    endif()
elseif(APPLE)
    if(XPLM_VERSION STREQUAL "400" OR XPLM_VERSION STREQUAL "410")
        target_compile_definitions(${PROJECT_NAME} PRIVATE
            APL=1
            XPLM200=1 XPLM210=1 XPLM300=1 XPLM301=1 XPLM303=1 XPLM${XPLM_VERSION}=1
        )
    elseif(XPLM_VERSION STREQUAL "303")
        target_compile_definitions(${PROJECT_NAME} PRIVATE
            APL=1
            XPLM200=1 XPLM210=1 XPLM300=1 XPLM301=1 XPLM303=1
        )
    elseif(XPLM_VERSION STREQUAL "301")
        target_compile_definitions(${PROJECT_NAME} PRIVATE
            APL=1
            XPLM200=1 XPLM210=1 XPLM300=1 XPLM301=1
        )
    elseif(XPLM_VERSION STREQUAL "300")
        target_compile_definitions(${PROJECT_NAME} PRIVATE
            APL=1
            XPLM200=1 XPLM210=1 XPLM300=1
        )
    endif()
elseif(UNIX)
    if(XPLM_VERSION STREQUAL "400" OR XPLM_VERSION STREQUAL "410")
        target_compile_definitions(${PROJECT_NAME} PRIVATE
            LIN=1
            XPLM200=1 XPLM210=1 XPLM300=1 XPLM301=1 XPLM303=1 XPLM${XPLM_VERSION}=1
        )
    elseif(XPLM_VERSION STREQUAL "303")
        target_compile_definitions(${PROJECT_NAME} PRIVATE
            LIN=1
            XPLM200=1 XPLM210=1 XPLM300=1 XPLM301=1 XPLM303=1
        )
    elseif(XPLM_VERSION STREQUAL "301")
        target_compile_definitions(${PROJECT_NAME} PRIVATE
            LIN=1
            XPLM200=1 XPLM210=1 XPLM300=1 XPLM301=1
        )
    elseif(XPLM_VERSION STREQUAL "300")
        target_compile_definitions(${PROJECT_NAME} PRIVATE
            LIN=1
            XPLM200=1 XPLM210=1 XPLM300=1
        )
    endif()
endif()

# 链接 XPLM 库文件
if (WIN32)
    target_link_libraries(${PROJECT_NAME}
        "${XPLM_SDK_PATH}/Libraries/Win/XPLM_64.lib"
        "opengl32.lib"
    )
elseif(APPLE)
    # macOS 使用框架，不需要链接库文件
    target_link_libraries(${PROJECT_NAME}
        "-framework XPLM"
        "-framework XPWidgets"
        "-framework OpenGL"
    )
elseif(UNIX)
    # Linux 不需要链接 XPLM 库
    target_link_libraries(${PROJECT_NAME}
        "GL"
    )
endif()

# 设置输出文件名（不带 lib 前缀）
set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    SUFFIX ""
    OUTPUT_NAME ${PLUGIN_OUTPUT_NAME}
)

# 输出目录（dist 文件夹）
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/dist/${CMAKE_BUILD_TYPE}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/dist/${CMAKE_BUILD_TYPE}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/dist/Release"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/dist/Release"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/dist/Debug"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/dist/Debug"
)

# Windows 下去掉控制台窗口
if (WIN32)
    target_link_options(${PROJECT_NAME} PRIVATE "/SUBSYSTEM:WINDOWS")
endif()

# 自动复制到 X-Plane 插件目录（可选）
if(DEFINED XPLANE_PATH AND EXISTS "${XPLANE_PATH}")
    message(STATUS "X-Plane installation found at: ${XPLANE_PATH}")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${XPLANE_PATH}/Resources/plugins/${PROJECT_NAME}"
        COMMAND ${CMAKE_COMMAND} -E copy
            "$<TARGET_FILE:${PROJECT_NAME}>"
            "${XPLANE_PATH}/Resources/plugins/${PROJECT_NAME}/${PLUGIN_OUTPUT_NAME}"
        COMMENT "Copying plugin to X-Plane installation"
    )
else()
    message(STATUS "XPLANE_PATH not set or doesn't exist. Plugin will only be built to dist/ directory.")
endif()

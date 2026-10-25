cmake_minimum_required(VERSION 3.21)

# macOS: 最低系统版本与 SDK 回退必须在 project() 之前设置，
# 否则 CMake 的编译器探测会先用默认 SDK 链接，直接踩到 tapi 错误。
if(APPLE)
    # 旧构建目录（包括 Qt Creator 的缓存）可能把部署目标留成宿主机版本，
    # 例如 26.6。本项目只要求 14.4，这里自动纠正。
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET OR CMAKE_OSX_DEPLOYMENT_TARGET VERSION_GREATER 15)
        if(CMAKE_OSX_DEPLOYMENT_TARGET)
            message(STATUS "macOS 部署目标由 ${CMAKE_OSX_DEPLOYMENT_TARGET} 纠正为 14.4")
        else()
            message(STATUS "macOS 部署目标设置为 14.4")
        endif()
        set(CMAKE_OSX_DEPLOYMENT_TARGET "14.4" CACHE STRING "Minimum macOS version" FORCE)
    endif()

    option(DAILYUP_SDK_FALLBACK
        "Fall back from macOS 27+ SDK when the local linker cannot handle it" ON)

    # 找出本机 27 以下的最新 SDK，作为兼容回退。
    set(_dailyup_fallback_version "")
    set(_dailyup_fallback_path "")
    file(GLOB _dailyup_sdk_candidates "/Library/Developer/CommandLineTools/SDKs/MacOSX*.sdk")
    foreach(_dailyup_sdk IN LISTS _dailyup_sdk_candidates)
        get_filename_component(_dailyup_sdk_name "${_dailyup_sdk}" NAME)
        if(_dailyup_sdk_name MATCHES "^MacOSX([0-9]+\\.[0-9]+)\\.sdk$")
            set(_dailyup_sdk_ver "${CMAKE_MATCH_1}")
            if(_dailyup_sdk_ver VERSION_LESS 27
               AND (_dailyup_fallback_version STREQUAL ""
                    OR _dailyup_sdk_ver VERSION_GREATER _dailyup_fallback_version))
                set(_dailyup_fallback_version "${_dailyup_sdk_ver}")
                set(_dailyup_fallback_path "${_dailyup_sdk}")
            endif()
        endif()
    endforeach()

    # 当前 SDK 版本：优先从缓存路径解析；没有显式指定时询问 xcrun。
    set(_dailyup_sdk_version "")
    if(CMAKE_OSX_SYSROOT)
        if(CMAKE_OSX_SYSROOT MATCHES "MacOSX([0-9]+\\.[0-9]+)\\.sdk/?$")
            set(_dailyup_sdk_version "${CMAKE_MATCH_1}")
        endif()
    else()
        execute_process(
            COMMAND xcrun --show-sdk-version
            OUTPUT_VARIABLE _dailyup_sdk_version
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
    endif()

    # macOS 27 SDK 与旧 Command Line Tools 链接器组合会报
    # "tapi error: malformed file"。即使缓存里已经写着 27.x 也要纠正。
    if(DAILYUP_SDK_FALLBACK
       AND _dailyup_sdk_version VERSION_GREATER_EQUAL 27
       AND NOT _dailyup_fallback_path STREQUAL "")
        message(STATUS "macOS SDK ${_dailyup_sdk_version} 与本机链接器不兼容，回退到 ${_dailyup_fallback_version}: ${_dailyup_fallback_path}")
        set(CMAKE_OSX_SYSROOT "${_dailyup_fallback_path}" CACHE PATH "macOS SDK" FORCE)
    endif()
endif()

project(DailyUp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Sql)

qt_standard_project_setup(REQUIRES 6.5)

# 可选的 Figma 导入内容
if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/importedcontent/CMakeLists.txt")
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/importedcontent)
endif()

# Windows 资源（仅在文件存在时使用，macOS 走 bundle 图标）
set(APP_ICON_RESOURCE "")
if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/app.rc")
    set(APP_ICON_RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/app.rc")
endif()

qt_add_executable(appDailyUp
    main.cpp
    ${APP_ICON_RESOURCE}
)
set_target_properties(appDailyUp PROPERTIES OUTPUT_NAME "DailyUp")

# macOS 应用包：图标、Info.plist、Bundle 标识
if(APPLE)
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.zes.dailyup")
    set(MACOSX_BUNDLE_BUNDLE_NAME "DailyUp")
    set(MACOSX_BUNDLE_ICON_FILE "DailyUp.icns")

    set(APP_ICON_MACOS "${CMAKE_CURRENT_SOURCE_DIR}/assets/DailyUp.icns")
    set_source_files_properties(${APP_ICON_MACOS} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(appDailyUp PRIVATE ${APP_ICON_MACOS})

    # 显式生成 Info.plist：CMake 对 MACOSX_BUNDLE_INFO_PLIST 的变量替换
    # 在部分 Qt/CMake 组合下会把可执行名和版本留空，导致 Finder 无法启动。
    set(APP_BUNDLE_EXECUTABLE "DailyUp")
    set(APP_BUNDLE_IDENTIFIER "com.zes.dailyup")
    set(APP_BUNDLE_NAME "DailyUp")
    set(APP_BUNDLE_ICON "DailyUp.icns")
    set(APP_BUNDLE_VERSION "${PROJECT_VERSION}")
    set(APP_BUNDLE_SHORT_VERSION "${PROJECT_VERSION}")
    set(APP_BUNDLE_MIN_MACOS "${CMAKE_OSX_DEPLOYMENT_TARGET}")
    set(APP_BUNDLE_COPYRIGHT "Copyright © 2026 Zes. All rights reserved.")
    set(APP_BUNDLE_DEVELOPMENT_REGION "zh_CN")

    set(APP_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/Info.plist")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
        "${APP_INFO_PLIST}"
        @ONLY)

    set_target_properties(appDailyUp PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${APP_INFO_PLIST}"
    )
else()
    set_target_properties(appDailyUp PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

# 应用内可用的图片资源
qt_add_resources(appDailyUp "app_resources"
    PREFIX "/"
    FILES
        images/app_icon.png
)

# QML 模块（Theme 作为单例）
set(QML_SOURCES
    Main.qml
    Theme.qml
    WindowControls.qml
    ThemePicker.qml
    DateStrip.qml
    TaskRow.qml
    TaskDialog.qml
)
set_source_files_properties(Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(appDailyUp
    URI DailyUp
    VERSION 1.0
    QML_FILES ${QML_SOURCES}
    SOURCES
        AppSettings.h
        AppSettings.cpp
        TaskManager.h
        TaskManager.cpp
        databasemanager.h
        databasemanager.cpp
)

target_link_libraries(appDailyUp
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Sql
)

# 安装规则（macOS .app / Windows exe）
include(GNUInstallDirs)
install(TARGETS appDailyUp
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
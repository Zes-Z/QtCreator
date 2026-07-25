cmake_minimum_required(VERSION 3.16)

project(DailyUp VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. 引入必要组件
find_package(Qt6 REQUIRED COMPONENTS Quick Sql)

qt_standard_project_setup(REQUIRES 6.5)

# 如果有 Figma 导入的组件内容
if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/importedcontent/CMakeLists.txt")
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/importedcontent)
endif()

# 定义 Windows 资源文件用于设置 .exe 图标 (仅在 Windows 平台生效)
set(APP_ICON_RESOURCE "")
if(WIN32)
    set(APP_ICON_RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/app.rc")
endif()

qt_add_executable(appDailyUp
    main.cpp
    ${APP_ICON_RESOURCE}  # 引入 Windows 图标资源
)

# 使用 qt_add_resources 将 images 文件夹打包进 qrc:/ 虚拟路径
qt_add_resources(appDailyUp "app_resources"
    PREFIX "/"
    FILES
        images/app_icon.png
)

# 配置 QML 模块（建议将 TaskManager 放入 SOURCES，方便 QML 识别）
qt_add_qml_module(appDailyUp
    URI DailyUp
    VERSION 1.0
    QML_FILES
        Main.qml
    SOURCES
        TaskManager.h
        TaskManager.cpp
)

# macOS 与 Windows 属性设置
set_target_properties(appDailyUp PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# 链接依赖库
target_link_libraries(appDailyUp
    PRIVATE
        Qt6::Quick
        Qt6::Sql
)

# 安装规则配置（为后续 windeployqt 打包做准备）
include(GNUInstallDirs)
install(TARGETS appDailyUp
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
cmake_minimum_required(VERSION 3.16)

project(LitApp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Qml QuickControls2 Network Sql)

qt_standard_project_setup()

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

set(SOURCES
    src/main.cpp
    src/models/FileItem.cpp
    src/models/FileListModel.cpp
    src/services/AuthService.cpp
    src/services/FileService.cpp
    src/services/SyncService.cpp
    src/services/WebDavClient.cpp
    src/storage/MetadataStore.cpp
    src/parsers/DocumentParser.cpp
)

set(HEADERS
    src/models/FileCategory.h
    src/models/FileItem.h
    src/models/FileListModel.h
    src/services/AuthService.h
    src/services/FileService.h
    src/services/SyncService.h
    src/services/WebDavClient.h
    src/storage/MetadataStore.h
    src/parsers/DocumentParser.h
)

qt_add_executable(LitApp ${SOURCES} ${HEADERS})

target_include_directories(LitApp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# 如果安装了 Qt PDF 模块，则启用 PDF 支持
find_package(Qt6 QUIET COMPONENTS Pdf)
if(Qt6Pdf_FOUND)
    target_link_libraries(LitApp PRIVATE Qt6::Pdf)
    target_compile_definitions(LitApp PRIVATE LITAPP_HAS_QTPDF=1)
    message(STATUS "Qt PDF module found: PDF support enabled")
else()
    message(STATUS "Qt PDF module not found: PDF support disabled until QtPdf is installed")
endif()

set(QML_FILES
    qml/Main.qml
    qml/LoginPage.qml
    qml/FilePage.qml
)
if(Qt6Pdf_FOUND)
    list(APPEND QML_FILES qml/PdfViewPage.qml qml/PdfPreviewPane.qml)
endif()

qt_add_qml_module(LitApp
    URI LitApp
    VERSION 1.0
    QML_FILES
        ${QML_FILES}
)

target_link_libraries(LitApp PRIVATE
    Qt6::Quick
    Qt6::Qml
    Qt6::QuickControls2
    Qt6::Network
    Qt6::Sql
)

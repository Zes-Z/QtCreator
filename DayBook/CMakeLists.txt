cmake_minimum_required(VERSION 3.21)

if(APPLE)
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "14.4" CACHE STRING "Minimum macOS version" FORCE)
    endif()
    # Some combinations of the macOS 27 SDK and an older command line tools
    # linker fail with "tapi error: malformed file". Fall back to the newest
    # installed SDK below 27 when that happens and no SDK was requested.
    if(NOT CMAKE_OSX_SYSROOT)
        execute_process(
            COMMAND xcrun --show-sdk-version
            OUTPUT_VARIABLE _daybook_sdk_version
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
        if(_daybook_sdk_version VERSION_GREATER_EQUAL 27)
            file(GLOB _daybook_sdk_candidates "/Library/Developer/CommandLineTools/SDKs/MacOSX*.sdk")
            set(_daybook_fallback_version "")
            set(_daybook_fallback_path "")
            foreach(_daybook_sdk IN LISTS _daybook_sdk_candidates)
                get_filename_component(_daybook_sdk_name "${_daybook_sdk}" NAME)
                if(_daybook_sdk_name MATCHES "^MacOSX([0-9]+\\.[0-9]+)\\.sdk$")
                    set(_daybook_sdk_ver "${CMAKE_MATCH_1}")
                    if(_daybook_sdk_ver VERSION_LESS 27)
                        if(_daybook_fallback_version STREQUAL "" OR _daybook_sdk_ver VERSION_GREATER _daybook_fallback_version)
                            set(_daybook_fallback_version "${_daybook_sdk_ver}")
                            set(_daybook_fallback_path "${_daybook_sdk}")
                        endif()
                    endif()
                endif()
            endforeach()
            if(NOT _daybook_fallback_path STREQUAL "")
                message(STATUS "Using fallback macOS SDK ${_daybook_fallback_version}: ${_daybook_fallback_path}")
                set(CMAKE_OSX_SYSROOT "${_daybook_fallback_path}" CACHE PATH "macOS SDK" FORCE)
            endif()
        endif()
    endif()
endif()

project(DayBook VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.9 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Sql)
qt_standard_project_setup(REQUIRES 6.9)

qt_add_executable(DayBook
    src/main.cpp
    src/database.h src/database.cpp
    src/models.h src/models.cpp
    src/appcontroller.h src/appcontroller.cpp
)

set_source_files_properties(
    qml/Theme.qml qml/Icons.qml qml/UiBus.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(DayBook
    URI DayBook
    VERSION 1.0
    RESOURCE_PREFIX "/"
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/Icons.qml
        qml/UiBus.qml
        qml/components/NavItem.qml
        qml/components/StatCard.qml
        qml/components/GlassCard.qml
        qml/components/Icon.qml
        qml/components/AppButton.qml
        qml/components/IconButton.qml
        qml/components/SegmentedControl.qml
        qml/components/AppTextField.qml
        qml/components/AmountText.qml
        qml/components/Tag.qml
        qml/components/Toast.qml
        qml/components/ConfirmDialog.qml
        qml/components/EmptyState.qml
        qml/components/TrafficLights.qml
        qml/components/MonthSwitcher.qml
        qml/components/ProgressRing.qml
        qml/components/ProgressBar.qml
        qml/components/TransactionRow.qml
        qml/components/AutoScrollBar.qml
        qml/components/DragGrip.qml
        qml/components/FlipCard.qml
        qml/components/FlipToggle.qml
        qml/components/SidebarSummaryCard.qml
        qml/components/TransactionDialog.qml
        qml/components/AccountDialog.qml
        qml/components/AccountBillsDialog.qml
        qml/components/AboutDialog.qml
        qml/components/BudgetDialog.qml
        qml/components/CategoryIcon.qml
        qml/charts/AreaChart.qml
        qml/charts/DonutChart.qml
        qml/charts/BarChart.qml
        qml/charts/Sparkline.qml
        qml/pages/OverviewPage.qml
        qml/pages/TransactionsPage.qml
        qml/pages/InsightsPage.qml
        qml/pages/AccountsPage.qml
        qml/pages/BudgetsPage.qml
        qml/pages/SettingsPage.qml
)

qt_add_resources(DayBook "app_assets"
    PREFIX "/"
    FILES
        assets/appicon.png
        assets/noise.png
        assets/fonts/DayBookIcons.ttf
        assets/fonts/LICENSE-MaterialSymbols.txt
        assets/DayBook.icns
        assets/DayBook.ico
)

target_link_libraries(DayBook PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Sql
)

if(APPLE)
    set_target_properties(DayBook PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "DayBook"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.daybook.app"
        MACOSX_BUNDLE_ICON_FILE "DayBook.icns"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    )
    set_source_files_properties(assets/DayBook.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(DayBook PRIVATE assets/DayBook.icns)
elseif(WIN32)
    set_target_properties(DayBook PROPERTIES WIN32_EXECUTABLE TRUE)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/app.rc.in")
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/app.rc.in"
            "${CMAKE_CURRENT_BINARY_DIR}/daybook_app.rc"
            @ONLY)
        target_sources(DayBook PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/daybook_app.rc")
    endif()
endif()

include(GNUInstallDirs)
install(TARGETS DayBook
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

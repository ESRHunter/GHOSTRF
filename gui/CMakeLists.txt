cmake_minimum_required(VERSION 3.16)
project(RFHubStudio VERSION 1.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets SerialPort)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets SerialPort)

set(SOURCES
    src/main.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/models/signalitem.h
    src/models/signalitem.cpp
    src/device/devicemanager.h
    src/device/devicemanager.cpp
    src/widgets/waveformwidget.h
    src/widgets/waveformwidget.cpp
    src/widgets/oleddisplaywidget.h
    src/widgets/oleddisplaywidget.cpp
    src/widgets/signaldetaildialog.h
    src/widgets/signaldetaildialog.cpp
    src/theme/darktheme.h
    src/theme/darktheme.cpp
    src/splash_frame.h
    resources.qrc
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    src/models
    src/device
    src/widgets
    src/theme
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::SerialPort
)

# Cross-platform installation and binary properties
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "RF-Hub 433 Studio"
    )
endif()

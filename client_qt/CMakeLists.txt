cmake_minimum_required(VERSION 3.10)
project(messenger_qt_client LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets Network)

add_executable(messenger_qt_client
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/NetClient.h
    src/NetClient.cpp
)

target_link_libraries(messenger_qt_client Qt5::Widgets Qt5::Network)

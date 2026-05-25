cmake_minimum_required(VERSION 3.10)
project(messenger_qt_client LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets Network)
find_package(OpenSSL REQUIRED)

set(COMMON_DIR ${CMAKE_CURRENT_LIST_DIR}/../common)

# Build common crypto as C static library (prevents C++ name mangling issues)
add_library(common_crypto STATIC
    ${COMMON_DIR}/crypto.c
)
target_include_directories(common_crypto PUBLIC ${COMMON_DIR})
target_link_libraries(common_crypto PUBLIC OpenSSL::SSL OpenSSL::Crypto)

add_executable(messenger_qt_client
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/NetClient.h
    src/NetClient.cpp
    src/LoginDialog.h
    src/LoginDialog.cpp
)

target_include_directories(messenger_qt_client PRIVATE ${COMMON_DIR})
target_link_libraries(messenger_qt_client Qt5::Widgets Qt5::Network common_crypto)

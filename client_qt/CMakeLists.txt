cmake_minimum_required(VERSION 3.10)
project(messenger_qt_client LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets Network)

# Полностью ручное указание OpenSSL (обход автоматического поиска)
set(OPENSSL_INCLUDE_DIR "C:/OpenSSL/include")
set(OPENSSL_CRYPTO_LIBRARY "C:/OpenSSL/lib/VC/x64/MT/libcrypto.lib")
set(OPENSSL_SSL_LIBRARY "C:/OpenSSL/lib/VC/x64/MT/libssl.lib")
set(OPENSSL_LIBRARIES ${OPENSSL_CRYPTO_LIBRARY} ${OPENSSL_SSL_LIBRARY})

# Проверяем, что файлы существуют
if(NOT EXISTS ${OPENSSL_INCLUDE_DIR}/openssl/evp.h)
    message(FATAL_ERROR "OpenSSL headers not found at ${OPENSSL_INCLUDE_DIR}")
endif()
if(NOT EXISTS ${OPENSSL_CRYPTO_LIBRARY})
    message(FATAL_ERROR "OpenSSL crypto lib not found at ${OPENSSL_CRYPTO_LIBRARY}")
endif()
if(NOT EXISTS ${OPENSSL_SSL_LIBRARY})
    message(FATAL_ERROR "OpenSSL ssl lib not found at ${OPENSSL_SSL_LIBRARY}")
endif()

message(STATUS "OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
message(STATUS "OpenSSL crypto lib: ${OPENSSL_CRYPTO_LIBRARY}")
message(STATUS "OpenSSL ssl lib: ${OPENSSL_SSL_LIBRARY}")

set(COMMON_DIR ${CMAKE_CURRENT_LIST_DIR}/../common)

add_library(common_crypto STATIC
    ${COMMON_DIR}/crypto.c
)

target_include_directories(common_crypto PUBLIC ${COMMON_DIR})
target_include_directories(common_crypto PRIVATE ${OPENSSL_INCLUDE_DIR})
target_link_libraries(common_crypto PRIVATE ${OPENSSL_LIBRARIES})

if(WIN32)
    target_link_libraries(common_crypto PUBLIC ws2_32)
endif()

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
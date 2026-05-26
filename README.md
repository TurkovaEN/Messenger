# Messenger — Кроссплатформенный мессенджер с шифрованием

## Описание

Messenger — кроссплатформенный мессенджер на C с шифрованием трафика AES-256-CBC. Поддерживает Windows и Linux, имеет графический (Qt5) и консольный клиенты.

## Возможности

- Кроссплатформенность: Windows / Linux
- Графический интерфейс (Qt5) и консольный клиент
- Шифрование сообщений AES-256-CBC
- Групповые чаты (комнаты)
- Хранение истории сообщений

## Быстрый старт

### Установка ключа шифрования

Linux / Mac:
export MSG_KEY="my_super_secret_key_32_bytes_len!!"

Windows (cmd):
set MSG_KEY=my_super_secret_key_32_bytes_len!!

### Запуск сервера

./server/server 5555 server/logs/server.log

### Запуск консольного клиента

./client/client 127.0.0.1 5555 Alice

### Запуск Qt-клиента

./client_qt/build/messenger_qt_client

## Команды

/create roomname - создать комнату
/join roomname - войти в комнату
/leave - покинуть комнату
/rooms - список комнат
/help - справка
/quit - выход

## Сборка

### Linux

sudo apt install build-essential libssl-dev qt5-default cmake
make -C server
make -C client
cd client_qt && mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=/usr/lib/x86_64-linux-gnu/cmake/Qt5
make

### Windows

Подробная инструкция в файле INSTALL.md
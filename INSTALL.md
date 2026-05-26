\# Инструкция по установке и сборке



\## Linux (Mint / Ubuntu)



\### Установка зависимостей



sudo apt update

sudo apt install -y build-essential git cmake

sudo apt install -y libssl-dev

sudo apt install -y qt5-default qt5-qmake qtbase5-dev-tools



\### Сборка сервера и консольного клиента



cd \~/Messenger

make -C server

make -C client



\### Сборка Qt-клиента



cd \~/Messenger/client\_qt

mkdir build \&\& cd build

cmake .. -DCMAKE\_PREFIX\_PATH=/usr/lib/x86\_64-linux-gnu/cmake/Qt5

make -j$(nproc)



\### Запуск



export MSG\_KEY="my\_super\_secret\_key\_32\_bytes\_len!!"

./server/server 5555 server/logs/server.log



\## Windows (Visual Studio 2019)



\### 1. Установка Visual Studio 2019



Скачать с официального сайта Community 2019

В установке отметить: "Разработка классических приложений на C++"



\### 2. Установка Qt5



Скачать Qt Online Installer

Выбрать Qt 5.15.2 -> MSVC 2019 64-bit

Установить в C:\\Qt\\5.15.2\\msvc2019\_64



\### 3. Установка OpenSSL (важно: НЕ Light версию!)



Скачать с https://slproweb.com/products/Win32OpenSSL.html

Выбрать Win64 OpenSSL v3.3.2 (не Light!)

Установить в C:\\OpenSSL

При установке выбрать: "Copy OpenSSL DLLs to: The OpenSSL binaries directory"



\### 4. Сборка сервера и консольного клиента



Открыть vs/Messenger.sln в Visual Studio

Выбрать x64-Release

Сборка -> Собрать решение



\### 5. Сборка Qt-клиента



В Visual Studio: Файл -> Открыть -> CMake...

Выбрать client\_qt/CMakeLists.txt

Выбрать конфигурацию x64-Release

Сборка -> Собрать все



\### 6. Подготовка Qt-клиента к запуску



Открыть командную строку Qt (Пуск -> Qt 5.15.2 MSVC 2019 64-bit)

cd путь\_к\\client\_qt\\build\\windows-x64-release\\Release

windeployqt messenger\_qt\_client.exe

copy C:\\OpenSSL\\bin\\libcrypto-3-x64.dll .

copy C:\\OpenSSL\\bin\\libssl-3-x64.dll .



\## VirtualBox (для кроссплатформенного тестирования)



\### Настройка NAT с пробросом порта (сервер на Linux)



VirtualBox -> Настройки ВМ -> Сеть -> Адаптер 1 -> NAT

Дополнительно -> Проброс портов -> Добавить:

&#x20; Имя: messenger

&#x20; Протокол: TCP

&#x20; Хост-порт: 5555

&#x20; Гостевой порт: 5555



\### Запуск сервера на Linux, клиента на Windows



В Linux:

export MSG\_KEY="my\_super\_secret\_key\_32\_bytes\_len!!"

./server/server 5555 server/logs/server.log



В Windows:

set MSG\_KEY=my\_super\_secret\_key\_32\_bytes\_len!!

client\_win.exe 127.0.0.1 5555 Alice



\### Запуск сервера на Windows, клиента на Linux



В Windows (узнать IP через ipconfig):

set MSG\_KEY=my\_super\_secret\_key\_32\_bytes\_len!!

server\_win.exe 5555 server/logs/server.log



В Linux:

export MSG\_KEY="my\_super\_secret\_key\_32\_bytes\_len!!"

./client/client 192.168.1.100 5555 Bob


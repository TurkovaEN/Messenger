# Сетевой протокол Messenger

Версия: 1.0
Дата: Май 2026

## Общее описание

Протокол работает поверх TCP. Все сообщения передаются в текстовом формате key=value; pairs.
Шифрованные сообщения используют формат type=enc;data=<base64>.

## Формат кадра

- uint32 length (порядок байт network byte order)
- payload (UTF-8 текст)

## Примеры

Обычное сообщение:
type=msg;to=bob;text=hello

Зашифрованное сообщение:
type=enc;data=U2FsdGVkX1/3j5k7G8pQrZyLmNvBw4tY6uIxO...

## Message types

### Client -> Server

- Login:
  - `type=login;user=<name>`

- Direct message:
  - `type=msg;to=<user>;text=<text>`

### Server -> Client

- Delivery to recipient:
  - `type=deliver;from=<user>;text=<text>`

- Info:
  - `type=info;text=<text>`

- Error:
  - `type=error;text=<text>`

## Group chats (rooms)

### Client -> Server

- Create room:
  - `type=room_create;room=<name>`
- Join room:
  - `type=room_join;room=<name>`
- Leave room:
  - `type=room_leave;room=<name>`
- Send message to room:
  - `type=room_msg;room=<name>;text=<text>`

### Server -> Client

- Delivery to room members:
  - `type=room_deliver;room=<name>;from=<user>;text=<text>`

## Шифрование

При включённом шифровании (переменная окружения MSG_KEY) сообщения передаются в формате:

`type=enc;data=<base64_encoded_ciphertext>`

Где base64_encoded_ciphertext - это зашифрованное AES-256-CBC сообщение,
включающее 16-байтный IV в начале.

Расшифрованное сообщение может иметь любой из вышеперечисленных типов
(login, msg, room_create, room_msg и т.д.)

### Пример зашифрованного пакета в Wireshark

0000 74 79 70 65 3d 65 6e 63 3b 64 61 74 61 3d 55 32 	type=enc;data=U2
0010 46 73 64 47 56 6b 58 31 2f 33 6a 35 6b 37 47 38 	FsdGVkX1/3j5k7G8
0020 70 51 72 5a 79 4c 6d 4e 76 42 77 34 74 59 36 75 	pQrZyLmNvBw4tY6u
0030 49 78 4f 42 4d 6a 41 3d 3d 			IxOBMjA==



### Примечание

- При выключенном шифровании (MSG_KEY не установлена) сообщения передаются в открытом виде
- Шифрование прозрачно для протокола — сервер не знает содержимого зашифрованных сообщений
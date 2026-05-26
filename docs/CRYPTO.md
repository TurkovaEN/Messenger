\# Криптографическая подсистема



\## Используемый алгоритм



AES-256-CBC (Advanced Encryption Standard с 256-битным ключом в режиме CBC)



\## Обоснование выбора



AES является федеральным стандартом США (FIPS 197)

256-битный ключ обеспечивает стойкость 2^256 вариантов

Режим CBC обеспечивает одинаковые блоки -> разный шифротекст

Поддерживается OpenSSL на Linux и Windows



\## Структура функций



int encrypt\_message(const unsigned char\* plaintext, size\_t plaintext\_len, unsigned char\*\* ciphertext, size\_t\* ciphertext\_len)



int decrypt\_message(const unsigned char\* ciphertext, size\_t ciphertext\_len, unsigned char\*\* plaintext, size\_t\* plaintext\_len)



\## Алгоритм шифрования



1\. Генерация случайного IV (16 байт)

2\. Шифрование AES-256-CBC

3\. Кодирование результата в base64

4\. Формирование строки: type=enc;data=<base64>



\## Алгоритм дешифрования



1\. Извлечение base64-строки из полученного сообщения

2\. Декодирование из base64

3\. Извлечение IV (первые 16 байт)

4\. Расшифровка AES-256-CBC



\## Ключ шифрования



Ключ длиной 32 байта берётся из переменной окружения MSG\_KEY.



const char\* key = getenv("MSG\_KEY");

if (!key || strlen(key) != 32) {

&#x20;   fprintf(stderr, "MSG\_KEY must be 32 bytes\\n");

&#x20;   exit(1);

}



\## Защита от атак



Replay attack - используется случайный IV для каждого сообщения

Padding oracle - проверка padding перед расшифровкой

Brute force - 256 бит = 1.15 x 10^77 вариантов



\## Проверка через Wireshark



1\. Запустить Wireshark

2\. Захватить трафик на loopback интерфейсе

3\. Отправить сообщение

4\. Найти пакет с портом 5555

5\. Во вкладке Data должна быть base64-строка



Пример захвата:

type=enc;data=U2FsdGVkX1/3j5k7G8pQrZyLmNvBw4tY6uIxO...



\## Кроссплатформенность



Сообщения, зашифрованные на Linux, расшифровываются на Windows и наоборот.



\## Производительность



Шифрование 1KB: около 0.03 мс

Дешифрование 1KB: около 0.03 мс

Накладные расходы: около 64 байт


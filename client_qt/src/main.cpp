// Точка входа графического клиента на Qt:
// создаёт QApplication, сетевой клиент, показывает диалог логина,
// затем запускает главное окно мессенджера
#include <QApplication>

#include "NetClient.h"
#include "LoginDialog.h"
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    // Главный объект Qt-приложения
    QApplication app(argc, argv);

    // Сетевой клиент (обрабатывает протокол, шифрование и TCP)
    NetClient net;

    // Диалог входа/регистрации
    LoginDialog dlg(&net);
    if (dlg.exec() != QDialog::Accepted) {
        // Пользователь отменил вход
        return 0;
    }

    // Главное окно чатов
    MainWindow w(&net);
    w.show();

    // Запуск цикла обработки событий Qt
    return app.exec();
}
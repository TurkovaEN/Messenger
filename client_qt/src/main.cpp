#include <QApplication>
#include "NetClient.h"
#include "LoginDialog.h"
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    NetClient net;

    LoginDialog dlg(&net);
    if (dlg.exec() != QDialog::Accepted) {
        return 0;
    }

    MainWindow w(&net);
    w.show();
    return app.exec();
}

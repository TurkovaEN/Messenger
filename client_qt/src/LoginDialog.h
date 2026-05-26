#pragma once

// Диалог авторизации/подключения: ввод host/port/user и опция Register
#include <QDialog>

class QLineEdit;
class QCheckBox;
class QPushButton;
class NetClient;

class LoginDialog : public QDialog {
    Q_OBJECT

public:
    // Создаёт диалог, привязанный к NetClient (для подключения и получения событий)
    explicit LoginDialog(NetClient* net, QWidget* parent = nullptr);

    // Текущие значения полей ввода
    QString host() const;
    quint16 port() const;
    QString user() const;

    // Флаг "Register" (нужно ли сначала отправлять регистрацию)
    bool doRegister() const;

private slots:
    // Нажатие Connect: запускает подключение через NetClient
    void onConnectClicked();

    // События от NetClient: успешный логин / разрыв / ошибка / сервисное сообщение
    void onNetConnected();
    void onNetDisconnected();
    void onNetError(const QString& msg);
    void onNetMessage(const QString& msg);

private:
    // Ссылка на сетевой клиент (не владеем, создаётся в main.cpp)
    NetClient* m_net;

    // UI-элементы формы
    QLineEdit* m_host;
    QLineEdit* m_port;
    QLineEdit* m_user;
    QCheckBox* m_register;
    QPushButton* m_connectBtn;
    QPushButton* m_cancelBtn;

    // Флаг: было ли хотя бы одно успешное подключение в рамках текущего показа диалога
    bool m_connectedOnce = false;
};
// Реализация диалога логина/регистрации для Qt-клиента
#include "LoginDialog.h"
#include "NetClient.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>

LoginDialog::LoginDialog(NetClient* net, QWidget* parent)
    : QDialog(parent), m_net(net)
{
    // Настраиваем заголовок и модальность
    setWindowTitle("Messenger login");
    setModal(true);

    // Корневой layout диалога
    auto* root = new QVBoxLayout();

    // Строка ввода host/port
    auto* row1 = new QHBoxLayout();
    m_host = new QLineEdit("127.0.0.1");
    m_port = new QLineEdit("5555");
    row1->addWidget(new QLabel("Host:"));
    row1->addWidget(m_host);
    row1->addWidget(new QLabel("Port:"));
    row1->addWidget(m_port);
    root->addLayout(row1);

    // Строка ввода user + чекбокс регистрации
    auto* row2 = new QHBoxLayout();
    m_user = new QLineEdit("alice");
    m_register = new QCheckBox("Register");
    row2->addWidget(new QLabel("User:"));
    row2->addWidget(m_user);
    row2->addWidget(m_register);
    root->addLayout(row2);

    // Ряд кнопок Connect/Cancel
    auto* btnRow = new QHBoxLayout();
    m_connectBtn = new QPushButton("Connect");
    m_cancelBtn = new QPushButton("Cancel");
    btnRow->addStretch(1);
    btnRow->addWidget(m_connectBtn);
    btnRow->addWidget(m_cancelBtn);
    root->addLayout(btnRow);

    // Устанавливаем layout на диалог
    setLayout(root);

    // Обработчики UI
    connect(m_connectBtn, &QPushButton::clicked, this, &LoginDialog::onConnectClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &LoginDialog::reject);

    // Подписываемся на события NetClient, чтобы реагировать на результат логина
    connect(m_net, &NetClient::connected, this, &LoginDialog::onNetConnected);
    connect(m_net, &NetClient::disconnected, this, &LoginDialog::onNetDisconnected);
    connect(m_net, &NetClient::error, this, &LoginDialog::onNetError);
    connect(m_net, &NetClient::message, this, &LoginDialog::onNetMessage);
}

// Возвращает host из поля ввода
QString LoginDialog::host() const { return m_host->text().trimmed(); }

// Возвращает порт, приведённый к 16-битному значению
quint16 LoginDialog::port() const { return (quint16)m_port->text().toUShort(); }

// Возвращает username из поля ввода
QString LoginDialog::user() const { return m_user->text().trimmed(); }

// Возвращает флаг "Register"
bool LoginDialog::doRegister() const { return m_register->isChecked(); }

// Нажатие кнопки Connect: инициируем подключение через NetClient
void LoginDialog::onConnectClicked() {
    // Валидация логина
    if (user().isEmpty()) {
        QMessageBox::warning(this, "Error", "Username is empty");
        return;
    }

    // Сбрасываем состояние и запускаем подключение
    m_connectedOnce = false;
    m_net->connectTo(host(), port(), user(), doRegister());

    // Пока идёт соединение — блокируем кнопку Connect, чтобы не спамить попытками
    m_connectBtn->setEnabled(false);
}

// Сигнал от NetClient: успешный логин ("login ok") -> закрываем диалог с Accepted
void LoginDialog::onNetConnected() {
    m_connectedOnce = true;
    accept();
}

// Если соединение оборвалось до успешного логина — даём повторить попытку
void LoginDialog::onNetDisconnected() {
    if (!m_connectedOnce) {
        m_connectBtn->setEnabled(true);
    }
}

// Сетевые ошибки показываем пользователю через QMessageBox
void LoginDialog::onNetError(const QString& msg) {
    QMessageBox::warning(this, "Network error", msg);
    m_connectBtn->setEnabled(true);
}

// Обрабатываем текстовые сообщения: при "[error]" (например логин не прошёл) показываем alert
void LoginDialog::onNetMessage(const QString& msg) {
    // if login failed, NetClient sends [error] text via message()
    if (msg.startsWith("[error]")) {
        QMessageBox::warning(this, "Login error", msg);
        m_connectBtn->setEnabled(true);
    }
}
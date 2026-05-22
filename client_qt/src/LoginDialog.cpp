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
    setWindowTitle("Messenger login");
    setModal(true);

    auto* root = new QVBoxLayout();

    auto* row1 = new QHBoxLayout();
    m_host = new QLineEdit("127.0.0.1");
    m_port = new QLineEdit("5555");
    row1->addWidget(new QLabel("Host:"));
    row1->addWidget(m_host);
    row1->addWidget(new QLabel("Port:"));
    row1->addWidget(m_port);
    root->addLayout(row1);

    auto* row2 = new QHBoxLayout();
    m_user = new QLineEdit("alice");
    m_register = new QCheckBox("Register");
    row2->addWidget(new QLabel("User:"));
    row2->addWidget(m_user);
    row2->addWidget(m_register);
    root->addLayout(row2);

    auto* btnRow = new QHBoxLayout();
    m_connectBtn = new QPushButton("Connect");
    m_cancelBtn = new QPushButton("Cancel");
    btnRow->addStretch(1);
    btnRow->addWidget(m_connectBtn);
    btnRow->addWidget(m_cancelBtn);
    root->addLayout(btnRow);

    setLayout(root);

    connect(m_connectBtn, &QPushButton::clicked, this, &LoginDialog::onConnectClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &LoginDialog::reject);

    connect(m_net, &NetClient::connected, this, &LoginDialog::onNetConnected);
    connect(m_net, &NetClient::disconnected, this, &LoginDialog::onNetDisconnected);
    connect(m_net, &NetClient::error, this, &LoginDialog::onNetError);
    connect(m_net, &NetClient::message, this, &LoginDialog::onNetMessage);
}

QString LoginDialog::host() const { return m_host->text().trimmed(); }
quint16 LoginDialog::port() const { return (quint16)m_port->text().toUShort(); }
QString LoginDialog::user() const { return m_user->text().trimmed(); }
bool LoginDialog::doRegister() const { return m_register->isChecked(); }

void LoginDialog::onConnectClicked() {
    if (user().isEmpty()) {
        QMessageBox::warning(this, "Error", "Username is empty");
        return;
    }
    m_connectedOnce = false;
    m_net->connectTo(host(), port(), user(), doRegister());
    m_connectBtn->setEnabled(false);
}

void LoginDialog::onNetConnected() {
    m_connectedOnce = true;
    accept();
}

void LoginDialog::onNetDisconnected() {
    if (!m_connectedOnce) {
        m_connectBtn->setEnabled(true);
    }
}

void LoginDialog::onNetError(const QString& msg) {
    QMessageBox::warning(this, "Network error", msg);
    m_connectBtn->setEnabled(true);
}

void LoginDialog::onNetMessage(const QString& msg) {
    // if login failed, NetClient sends [error] text via message()
    if (msg.startsWith("[error]")) {
        QMessageBox::warning(this, "Login error", msg);
        m_connectBtn->setEnabled(true);
    }
}

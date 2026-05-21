#include "MainWindow.h"
#include "NetClient.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      m_net(new NetClient(this))
{
    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout();

    auto* connRow = new QHBoxLayout();
    m_host = new QLineEdit("127.0.0.1");
    m_port = new QLineEdit("5555");
    m_user = new QLineEdit("alice");
    m_connectBtn = new QPushButton("Connect");

    connRow->addWidget(new QLabel("Host:"));
    connRow->addWidget(m_host);
    connRow->addWidget(new QLabel("Port:"));
    connRow->addWidget(m_port);
    connRow->addWidget(new QLabel("User:"));
    connRow->addWidget(m_user);
    connRow->addWidget(m_connectBtn);
    root->addLayout(connRow);

    m_log = new QTextEdit();
    m_log->setReadOnly(true);
    root->addWidget(m_log);

    auto* sendRow = new QHBoxLayout();
    m_to = new QLineEdit("bob");
    m_text = new QLineEdit();
    m_sendBtn = new QPushButton("Send");

    sendRow->addWidget(new QLabel("To:"));
    sendRow->addWidget(m_to);
    sendRow->addWidget(new QLabel("Text:"));
    sendRow->addWidget(m_text);
    sendRow->addWidget(m_sendBtn);
    root->addLayout(sendRow);

    central->setLayout(root);
    setCentralWidget(central);

    connect(m_connectBtn, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(m_sendBtn, &QPushButton::clicked, this, &MainWindow::onSendClicked);

    connect(m_net, &NetClient::connected, this, &MainWindow::onNetConnected);
    connect(m_net, &NetClient::disconnected, this, &MainWindow::onNetDisconnected);
    connect(m_net, &NetClient::error, this, &MainWindow::onNetError);
    connect(m_net, &NetClient::message, this, &MainWindow::onNetMessage);

    setUiEnabled(false);
}

MainWindow::~MainWindow() = default;

void MainWindow::setUiEnabled(bool connected) {
    m_to->setEnabled(connected);
    m_text->setEnabled(connected);
    m_sendBtn->setEnabled(connected);
}

void MainWindow::onConnectClicked() {
    const QString host = m_host->text().trimmed();
    const quint16 port = (quint16)m_port->text().toUShort();
    const QString user = m_user->text().trimmed();

    m_log->append(QString("Connecting to %1:%2 as %3...").arg(host).arg(port).arg(user));
    m_net->connectTo(host, port, user);
}

void MainWindow::onSendClicked() {
    const QString to = m_to->text().trimmed();
    const QString text = m_text->text();
    if (to.isEmpty() || text.isEmpty()) return;

    m_net->sendDm(to, text);
    m_text->clear();
}

void MainWindow::onNetConnected() {
    m_log->append("[net] connected");
    setUiEnabled(true);
}

void MainWindow::onNetDisconnected() {
    m_log->append("[net] disconnected");
    setUiEnabled(false);
}

void MainWindow::onNetError(const QString& msg) {
    m_log->append("[error] " + msg);
}

void MainWindow::onNetMessage(const QString& msg) {
    m_log->append(msg);
}

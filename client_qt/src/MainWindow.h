#pragma once

#include <QMainWindow>

class QLineEdit;
class QPushButton;
class QTextEdit;
class QCheckBox;

class NetClient;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onConnectClicked();
    void onSendClicked();

    void onNetConnected();
    void onNetDisconnected();
    void onNetError(const QString& msg);
    void onNetMessage(const QString& msg);

private:
    void setUiEnabled(bool connected);

    NetClient* m_net;

    QLineEdit* m_host;
    QLineEdit* m_port;
    QLineEdit* m_user;
    QPushButton* m_connectBtn;
    QCheckBox* m_register;

    QTextEdit* m_log;

    QLineEdit* m_to;
    QLineEdit* m_text;
    QPushButton* m_sendBtn;
};

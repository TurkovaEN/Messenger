#pragma once
#include <QDialog>

class QLineEdit;
class QCheckBox;
class QPushButton;

class NetClient;

class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(NetClient* net, QWidget* parent = nullptr);

    QString host() const;
    quint16 port() const;
    QString user() const;
    bool doRegister() const;

private slots:
    void onConnectClicked();
    void onNetConnected();
    void onNetDisconnected();
    void onNetError(const QString& msg);
    void onNetMessage(const QString& msg);

private:
    NetClient* m_net;

    QLineEdit* m_host;
    QLineEdit* m_port;
    QLineEdit* m_user;
    QCheckBox* m_register;
    QPushButton* m_connectBtn;
    QPushButton* m_cancelBtn;

    bool m_connectedOnce = false;
};

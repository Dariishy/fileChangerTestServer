#ifndef CLIENTHANDLER_H
#define CLIENTHANDLER_H

#include <QObject>
#include <QTcpSocket>
#include <QFile>
#include <QDataStream>
#include <QThread>

class ClientHandler : public QObject
{
    Q_OBJECT

public:
    explicit ClientHandler(qintptr socketDescriptor, QObject *parent = nullptr);

public slots:
    void start();        // Инициализация после перемещения в поток
    void readClient();   // Чтение данных от клиента

private slots:
    void onDisconnected();

private:
    void sendFileRequest(const QString &fileName);
    void sendFileData();

    qintptr m_socketDescriptor;
    QTcpSocket *m_socket;
    QDataStream m_in;
    QString m_pendingFileName; // Имя файла, ожидающее подтверждения
    bool m_transferApproved = false;
};

#endif // CLIENTHANDLER_H

#ifndef SERVER_H
#define SERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QFile>
#include <QFileInfo>
#include <QQueue>
#include <QTimer>

struct fileTransfer
{
    QString filePath;
    QFile *file;
    qint64 fileSize;
    qint64 bytesRemaining;
    QString fileName;
};

class Server : public QTcpServer
{
    Q_OBJECT

public:
    Server();
    void sendFileToClient(const QString &filePath);
    void incomingConnection(qintptr socketDescriptor);
    QString filePath;

private slots:
    void onDisconnected();
    void onBytesWritten();
    void onReadyRead();
    void processingNextFile();

private:

    QTcpSocket *socket;
    QQueue<fileTransfer> transferQueue;
    fileTransfer currentTransfer;
    bool isSendingFile;
    quint64 expectedBlockSize = 0;

    void sendFileChunk();
    void startFileTransfer(const fileTransfer &transfer);
};

#endif // SERVER_H

// clienthandler.cpp
#include "clienthandler.h"
#include <QDebug>

ClientHandler::ClientHandler(qintptr socketDescriptor, QObject *parent)
    : QObject(parent), m_socketDescriptor(socketDescriptor)
{
    qDebug() << "Client handler created for socket:" << socketDescriptor;
}

void ClientHandler::start()
{
    m_socket = new QTcpSocket();
    if (!m_socket->setSocketDescriptor(m_socketDescriptor)) {
        qDebug() << "Error setting socket descriptor";
        deleteLater();
        return;
    }

    m_in.setDevice(m_socket);
    m_in.setVersion(QDataStream::Qt_6_9);

    connect(m_socket, &QTcpSocket::disconnected, this, &ClientHandler::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &ClientHandler::readClient);

    qDebug() << "Client connected in thread:" << QThread::currentThreadId();

    // Отправляем клиенту запрос на первый файл
    sendFileRequest("example_large_file.zip");
}

void ClientHandler::readClient()
{
    QDataStream in(m_socket);
    in.setVersion(QDataStream::Qt_6_9);

    // Читаем ответ клиента
    QString response;
    in >> response;
    qDebug() << "Received response from client:" << response;

    if (response == "accept") {
        m_transferApproved = true;
        sendFileData(); // Начинаем передачу файла
    } else {
        qDebug() << "Client rejected the file transfer";
        // Можно отправить запрос на другой файл или закрыть соединение
    }
}

void ClientHandler::sendFileRequest(const QString &fileName)
{
    if (!m_socket || !m_socket->isOpen()) return;

    m_pendingFileName = fileName;

    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_9);

    out << quint16(0) << QString("FILE_REQUEST") << fileName;
    out.device()->seek(0);
    out << quint16(block.size() - sizeof(quint16));

    m_socket->write(block);
    qDebug() << "Sent file request for:" << fileName;
}

void ClientHandler::sendFileData()
{
    QFile file(m_pendingFileName);
    if (!file.exists()) {
        qDebug() << "File does not exist:" << m_pendingFileName;
        return;
    }

    if (!file.open(QFile::ReadOnly)) {
        qDebug() << "Cannot open file:" << m_pendingFileName;
        return;
    }

    // Отправляем заголовок с информацией о файле
    QByteArray header;
    QDataStream out(&header, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_9);

    quint64 fileSize = file.size();
    out << quint16(0) << QString("FILE_DATA") << QFileInfo(file).fileName() << fileSize;
    out.device()->seek(0);
    out << quint16(header.size() - sizeof(quint16));

    m_socket->write(header);

    // Передаем файл блоками для экономии памяти
    qint64 bytesSent = 0;
    char buffer[64 * 1024]; // Блок 64 КБ
    while (!file.atEnd()) {
        qint64 bytesRead = file.read(buffer, sizeof(buffer));
        if (bytesRead > 0) {
            qint64 result = m_socket->write(buffer, bytesRead);
            if (result == -1) {
                qDebug() << "Error writing to socket";
                break;
            }
            bytesSent += result;
            // Для очень больших файлов здесь можно реализовать отправку прогресса
        }
    }

    file.close();
    qDebug() << "File sent:" << m_pendingFileName << "Size:" << bytesSent << "bytes";
    // После передачи можно отправить запрос на следующий файл или завершить соединение
}

void ClientHandler::onDisconnected()
{
    qDebug() << "Client disconnected";
    m_socket->deleteLater();
    deleteLater();
}

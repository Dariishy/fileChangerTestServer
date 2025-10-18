#include "server.h"
#include <QDebug>

Server::Server() : socket(nullptr), isSendingFile(false)
{
    if(this->listen(QHostAddress::Any, 2323))
    {
        qDebug() << "Сервер запущен на порту 2323";
    }
    else
    {
        qDebug() << "Ошибка запуска сервера";
    }
}

void Server::incomingConnection(qintptr socketDescriptor)//Новое соединение
{
    //Если клиент уже подключен
    if(socket && socket->state() == QAbstractSocket::ConnectedState)
    {
        qDebug() << "Сервер занят";
        QTcpSocket* newSocket = new QTcpSocket;
        newSocket->setSocketDescriptor(socketDescriptor);
        newSocket->disconnectFromHost();
        newSocket->deleteLater();
        return;
    }

    socket = new QTcpSocket;
    socket->setSocketDescriptor(socketDescriptor);

    connect(socket, &QTcpSocket::disconnected, this, &Server::onDisconnected);
    connect(socket, &QTcpSocket::bytesWritten, this, &Server::onBytesWritten);
    connect(socket, &QTcpSocket::readyRead, this, &Server::onReadyRead);

    qDebug() << "Клиент подключен:" << socketDescriptor;
    isSendingFile = false;
}

void Server::sendFileToClient(const QString &filePath)//Отправка файла
{
    if(!socket || socket->state() != QAbstractSocket::ConnectedState)
    {
        qDebug() << "Нет подключенного клиента";
        return;
    }

    QFile file(filePath);
    if(!file.exists())
    {
        qDebug() << "Файл не существует:" << filePath;
        return;
    }

    fileTransfer transfer;
    transfer.filePath = filePath;
    transfer.fileName = QFileInfo(filePath).fileName();
    transfer.fileSize = file.size();
    transfer.bytesRemaining = transfer.fileSize;
    transfer.file = nullptr;

    transferQueue.enqueue(transfer);
    qDebug() << "Файл добавлен в очередь:" << transfer.fileName << "Размер:" << transfer.fileSize << "байт";

    if(!isSendingFile && !transferQueue.isEmpty())
    {
        processingNextFile();
    }

    // if(!file.open(QFile::ReadOnly))
    // {
    //     qDebug() << "Не удалось открыть файл:" << file.errorString();
    //     return;
    // }

    // // информация о текущем файле
    // currentFile = new QFile(filePath);
    // if(!currentFile->open(QFile::ReadOnly))
    // {
    //     qDebug() << "Ошибка чтения файла";
    //     delete currentFile;
    //     currentFile = nullptr;
    //     return;
    // }

    // fileSize = currentFile->size();
    // bytesRemaining = fileSize;
    // isSendingFile = true;

    // qDebug() << "Передача файла:" << QFileInfo(filePath).fileName() << "/nРазмер:" << fileSize << "байт/n";

    // // Информация о файле
    // QByteArray header;
    // QDataStream out(&header, QIODevice::WriteOnly);
    // out.setVersion(QDataStream::Qt_6_9);

    // // размер имени + имя файла + размер файла
    // QString fileName = QFileInfo(filePath).fileName();
    // out << quint64(0) << fileName << quint64(fileSize);
    // out.device()->seek(0);
    // out << quint64(header.size() - sizeof(quint64));

    // socket->write(header);
    // qDebug() << "Отправлено имя файла, размер:" << header.size() << "байт";

    // QTimer::singleShot(0, this, &Server::sendFileChunk);
}

void Server::sendFileChunk()//64Кб
{
    if(!currentTransfer.file || !isSendingFile || currentTransfer.bytesRemaining <= 0)
        return;

    // chunk размером 64KB
    qint64 chunkSize = qMin(currentTransfer.bytesRemaining, qint64(65536));
    QByteArray data = currentTransfer.file->read(chunkSize);

    if(data.isEmpty())
    {
        qDebug() << "Ошибка чтения файла";
        onDisconnected();
        return;
    }

    qint64 written = socket->write(data);
    if(written == -1)
    {
        qDebug() << "Ошибка отправки данных:" << socket->errorString();
        onDisconnected();
        return;
    }

    currentTransfer.bytesRemaining -= written;

    if(currentTransfer.bytesRemaining <= 0)
    {
        qDebug() << "Файл успешно отправлен";
        currentTransfer.file->close();
        delete currentTransfer.file;
        currentTransfer.file = nullptr;
        isSendingFile = false;
        QTimer::singleShot(0, this, &Server::processingNextFile);
    }
}

void Server::onBytesWritten()//Отправка следующего куска
{
    if(isSendingFile && currentTransfer.bytesRemaining > 0)
    {
        QTimer::singleShot(0, this, &Server::sendFileChunk);
    }
}

void Server::onReadyRead()//Чтение команды
{
    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_6_9);

    while(true)
    {
        if (expectedBlockSize == 0)
        {
            if (socket->bytesAvailable() < sizeof(quint64))
                return;
            in >> expectedBlockSize;
            if (in.status() != QDataStream::Ok)
            {
                qDebug() << "Ошибка чтения размера блока";
                expectedBlockSize = 0;
                return;
            }
        }
        if (socket->bytesAvailable() < expectedBlockSize)
            return;

        QString command;
        in >> command;
        qDebug() << "Получена команда:" << command;
        if (in.status() != QDataStream::Ok)
        {
            qDebug() << "Ошибка чтения команды";
            expectedBlockSize = 0;
            return;
        }
        if (command == "GET_FILE")
        {
            if (!filePath.isEmpty())
            {
                sendFileToClient(filePath);
            }
            else
            {
                qDebug() << "Путь к файлу не установлен!";
            }
        }
        else
        {
            qDebug() << "Неизвестная команда:" << command;
        }
        expectedBlockSize = 0;
        if (socket->bytesAvailable() == 0)
            break;
    }
}

void Server::onDisconnected()//Отключение клиента
{
    qDebug() << "Клиент отключен";

    if(currentTransfer.file)
    {
        currentTransfer.file->close();
        delete currentTransfer.file;
        currentTransfer.file = nullptr;
    }

    transferQueue.clear();
    isSendingFile = false;

    if(socket)
    {
        socket->deleteLater();
        socket = nullptr;
    }
}

void Server::startFileTransfer(const fileTransfer &transfer)
{
    //Если файл пустой
    if(transfer.fileSize == 0)
    {
        qDebug() << "Передача пустого файла:" << transfer.fileName;

        QByteArray header;
        QDataStream out(&header, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_9);

        out << quint64(0) << transfer.fileName << quint64(0);
        out.device()->seek(0);
        out << quint64(header.size() - sizeof(quint64));

        socket->write(header);
        qDebug() << "Отправлен заголовок пустого файла";

        isSendingFile = false;
        QTimer::singleShot(0, this, &Server::processingNextFile);
        return;
    }

    currentTransfer.file = new QFile(transfer.filePath);
    if(!currentTransfer.file->open(QFile::ReadOnly))
    {
        qDebug() << "Ошибка чтения файла:" << transfer.filePath;
        delete currentTransfer.file;
        currentTransfer.file = nullptr;

        // Пробуем обработать следующий файл
        QTimer::singleShot(0, this, &Server::processingNextFile);
        return;
    }

    isSendingFile = true;

    qDebug() << "Начата передача файла:" << currentTransfer.fileName
             << "Размер:" << currentTransfer.fileSize << "байт";

    // Отправляем заголовок с информацией о файле
    QByteArray header;
    QDataStream out(&header, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_9);

    out << quint64(0) << currentTransfer.fileName << quint64(currentTransfer.fileSize);
    out.device()->seek(0);
    out << quint64(header.size() - sizeof(quint64));

    socket->write(header);
    qDebug() << "Отправлен заголовок файла, размер:" << header.size() << "байт";
}

void Server::processingNextFile()
{
    if(transferQueue.isEmpty()||isSendingFile)
        return;

    currentTransfer = transferQueue.dequeue();
    startFileTransfer(currentTransfer);
}

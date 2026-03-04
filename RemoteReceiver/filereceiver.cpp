#include "filereceiver.h"
#include "qdir.h"
#include <QDebug>
#include <QFileInfo>
#include <QByteArray>
#include <QDateTime>
#include <QException>
#include <QSharedPointer>
#include <QAbstractSocket>

// Constructor: Initialize TCP server and start listening on port 9999
FileReceiver::FileReceiver(QObject *parent) : QTcpServer(parent)
{
    // ========== Fix 1: Force release old port resources before listening ==========
    if (this->isListening()) {
        this->close(); // Close old listening first (if exists) to release port
    }

    // Start TCP server and listen on port 9999 (all network interfaces)
    if (!this->listen(QHostAddress::Any, 9999)) {
        qCritical() << "Failed to start server:" << this->errorString();
        return;
    }
    qInfo() << "File receiver running on port 9999";
    qInfo() << "Supports custom remote save paths (auto-creates directories)";

    // Critical reinforcement 1: Enhance listening restart logic - release port first then restart
    connect(this, &QTcpServer::acceptError, this, [=](QAbstractSocket::SocketError error) {
        qCritical() << "Server accept error:" << this->errorString();
        // Attempt to restart listening if listening fails (prevent accidental port loss)
        if (!this->isListening()) {
            this->close(); // Close old listening first to release port
            this->listen(QHostAddress::Any, 9999);
            qInfo() << "Restarted server listening on port 9999";
        }
    });
}

// Override: Handle new incoming TCP connections
void FileReceiver::incomingConnection(qintptr socketDescriptor)
{
    // Critical reinforcement 2: Catch all exceptions to prevent program crash
    try {
        // Create new QTcpSocket for client communication
        QTcpSocket *clientSocket = new QTcpSocket(this);
        if (!clientSocket->setSocketDescriptor(socketDescriptor)) {
            qCritical() << "Failed to set socket descriptor:" << clientSocket->errorString();
            // ========== Fix 2: Clean up Socket completely on failure ==========
            clientSocket->disconnect(); // Disconnect all signal-slot connections first
            clientSocket->abort();      // Force close connection to release descriptor
            clientSocket->deleteLater();
            return;
        }

        qInfo() << "New connection from:" << clientSocket->peerAddress().toString()
                << "Socket descriptor:" << socketDescriptor;

        // ========== Critical Modification 1: Manage state with smart pointer ==========
        struct TransferState {
            QFile file;                      // File object for writing received data
            QString remotePath;              // Target path to save file on remote machine
            bool isReceivingPath = true;     // Phase 1: Receiving remote path (length + content)
            bool isReceivingFileSize = false;// Phase 2: Receiving file size (4-byte header)
            bool isReceivingFile = false;    // Phase 3: Receiving actual file content
            quint32 pathLen = 0;             // Length of remote path (from 4-byte header)
            quint32 fileSize = 0;            // Total expected file size (from client metadata)
            qint64 bytesReceived = 0;        // Total bytes received so far
            bool isDeleted = false;          // Flag to mark if state is deleted
        };
        QSharedPointer<TransferState> state = QSharedPointer<TransferState>(new TransferState());

        // Slot: Handle incoming data from client socket (core receive logic)
        connect(clientSocket, &QTcpSocket::readyRead, this, [=]() {
            if (!state || state->isDeleted) return;

            try {
                QTcpSocket *sock = qobject_cast<QTcpSocket*>(sender());
                if (!sock) return;

                // Phase 1: Receive remote save path (protocol: 4-byte length header + UTF-8 path content)
                if (state->isReceivingPath) {
                    if (state->pathLen == 0 && sock->bytesAvailable() >= 4) {
                        QByteArray lenBytes = sock->read(4);
                        QDataStream lenStream(lenBytes);
                        lenStream.setByteOrder(QDataStream::BigEndian);
                        lenStream >> state->pathLen;
                    }
                    if (state->pathLen > 0 && sock->bytesAvailable() >= state->pathLen) {
                        QByteArray pathData = sock->read(state->pathLen);
                        state->remotePath = QString::fromUtf8(pathData);

                        QFileInfo fileInfo(state->remotePath);
                        QDir dir;
                        if (!dir.mkpath(fileInfo.path())) {
                            qCritical() << "Failed to create directory:" << fileInfo.path();
                            sock->write("ERROR: Cannot create directory");
                            sock->disconnectFromHost();
                            state->isDeleted = true;
                            return;
                        }

                        QString targetPath = state->remotePath;
                        if (QFile::exists(targetPath)) {
                            QString bakPath = targetPath + "." + QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz") + ".bak";
                            if (QFile::rename(targetPath, bakPath)) {
                                qInfo() << "Renamed existing file to:" << bakPath;
                                if (!QFile::remove(bakPath)) {
                                    qWarning() << "Failed to delete old file:" << bakPath;
                                    sock->write("ERROR: Failed to delete old file");
                                    sock->disconnectFromHost();
                                    state->isDeleted = true;
                                    return;
                                }
                            } else {
                                qCritical() << "Failed to rename existing file:" << targetPath;
                                sock->write("ERROR: Failed to rename old file");
                                sock->disconnectFromHost();
                                state->isDeleted = true;
                                return;
                            }
                        }

                        state->file.setFileName(targetPath);
                        if (!state->file.open(QIODevice::WriteOnly)) {
                            qCritical() << "Cannot open file for writing:" << targetPath;
                            sock->write("ERROR: Cannot open file");
                            sock->disconnectFromHost();
                            state->isDeleted = true;
                            return;
                        }

                        qInfo() << "Ready to receive file size (next 4 bytes) to:" << targetPath;
                        state->isReceivingPath = false;
                        state->isReceivingFileSize = true;
                    }
                }

                // Phase 2: Receive file size (4-byte big-endian header from client)
                if (state->isReceivingFileSize) {
                    if (sock->bytesAvailable() >= 4) {
                        QByteArray sizeBytes = sock->read(4);
                        QDataStream sizeStream(sizeBytes);
                        sizeStream.setByteOrder(QDataStream::BigEndian);
                        sizeStream >> state->fileSize;
                        qInfo() << "Expected file size:" << state->fileSize << "bytes";
                        state->isReceivingFileSize = false;
                        state->isReceivingFile = true;
                    }
                }

                // Phase 3: Receive file content (core fix: loop to read all data)
                if (state->isReceivingFile) {
                    while (sock->bytesAvailable() > 0 && state->bytesReceived < state->fileSize) {
                        qint64 bytesToRead = qMin((qint64)sock->bytesAvailable(), state->fileSize - state->bytesReceived);
                        QByteArray data = sock->read(bytesToRead);
                        if (data.isEmpty()) break;

                        qint64 written = state->file.write(data);
                        if (written == -1) {
                            qCritical() << "Failed to write file data";
                            sock->write("ERROR: Write file failed");
                            sock->disconnectFromHost();
                            state->file.close();
                            state->isDeleted = true;
                            return;
                        }

                        state->bytesReceived += written;
                        qDebug() << "Received:" << state->bytesReceived << "/" << state->fileSize << "bytes";
                    }

                    if (state->bytesReceived == state->fileSize) {
                        state->file.close();
                        qInfo() << "File received completely! Total size:" << state->bytesReceived << "bytes";
                        sock->write("OK");
                        sock->waitForBytesWritten(1000);
                        state->isDeleted = true;
                        // ========== Fix 3: Actively disconnect Socket after transfer completion ==========
                        sock->disconnectFromHost(); // Actively disconnect to avoid TIME_WAIT blocking
                    }
                }
            } catch (const QException &e) {
                qCritical() << "Data processing exception:" << e.what();
                if (state && !state->isDeleted) {
                    state->isDeleted = true;
                    if (state->file.isOpen()) state->file.close();
                }
                // ========== Fix 4: Clean up Socket completely on exception ==========
                clientSocket->disconnect();
                clientSocket->abort();
                clientSocket->deleteLater();
            } catch (...) {
                qCritical() << "Unknown data processing exception";
                if (state && !state->isDeleted) {
                    state->isDeleted = true;
                    if (state->file.isOpen()) state->file.close();
                }
                clientSocket->disconnect();
                clientSocket->abort();
                clientSocket->deleteLater();
            }
        });

        // Slot: Clean up resources when connection closed
        connect(clientSocket, &QTcpSocket::disconnected, [=]() {
            qInfo() << "Connection closed:" << clientSocket->peerAddress().toString()
            << "Socket descriptor:" << socketDescriptor;

            if (state && !state->isDeleted) {
                if (state->file.isOpen()) {
                    state->file.close();
                    qWarning() << "Transfer aborted! Received:" << state->bytesReceived << "/"
                               << (state->fileSize > 0 ? QString::number(state->fileSize) : QString("unknown")) << "bytes";
                }
                state->isDeleted = true;
            }

            // ========== Fix 5: Clean up Socket completely after disconnection (Core!) ==========
            clientSocket->disconnect(); // Disconnect all signal-slot connections to avoid repeated triggering
            clientSocket->abort();      // Force close connection to release file descriptor
            clientSocket->deleteLater();

            qInfo() << "Server still listening on port 9999:" << (this->isListening() ? "YES" : "NO");
        });

        // Slot: Handle socket errors
        connect(clientSocket, &QTcpSocket::errorOccurred, [=](QTcpSocket::SocketError error) {
            qCritical() << "Socket error:" << clientSocket->errorString()
            << "Error code:" << error;

            if (state && !state->isDeleted) {
                if (state->file.isOpen()) state->file.close();
                state->isDeleted = true;
            }
            // ========== Fix 6: Clean up Socket completely on error ==========
            clientSocket->disconnect();
            clientSocket->abort();
            clientSocket->deleteLater();
        });
    } catch (const QException &e) {
        qCritical() << "Connection handling exception:" << e.what();
    } catch (...) {
        qCritical() << "Unknown connection handling exception";
    }
}

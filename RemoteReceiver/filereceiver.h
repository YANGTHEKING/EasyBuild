#ifndef FILERECEIVER_H
#define FILERECEIVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QFile>
#include <QDir>

class FileReceiver : public QTcpServer
{
    Q_OBJECT  // MOC will process this automatically for headers
public:
    explicit FileReceiver(QObject *parent = nullptr);

protected:
    void incomingConnection(qintptr socketDescriptor) override;
};

#endif // FILERECEIVER_H

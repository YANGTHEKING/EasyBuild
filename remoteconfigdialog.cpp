#include "remoteconfigdialog.h"
#include "qthread.h"
#include <QMessageBox>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTcpSocket>
#include <QHostAddress>

RemoteConfigDialog::RemoteConfigDialog(QWidget *parent)
    : QDialog(parent)
    , m_settings(new QSettings("EazyBuild", "RemoteConfig", this))
{
    // 1. UI Layout initialization
    this->setWindowTitle("Remote Deployment Config");
    this->setFixedSize(550, 140); // Slightly expand width for hint label
    QGridLayout *layout = new QGridLayout(this);

    // Control initialization
    m_leHost = new QLineEdit(this);
    m_leHost->setPlaceholderText("192.168.1.100 or \\\\192.168.1.100 (IP/Hostname)");

    // Add format hint label for host input
    m_labHostHint = new QLabel(this);
    m_labHostHint->setStyleSheet("color: red; font-size: 10px;");
    m_labHostHint->setText(""); // Empty by default

    m_leRemotePath = new QLineEdit(this);
    QPushButton *btnTest = new QPushButton("Test Connection", this);
    QPushButton *btnOk = new QPushButton("OK", this);
    QPushButton *btnCancel = new QPushButton("Cancel", this);

    // Add controls to layout
    layout->addWidget(new QLabel("Remote Host:"), 0, 0);
    layout->addWidget(m_leHost, 0, 1, 1, 2);
    // Add format hint label below host input
    layout->addWidget(m_labHostHint, 1, 1, 1, 2);

    layout->addWidget(new QLabel("Remote Path:"), 2, 0);
    layout->addWidget(m_leRemotePath, 2, 1);

    layout->addWidget(btnTest, 3, 1);
    layout->addWidget(btnOk, 4, 1);
    layout->addWidget(btnCancel, 4, 2);

    // 2. Signal-slot connections
    connect(btnTest, &QPushButton::clicked, this, &RemoteConfigDialog::onTestConnection);
    connect(btnOk, &QPushButton::clicked, this, [=]() {
        // Validate host format before saving configuration
        if (!isValidHostFormat(m_leHost->text().trimmed())) {
            QMessageBox::warning(this, "Invalid Format",
                                 "Please input valid host format!\nExample: 10.40.43.202 or \\\\10.40.43.202");
            m_leHost->setFocus();
            return;
        }
        saveConfig();
        this->accept();
    });
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    // Real-time format check while typing in host input
    connect(m_leHost, &QLineEdit::textChanged, this, &RemoteConfigDialog::onHostTextChanged);

    // 3. Load historical configuration from local storage
    loadConfig();
    // Initial format check for loaded configuration
    onHostTextChanged(m_leHost->text());
}

// Load locally saved remote configuration parameters
void RemoteConfigDialog::loadConfig()
{
    m_leHost->setText(m_settings->value("host").toString());
    m_leRemotePath->setText(m_settings->value("path").toString());
}

// Save current remote configuration to local storage
void RemoteConfigDialog::saveConfig()
{
    // Save normalized host path (add \\ prefix if missing)
    QString normalizedHost = getValidUNCPath(m_leHost->text().trimmed());
    m_settings->setValue("host", normalizedHost);
    m_settings->setValue("path", m_leRemotePath->text());
}

// Validate host format (supports IP address or hostname with/without \\ prefix)
// Return true if format is valid, false otherwise
bool RemoteConfigDialog::isValidHostFormat(const QString& host)
{
    if (host.isEmpty()) return false;

    // Step 1: Remove \\ prefix (if exists) to get pure IP/hostname string
    QString cleanHost = host.trimmed();
    if (cleanHost.startsWith("\\\\")) {
        cleanHost = cleanHost.mid(2);
    }

    // Step 2: Strict IPv4 regex (ENFORCES 4 segments, each 0-255)
    // Regex breakdown:
    // ^ : Start of string
    // (?:...) : Non-capturing group for IP segment (0-255)
    // {3} : Exactly 3 more segments (total 4)
    // $ : End of string (no extra characters allowed)
    QRegularExpression strictIpRegex(
        R"(^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)"
        );

    // Step 3: Validate (only full IPv4 address is allowed)
    QRegularExpressionMatch match = strictIpRegex.match(cleanHost);
    return match.hasMatch();
}

// Real-time host format check handler (triggered on text change)
void RemoteConfigDialog::onHostTextChanged(const QString& text)
{
    if (text.isEmpty()) {
        m_labHostHint->setText("");
        return;
    }

    if (isValidHostFormat(text)) {
        m_labHostHint->setStyleSheet("color: green; font-size: 10px;");
        m_labHostHint->setText("✓ Valid host format");
    } else {
        m_labHostHint->setStyleSheet("color: red; font-size: 10px;");
        m_labHostHint->setText("✗ Invalid format (Example: 10.40.43.202 or \\10.40.43.202)");
    }
}

// Convert input host string to valid UNC path format
// UNC format: \\[IP/Hostname]
QString RemoteConfigDialog::getValidUNCPath(const QString& inputHost)
{
    QString host = inputHost.trimmed();
    if (!host.startsWith("\\\\")) {
        host = "\\\\" + host;
    }
    return host;
}

// Generate qualified username with host prefix (e.g., 10.40.63.202\innosilicon)
QString RemoteConfigDialog::getQualifiedUsername(const QString& inputHost, const QString& username)
{
    // Remove \\ prefix from host to get pure IP address
    QString cleanHost = inputHost.trimmed();
    if (cleanHost.startsWith("\\\\")) {
        cleanHost = cleanHost.mid(2);
    }

    // Add host prefix to username (e.g., 10.40.63.202\innosilicon)
    if (username.contains("\\")) {
        return username; // Already has host prefix, use directly
    }
    return cleanHost + "\\" + username;
}

// Test remote server connection and file transfer functionality
void RemoteConfigDialog::onTestConnection()
{

    // Remote host configuration (IP + port)
    QString remoteIp = remoteHost();   // Remote host IP
    remoteIp = remoteIp.trimmed();
    if (remoteIp.startsWith("\\\\")) {
        remoteIp = remoteIp.mid(2);
    }

    quint16 remotePort = 9999;         // Matches receiver's listening port
    int connectTimeout = 5000;         // Connection timeout (5 seconds)

    // Create TCP socket for connection test
    QTcpSocket socket;
    socket.connectToHost(remoteIp, remotePort);

    // Wait for connection (blocking with timeout, safe for UI thread in dialog)
    bool isConnected = socket.waitForConnected(connectTimeout);

    // Optional: Update UI or log connection result
    if (isConnected) {
        QMessageBox::information(this, "Success",
                                 QString("Remote server connection test succeeded: %1 : %2")
                                     .arg(remoteIp).arg(remotePort));
        // Close the connection immediately after test
        socket.disconnectFromHost();
        if (socket.state() != QTcpSocket::UnconnectedState) {
            socket.waitForDisconnected(1000);
        }
    } else {
        // Output detailed error info for troubleshooting
        QMessageBox::critical(this, "Failed",
                              QString("Remote server connection test failed: %1 : %2\nError code: %3\nError info: %4")
                                  .arg(remoteIp).arg(remotePort).arg(socket.error()).arg(socket.errorString()));
    }
}

// Core function: Send local file to remote receiver via TCP (no Python/SMB dependency)
// Parameters:
//   - remoteIp: IP address of the remote receiver server
//   - port: TCP port number of the remote receiver (must match server's listening port)
//   - localFilePath: Full path of the local file to be transferred
//   - remoteSavePath: Full target path to save the file on remote machine (including filename)
// Return: true if transfer succeeds, false if any error occurs
bool RemoteConfigDialog::sendFileToRemote(
    const QString& remoteIp,
    int port,
    const QString& localFilePath,
    const QString& remoteSavePath
    ) {
    // 1. Validate local file existence and read permission
    QFile localFile(localFilePath);
    if (!localFile.exists()) {
        QMessageBox::warning(this, "Error", QString("Local file not found:\n%1").arg(localFilePath));
        return false;
    }
    if (!localFile.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Error", QString("Cannot open file:\n%1").arg(localFile.errorString()));
        return false;
    }
    qint64 totalFileSize = localFile.size(); // Get total file size (critical for transfer completion check)
    qInfo() << "Local file size:" << totalFileSize << "bytes";

    // 2. Establish TCP connection to remote server
    QTcpSocket socket;
    socket.connectToHost(QHostAddress(remoteIp), port);
    if (!socket.waitForConnected(5000)) { // 5-second connection timeout
        QMessageBox::critical(this, "Connection Failed",
                              QString("Cannot connect to %1:%2\n%3").arg(remoteIp).arg(port).arg(socket.errorString()));
        localFile.close();
        socket.disconnectFromHost();
        return false;
    }

    // 3. Send metadata: Step 1 - Remote save path (protocol: 4-byte length header + UTF-8 encoded path)
    QByteArray pathData = remoteSavePath.toUtf8();
    quint32 pathLen = pathData.size();
    QByteArray lenBytes;
    QDataStream lenStream(&lenBytes, QIODevice::WriteOnly);
    lenStream.setByteOrder(QDataStream::BigEndian); // Match server's byte order (big-endian)
    lenStream << pathLen;

    // Send path length header (ensure complete write to socket buffer)
    if (socket.write(lenBytes) == -1 || !socket.waitForBytesWritten(2000)) {
        QMessageBox::critical(this, "Metadata Failed", "Failed to send path length");
        localFile.close();
        socket.disconnectFromHost();
        return false;
    }
    // Send actual remote path content
    if (socket.write(pathData) == -1 || !socket.waitForBytesWritten(2000)) {
        QMessageBox::critical(this, "Metadata Failed", "Failed to send remote path");
        localFile.close();
        socket.disconnectFromHost();
        return false;
    }

    // 4. Send metadata: Step 2 - File size (4-byte big-endian header, required for server completion check)
    QByteArray sizeBytes;
    QDataStream sizeStream(&sizeBytes, QIODevice::WriteOnly);
    sizeStream.setByteOrder(QDataStream::BigEndian);
    sizeStream << (quint32)totalFileSize; // Convert to 32-bit unsigned (matches server's data type)
    if (socket.write(sizeBytes) == -1 || !socket.waitForBytesWritten(2000)) {
        QMessageBox::critical(this, "Metadata Failed", "Failed to send file size");
        localFile.close();
        socket.disconnectFromHost();
        return false;
    }

    // 5. Send file data (optimized with larger buffer, retry mechanism and progress tracking)
    qint64 totalBytesSent = 0;
    const int bufferSize = 4096; // Increased buffer to 4KB (improves transfer efficiency, reduces write frequency)
    const int maxRetries = 3;    // Maximum retry attempts for failed writes
    QByteArray buffer;

    while (!(buffer = localFile.read(bufferSize)).isEmpty()) {
        // Check TCP connection status before each write operation
        if (socket.state() != QTcpSocket::ConnectedState) {
            QMessageBox::critical(this, "Transfer Failed", "Socket disconnected during transfer!");
            localFile.close();
            socket.disconnectFromHost();
            return false;
        }

        // Write logic with retry mechanism (handle transient network errors)
        int retryCount = 0;
        qint64 bytesWritten = -1;
        while (retryCount < maxRetries && bytesWritten == -1) {
            bytesWritten = socket.write(buffer);
            if (bytesWritten == -1) {
                retryCount++;
                qWarning() << "Write failed, retry" << retryCount << "/" << maxRetries;
                QThread::msleep(100); // Short delay before retry (100ms)
            }
        }
        // Final failure handling after all retries exhausted
        if (bytesWritten == -1) {
            QMessageBox::critical(this, "Transfer Failed",
                                  QString("Failed to write data after %1 retries!\n%2").arg(maxRetries).arg(socket.errorString()));
            localFile.close();
            socket.disconnectFromHost();
            return false;
        }

        // Ensure data is fully written to network buffer (extended timeout to 3 seconds)
        if (!socket.waitForBytesWritten(3000)) {
            QMessageBox::critical(this, "Transfer Failed", "Timeout waiting for bytes written");
            localFile.close();
            socket.disconnectFromHost();
            return false;
        }

        totalBytesSent += bytesWritten;
        qDebug() << "Sent:" << totalBytesSent << "/" << totalFileSize << "bytes"; // Log transfer progress
    }

    // 6. Wait for server acknowledgment (extended timeout to 10 seconds for large files)
    localFile.close();
    qInfo() << "All data sent, waiting for server ACK...";
    if (!socket.waitForReadyRead(10000)) {
        QMessageBox::critical(this, "Transfer Failed", "Timeout waiting for server confirmation");
        socket.disconnectFromHost();
        return false;
    }

    // 7. Process server acknowledgment response
    QByteArray ack = socket.readAll();
    if (ack == "OK") {
        QMessageBox::information(this, "Success",
                                 QString("File transferred successfully!\nTotal size: %1 bytes\nRemote path: %2")
                                     .arg(totalBytesSent).arg(remoteSavePath));
    } else {
        QMessageBox::critical(this, "Transfer Failed",
                              QString("Receiver returned error:\n%1").arg(ack.isEmpty() ? "No response" : ack));
    }

    // Clean up: Disconnect socket after transfer completion/failure
    socket.disconnectFromHost();
    return true;
}

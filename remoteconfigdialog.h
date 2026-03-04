#ifndef REMOTECONFIGDIALOG_H
#define REMOTECONFIGDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QGridLayout>
#include <QLabel>
#include <QSettings>
#include <QProcess>
#include <QFileDialog>
#include <QRegularExpression>

class RemoteConfigDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RemoteConfigDialog(QWidget *parent = nullptr);
    // Get saved remote configuration
    QString remoteHost() const { return m_leHost->text().trimmed(); }
    QString remotePath() const { return m_leRemotePath->text().trimmed(); }
    bool sendFileToRemote(const QString &remoteIp, int port, const QString &localFilePath, const QString &remoteSavePath);

private slots:
    // Test remote connection
    void onTestConnection();
    // Save configuration to local
    void saveConfig();
    // Load locally saved configuration
    void loadConfig();
    // Real-time check host format while typing
    void onHostTextChanged(const QString& text);

private:
    QLineEdit *m_leHost;      // Remote host (e.g., \\192.168.1.100 or 192.168.1.100)
    QLineEdit *m_leRemotePath;// Remote target path
    QLabel *m_labHostHint;    // Hint label for host format validation
    QSettings *m_settings;    // Configuration saving (reuse Qt configuration system)

    // Validate host format (IP address or hostname)
    bool isValidHostFormat(const QString& host);
    // Convert input to valid UNC path format
    QString getValidUNCPath(const QString& inputHost);
    QString getQualifiedUsername(const QString &inputHost, const QString &username);
};

#endif // REMOTECONFIGDIALOG_H

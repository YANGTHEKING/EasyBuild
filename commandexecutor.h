#ifndef COMMANDEXECUTOR_H
#define COMMANDEXECUTOR_H

#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QDebug>

class CommandExecutor : public QObject
{
    Q_OBJECT
public:
    explicit CommandExecutor(QObject *parent = nullptr);

    /**
     * @brief Generate and execute multi-line command line instructions (synchronous blocking)
     * @param commands List of multi-line instructions (one instruction per line)
     * @param workingDir Working directory (empty by default, use current program directory)
     * @return Execution result: true = success, false = failure
     */
    bool executeMultiCommands(const QStringList& commands, const QString& workingDir = "");

    /**
     * @brief Execute multi-line commands asynchronously (non-blocking, return result via signal)
     * @param commands List of multi-line instructions
     * @param workingDir Working directory
     */
    void executeMultiCommandsAsync(const QStringList& commands, const QString& workingDir = "");

signals:
    // Signal emitted when asynchronous execution completes:
    // Param1 = success status, Param2 = stdout log, Param3 = stderr log
    void commandFinished(bool success, const QString& stdoutLog, const QString& stderrLog);

private slots:
    // Internal slot: handle asynchronous process completion
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    // Concatenate multi-line commands (compatible with Windows/Linux)
    QString joinCommands(const QStringList& commands);

    QProcess* m_process; // Process object for command execution
};

#endif // COMMANDEXECUTOR_H

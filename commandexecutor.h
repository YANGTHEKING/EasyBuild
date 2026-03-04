#ifndef COMMANDEXECUTOR_H
#define COMMANDEXECUTOR_H

#include "qdir.h"
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QDebug>
#include <QFile>
#include <QDateTime>
#include <QRegularExpression>

/**
 * @brief Asynchronous command executor with real-time log output and strict execution order
 * @note Guarantees sequential execution (next command starts only after previous one finishes)
 * @note Non-blocking execution (UI remains responsive during command execution)
 */
class CommandExecutor : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief Constructor
     * @param parent Parent QObject pointer
     */
    explicit CommandExecutor(QObject *parent = nullptr);

    /**
     * @brief Execute multiple commands asynchronously (non-blocking)
     * @param commands List of commands to execute (in execution order)
     * @param Path List of files to copy (in strict execution order)
     * @param Path List of files to delete (in strict execution order)
     * @param workingDir Working directory for command execution (empty = current directory)
     */
    void executeMultiCommandsAsync(const QStringList& commands, QList<QPair<QString, QString>> pendingCopyTasks, QList<QString> pendingDelTasks, const QString& workingDir = "", bool isRemote = false);

    /**
     * @brief Set log file path for persistent log storage
     * @param path Full path to log file
     */
    void setLogFilePath(const QString& path) { m_logFilePath = path; }

    /**
     * @brief Set maximum number of log lines (prevent UI lag/memory overflow)
     * @param maxLines Maximum allowed log lines
     */
    void setMaxLogLines(int maxLines) { m_maxLogLines = maxLines; }


    void setRemoteHost(const QString& host) { m_remoteHost = host; }


    void setRemotePath(const QString& path) { m_remotePath = path; }

    /**
     * @brief Stop current command execution immediately
     */
    void stopExecution();

    QString logFilePath() {return m_logFilePath;};
    QString logFileFolder() {return m_logFileFolder;};
signals:
    /**
     * @brief Emitted when all commands finish execution
     * @param success True = all commands executed successfully
     * @param stdoutLog Combined stdout of all commands
     * @param stderrLog Combined stderr of all commands (including extracted compile errors)
     */
    void commandFinished(bool success, const QString& stdoutLog, const QString& stderrLog);

    /**
     * @brief Emitted when CMake command execution fails (including compile errors)
     * @param errorMsg Detailed error message
     * @param failedCommand The failed CMake command
     */
    void cmakeFailed(const QString& errorMsg, const QString& failedCommand);

    /**
     * @brief Emitted in real-time when new log content is available (for UI display)
     * @param logContent New log content (with timestamp)
     * @param isError True = error log (stderr/compile error), False = normal log (stdout)
     */
    void logUpdated(const QString& logContent, bool isError);

    /**
     * @brief Emitted when command execution progress changes
     * @param current Index of currently executing command (1-based)
     * @param total Total number of commands
     * @param cmd Currently executing command string
     */
    void commandProgress(int current, int total, const QString& cmd);

private slots:
    /**
     * @brief Callback when single command execution finishes (async)
     * @param exitCode Process exit code (0 = success)
     * @param exitStatus Process exit status (NormalExit/CrashExit)
     */
    void onSingleCommandFinished(int exitCode, QProcess::ExitStatus exitStatus);

    /**
     * @brief Real-time read of standard output (stdout)
     * @note Triggered when process has new stdout data available
     */
    void onReadyReadStandardOutput();

    /**
     * @brief Real-time read of standard error (stderr)
     * @note Triggered when process has new stderr data available
     */
    void onReadyReadStandardError();

private:
    /**
     * @brief Start next command in the async command list (sequential execution)
     * @note Only called after current command finishes execution
     */
    void startNextAsyncCommand();

    /**
     * @brief Extract compile errors (e.g., error C2146) from stdout to stderr
     * @param stdoutContent Full stdout content of current command
     * @return Extracted error messages (empty if no errors)
     */
    QString extractErrorsFromStdout(const QString& stdoutContent);

    /**
     * @brief Format real-time log with timestamp
     * @param content Raw log content from process output
     * @return Formatted log string with timestamp prefix
     */
    QString formatRealTimeLog(const QString& content);

    /**
     * @brief Save log content to file (persistent storage)
     * @param content Log content to save
     * @param isError True = mark as error log, False = mark as info log
     */
    void saveLog(const QString& content, bool isError = false);

    /**
     * @brief Check if CMake command actually failed (including compile errors)
     * @param cmd Executed CMake command
     * @param exitCode Process exit code
     * @param exitStatus Process exit status
     * @param stderrLog Combined stderr (including extracted errors from stdout)
     * @return True = CMake command failed
     */
    bool isCmakeReallyFailed(const QString& stderrLog);

    /**
     * @brief Parse CMake arguments to preserve space-containing values (e.g., "-G Visual Studio 16 2019")
     * @param cmd Full CMake command string
     * @return Properly split argument list (space-containing values as single items)
     */
    QStringList parseCmakeArguments(const QString& cmd);

    bool copyFileWithQt(const QString &srcFile, const QString &targetDir, QString &errorMsg);

    bool executeCopyCmds(QList<QPair<QString, QString> > pendingCopyTasks);

    bool deleteFileWithQt(const QString &targetDir, QString &errorMsg);

    bool deleteDirRecursively(const QDir &dir);

    bool executeDelCmds(QList<QString> pendingDelTasks);

    QProcess* m_process;          // Single process instance (only one command runs at a time)
    QString m_logFilePath;        // Path to persistent log file
    QString m_logFileFolder;
    QString m_currentCmdStdout;   // Stdout buffer for current command
    QString m_currentCmdStderr;   // Stderr buffer for current command
    QString m_asyncWorkingDir;    // Working directory for command execution
    QStringList m_asyncCmds;      // List of commands to execute (preserves input order)
    QString m_remoteHost;         // Remote host
    QString m_remotePath;         // Remote target path
    int m_currentAsyncCmdIdx;     // Index of currently executing command (0-based)
    int m_maxLogLines;            // Maximum log lines (prevent UI/memory issues)
    bool m_isExecuting;           // Execution state flag (prevent concurrent execution)
    bool m_isRemote;
    QList<QPair<QString, QString>> m_pendingCopyTasks;
};

#endif // COMMANDEXECUTOR_H

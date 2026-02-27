#include "CommandExecutor.h"
#include <QOperatingSystemVersion>
#include <QDir>

CommandExecutor::CommandExecutor(QObject *parent)
    : QObject(parent)
{
    m_process = new QProcess(this);
    // Bind signal for asynchronous process completion
    connect(m_process, &QProcess::finished, this, &CommandExecutor::onProcessFinished);
}

// Concatenate multi-line commands: use & for Windows, ; for Linux/macOS
QString CommandExecutor::joinCommands(const QStringList& commands)
{
    QString separator;
    // Determine operating system type
    if (QOperatingSystemVersion::currentType() == QOperatingSystemVersion::Windows)
    {
        separator = " & "; // Windows cmd multi-command separator (& = execute in sequence)
    }
    else
    {
        separator = "; "; // Linux/macOS bash separator
    }

    // Concatenate all commands and filter empty lines
    QStringList validCommands;
    for (const QString& cmd : commands)
    {
        QString trimmedCmd = cmd.trimmed();
        if (!trimmedCmd.isEmpty())
        {
            validCommands.append(trimmedCmd);
        }
    }
    return validCommands.join(separator);
}

// Execute multi-line commands synchronously (blocking until completion)
bool CommandExecutor::executeMultiCommands(const QStringList& commands, const QString& workingDir)
{
    if (commands.isEmpty())
    {
        qWarning() << "Command list is empty!";
        return false;
    }

    // 1. Concatenate multi-line commands
    QString fullCommand = joinCommands(commands);
    qInfo() << "Executing command:" << fullCommand;

    // 2. Configure process
    QProcess process;
    // Set working directory (optional)
    if (!workingDir.isEmpty() && QDir(workingDir).exists())
    {
        process.setWorkingDirectory(workingDir);
    }

    // 3. Execute command (distinguish system: cmd for Windows, bash for Linux/macOS)
    if (QOperatingSystemVersion::currentType() == QOperatingSystemVersion::Windows)
    {
        // Windows: execute via cmd /c
        process.start("cmd.exe", QStringList() << "/c" << fullCommand);
    }
    else
    {
        // Linux/macOS: execute via bash -c
        process.start("bash", QStringList() << "-c" << fullCommand);
    }

    // 4. Wait for execution completion (timeout 100 seconds, adjustable as needed)
    bool isFinished = process.waitForFinished(100000);
    if (!isFinished)
    {
        qCritical() << "Command execution timeout!";
        process.kill(); // Kill process on timeout
        return false;
    }

    // 5. Get output and error information
    QString stdoutLog = QString::fromLocal8Bit(process.readAllStandardOutput());
    QString stderrLog = QString::fromLocal8Bit(process.readAllStandardError());

    // Print log (for debugging)
    qInfo() << "Command output:\n" << stdoutLog;
    if (!stderrLog.isEmpty())
    {
        qWarning() << "Command error:\n" << stderrLog;
    }

    // 6. Check execution success (exit code 0 = success)
    return process.exitCode() == 0;
}

// Execute multi-line commands asynchronously (non-blocking)
void CommandExecutor::executeMultiCommandsAsync(const QStringList& commands, const QString& workingDir)
{
    if (commands.isEmpty())
    {
        qWarning() << "Command list is empty!";
        emit commandFinished(false, "", "Command list is empty");
        return;
    }

    QString fullCommand = joinCommands(commands);
    qInfo() << "Executing async command:" << fullCommand;

    // Configure working directory
    if (!workingDir.isEmpty() && QDir(workingDir).exists())
    {
        m_process->setWorkingDirectory(workingDir);
    }

    // Start process
    if (QOperatingSystemVersion::currentType() == QOperatingSystemVersion::Windows)
    {
        m_process->start("cmd.exe", QStringList() << "/c" << fullCommand);
    }
    else
    {
        m_process->start("bash", QStringList() << "-c" << fullCommand);
    }
}

// Callback for asynchronous process completion
void CommandExecutor::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    QString stdoutLog = QString::fromLocal8Bit(m_process->readAllStandardOutput());
    QString stderrLog = QString::fromLocal8Bit(m_process->readAllStandardError());

    bool success = (exitCode == 0) && (exitStatus == QProcess::NormalExit);
    emit commandFinished(success, stdoutLog, stderrLog);

    // Print log
    qInfo() << "Async command finished. Success:" << success;
    if (!stdoutLog.isEmpty()) qInfo() << "Output:\n" << stdoutLog;
    if (!stderrLog.isEmpty()) qWarning() << "Error:\n" << stderrLog;
}

#include "CommandExecutor.h"
#include <QOperatingSystemVersion>
#include <QDir>
#include <QTextStream>
#include <QRegularExpression>

/**
 * @brief Constructor: Initialize process and default settings
 */
CommandExecutor::CommandExecutor(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_currentAsyncCmdIdx(0)
    , m_maxLogLines(10000)
    , m_isExecuting(false)
{
    // Generate default log file name with timestamp
    QString defaultLogName = QString("cmd_exec_%1.log").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    m_logFileFolder = QDir::currentPath();
    m_logFilePath = m_logFileFolder + "/" + defaultLogName;

    // Bind process signals (fully asynchronous, no blocking calls)
    connect(m_process, &QProcess::finished, this, &CommandExecutor::onSingleCommandFinished);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &CommandExecutor::onReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &CommandExecutor::onReadyReadStandardError);
}

/**
 * @brief Stop current execution immediately (kill running process)
 */
void CommandExecutor::stopExecution()
{
    if (m_isExecuting && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_isExecuting = false;
        m_currentAsyncCmdIdx = 0;
        m_asyncCmds.clear();
        m_currentCmdStdout.clear();
        m_currentCmdStderr.clear();
        emit logUpdated(formatRealTimeLog("Execution stopped by user!"), true);
        emit commandFinished(false, "", "Execution stopped");
    }
}

/**
 * @brief Real-time read of stdout (non-blocking)
 * @note Triggered by QProcess::readyReadStandardOutput signal
 */
void CommandExecutor::onReadyReadStandardOutput()
{
    QString content = QString::fromLocal8Bit(m_process->readAllStandardOutput());
    m_currentCmdStdout += content;
    emit logUpdated(formatRealTimeLog(content), false);
}

/**
 * @brief Real-time read of stderr (non-blocking)
 * @note Triggered by QProcess::readyReadStandardError signal
 */
void CommandExecutor::onReadyReadStandardError()
{
    QString content = QString::fromLocal8Bit(m_process->readAllStandardError());
    m_currentCmdStderr += content;
    emit logUpdated(formatRealTimeLog(content), true);
}

/**
 * @brief Format real-time log with timestamp prefix for each line
 * @param content Raw log content from process output
 * @return Formatted log string with standardized timestamp
 */
QString CommandExecutor::formatRealTimeLog(const QString& content)
{
    if (content.isEmpty()) return "";
    QString timestamp = QDateTime::currentDateTime().toString("[yyyy-MM-dd hh:mm:ss]");
    QStringList lines = content.split("\n", Qt::SkipEmptyParts);
    QStringList formattedLines;
    for (const QString& line : lines) {
        formattedLines.append(timestamp + " " + line);
    }
    return formattedLines.join("\n") + "\n";
}

/**
 * @brief Extract complete error lines from stdout using predefined error keywords
 * @param stdoutContent Full stdout content of current command
 * @return Extracted complete error lines (joined by newlines, one line per error)
 * @note Uses the same error keywords as isCmakeReallyFailed for consistency
 */
QString CommandExecutor::extractErrorsFromStdout(const QString& stdoutContent)
{
    if (stdoutContent.isEmpty()) return "";

    // Predefined error keywords (matches isCmakeReallyFailed logic)
    QStringList errorKeywords = {
        "error",          // Generic error (C/C++ compiler)
        "fatal error",    // Fatal compilation error
        "c2146",          // Specific syntax error code you mentioned
        "c2065",          // Undeclared identifier
        "c2059",          // Syntax error
        "link : fatal",   // Link error
        "undefined reference" // Link error (Linux)
    };

    // Split content into individual lines (process complete lines only)
    QStringList lines = stdoutContent.split("\n", Qt::SkipEmptyParts);
    QStringList errorLines;

    // Iterate each line to check for error keywords (case-insensitive)
    for (const QString& line : lines) {
        QString trimmedLine = line.trimmed();
        QString lowerLine = trimmedLine.toLower(); // Convert to lowercase for case-insensitive match

        // Check if line contains ANY of the error keywords
        bool isErrorLine = false;
        for (const QString& keyword : errorKeywords) {
            if (lowerLine.contains(keyword.toLower())) {
                isErrorLine = true;
                break; // Stop checking keywords once match found
            }
        }

        // Add to error lines if keyword matched (avoid empty lines)
        if (isErrorLine && !trimmedLine.isEmpty()) {
            errorLines.append(trimmedLine);
        }
    }

    // Remove duplicate error lines (avoid redundant entries)
    errorLines.removeDuplicates();

    // Join error lines with newlines (one error per line)
    return errorLines.join("\n");
}

/**
 * @brief Save log content to persistent file (with error/info marking)
 * @param content Log content to save
 * @param isError True = mark as ERROR log, False = mark as INFO log
 */
void CommandExecutor::saveLog(const QString& content, bool isError)
{
    QFile logFile(m_logFilePath);
    if (!logFile.open(QIODevice::Append | QIODevice::Text)) {
        qWarning() << "Failed to open log file:" << logFile.errorString();
        emit logUpdated(formatRealTimeLog("Failed to open log file: " + logFile.errorString()), true);
        return;
    }

    QTextStream stream(&logFile);
    if (isError) {
        stream << "[ERROR] " << content << "\n";
    } else {
        stream << "[INFO] " << content << "\n";
    }
    logFile.close();
}

/**
 * @brief Enhanced CMake failure detection (includes compile errors from stdout)
 * @param cmd Executed CMake command
 * @param exitCode Process exit code (0 = success)
 * @param exitStatus Process exit status (NormalExit/CrashExit)
 * @param stderrLog Combined stderr (including extracted errors from stdout)
 * @return True = CMake command failed (process error or compile error)
 */
bool CommandExecutor::isCmakeReallyFailed(const QString& stderrLog)
{
    // Check error keywords in combined stderr (including extracted errors)
    QString lowerStderr = stderrLog.toLower();

    QStringList errorKeywords =
        {
        "error",          // Generic error (C/C++ compiler)
        "fatal error",    // Fatal compilation error
        "c2146",          // Specific syntax error code you mentioned
        "c2065",          // Undeclared identifier
        "c2059",          // Syntax error
        "link : fatal",   // Link error
        "undefined reference" // Link error (Linux)
    };

    for (const QString& keyword : errorKeywords)
    {
        if (lowerStderr.contains(keyword) && !lowerStderr.contains("error.cpp")) return true;
    }
    return false;
}

/**
 * @brief Parse CMake arguments to preserve space-containing values (critical for "-G Visual Studio 16 2019")
 * @param cmd Full CMake command string (e.g., "cmake -S. -B build -G Visual Studio 16 2019")
 * @return Properly split argument list (space-containing generator name as single item)
 */
QStringList CommandExecutor::parseCmakeArguments(const QString& cmd)
{
    QStringList result;
    QString trimmedCmd = cmd.trimmed();
    int pos = 0;
    int len = trimmedCmd.length();

    while (pos < len) {
        // Skip consecutive whitespace
        while (pos < len && trimmedCmd[pos].isSpace()) pos++;
        if (pos >= len) break;

        // Special handling for -G parameter (preserve full generator name with spaces)
        if (trimmedCmd.mid(pos, 2) == "-G" && (pos + 2 == len || trimmedCmd[pos + 2].isSpace())) {
            result << "-G";
            pos += 2;
            // Skip whitespace after -G
            while (pos < len && trimmedCmd[pos].isSpace()) pos++;
            // Extract full generator name (until next parameter starting with '-')
            int generatorStart = pos;
            while (pos < len) {
                if (trimmedCmd[pos] == '-' && (pos == 0 || trimmedCmd[pos-1].isSpace())) break;
                pos++;
            }
            // Add generator name as single argument (trim extra whitespace)
            QString generator = trimmedCmd.mid(generatorStart, pos - generatorStart).trimmed();
            result << generator;
        }
        // Normal parameters (-D/-S/-B etc.) - split by whitespace
        else {
            int argStart = pos;
            while (pos < len && !trimmedCmd[pos].isSpace()) pos++;
            QString arg = trimmedCmd.mid(argStart, pos - argStart);
            result << arg;
        }
    }
    return result;
}

/**
 * @brief Start next command in the async list (sequential execution core)
 * @note Only called after current command finishes execution (success/failure)
 * @note Guarantees strict execution order (command N+1 starts only after command N finishes)
 */
void CommandExecutor::startNextAsyncCommand()
{
    // Clear output buffers for next command
    m_currentCmdStdout.clear();
    m_currentCmdStderr.clear();

    // All commands executed successfully
    if (m_currentAsyncCmdIdx >= m_asyncCmds.size()) {
        if(executeCopyCmds(m_pendingCopyTasks))
        {
            emit logUpdated(formatRealTimeLog("✅ All commands executed successfully!"), false);
        }

        m_isExecuting = false;
        emit commandFinished(true, "", "");

        return;
    }

    // Get current command (by index - preserves execution order)
    QString currentCmd = m_asyncCmds[m_currentAsyncCmdIdx].trimmed();
    // Skip empty commands
    if (currentCmd.isEmpty()) {
        m_currentAsyncCmdIdx++;
        startNextAsyncCommand();
        return;
    }

    // Emit progress signal (for UI display)
    emit commandProgress(m_currentAsyncCmdIdx + 1, m_asyncCmds.size(), currentCmd);
    emit logUpdated(formatRealTimeLog(QString("📝 Executing command (%1/%2): %3").arg(m_currentAsyncCmdIdx + 1).arg(m_asyncCmds.size()).arg(currentCmd)), false);

    // Prepare command execution (platform-specific handling)
    bool isCmakeCmd = currentCmd.startsWith("cmake", Qt::CaseInsensitive);
    if (isCmakeCmd)
    {
        QStringList cmakeArgs = parseCmakeArguments(currentCmd);
        QString cmakeProgram = cmakeArgs.takeFirst();
        // Use cmake.exe explicitly on Windows (ensure executable resolution)
        if (QOperatingSystemVersion::currentType() == QOperatingSystemVersion::Windows)
        {
            cmakeProgram = "cmake.exe";
        }
        m_process->setProgram(cmakeProgram);
        m_process->setArguments(cmakeArgs);
    }
    // Windows non-CMake commands (via cmd.exe)
    else if (QOperatingSystemVersion::currentType() == QOperatingSystemVersion::Windows)
    {
        m_process->setProgram("cmd.exe");
        m_process->setArguments(QStringList() << "/c" << currentCmd);
    }
    // Linux/macOS non-CMake commands (via bash)
    else
    {
        m_process->setProgram("bash");
        m_process->setArguments(QStringList() << "-c" << currentCmd);
    }

    // Set working directory if valid
    if (!m_asyncWorkingDir.isEmpty() && QDir(m_asyncWorkingDir).exists())
    {
        m_process->setWorkingDirectory(m_asyncWorkingDir);
    }

    // Start process (non-blocking - returns immediately)
    m_process->start();
}

/**
 * @brief Callback for single command completion (async execution core)
 * @param exitCode Process exit code (0 = success)
 * @param exitStatus Process exit status (NormalExit/CrashExit)
 * @note Guarantees sequential execution: next command starts only after this callback is triggered
 */
void CommandExecutor::onSingleCommandFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    QString currentCmd = m_asyncCmds[m_currentAsyncCmdIdx];

    // Merge compile errors from stdout to stderr (for unified error detection)
    QString errorsFromStdout = extractErrorsFromStdout(m_currentCmdStdout);
    if (!errorsFromStdout.isEmpty()) {
        if (!m_currentCmdStderr.isEmpty()) m_currentCmdStderr += "\n";
        m_currentCmdStderr += "[Extracted from stdout] " + errorsFromStdout;
    }

    // Save detailed execution log (includes timestamp, exit code, output)
    QString logContent = QString("[%1] Command: %2\nExitCode: %3\nStdout:\n%4\nStderr:\n%5\n---\n")
                             .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"))
                             .arg(currentCmd)
                             .arg(exitCode)
                             .arg(m_currentCmdStdout)
                             .arg(m_currentCmdStderr);
    saveLog(logContent, !m_currentCmdStderr.isEmpty());

    // Check if CMake command failed (including compile errors)
    bool isCmakeCmd = currentCmd.startsWith("cmake", Qt::CaseInsensitive);
    bool CmakeFailed = isCmakeCmd && isCmakeReallyFailed(m_currentCmdStderr);

    // CMake failure: terminate execution immediately
    if (CmakeFailed) {
        m_isExecuting = false;
        QString errorMsg = QString("❌ CMake failed: %1").arg(m_currentCmdStderr);
        emit logUpdated(formatRealTimeLog(errorMsg), true);
        emit cmakeFailed(m_currentCmdStderr, currentCmd);
        emit commandFinished(false, m_currentCmdStdout, m_currentCmdStderr);
        return;
    }

    // Non-CMake command failure: terminate execution
    if (exitCode != 0 || exitStatus != QProcess::NormalExit) {
        m_isExecuting = false;
        QString errorMsg = QString("❌ Command failed: %1\nError: %2").arg(currentCmd, m_currentCmdStderr);
        emit logUpdated(formatRealTimeLog(errorMsg), true);
        emit commandFinished(false, m_currentCmdStdout, m_currentCmdStderr);
        return;
    }

    // Command succeeded: move to next command (preserve execution order)
    emit logUpdated(formatRealTimeLog(QString("✅ Command (%1/%2) executed successfully").arg(m_currentAsyncCmdIdx + 1).arg(m_asyncCmds.size())), false);
    m_currentAsyncCmdIdx++; // Increment index (point to next command)
    startNextAsyncCommand(); // Start next command (only after current finishes)
}

/**
 * @brief Public entry point for asynchronous command execution (non-blocking)
 * @param commands List of commands to execute (in strict execution order)
 * @param Path List of files to copy (in strict execution order)
 * @param Path List of files to delete (in strict execution order)
 * @param workingDir Working directory for command execution
 * @note Ensures commands run sequentially (next starts only after previous finishes)
 * @note UI remains fully responsive during execution
 */
void CommandExecutor::executeMultiCommandsAsync(const QStringList& commands, QList<QPair<QString, QString>> pendingCopyTasks, QList<QString> pendingDelTasks, const QString& workingDir)
{
    m_pendingCopyTasks = pendingCopyTasks;
    // Stop ongoing execution if already running
    if (m_isExecuting) {
        stopExecution();
    }

    // Validate command list
    if (commands.isEmpty()) {
        emit logUpdated(formatRealTimeLog("❌ Command list is empty!"), true);
        emit commandFinished(false, "", "Command list is empty");
        return;
    }

    if (!pendingDelTasks.empty())
    {
        executeDelCmds(pendingDelTasks);
    }

    // Initialize async execution state
    m_asyncCmds = commands;          // Preserve input order
    m_asyncWorkingDir = workingDir;
    m_currentAsyncCmdIdx = 0;        // Start with first command (index 0)
    m_isExecuting = true;

    emit logUpdated(formatRealTimeLog("🚀 Start executing async commands..."), false);
    // Start first command (non-blocking - returns immediately)
    startNextAsyncCommand();
}

bool CommandExecutor::executeCopyCmds(QList<QPair<QString, QString>> pendingCopyTasks)
{
    for (const auto& task : qAsConst(pendingCopyTasks))
    {
        QString srcFile = task.first;
        QString targetDir = task.second;
        QString errorMsg;

        QString startLog = QString("📤 Copying file: %1 -> %2").arg(srcFile).arg(targetDir);
        emit logUpdated(formatRealTimeLog(startLog), false);
        saveLog(startLog, false);
        qDebug() << startLog;

        if (!copyFileWithQt(srcFile, targetDir, errorMsg))
        {
            QString failLog = QString("❌ Copy failed: %1 (Reason: %2)").arg(srcFile).arg(errorMsg);
            emit logUpdated(formatRealTimeLog(failLog), true);
            saveLog(failLog, true);
            qDebug() << failLog;
            // Optional
            // break;
        }
        else
        {
            QString successLog = QString("✅ Copy success: %1 -> %2").arg(srcFile).arg(targetDir);
            emit logUpdated(formatRealTimeLog(successLog), false);
            saveLog(successLog, false);
            qDebug() << successLog;
        }
    }
    return true;
}

bool CommandExecutor::copyFileWithQt(const QString& srcFile, const QString& targetDir, QString& errorMsg)
{
    QFile src(srcFile);
    if (!src.exists())
    {
        errorMsg = QString("Source file does not exist: %1").arg(srcFile);
        return false;
    }

    QDir target(targetDir);
    if (!target.exists())
    {
        if (!target.mkpath(targetDir))
        {
            errorMsg = QString("Failed to create target directory: %1").arg(targetDir);
            return false;
        }
    }

    QString targetPath = target.filePath(QFileInfo(srcFile).fileName());

    QFile::remove(targetPath);

    if (src.copy(targetPath))
    {
        return true;
    }
    else
    {
        errorMsg = QString("Qt copy failed: %1 (Source: %2, Target: %3)")
        .arg(src.errorString())
            .arg(srcFile)
            .arg(targetPath);
        return false;
    }
}

bool CommandExecutor::executeDelCmds(QList<QString> pendingDelTasks)
{
    bool allSuccess = true; // Track overall success status
    for (const auto& targetDir : qAsConst(pendingDelTasks))
    {
        QString errorMsg;
        QString startLog = QString("🗑️ Deleting directory: %1").arg(targetDir);
        emit logUpdated(formatRealTimeLog(startLog), false);
        saveLog(startLog, false);
        qDebug() << startLog;

        if (!deleteFileWithQt(targetDir, errorMsg))
        {
            QString failLog = QString("❌ Delete failed: %1 (Reason: %2)").arg(targetDir).arg(errorMsg);
            emit logUpdated(formatRealTimeLog(failLog), true);
            saveLog(failLog, true);
            qDebug() << failLog;
            allSuccess = false; // Mark overall status as failed
            // break; // Optional: Stop on first failure
        }
        else
        {
            QString successLog = QString("✅ Delete success: %1").arg(targetDir);
            emit logUpdated(formatRealTimeLog(successLog), false);
            saveLog(successLog, false);
            qDebug() << successLog;
        }
    }
    return allSuccess; // Return overall success status
}

// Helper function: Recursively delete directory (compatible with Qt 4.x)
bool CommandExecutor::deleteDirRecursively(const QDir& dir)
{
    QFileInfoList entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
    for (const QFileInfo& entry : entries)
    {
        if (entry.isFile())
        {
            QFile file(entry.absoluteFilePath());
            if (!file.remove()) return false;
        }
        else if (entry.isDir())
        {
            if (!deleteDirRecursively(QDir(entry.absoluteFilePath()))) return false;
        }
    }
    return dir.rmdir(dir.absolutePath());
}

// Updated deleteFileWithQt with Qt 4.x compatibility (comments in English)
bool CommandExecutor::deleteFileWithQt(const QString& targetDir, QString& errorMsg)
{
    // 1. Check if the target directory exists
    QDir dir(targetDir);
    if (!dir.exists())
    {
        errorMsg = QString("Target directory does not exist: %1").arg(targetDir);
        return false;
    }

    // 2. Attempt to delete the directory recursively (Qt 4.x compatible implementation)
    if (deleteDirRecursively(dir))
    {
        return true;
    }
    else
    {
        // 3. Capture detailed error information for deletion failure
        // Supplement: Traverse the directory to attempt deleting items individually to locate the exact failure cause
        QString detailError = "Unknown error";
        QFileInfoList entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
        for (const QFileInfo& entry : entries)
        {
            QFile file(entry.absoluteFilePath());
            QDir subDir(entry.absoluteFilePath());

            if (entry.isFile())
            {
                if (!file.remove())
                {
                    detailError = QString("Failed to delete file: %1 (Reason: %2)")
                    .arg(entry.absoluteFilePath())
                        .arg(file.errorString());
                    break;
                }
            }
            else if (entry.isDir())
            {
                if (!deleteDirRecursively(QDir(entry.absoluteFilePath())))
                {
                    detailError = QString("Failed to delete subdirectory: %1").arg(entry.absoluteFilePath());
                    break;
                }
            }
        }

        errorMsg = QString("Qt delete directory failed: %1 (Target: %2)")
                       .arg(detailError)
                       .arg(targetDir);
        return false;
    }
}

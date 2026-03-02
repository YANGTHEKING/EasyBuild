#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "QMessageBox.h"
#include <QScrollBar>
#include <QThread>
#include <QDesktopServices>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    initUI();
    setStyleSheet();
    connectSignalsAndSlots();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initUI()
{
    m_executor            = new CommandExecutor(this);
    m_projectpathselector = new FilePathSelector("ProjectPath", ui->cbProjectPath, ui->bProjectPathBrowser, this);
    m_targetpathselector  = new FilePathSelector("TargetPath", ui->cbTargetPath, ui->bTargetPathBrowser, this);

    m_projctPath          = ui->cbProjectPath->currentText();
    m_targetPath          = ui->cbTargetPath->currentText();

    m_configButtonGroup   = new QButtonGroup(this);

    m_configButtonGroup->addButton(ui->rbDebug, 0);
    m_configButtonGroup->addButton(ui->rbRelease, 1);
}

void MainWindow::connectSignalsAndSlots()
{
    connect(ui->bG0MBuild, &QPushButton::clicked, this, [=]() {
        buildCmd(GPUDriver, G0M, m_buildConfig, AllDriver, m_bit, m_fwversion, m_isForceDelKMD, m_isAutoReplace, m_uniq, m_pvr, m_stat);
    });
    connect(ui->bG0MUMDBuild, &QPushButton::clicked, this, [=]() {
        buildCmd(GPUDriver, G0M, m_buildConfig, UMD, m_bit, m_fwversion, m_isForceDelKMD, m_isAutoReplace, m_uniq, m_pvr, m_stat);
    });
    connect(ui->bG0MKMDBuild, &QPushButton::clicked, this, [=]() {
        buildCmd(GPUDriver, G0M, m_buildConfig, KMD, m_bit, m_fwversion, m_isForceDelKMD, m_isAutoReplace, m_uniq, m_pvr, m_stat);
    });
    connect(ui->bG3Build, &QPushButton::clicked, this, [=]() {
        buildCmd(GPUDriver, G3, m_buildConfig, AllDriver, m_bit, m_fwversion, m_isForceDelKMD, m_isAutoReplace, m_uniq, m_pvr, m_stat);
    });
    connect(ui->bG3UMDBuild, &QPushButton::clicked, this, [=]() {
        buildCmd(GPUDriver, G3, m_buildConfig, UMD, m_bit, m_fwversion, m_isForceDelKMD, m_isAutoReplace, m_uniq, m_pvr, m_stat);
    });
    connect(ui->bG3KMDBuild, &QPushButton::clicked, this, [=]() {
        buildCmd(GPUDriver, G3, m_buildConfig, KMD, m_bit, m_fwversion, m_isForceDelKMD, m_isAutoReplace, m_uniq, m_pvr, m_stat);
    });
    connect(ui->bG0MPanelBuild, &QPushButton::clicked, this, [=]() {
        buildCmd(FantasyPanel, G0M, m_buildConfig);
    });
    connect(ui->bG3PanelBuild, &QPushButton::clicked, this, [=]() {
        buildCmd(FantasyPanel, G3, m_buildConfig);
    });
    connect(ui->bManuallyBuild, &QPushButton::clicked, this, [=]() {
        buildCmd(m_buildType, m_gpuModel, m_buildConfig, m_driverType, m_bit, m_fwversion, m_isForceDelKMD, m_isAutoReplace, m_uniq, m_pvr, m_stat);
    });
    connect(ui->bClear, &QPushButton::clicked, this, [=]() {
        ui->txtLog->clear();
    });
    connect(ui->bLogFile, &QPushButton::clicked, this, [=]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_executor->logFileFolder()));
    });

    connect(ui->cbProjectPath, &QComboBox::currentTextChanged, this, &MainWindow::setProjectPath);
    connect(ui->cbTargetPath, &QComboBox::currentTextChanged, this, &MainWindow::setTargetPath);

    connect(ui->cbBuildType, &QComboBox::currentIndexChanged, this, &MainWindow::setBuildType);
    connect(ui->cbGPUModel, &QComboBox::currentIndexChanged, this, &MainWindow::setGPUModel);
    connect(ui->cbDriverType, &QComboBox::currentIndexChanged, this, &MainWindow::setDriverType);
    connect(ui->cbBitType, &QComboBox::currentIndexChanged, this, &MainWindow::setBit);
    connect(ui->cbFW, &QComboBox::currentIndexChanged, this, &MainWindow::setFirmwareVersion);

    connect(ui->cbReplace, &QCheckBox::clicked, this, &MainWindow::setReplace);
    connect(ui->cbDeleteKMD, &QCheckBox::clicked, this, &MainWindow::setDeleteKMD);
    connect(ui->cbSTAT, &QCheckBox::clicked, this, &MainWindow::setProcessStat);
    connect(ui->cbUNIQ, &QCheckBox::clicked, this, &MainWindow::setUniQ);
    connect(ui->cbPVR, &QCheckBox::clicked, this, &MainWindow::setPvrtune);

    connect(m_configButtonGroup, &QButtonGroup::idClicked, this, &MainWindow::setConfig);

    // 1. Bind real-time log signal to QTextEdit (non-blocking update)
    connect(m_executor, &CommandExecutor::logUpdated, this, [this](const QString& logContent, bool isError) {
        // Ensure log update runs in UI thread (prevent cross-thread issues)
        if (QThread::currentThread() != thread()) {
            QMetaObject::invokeMethod(this, [this, logContent, isError]() {
                this->on_logUpdated(logContent, isError);
            }, Qt::QueuedConnection);
            return;
        }

        // Append log to text edit
        QTextCursor cursor = ui->txtLog->textCursor();
        cursor.movePosition(QTextCursor::End);

        // Color coding: red for errors, black for normal logs
        if (isError) {
            ui->txtLog->setTextColor(Qt::red);
        } else {
            ui->txtLog->setTextColor(Qt::black);
        }

        ui->txtLog->insertPlainText(logContent);

        // Auto-scroll to bottom (user always sees latest log)
        QScrollBar* scrollBar = ui->txtLog->verticalScrollBar();
        scrollBar->setValue(scrollBar->maximum());

        // Limit log lines to prevent memory overflow/UI lag
        limitLogLines(ui->txtLog, 10000);
    });

    // 2. Bind command completion signal
    connect(m_executor, &CommandExecutor::commandFinished, this, &MainWindow::onCommandFinished);

    // 3. Bind progress signal (optional - for status bar display)
    connect(m_executor, &CommandExecutor::commandProgress, this, [this](int current, int total, const QString& cmd) {
        ui->statusbar->showMessage(QString("Progress: %1/%2 - Executing: %3").arg(current).arg(total).arg(cmd));
    });

}

void MainWindow::setStyleSheet()
{
    this->setWindowTitle("EazyBuild");
}

bool MainWindow::buildCmd(BuildType btype, GPUModel gpu, BuildConfig build, DriverType driver, Bit bit, FirmwareVersion fw,
                          bool isForceDeleteKMD, bool isAutomaticallyReplace, bool isUniQ, bool isPvr, bool isStat)
{
    QStringList     cmds;
    QString         workDir;
    QString         foldername = "cmake_build_";
    QString         umdfoldername;
    QString         umd32foldername;
    QString         kmdfoldername;
    QString         panelfoldername;
    QString         soctype;
    QString         buildconfig;
    QString         firmware;
    QString         UNIQ;
    QString         procstat;
    QString         pvrtune;
    QString         cmakeGenerator = "Visual Studio 16 2019"; // Store generator name separately without quotes
    QString         bkmd       = "BUILD_KMD=1";
    QString         conf32     = "CONFIG=wnow32";
    QString         renderonly = "RENDER_ONLY=1";
    bool            isGPU      = btype  != FantasyPanel;
    bool            isPanel    = btype  != GPUDriver;
    bool            isUMD      = driver != KMD;
    bool            isKMD      = driver != UMD;
    bool            is32Bit    = bit    != Bit64;
    bool            is64Bit    = bit    != Bit32;

    if(!FilePathSelector::isPathValid(m_projctPath))
    {
        QMessageBox::warning(this, "Invalid Path", "Please select a valid project directory first!");
        return false;
    }

    if(isAutomaticallyReplace)
    {
        if(!FilePathSelector::isPathValid(m_targetPath))
        {
            QMessageBox::warning(this, "Invalid Path", "Please select a valid target directory or uncheck \"isAutomaticallyReplace\"!");
            return false;
        }
    }

    workDir = ui->cbProjectPath->currentText();

    switch(gpu)
    {
    case G3:
        soctype = "G3";
        break;
    case G0M:
        soctype = "G0M";
        break;
    case G1P:
        soctype = "G1P";
        break;
    default:
        break;
    }

    switch(fw)
    {
    case Firmware116:
        firmware = "IMGDDK116=1"; // Remove leading space and -D for later concatenation
        break;
    case Firmware119:
        firmware = "IMGDDK119=1"; // Remove leading space and -D for later concatenation
        break;
    default:
        break;
    }

    UNIQ       = isUniQ ? "SUPPORT_UNIQ=1" : "";       // Remove leading space and -D for later concatenation
    procstat   = isStat ? "ENABLE_PROCESS_STATS=1" : "";// Remove leading space and -D for later concatenation
    pvrtune    = isPvr ? "SUPPORT_PERF=1" : "";        // Remove leading space and -D for later concatenation

    umdfoldername   = foldername + "umd_" + soctype;
    umd32foldername = umdfoldername + "_win32";
    kmdfoldername   = foldername + "kmd_" + soctype;
    panelfoldername = foldername + soctype;
    soctype         = "SOC_TYPE=" + soctype;           // Remove leading space and -D for later concatenation

    if(build == Debug)
    {
        buildconfig = "Debug";
    }
    else
    {
        buildconfig = "Release";
    }

    setButtonState(false);
    ui->bClear->click();
    // ========== GPU Driver Logic ==========
    if (isGPU) {
        // UMD logic (simplified by merging 32/64-bit handling)
        if (isUMD) {
            // Define UMD 32/64-bit configuration map using nested QPair
            // Format: QPair<bool, QPair<QString, QPair<QString, QString>>>
            //         {isBitEnabled, {buildDir, {extraParams, dllFileName}}}
            QList<QPair<bool, QPair<QString, QPair<QString, QString>>>> umdConfigs = {
                qMakePair(is32Bit, qMakePair(umd32foldername, qMakePair(conf32, "fantumd32.dll"))),
                qMakePair(is64Bit, qMakePair(umdfoldername, qMakePair("", "fantumd64.dll")))
            };

            for (const auto& config : umdConfigs) {
                bool isBitEnabled = config.first;
                QString buildDir = config.second.first;
                QString extraParams = config.second.second.first;
                QString dllName = config.second.second.second;

                if (isBitEnabled) {
                    QStringList cmakeConfigureArgs;
                    cmakeConfigureArgs << "-S." << "-B" << buildDir
                                       << "-D" << soctype
                                       << (UNIQ.isEmpty() ? "" : "-D") << UNIQ
                                       << (firmware.isEmpty() ? "" : "-D") << firmware
                                       << "-G" << cmakeGenerator // Generator as independent parameter (no quotes)
                                       << (extraParams.isEmpty() ? "" : "-D") << extraParams;
                    // Concatenate to standardized command string (no quotes, space-separated only)
                    QString cmakeConfigureCmd = "cmake " + cmakeConfigureArgs.join(" ");

                    // Keep original logic: Add CMake configure/build commands
                    cmds.append(cmakeConfigureCmd);
                    cmds.append(generateCmakeBuildCmd(buildDir, buildconfig));
                    // Automatic file copy logic
                    if (isAutomaticallyReplace) {
                        QString srcFile = joinPath({workDir, buildDir, buildconfig, dllName});
                        cmds.append(generateCopyCmd(srcFile, ui->cbTargetPath->currentText()));
                    }
                }
            }
        }

        // KMD logic
        if (isKMD) {
            // Force delete KMD directory if enabled
            if (isForceDeleteKMD) {
                cmds.append(QString("rd /s /q \"%1\"").arg(kmdfoldername));
            }
            QStringList cmakeKmdArgs;
            cmakeKmdArgs << "-S." << "-B" << kmdfoldername
                         << "-D" << soctype
                         << "-D" << bkmd
                         << (UNIQ.isEmpty() ? "" : "-D") << UNIQ
                         << (firmware.isEmpty() ? "" : "-D") << firmware
                         << (pvrtune.isEmpty() ? "" : "-D") << pvrtune
                         << (procstat.isEmpty() ? "" : "-D") << procstat
                         << "-G" << cmakeGenerator; // Generator as independent parameter (no quotes)
            QString cmakeKmdConfigureCmd = "cmake " + cmakeKmdArgs.join(" ");

            // Keep original logic: Add KMD configure/build commands
            cmds.append(cmakeKmdConfigureCmd);
            cmds.append(generateCmakeBuildCmd(kmdfoldername, buildconfig));
        }

        m_executor->executeMultiCommandsAsync(cmds, workDir);
    }

    // ========== FantasyPanel Logic ==========
    cmds.clear();
    if (isPanel) {
        QStringList cmakePanelArgs;
        cmakePanelArgs << "-S." << "-B" << panelfoldername
                       << "-D" << soctype
                       << "-G" << cmakeGenerator; // Generator as independent parameter (no quotes)
        QString cmakePanelConfigureCmd = "cmake " + cmakePanelArgs.join(" ");

        // 1. Combine Panel path and additional parameters (keep original comment, adjusted logic)
        QString srcFile = joinPath({workDir, panelfoldername, buildconfig, "FantasyPanel.exe"});
        // 2. Update working directory (NOTE: Use temporary variable to avoid polluting original workDir)
        QString panelWorkDir = joinPath({workDir, "tool", "fantasypanel"});
        // 3. Generate CMake commands for Panel (keep original logic)
        cmds.append(cmakePanelConfigureCmd);
        cmds.append(generateCmakeBuildCmd(panelfoldername, buildconfig));
        // 4. Automatic file copy if enabled
        if (isAutomaticallyReplace) {
            cmds.append(generateCopyCmd(srcFile, ui->cbTargetPath->currentText()));
        }

        // m_executor->executeMultiCommandsAsync(cmds, panelWorkDir); // Use temporary workDir to avoid modifying original
    }

    return true;
}

QString MainWindow::generateCmakeConfigureCmd(const QString& buildDir, const QString& extraParams)
{
    QString cmd = "cmake -S. -B " + buildDir;
    if (!extraParams.isEmpty()) {
        cmd += " " + extraParams;
    }
    return cmd;
}

QString MainWindow::generateCmakeBuildCmd(const QString& buildDir, const QString& buildConfig)
{
    return "cmake --build " + buildDir + " --config " + buildConfig;
}

QString MainWindow::generateCopyCmd(const QString& srcFile, const QString& targetPath)
{
    QString quotedSrc = QString("\"%1\"").arg(srcFile);
    QString quotedTarget = QString("\"%1\"").arg(targetPath);
    return "copy /y " + quotedSrc + " " + quotedTarget;
}

QString MainWindow::joinPath(const QStringList& parts)
{
    QString path;
    for (const QString& part : parts) {
        if (path.isEmpty()) {
            path = part;
        } else {
            path += "/" + part;
        }
    }
    return path;
}

void MainWindow::limitLogLines(QTextEdit* logWidget, int maxLines)
{
    QStringList lines = logWidget->toPlainText().split("\n", Qt::SkipEmptyParts);
    if (lines.size() > maxLines) {
        QStringList keepLines = lines.mid(lines.size() - maxLines);
        logWidget->setText(keepLines.join("\n"));
    }
}

/**
 * @brief Command completion callback (UI feedback)
 * @param success True = all commands executed successfully
 * @param stdoutLog Combined stdout of all commands
 * @param stderrLog Combined stderr of all commands
 */
void MainWindow::onCommandFinished(bool success, const QString& stdoutLog, const QString& stderrLog)
{
    // Restore UI state
    setButtonState(true);

    // Show completion message
    if (success) {
        QMessageBox::information(this, "Success", "All commands executed successfully!");
        ui->statusbar->showMessage("Execution completed successfully", 3000);
    } else {
        QMessageBox::critical(this, "Error", QString("Command execution failed:\n%1").arg(stderrLog));
        ui->statusbar->showMessage("Execution failed", 3000);
    }
}

// ========== Implemented: Real-time log update handler (UI thread) ==========
/**
 * @brief Handle real-time log updates (runs in UI thread only)
 * @param logContent Formatted log content with timestamp
 * @param isError True = error log (red color), False = normal log (black color)
 * @note This function is guaranteed to run in UI thread (prevents cross-thread UI issues)
 */
void MainWindow::on_logUpdated(const QString& logContent, bool isError)
{
    // Append log to QTextEdit (UI widget)
    QTextCursor cursor = ui->txtLog->textCursor();
    cursor.movePosition(QTextCursor::End);

    // Color coding for logs: red = error, black = normal
    if (isError) {
        ui->txtLog->setTextColor(Qt::red);
    } else {
        ui->txtLog->setTextColor(Qt::black);
    }

    ui->txtLog->insertPlainText(logContent);

    // Auto-scroll to bottom (ensure user sees latest log)
    QScrollBar* scrollBar = ui->txtLog->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());

    // Limit log lines to prevent memory overflow/UI lag
    limitLogLines(ui->txtLog, 10000);
}

void MainWindow::setButtonState(bool isEnable)
{
    this->setWindowTitle(isEnable ? "EazyBuild" : "EazyBuild(Building)");

    ui->bG0MBuild->setEnabled(isEnable);
    ui->bG0MUMDBuild->setEnabled(isEnable);
    ui->bG0MKMDBuild->setEnabled(isEnable);
    ui->bG3Build->setEnabled(isEnable);
    ui->bG3UMDBuild->setEnabled(isEnable);
    ui->bG3KMDBuild->setEnabled(isEnable);
    ui->bG0MPanelBuild->setEnabled(isEnable);
    ui->bG3PanelBuild->setEnabled(isEnable);

    ui->cbBuildType->setEnabled(isEnable);
    ui->cbBitType->setEnabled(isEnable);
    ui->cbDriverType->setEnabled(isEnable);
    ui->cbDeleteKMD->setEnabled(isEnable);
    ui->cbFW->setEnabled(isEnable);
    ui->cbGPUModel->setEnabled(isEnable);
    ui->cbPVR->setEnabled(isEnable);
    ui->cbProjectPath->setEnabled(isEnable);
    ui->cbTargetPath->setEnabled(isEnable);
    ui->cbReplace->setEnabled(isEnable);
    ui->cbSTAT->setEnabled(isEnable);
    ui->cbUNIQ->setEnabled(isEnable && m_fwversion != Firmware116);

    ui->bManuallyBuild->setEnabled(isEnable);
    ui->bProjectPathBrowser->setEnabled(isEnable);
    ui->bTargetPathBrowser->setEnabled(isEnable);
    ui->bReplace->setEnabled(isEnable);

    ui->rbDebug->setEnabled(isEnable);
    ui->rbRelease->setEnabled(isEnable);
}

void MainWindow::setProjectPath(const QString& path)
{
    m_projctPath = path;
}

void MainWindow::setTargetPath(const QString& path)
{
    m_targetPath = path;
}

void MainWindow::setBuildType(int index)
{
    m_buildType = static_cast<BuildType>(index);
}

void MainWindow::setGPUModel(int index)
{
    m_gpuModel = static_cast<GPUModel>(index);
}

void MainWindow::setDriverType(int index)
{
    m_driverType = static_cast<DriverType>(index);
}

void MainWindow::setBit(int index)
{
    m_bit = static_cast<Bit>(index);
}

void MainWindow::setConfig(int id)
{
    if (id == 0)
    {
        m_buildConfig = Debug;
    }
    else if (id == 1)
    {
        m_buildConfig = Release;
    }
}

void MainWindow::setReplace(bool checked)
{
    m_isAutoReplace = checked;
}

void MainWindow::setDeleteKMD(bool checked)
{
    m_isForceDelKMD = checked;
}

void MainWindow::setProcessStat(bool checked)
{
    m_stat = checked;
}

void MainWindow::setUniQ(bool checked)
{
    m_uniq = checked;
}

void MainWindow::setPvrtune(bool checked)
{
    m_pvr = checked;
}

void MainWindow::setFirmwareVersion(int index)
{
    m_fwversion = static_cast<FirmwareVersion>(index);

    if(m_fwversion == Firmware116)
    {
        if(m_uniq)
        {
            ui->cbUNIQ->click();
        }
        ui->cbUNIQ->setEnabled(false);
    }
    else
    {
        ui->cbUNIQ->setEnabled(true);
    }
}

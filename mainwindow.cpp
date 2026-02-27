#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "QMessageBox.h"
#include "CommandExecutor.h"

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
        buildCmd(GPUDriver, G0M, m_buildConfig, AllDriver, m_bit, m_isForceDelKMD, m_isAutoReplace);
    });
    connect(ui->bG0MUMDBuild, &QPushButton::clicked, this, [=]() {
        buildCmd(GPUDriver, G0M, m_buildConfig, UMD, m_bit, m_isForceDelKMD, m_isAutoReplace);
    });
    connect(ui->bG0MKMDBuild, &QPushButton::clicked, this, [=]() {
        buildCmd(GPUDriver, G0M, m_buildConfig, KMD, m_bit, m_isForceDelKMD, m_isAutoReplace);
    });
    connect(ui->bG3Build, &QPushButton::clicked, this, [=]() {
        buildCmd(GPUDriver, G3, m_buildConfig, AllDriver, m_bit, m_isForceDelKMD, m_isAutoReplace);
    });
    connect(ui->bG3UMDBuild, &QPushButton::clicked, this, [=]() {
        buildCmd(GPUDriver, G3, m_buildConfig, UMD, m_bit, m_isForceDelKMD, m_isAutoReplace);
    });
    connect(ui->bG3KMDBuild, &QPushButton::clicked, this, [=]() {
        buildCmd(GPUDriver, G3, m_buildConfig, KMD, m_bit, m_isForceDelKMD, m_isAutoReplace);
    });
    connect(ui->bG0MPanelBuild, &QPushButton::clicked, this, [=]() {
        buildCmd(FantasyPanel, G0M, m_buildConfig);
    });
    connect(ui->bG3PanelBuild, &QPushButton::clicked, this, [=]() {
        buildCmd(FantasyPanel, G3, m_buildConfig);
    });
    connect(ui->bManuallyBuild, &QPushButton::clicked, this, [=]() {
        buildCmd(m_buildType, m_gpuModel, m_buildConfig, m_driverType, m_bit, m_isForceDelKMD, m_isAutoReplace);
    });

    connect(ui->cbProjectPath, &QComboBox::currentTextChanged, this, &MainWindow::setProjectPath);
    connect(ui->cbTargetPath, &QComboBox::currentTextChanged, this, &MainWindow::setTargetPath);

    connect(ui->cbBuildType, &QComboBox::currentIndexChanged, this, &MainWindow::setBuildType);
    connect(ui->cbGPUModel, &QComboBox::currentIndexChanged, this, &MainWindow::setGPUModel);
    connect(ui->cbDriverType, &QComboBox::currentIndexChanged, this, &MainWindow::setDriverType);
    connect(ui->cbBitType, &QComboBox::currentIndexChanged, this, &MainWindow::setBit);

    connect(ui->cbReplace, &QCheckBox::clicked, this, &MainWindow::setReplace);
    connect(ui->cbDeleteKMD, &QCheckBox::clicked, this, &MainWindow::setDeleteKMD);

    connect(m_configButtonGroup, &QButtonGroup::idClicked, this, &MainWindow::setConfig);
}

void MainWindow::setStyleSheet()
{
    this->setWindowTitle("EazyBuild");
}

bool MainWindow::buildCmd(BuildType btype, GPUModel gpu, BuildConfig build, DriverType driver, Bit bit,
                          bool isForceDeleteKMD, bool isAutomaticallyReplace)
{
    CommandExecutor executor;
    QStringList     cmds;
    QString         workDir;
    QString         foldername = "cmake_build_";
    QString         umdfoldername;
    QString         umd32foldername;
    QString         kmdfoldername;
    QString         panelfoldername;
    QString         soctype;
    QString         buildconfig;
    QString         UNIQ;
    QString         compiler   = " -G \"Visual Studio 16 2019\"";
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
        UNIQ = " -D SUPPORT_UNIQ=1";
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

    umdfoldername   = foldername + "umd_" + soctype;
    umd32foldername = umdfoldername + "_win32";
    kmdfoldername   = foldername + "kmd_" + soctype;
    panelfoldername = foldername + soctype;

    if(build == Debug)
    {
        buildconfig = "Debug";
    }
    else
    {
        buildconfig = "Release";
    }

    if(isGPU)
    {
        if(isUMD)
        {
            if(is32Bit)
            {
                cmds.append("cmake -S. -B " + umd32foldername + " -D SOC_TYPE=" + soctype + UNIQ + compiler + " -DCONFIG=wnow32");
                cmds.append("cmake --build " + umd32foldername + " --config " + buildconfig);

                if(isAutomaticallyReplace)
                {
                    cmds.append("copy /y \"" + workDir + "/" + umd32foldername + "/" + buildconfig + "/fantumd32.dll\" " + ui->cbTargetPath->currentText() + "/\"");
                }
            }
            if(is64Bit)
            {
                cmds.append("cmake -S. -B " + umdfoldername + " -D SOC_TYPE=" + soctype+ UNIQ + " -D IMGDDK119=1" + compiler);
                cmds.append("cmake --build " + umdfoldername + " --config " + buildconfig);

                if(isAutomaticallyReplace)
                {
                    cmds.append("copy /y \"" + workDir + "/" + umdfoldername + "/" + buildconfig + "/fantumd64.dll\" " + ui->cbTargetPath->currentText() + "/\"");
                }
            }
        }
        if(isKMD)
        {
            if(isForceDeleteKMD)
            {
                cmds.append("rd /s /q \"" + kmdfoldername + "\"");
            }

            cmds.append("cmake -S. -B " + kmdfoldername + " -D IMGDDK119=1 -D BUILD_KMD=1 -D SOC_TYPE=" + soctype + UNIQ + " -D ENABLE_PROCESS_STATS=1" + compiler);
            cmds.append("cmake --build " + kmdfoldername + " --config " + buildconfig);
        }
        //executor.executeMultiCommands(cmds, workDir);
    }

    cmds.clear();
    if(isPanel)
    {
        workDir += "\tool\fantasypanel";

        cmds.append("cmake -S. -B " + panelfoldername + " -D SOC_TYPE=" + soctype + " -G \"Visual Studio 16 2019\"");
        cmds.append("cmake --build " + panelfoldername + " --config " + buildconfig);

        if(isAutomaticallyReplace)
        {
            cmds.append("copy /y \"" + workDir + panelfoldername + "\\" + buildconfig + "\\FantasyPanel.exe\" " + ui->cbTargetPath->currentText() + "\\\"");
        }
        //executor.executeMultiCommands(cmds, workDir);
    }

    return true;
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

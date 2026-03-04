#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QButtonGroup>
#include "FilePathSelector.h"
#include "CommandExecutor.h"
#include "qtextedit.h"
#include "remoteconfigdialog.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    enum BuildType
    {
        GPUDriver,
        FantasyPanel,
        AllType,
    };

    enum GPUModel
    {
        G0M,
        G3,
        G1P,
    };

    enum DriverType
    {
        UMD,
        KMD,
        AllDriver,
    };

    enum BuildConfig
    {
        Debug,
        Release,
    };

    enum Bit
    {
        Bit32,
        Bit64,
        AllBit
    };

    enum FirmwareVersion
    {
        Firmware116,
        Firmware119,
    };

    void connectSignalsAndSlots();
    void setStyleSheet();
    void initUI();
    bool buildCmd(BuildType btype = AllType, GPUModel gpu = G0M, BuildConfig build = Debug, bool isAutomaticallyReplace = false, DriverType driver = AllDriver, Bit bit = Bit64,
                  FirmwareVersion fw = Firmware119, bool isForceDeleteKMD = false, bool isUniQ = false, bool isPvr = false, bool isStat = false);
private slots:
    void on_logUpdated(const QString &logContent, bool isError);
    void onCommandFinished(bool success, const QString &stdoutLog, const QString &stderrLog);
    void setRemoteDeploy(bool checked);  // Enable/disable remote deployment
    void onRemoteConfigClicked();        // Open remote configuration dialog
private:
    FilePathSelector *m_projectpathselector = nullptr;
    FilePathSelector *m_targetpathselector  = nullptr;
    CommandExecutor  *m_executor            = nullptr;
    QString          m_projctPath           = "";
    QString          m_targetPath           = "";
    QString          m_remoteHost;                     // Remote host
    QString          m_remotePath;                     // Remote target path
    bool             m_isRemoteDeploy       = false;   // Whether to enable remote deployment
    bool             m_isForceDelKMD        = false;
    bool             m_isAutoReplace        = false;
    bool             m_uniq                 = false;
    bool             m_pvr                  = false;
    bool             m_stat                 = false;
    Bit              m_bit                  = Bit64;
    BuildConfig      m_buildConfig          = Debug;
    BuildType        m_buildType            = AllType;
    DriverType       m_driverType           = AllDriver;
    GPUModel         m_gpuModel             = G0M;
    FirmwareVersion  m_fwversion            = Firmware119;
    QButtonGroup*    m_configButtonGroup    = nullptr;

    void setProjectPath(const QString& path);
    void setTargetPath(const QString& path);
    void setBuildType(int index);
    void setGPUModel(int index);
    void setDriverType(int index);
    void setBit(int id);
    void setConfig(int id);
    void setReplace(bool checked);
    void setDeleteKMD(bool checked);
    void setProcessStat(bool checked);
    void setUniQ(bool checked);
    void setPvrtune(bool checked);
    void setFirmwareVersion(int index);
    void setButtonState(bool isEnable);
    void limitLogLines(QTextEdit *logWidget, int maxLines);

    QString generateCmakeConfigureCmd(const QString &buildDir, const QString &extraParams = "");
    QString generateCmakeBuildCmd(const QString &buildDir, const QString &buildConfig);
    QString generateCopyCmd(const QString &srcFile, const QString &targetPath);
    QString generateRemoteCopyCmd(const QString &srcFile, const QString &remotePath);
    QString joinPath(const QStringList &parts);

    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H

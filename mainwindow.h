#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QButtonGroup>
#include "FilePathSelector.h"

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

    void connectSignalsAndSlots();
    void setStyleSheet();
    void initUI();
    bool buildCmd(BuildType btype = AllType, GPUModel gpu = G0M, BuildConfig build = Debug, DriverType driver = AllDriver, Bit bit = Bit64,
                  bool isForceDeleteKMD= false, bool isAutomaticallyReplace= false);
private:
    FilePathSelector *m_projectpathselector = nullptr;
    FilePathSelector *m_targetpathselector  = nullptr;
    QString          m_projctPath           = "";
    QString          m_targetPath           = "";
    bool             m_isForceDelKMD        = false;
    bool             m_isAutoReplace        = false;
    Bit              m_bit                  = Bit64;
    BuildConfig      m_buildConfig          = Debug;
    BuildType        m_buildType            = AllType;
    DriverType       m_driverType           = AllDriver;
    GPUModel         m_gpuModel             = G0M;
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

    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H

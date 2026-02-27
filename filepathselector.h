#ifndef FILEPATHSELECTOR_H
#define FILEPATHSELECTOR_H

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QSettings>
#include <QStringList>
#include <QRegularExpression>
#include <QOperatingSystemVersion>

class FilePathSelector : public QWidget
{
    Q_OBJECT
public:
    // Constructor: title is the file dialog title, isDir indicates whether to select a folder (false for file)
    explicit FilePathSelector(const QString& title = "Select Folder", QComboBox* path = nullptr, QPushButton* browser = nullptr, QWidget *parent = nullptr);

    // Get the currently selected path
    QString currentPath() const;

    // Core: Check if a path string is valid (static callable for external use)
    static bool isPathValid(const QString& path,
                            bool checkExists = true,
                            bool checkPermission = false);


private slots:
    // Trigger file/folder selection when the button is clicked
    void SelectPath();

private:
    // Initialize UI components
    void initUI();
    // Load historical paths (from configuration file)
    void loadHistoryPaths();
    // Save historical paths (to configuration file)
    void saveHistoryPaths();
    // Add path to combo box (remove duplicates)
    void addPathToComboBox(const QString& path);
    // Internal method: Check path format validity (cross-platform)
    static bool isPathFormatValid(const QString& path);

    QComboBox* m_comboBox;    // Combo box for displaying path and historical records
    QPushButton* m_selectBtn; // Button for triggering path selection
    QString m_dialogTitle;    // Title of the file selection dialog
    QSettings* m_settings;    // Configuration file for saving historical paths
    const QString HISTORY_KEY = "FilePathHistory"; // Configuration key for historical paths
    const int MAX_HISTORY_COUNT = 10; // Maximum number of historical records
};

#endif // FILEPATHSELECTOR_H

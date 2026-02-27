#include "FilePathSelector.h"

FilePathSelector::FilePathSelector(const QString& title, QComboBox* path, QPushButton* browser, QWidget *parent)
    : QWidget(parent)
    , m_comboBox(path)
    , m_selectBtn(browser)
    , m_dialogTitle(title)
{
    // Initialize QSettings (named with company + app name to ensure unique configuration)
    m_settings = new QSettings("Innosilicon", title, this);
    initUI();
    loadHistoryPaths(); // Load historical paths on startup
}

// Get the currently selected path
QString FilePathSelector::currentPath() const
{
    return m_comboBox->currentText().trimmed();
}

// Initialize UI layout
void FilePathSelector::initUI()
{
    // Initialize combo box: editable (manual path input), drop-down list for history
    m_comboBox->setEditable(true);

    // Bind button click signal
    connect(m_selectBtn, &QPushButton::clicked, this, &FilePathSelector::SelectPath);
}

// Load historical paths (from configuration file)
void FilePathSelector::loadHistoryPaths()
{
    QStringList historyPaths = m_settings->value(HISTORY_KEY).toStringList();
    if (!historyPaths.isEmpty())
    {
        // Add historical paths to combo box
        m_comboBox->addItems(historyPaths);
        // Select the last selected path by default (first item in the list)
        m_comboBox->setCurrentIndex(0);
    }
}

// Save historical paths (to configuration file)
void FilePathSelector::saveHistoryPaths()
{
    QStringList historyPaths;
    // Read all historical paths from combo box
    for (int i = 0; i < m_comboBox->count(); ++i)
    {
        QString path = m_comboBox->itemText(i).trimmed();
        if (!path.isEmpty())
        {
            historyPaths.append(path);
        }
    }
    // Save to configuration file
    m_settings->setValue(HISTORY_KEY, historyPaths);
}

// Add path to combo box (remove duplicates and limit maximum count)
void FilePathSelector::addPathToComboBox(const QString& path)
{
    if (path.isEmpty()) return;

    // Remove duplicates: if path exists, remove old entry first, then add to front
    int existIndex = m_comboBox->findText(path, Qt::MatchExactly);
    if (existIndex != -1)
    {
        m_comboBox->removeItem(existIndex);
    }

    // Insert at the first position (newest selection at the top)
    m_comboBox->insertItem(0, path);
    // Set current selection to the newly added path
    m_comboBox->setCurrentIndex(0);

    // Limit number of historical records (delete last item if exceeding MAX_HISTORY_COUNT)
    while (m_comboBox->count() > MAX_HISTORY_COUNT)
    {
        m_comboBox->removeItem(m_comboBox->count() - 1);
    }

    // Save updated historical paths
    saveHistoryPaths();
}

// Select folder when button is clicked
void FilePathSelector::SelectPath()
{
    QString selectedPath;

    // Select folder: open last selected path by default
    selectedPath = QFileDialog::getExistingDirectory(this, m_dialogTitle, currentPath());

    // Add to combo box and save if valid path is selected
    if (!selectedPath.isEmpty())
    {
        addPathToComboBox(selectedPath);
    }
}

// Internal method: Check path format validity (cross-platform)
bool FilePathSelector::isPathFormatValid(const QString& path)
{
    if (path.isEmpty()) return false;

    // Distinguish path formats between Windows and Linux/macOS
    if (QOperatingSystemVersion::currentType() == QOperatingSystemVersion::Windows)
    {
        // Windows path rules:
        // 1. Drive letter prefix (e.g. C:\)
        // 2. Relative path (e.g. .\test)
        // 3. Network path (e.g. \\server\share)
        QRegularExpression winPathRegex(
            R"(^(?:[a-zA-Z]:\\|\\{2}[^\\]+\\|\\?\.{0,2}\\?).*)",
            QRegularExpression::CaseInsensitiveOption
            );
        return winPathRegex.match(path).hasMatch();
    }
    else
    {
        // Linux/macOS path rules:
        // 1. Absolute path starts with /
        // 2. Relative path starts with ./ or ../
        QRegularExpression unixPathRegex(R"(^(?:/|(\.{1,2}/)?).+)");
        return unixPathRegex.match(path).hasMatch();
    }
}

// Core: Check if path is valid (static method, callable externally)
bool FilePathSelector::isPathValid(const QString& path,
                                   bool checkExists,
                                   bool checkPermission)
{
    // Step 1: Check non-empty path + valid format
    QString trimmedPath = path.trimmed();
    if (trimmedPath.isEmpty() || !isPathFormatValid(trimmedPath))
    {
        return false;
    }

    // Step 2: Return valid if existence check is not required (format-only validation)
    if (!checkExists)
    {
        return true;
    }

    // Step 3: Check path existence + type matching
    QFileInfo fileInfo(trimmedPath);
    if (!fileInfo.exists())
    {
        return false;
    }

    // Check path type matching
    if (!fileInfo.isDir())
    {
        return false;
    }


    // Step 4: Optional: Check read/write permissions
    if (checkPermission)
    {
        // Directory needs access permission
        QDir dir(trimmedPath);
        if (!dir.isReadable())
        {
            return false;
        }
    }

    return true;
}

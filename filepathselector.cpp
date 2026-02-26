#include "FilePathSelector.h"

FilePathSelector::FilePathSelector(const QString& title, bool isDir, QWidget *parent)
    : QWidget(parent)
    , m_dialogTitle(title)
    , m_isDir(isDir)
{
    // Initialize QSettings (named with company + app name to ensure unique configuration)
    m_settings = new QSettings("MyCompany", "FileSelectorApp", this);
    initUI();
    loadHistoryPaths(); // Load historical paths on startup
}

// Get the currently selected path
QString FilePathSelector::currentPath() const
{
    return m_comboBox->currentText().trimmed();
}

// Set whether to select a folder
void FilePathSelector::setSelectDir(bool isDir)
{
    m_isDir = isDir;
}

// Initialize UI layout
void FilePathSelector::initUI()
{
    // Initialize combo box: editable (manual path input), drop-down list for history
    m_comboBox = new QComboBox(this);
    m_comboBox->setEditable(true);
    m_comboBox->setMinimumWidth(300); // Set minimum width to avoid path truncation

    // Initialize selection button
    m_selectBtn = new QPushButton("Select", this);
    m_selectBtn->setMinimumWidth(80);

    // Layout: combo box + button arranged horizontally
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(m_comboBox);
    layout->addWidget(m_selectBtn);
    layout->setContentsMargins(0, 0, 0, 0); // Remove outer borders to fit parent widget
    setLayout(layout);

    // Bind button click signal
    connect(m_selectBtn, &QPushButton::clicked, this, &FilePathSelector::onSelectPathClicked);
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

// Select file/folder when button is clicked
void FilePathSelector::onSelectPathClicked()
{
    QString selectedPath;

    if (m_isDir)
    {
        // Select folder: open last selected path by default
        selectedPath = QFileDialog::getExistingDirectory(this, m_dialogTitle, currentPath());
    }
    else
    {
        // Select file: open last selected path by default
        selectedPath = QFileDialog::getOpenFileName(this, m_dialogTitle, currentPath());
    }

    // Add to combo box and save if valid path is selected
    if (!selectedPath.isEmpty())
    {
        addPathToComboBox(selectedPath);
    }
}

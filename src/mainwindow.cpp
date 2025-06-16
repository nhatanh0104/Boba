#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QDir>
#include <QDebug>
#include <QTimer>
#include <QFileInfo>
#include <QMenu>
#include <QShortcut>
#include <QInputDialog>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QDesktopServices>
#include <QSplitter>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , detailsWidget(nullptr)
    , detailsVisible(false)
    , searchManager(nullptr)
    , searchProxyModel(nullptr)
    , searchResultsModel(nullptr)
    , isSearching(false)
{
    ui->setupUi(this);
    init();
    setupDetailsWidget();
    setupSearch();
}

MainWindow::~MainWindow()
{
    if (searchManager)
    {
        searchManager->stopSearch();
        delete searchManager;
    }
    if (searchResultsModel) {
        searchResultsModel->clear();
    }
    delete ui;
}






void MainWindow::onTreeViewClicked(const QModelIndex &index)
{
    // Map proxy index to source index
    QModelIndex sourceIndex = mapToSourceModel(index);
    if (!sourceIndex.isValid())
    {
        qDebug() << "Invalid source index for model [treeView_clicked]";
        return;
    }

    // If index is the same as root index of the folderView -> do nothing
    if (ui->folderView->rootIndex() == sourceIndex)
    {
        return;
    }

    // Else display the directory specified by index
    history_paths.push(ui->addressBar->text());
    ui->treeView->expand(index);
    ui->folderView->setRootIndex(sourceIndex);
    ui->addressBar->setText(model.filePath(sourceIndex));
    if (detailsVisible)
        detailsWidget->setFileInfo(model.fileInfo(sourceIndex));
}

void MainWindow::onFolderViewDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }

    if (isSearching) {
        // Handle search results
        QModelIndex nameIndex = index.sibling(index.row(), 0);
        QString fullPath = nameIndex.data(Qt::UserRole).toString();

        qDebug() << "Folder view interaction on search result:" << fullPath;

        if (!fullPath.isEmpty()) {
            QFileInfo fileInfo(fullPath);
            if (fileInfo.exists()) {
                if (fileInfo.isDir()) {
                    // Navigate to the directory
                    qDebug() << "Navigating to directory:" << fullPath;
                    clearSearch();
                    history_paths.push(ui->addressBar->text());
                    changeDir(fullPath);
                } else {
                    // Show message for files
                    QMessageBox msgBox;
                    QString message = "Opened " + fullPath;
                    msgBox.setText(message);
                    msgBox.exec();
                }
            } else {
                qDebug() << "File/directory no longer exists:" << fullPath;
                QMessageBox::warning(this, "Error", "File or directory no longer exists:\n" + fullPath);
            }
        } else {
            qDebug() << "No path data found for search result";
        }
    } else {
        // Handle normal file system interaction
        if (model.isDir(index)) {
            // If index is the same as root index of the folderView -> do nothing
            if (ui->folderView->rootIndex() == index) {
                return;
            }

            // Navigate to directory
            history_paths.push(ui->addressBar->text());
            ui->folderView->setRootIndex(index);

            // Map source index to proxy index for tree view
            QModelIndex proxyIndex = mapFromSourceModel(index);
            ui->treeView->setCurrentIndex(proxyIndex);
            ui->treeView->expand(proxyIndex);
            ui->addressBar->setText(model.filePath(index));
            if (detailsVisible)
                detailsWidget->setFileInfo(model.fileInfo(index));
        } else {
            QString filePath = model.filePath(index);
            QMessageBox msgBox;
            QString message = "Opened " + filePath;
            msgBox.setText(message);
            msgBox.exec();
        }
    }
}

void MainWindow::onFolderViewClicked(const QModelIndex &index)
{
    if (detailsVisible)
        detailsWidget->setFileInfo(model.fileInfo(index));
}



void MainWindow::onFolderViewContextMenuRequested(const QPoint &pos)
{
    qDebug() << "Context menu requested at" << pos;

    QMenu contextMenu(this);

    // Check if right-click is on an item
    QModelIndex index = ui->folderView->indexAt(pos);
    if (index.isValid()) {
        // Right-clicked on an item
        QAction *detailsAction = contextMenu.addAction("Show Details");
        connect(detailsAction, &QAction::triggered, [this, index]() {
            showFileDetails(index);
        });

        contextMenu.addSeparator();

        // Only allow rename if not in search mode
        if (!isSearching) {
            QAction *renameAction = contextMenu.addAction("Rename");
            connect(renameAction, &QAction::triggered, this, &MainWindow::onRename);
        }
    }

    // Only show "Create New Folder" option if not in search mode
    if (!isSearching) {
        QAction *newFolderAction = contextMenu.addAction("Create New Folder");
        connect(newFolderAction, &QAction::triggered, this, &MainWindow::onNewFolder);
    }
    contextMenu.exec(ui->folderView->viewport()->mapToGlobal(pos));
}

void MainWindow::onNewFolder()
{
    // Get the current directory from the address bar
    QString currentPath = ui->addressBar->text();
    QDir dir(currentPath);
    QFileInfo dirInfo(currentPath);
    qDebug() << "Current path:" << currentPath << "Exists:" << dir.exists() << "Writable:" << dirInfo.isWritable();
    if (!dir.exists()) {
        qDebug() << "Current directory does not exist:" << currentPath;
        return;
    }
    if (!dirInfo.isWritable()) {
        qDebug() << "Current directory is not writable:" << currentPath;
        return;
    }

    // Generate a unique folder name
    QString newFolderName = "New Folder";
    QString newFolderPath = dir.filePath(newFolderName);
    int suffix = 1;
    while (dir.exists(newFolderName)) {
        newFolderName = QString("New Folder (%1)").arg(suffix++);
        newFolderPath = dir.filePath(newFolderName);
    }
    qDebug() << "Creating new folder:" << newFolderPath;

    // Create the directory using QFileSystemModel
    QModelIndex parentIndex = model.index(currentPath);
    if (!parentIndex.isValid()) {
        qDebug() << "Invalid parent index for" << currentPath;
        return;
    }
    QModelIndex newFolderIndex = model.mkdir(parentIndex, newFolderName);
    if (!newFolderIndex.isValid()) {
        qDebug() << "Failed to create directory:" << newFolderPath;
        return;
    }
    qDebug() << "Directory created successfully:" << newFolderPath;

    // Ensure model updates before editing
    QTimer::singleShot(50, this, [=]() {
        qDebug() << "QTimer triggered for" << newFolderPath;

        // Re-fetch the index to ensure model is updated
        QModelIndex updatedFolderIndex = model.index(newFolderPath);
        if (!updatedFolderIndex.isValid()) {
            qDebug() << "Failed to find updated index for:" << newFolderPath;
            return;
        }

        // Get the index for the name column (column 0)
        QModelIndex nameIndex = model.index(updatedFolderIndex.row(), 0, updatedFolderIndex.parent());
        if (!nameIndex.isValid()) {
            qDebug() << "Failed to find name index for:" << newFolderPath;
            return;
        }
        qDebug() << "Name index valid for" << newFolderPath;

        Qt::ItemFlags flags = model.flags(nameIndex);

        // Select and highlight the new folder
        ui->folderView->clearSelection();
        ui->folderView->setCurrentIndex(nameIndex);
        ui->folderView->scrollTo(nameIndex);
        ui->folderView->setFocus();
        qDebug() << "FolderView has focus:" << ui->folderView->hasFocus();

        if (flags & Qt::ItemIsEditable) {
            ui->folderView->edit(nameIndex);
            qDebug() << "Attempted to start editing for" << newFolderPath;
        }
    });
}

void MainWindow::onRename()
{
    qDebug() << "on_raname_folder slot called";

    QModelIndex currentIndex = ui->folderView->currentIndex();
    if (!currentIndex.isValid())
    {
        qDebug() << "No valid folder selected for renaming";
        return;
    }

    QModelIndex nameIndex = model.index(currentIndex.row(), 0, currentIndex.parent());
    if (!nameIndex.isValid())
    {
        qDebug() << "Invalid name index for:" << model.filePath(currentIndex);
        return;
    }

    Qt::ItemFlags flags = model.flags(nameIndex);
    qDebug() << "Flags for name index:" << flags;
    if (!flags & Qt::ItemIsEditable)
    {
        qDebug() << "Name index is not editable for: " << model.filePath(currentIndex);
        return;
    }

    ui->folderView->clearSelection();
    ui->folderView->setCurrentIndex(nameIndex);
    ui->folderView->scrollTo(nameIndex);
    ui->folderView->setFocus();
    ui->folderView->edit(nameIndex);
    qDebug() << "Attempted to start renaming for" << model.filePath(nameIndex);
}






void MainWindow::onBackButtonClicked()
{
    // Clear search first
    clearSearch();

    if (!history_paths.isEmpty())
    {
        QString new_path = history_paths.pop();
        changeDir(new_path);
    }
}

void MainWindow::onUpButtonClicked()
{
    // Clear search first
    clearSearch();

    QString current_path = ui->addressBar->text();
    QDir dir(current_path);
    if (dir.cdUp())
    {
        history_paths.push(current_path);
        changeDir(dir.absolutePath());
    }
}


void MainWindow::changeDir(const QString &path)
{
    QDir dir(path);
    if (dir.exists())
    {
        QModelIndex sourceIndex = model.index(path);
        QModelIndex proxyIndex = mapFromSourceModel(sourceIndex);

        ui->folderView->setRootIndex(sourceIndex);
        ui->treeView->setCurrentIndex(proxyIndex);
        ui->treeView->expand(proxyIndex);
        ui->addressBar->setText(path);
    }
    else
    {
        qDebug() << "Invalid path: " << path;
    }
}






void MainWindow::onShowDetails()
{
    // Get the currently selected item in folderView
    QModelIndex currentIndex = ui->folderView->currentIndex();
    if (currentIndex.isValid()) {
        showFileDetails(currentIndex);
    } else {
        qDebug() << "No item selected for showing details";
    }
}

void MainWindow::onDetailsWidgetCloseRequested()
{
    if (detailsVisible) {
        detailsVisible = false;
        detailsWidget->hide();
    }
}







void MainWindow::onSearchResultsFound(const QList<SearchResult> &results)
{
    for (const SearchResult &result : results)
    {
        QList<QStandardItem*> rowItems;
        if (currentSearchOptions.mode == SearchMode::FileName) {
            // Filename search result
            QStandardItem *nameItem = new QStandardItem(result.icon, result.fileName);
            nameItem->setData(result.fullPath, Qt::UserRole);
            nameItem->setEditable(false);
            rowItems << nameItem;

            // Location
            QFileInfo fileInfo(result.fullPath);
            QString location = fileInfo.absolutePath();
            QString searchRoot = ui->addressBar->text();
            if (location.startsWith(searchRoot)) {
                location = location.mid(searchRoot.length());
                if (location.startsWith('/') || location.startsWith('\\'))
                    location = location.mid(1);
                if (location.isEmpty())
                    location = ".";
            }
            rowItems << new QStandardItem(location);

            // Size
            QString sizeText = result.isDirectory ? "" : formatFileSize(result.fileSize);
            rowItems << new QStandardItem(sizeText);

            // Type
            rowItems << new QStandardItem(result.fileType);

            // Modified
            rowItems << new QStandardItem(result.lastModified);

        } else {
            // Content search result
            QStandardItem *nameItem = new QStandardItem(result.icon, result.fileName);
            nameItem->setData(result.fullPath, Qt::UserRole);
            nameItem->setEditable(false);
            rowItems << nameItem;

            // Line number
            rowItems << new QStandardItem(QString::number(result.lineNumber));

            // Matched line
            QStandardItem *matchItem = new QStandardItem(result.matchedLine);
            matchItem->setToolTip(result.matchedLine); // Full text in tooltip
            rowItems << matchItem;

            // Size
            rowItems << new QStandardItem(formatFileSize(result.fileSize));

            // Modified
            rowItems << new QStandardItem(result.lastModified);
        }

        // Make all items non-editable
        for (QStandardItem *item : rowItems) {
            item->setEditable(false);
        }

        searchResultsModel->appendRow(rowItems);
    }
}

void MainWindow::onSearchCompleted(int totalResults)
{
    QString message = QString("Found %1 result%2").arg(totalResults).arg(totalResults == 1 ? "" : "s");
    ui->statusbar->showMessage(message, 10000);
    ui->searchButton->setText("Search");
}

void MainWindow::onSearchCancelled()
{
    QString message = QString("Search cancelled");
    ui->statusbar->showMessage(message, 5000);
    ui->searchButton->setText("Search");
}

void MainWindow::onSearchProgress(int filesProcessed, int directoriesProcessed)
{
    QString message = QString("Searching... %1 files, %2 folders").arg(filesProcessed).arg(directoriesProcessed);
    ui->statusbar->showMessage(message, 0);
}

void MainWindow::onSearchButtonClicked()
{
    // Check if we're currently searching - if so, stop the search
    if (isSearching && searchManager && searchManager->isSearching()) {
        searchManager->stopSearch();
        ui->searchButton->setText("Search");
        ui->statusbar->showMessage("Search stopped", 3000);
        return;
    }

    // Otherwise, start a new search if there's text
    QString searchText = ui->searchPrompt->text().trimmed();
    if (searchText.isEmpty()) {
        return;
    }

    startSearch(searchText);
}

void MainWindow::onClearButtonClicked()
{
    // If search is running, stop
    if (searchManager && searchManager->isSearching())
    {
        searchManager->stopSearch();
    }

    if (isSearching) {
        // Restore original view
        ui->folderView->setModel(&model);
        ui->folderView->setRootIndex(savedFolderViewRoot);
        isSearching = false;

        // Reset UI state
        ui->searchPrompt->clear();
        ui->searchButton->setText("Search");

        // Restore column widths
        ui->folderView->setColumnWidth(0, 400);
        for (int i = 1; i < 4; i++)
            ui->folderView->setColumnWidth(i, 150);

        ui->statusbar->showMessage("Search cleared", 2000);
    }
}

void MainWindow::onSearchPromptReturnPressed()
{
    onSearchButtonClicked();
}

void MainWindow::onSearchModeComboCurrentIndexChanged(int index)
{
    switch (index) {
    case 0:
        currentSearchOptions.mode = SearchMode::FileName;
        qDebug() << "Search mode set to: FileName";
        break;
    case 1:
        currentSearchOptions.mode = SearchMode::FileContent;
        qDebug() << "Search mode set to: FileContent";
        break;
    default:
        currentSearchOptions.mode = SearchMode::FileName;
        qDebug() << "Unknown index, defaulting to FileName";
        break;
    }

    // If we're currently searching, clear the search
    if (isSearching) {
        clearSearch();
    }
}


void MainWindow::startSearch(const QString &searchText)
{
    // Cancel any ongoing search
    if (searchManager && searchManager->isSearching()) {
        searchManager->stopSearch();
    }

    if (!isSearching) {
        // First time searching - setup UI
        savedFolderViewRoot = ui->folderView->rootIndex();

        // Setup model columns based on search mode
        searchResultsModel->clear();
        if (currentSearchOptions.mode == SearchMode::FileName) {
            searchResultsModel->setHorizontalHeaderLabels(
                QStringList() << "Name" << "Location" << "Size" << "Type" << "Modified");
        } else {
            searchResultsModel->setHorizontalHeaderLabels(
                QStringList() << "Name" << "Line" << "Match" << "Size" << "Modified");
        }

        // Switch to search results model
        ui->folderView->setModel(searchResultsModel);
        isSearching = true;

        // Update UI state
        ui->searchButton->setText("Stop");
        ui->statusbar->showMessage("Starting search...", 0);

        // Adjust column widths
        if (currentSearchOptions.mode == SearchMode::FileName) {
            ui->folderView->setColumnWidth(0, 300);
            ui->folderView->setColumnWidth(1, 350);
            ui->folderView->setColumnWidth(2, 80);
            ui->folderView->setColumnWidth(3, 80);
            ui->folderView->setColumnWidth(4, 120);
        } else {
            ui->folderView->setColumnWidth(0, 250);
            ui->folderView->setColumnWidth(1, 60);
            ui->folderView->setColumnWidth(2, 400);
            ui->folderView->setColumnWidth(3, 80);
            ui->folderView->setColumnWidth(4, 120);
        }
    } else {
        // Already searching - just clear results
        searchResultsModel->clear();

        // Setup headers
        if (currentSearchOptions.mode == SearchMode::FileName) {
            searchResultsModel->setHorizontalHeaderLabels(
                QStringList() << "Name" << "Location" << "Size" << "Type" << "Modified");
        } else {
            searchResultsModel->setHorizontalHeaderLabels(
                QStringList() << "Name" << "Line" << "Match" << "Size" << "Modified");
        }

        // Show message
        ui->statusbar->showMessage("Restarting search...", 0);
    }

    // Start the search
    QString searchDir = ui->addressBar->text();
    searchManager->startSearch(searchText, searchDir, currentSearchOptions);
}

void MainWindow::clearSearch()
{
    if (searchManager && searchManager->isSearching())
    {
        searchManager->stopSearch();
    }

    if (isSearching) {
        if (searchResultsModel) {
            searchResultsModel->clear();
            searchResultsModel->removeRows(0, searchResultsModel->rowCount());
        }

        // Restore original view
        ui->folderView->setModel(&model);
        ui->folderView->setRootIndex(savedFolderViewRoot);
        isSearching = false;

        // Reset UI state
        ui->searchPrompt->clear();
        ui->searchButton->setText("Search");

        // Restore column widths
        ui->folderView->setColumnWidth(0, 400);
        for (int i = 1; i < 4; i++)
            ui->folderView->setColumnWidth(i, 150);
    }
}



void MainWindow::init()
{
    QString rootPath = QDir::rootPath();
    model.setRootPath(rootPath);
    model.setReadOnly(false);

    // Create and setup the directory filter proxy model
    treeProxyModel = new DirectoryFilterProxyModel(this);
    treeProxyModel->setSourceModel(&model);

    // Set models
    ui->treeView->setModel(treeProxyModel);
    ui->folderView->setModel(&model);

    // Configure folderView for editing
    ui->folderView->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
    ui->folderView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->folderView->setSelectionBehavior(QAbstractItemView::SelectRows); // Select entire row
    ui->folderView->setFocusPolicy(Qt::StrongFocus);

    // Enable context menu
    ui->folderView->setContextMenuPolicy(Qt::CustomContextMenu);

    // Setup treeView, folderView and addressBar
    QString homePath = QDir::homePath();
    QModelIndex homeIndex = model.index(homePath);
    QModelIndex sourceIndex = model.index(rootPath);
    if (!sourceIndex.isValid()) {
        qDebug() << "Invalid root index for" << rootPath;
    }
    ui->searchPrompt->setPlaceholderText("Search current directory...");

    // Map source index to proxy index for treeView
    QModelIndex proxyIndex = treeProxyModel->mapFromSource(sourceIndex);
    ui->treeView->setRootIndex(proxyIndex);
    ui->folderView->setRootIndex(homeIndex);
    ui->addressBar->setText(homePath);
    ui->addressBar->setReadOnly(true);

    // Hide all column of treeView except for the first one
    for (int i = 1; i < model.columnCount(); i++)
    {
        ui->treeView->hideColumn(i);
    }
    ui->folderView->verticalHeader()->hide();
    ui->folderView->setColumnWidth(0, 400);
    for (int i = 1; i < 4; i++)
        ui->folderView->setColumnWidth(i, 150);

    // Signal connection
    connect(ui->treeView, &QTreeView::clicked, this, &MainWindow::onTreeViewClicked);
    connect(ui->folderView, &QTableView::doubleClicked, this, &MainWindow::onFolderViewDoubleClicked);
    connect(ui->folderView, &QTableView::clicked, this, &MainWindow::onFolderViewClicked);
    connect(ui->backButton, &QPushButton::clicked, this, &MainWindow::onBackButtonClicked);
    connect(ui->upButton, &QPushButton::clicked, this, &MainWindow::onUpButtonClicked);
    connect(ui->clearButton, &QPushButton::clicked, this, &MainWindow::onClearButtonClicked);
    connect(ui->folderView, &QTableView::customContextMenuRequested, this, &MainWindow::onFolderViewContextMenuRequested);
    connect(ui->searchModeCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onSearchModeComboCurrentIndexChanged);
    connect(ui->searchButton, &QPushButton::clicked, this, &MainWindow::onSearchButtonClicked);
    connect(ui->clearButton, &QPushButton::clicked, this, &MainWindow::onClearButtonClicked);
    connect(ui->searchPrompt, &QLineEdit::returnPressed, this, &MainWindow::onSearchPromptReturnPressed);

    // Shortcut for creating new folder
    QShortcut *newFolderShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N), this);
    connect(newFolderShortcut, &QShortcut::activated, this, &MainWindow::onNewFolder);

    // Shortcut for rename
    QShortcut *renameShortcut = new QShortcut(QKeySequence(Qt::Key_F2), ui->treeView);
    connect(renameShortcut, &QShortcut::activated, this, &MainWindow::onRename);

    // Init lastClickTime
    lastClickTime = QTime::currentTime();
}

void MainWindow::setupDetailsWidget()
{
    // Create the details widget
    detailsWidget = new FileDetailsWidget(this);

    // Connect the close signal
    connect(detailsWidget, &FileDetailsWidget::closeRequested,
            this, &MainWindow::onDetailsWidgetCloseRequested);

    // Add the details widget to the content layout (next to the main splitter)
    ui->contentLayout->addWidget(detailsWidget);

    // Initially hide the details widget
    detailsWidget->hide();
    detailsVisible = false;
}

void MainWindow::setupSearch()
{
    // Create search thread
    searchManager = new SearchManager(this);

    // Create search results model
    searchResultsModel = new QStandardItemModel(this);
    searchResultsModel->setHorizontalHeaderLabels(QStringList() << "Name" << "Size" << "Type" << "Date Modified");

    // Set default search mode
    ui->searchModeCombo->setCurrentIndex(0);

    // Connect search signals
    connect(searchManager, &SearchManager::resultsFound, this, &MainWindow::onSearchResultsFound);
    connect(searchManager, &SearchManager::searchCompleted, this, &MainWindow::onSearchCompleted);
    connect(searchManager, &SearchManager::searchCancelled, this, &MainWindow::onSearchCancelled);
    connect(searchManager, &SearchManager::searchProgress, this, &MainWindow::onSearchProgress);
}


void MainWindow::showFileDetails(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }

    QFileInfo fileInfo;

    if (isSearching) {
        // Get file path from search result data
        QModelIndex nameIndex = index.sibling(index.row(), 0);
        QString fullPath = nameIndex.data(Qt::UserRole).toString();
        if (fullPath.isEmpty()) {
            qDebug() << "No path data found for search result";
            return;
        }
        fileInfo = QFileInfo(fullPath);
    } else {
        // Normal file system mode
        QString filePath = model.filePath(index);
        fileInfo = QFileInfo(filePath);
    }

    detailsWidget->setFileInfo(fileInfo);
    if (!detailsVisible) {
        detailsVisible = true;
        detailsWidget->show();
        qDebug() << "Details widget shown as sidebar";
    } else {
        qDebug() << "Details widget already visible, just updating content";
    }
}


QModelIndex MainWindow::mapToSourceModel(const QModelIndex &proxyIndex)
{
    qDebug() << "mapToSourceModel is called";

    // Check if proxyIndex is valid
    if (!proxyIndex.isValid()) {
        return QModelIndex();
    }

    // Check if treeProxyModel is valid
    if (!treeProxyModel) {
        return proxyIndex;
    }

    if (proxyIndex.model() == treeProxyModel)
    {
        QModelIndex sourceIndex = treeProxyModel->mapToSource(proxyIndex);
        return sourceIndex;
    }

    return proxyIndex;
}

QModelIndex MainWindow::mapFromSourceModel(const QModelIndex &sourceIndex)
{
    return treeProxyModel->mapFromSource(sourceIndex);
}

QString MainWindow::formatFileSize(qint64 size)
{
    const qint64 kb = 1024;
    const qint64 mb = kb * 1024;
    const qint64 gb = mb * 1024;

    if (size >= gb) {
        return QString::number(size / gb, 'f', 1) + " GB";
    } else if (size >= mb) {
        return QString::number(size / mb, 'f', 1) + " MB";
    } else if (size >= kb) {
        return QString::number(size / kb, 'f', 1) + " KB";
    } else {
        return QString::number(size) + " bytes";
    }
}

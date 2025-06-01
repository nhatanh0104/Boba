#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QDir>
#include <QDebug>
#include <QTimer>
#include <QFileInfo>
#include <QMenu>
#include <QShortcut>
#include <QInputDialog>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    init();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::init()
{
    model.setRootPath(QDir::rootPath());
    model.setReadOnly(false);
    ui->treeView->setModel(&model);
    ui->folderView->setModel(&model);

    // Configure folderView for editing
    ui->folderView->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
    ui->folderView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->folderView->setSelectionBehavior(QAbstractItemView::SelectRows); // Select entire row
    ui->folderView->setFocusPolicy(Qt::StrongFocus);

    // Enable context menu
    ui->folderView->setContextMenuPolicy(Qt::CustomContextMenu);

    // Setup treeView, folderView and addressBar
    QString rootPath = "D:/KAIST/Academic/2025_Spring";
    QModelIndex index = model.index(rootPath);
    if (!index.isValid()) {
        qDebug() << "Invalid root index for" << rootPath;
    }
    ui->treeView->setRootIndex(index);
    ui->folderView->setRootIndex(index);
    ui->addressBar->setText(rootPath);

    // Hide all column of treeView except for the first one
    for (int i = 1; i < model.columnCount(); i++)
    {
        ui->treeView->hideColumn(i);
    }
    ui->folderView->verticalHeader()->hide();

    // Signal connection
    connect(ui->treeView, &QTreeView::clicked, this, &MainWindow::on_treeView_clicked);
    connect(ui->folderView, &QTableView::activated, this, &MainWindow::on_folderView_activated);
    connect(ui->backButton, &QPushButton::clicked, this, &MainWindow::on_backButton_clicked);
    connect(ui->upButton, &QPushButton::clicked, this, &MainWindow::on_upButton_clicked);
    connect(ui->searchButton, &QPushButton::clicked, this, &MainWindow::on_new_folder);
    connect(ui->folderView, &QTableView::customContextMenuRequested, this, &MainWindow::on_folderView_contextMenu_requested);

    QShortcut *newFolderShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N), this);
    connect(newFolderShortcut, &QShortcut::activated, this, &MainWindow::on_new_folder);
    qDebug() << "Ctrl+Shift+N shortcut connected";
}

void MainWindow::on_treeView_clicked(const QModelIndex &index)
{
    // If index is the same as root index of the folderView -> do nothing
    if (ui->folderView->rootIndex() == index)
    {
        return;
    }

    // Else display the directory specified by index
    history_paths.push(ui->addressBar->displayText());
    qDebug() << "Pushed" << ui->addressBar->displayText() <<" to stack";
    ui->treeView->expand(index);
    ui->folderView->setRootIndex(index);
    ui->addressBar->setText(model.filePath(index));
    history_paths.push(model.filePath(index));
}

void MainWindow::on_folderView_activated(const QModelIndex &index)
{
    // If index is the same as root index of the folderView -> do nothing
    if (ui->folderView->rootIndex() == index)
    {
        return;
    }

    // Else display the directory specified by index
    history_paths.push(ui->addressBar->displayText());
    qDebug() << "Pushed" << ui->addressBar->displayText() << "to stack";
    ui->folderView->setRootIndex(index);
    ui->treeView->setCurrentIndex(index);
    ui->treeView->expand(index);
    ui->addressBar->setText(model.filePath(index));
}

void MainWindow::on_new_folder()
{
    qDebug() << "on_new_folder slot called";

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
    QTimer::singleShot(100, this, [=]() {
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

        // Verify editability
        Qt::ItemFlags flags = model.flags(nameIndex);
        qDebug() << "Flags for name index:" << flags;

        // Select and highlight the new folder
        ui->folderView->clearSelection();
        ui->folderView->setCurrentIndex(nameIndex);
        ui->folderView->scrollTo(nameIndex);
        ui->folderView->setFocus();
        qDebug() << "FolderView has focus:" << ui->folderView->hasFocus();

        if (flags & Qt::ItemIsEditable) {
            // Attempt to open the rename editor
            ui->folderView->edit(nameIndex);
            qDebug() << "Attempted to start editing for" << newFolderPath;
        } else {
            qDebug() << "Name index is not editable for" << newFolderPath;
            // Fallback to QInputDialog
            QString newName = QInputDialog::getText(this, "Rename Folder", "Enter folder name:", QLineEdit::Normal, newFolderName);
            if (!newName.isEmpty() && newName != newFolderName) {
                bool renamed = model.setData(nameIndex, newName, Qt::EditRole);
                qDebug() << "Rename via QInputDialog" << (renamed ? "succeeded" : "failed") << "to" << newName;
            } else {
                qDebug() << "Rename via QInputDialog cancelled or unchanged";
            }
        }
    });
}

void MainWindow::on_folderView_contextMenu_requested(const QPoint &pos)
{
    qDebug() << "Context menu requested at" << pos;

    QMenu contextMenu(this);
    QAction *newFolderAction = contextMenu.addAction("Create New Folder");
    connect(newFolderAction, &QAction::triggered, this, &MainWindow::on_new_folder);

    contextMenu.exec(ui->folderView->viewport()->mapToGlobal(pos));
}

void MainWindow::on_backButton_clicked()
{
    if (!history_paths.isEmpty())
    {
        QString new_path = history_paths.pop();
        qDebug() << "Popped" << new_path << "from stack";
        change_dir(new_path);
    }
}

void MainWindow::on_upButton_clicked()
{
    QString current_path = ui->addressBar->displayText();
    QDir dir(current_path);
    if (dir.cdUp())
    {
        history_paths.push(current_path);
        qDebug() << "Pushed" << current_path << "to stack";
        change_dir(dir.absolutePath());
    }
}

void MainWindow::change_dir(const QString &path)
{
    QDir dir(path);
    if (dir.exists())
    {
        QModelIndex index = model.index(path);
        ui->folderView->setRootIndex(index);
        ui->treeView->setCurrentIndex(index);
        ui->treeView->expand(index);
        ui->addressBar->setText(path);
    }
    else
    {
        qDebug() << "Invalid path: " << path;
    }
}

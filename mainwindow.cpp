#include "mainwindow.h"
#include "./ui_mainwindow.h"

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
    ui->treeView->setModel(&model);
    ui->folderView->setModel(&model);

    QModelIndex index = model.index("D:/KAIST/Academic/2024_Fall");

    ui->treeView->setRootIndex(index);
    ui->folderView->setRootIndex(index);
    ui->addressBar->setText("D:/KAIST/Academic/2024_Fall");

    for (int i = 1; i < model.columnCount(); i++)
    {
        ui->treeView->hideColumn(i);
    }
    ui->folderView->verticalHeader()->hide();
}

void MainWindow::on_treeView_activated(const QModelIndex &index)
{
    history_paths.push(ui->addressBar->displayText());
    qDebug() << "Pushed " << ui->addressBar->displayText() << " to stack.";
    ui->folderView->setRootIndex(index);
    ui->addressBar->setText(model.filePath(index));
    history_paths.push(model.filePath(index));
}


void MainWindow::on_folderView_activated(const QModelIndex &index)
{
    history_paths.push(ui->addressBar->displayText());
    qDebug() << "Pushed " << ui->addressBar->displayText() << " to stack.";
    ui->folderView->setRootIndex(index);
    ui->treeView->setCurrentIndex(index);
    ui->treeView->expand(index);
    ui->addressBar->setText(model.filePath(index));
}


void MainWindow::on_backButton_clicked()
{
    if (!history_paths.isEmpty())
    {
        QString new_path = history_paths.pop();
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
        qDebug() << "Pushed " << current_path << " to stack.";
        change_dir(dir.absolutePath());
    }
}

void MainWindow::change_dir(const QString &path)
{
    QModelIndex index = model.index(path);
    ui->folderView->setRootIndex(index);
    ui->treeView->setCurrentIndex(index);
    ui->treeView->expand(index);
    ui->addressBar->setText(path);
}

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileSystemModel>
#include <QStack>
#include <QPoint>

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

private slots:
    void on_treeView_clicked(const QModelIndex &index);
    void on_folderView_activated(const QModelIndex &index);
    void on_backButton_clicked();
    void on_upButton_clicked();
    void on_new_folder();
    void on_folderView_contextMenu_requested(const QPoint &pos);

private:
    Ui::MainWindow *ui;
    QFileSystemModel model;
    QStack<QString> history_paths;

    void init();
    void change_dir(const QString &path);
};
#endif // MAINWINDOW_H

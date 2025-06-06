#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileSystemModel>
#include <QStack>
#include <QPoint>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include "filedetailswidget.h"
#include "directoryfilterproxymodel.h"

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

protected:
    // Override drag and drop event
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void on_treeView_clicked(const QModelIndex &index);
    void on_folderView_activated(const QModelIndex &index);
    void on_new_folder();
    void on_rename();
    void on_folderView_contextMenu_requested(const QPoint &pos);
    void on_backButton_clicked();
    void on_upButton_clicked();
    void on_showDetails();
    void on_detailsWidget_closeRequested();

private:
    void init();
    void change_dir(const QString &path);
    void setupDetailsWidget();
    void showFileDetails(const QModelIndex &index);
    QModelIndex mapToSourceModel(const QModelIndex &proxyIndex);
    QModelIndex mapFromSourceModel(const QModelIndex &sourceIndex);

    Ui::MainWindow *ui;
    QFileSystemModel model;
    DirectoryFilterProxyModel *treeProxyModel;
    QStack<QString> history_paths;

    // Details widget
    FileDetailsWidget *detailsWidget;
    QWidget *centralWidget;
    QHBoxLayout *mainLayout;
    bool detailsVisible;
};
#endif // MAINWINDOW_H

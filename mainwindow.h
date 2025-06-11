#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileSystemModel>
#include <QStack>
#include <QPoint>
#include <QStandardItemModel>
#include <QTime>
#include "filedetailswidget.h"
#include "directoryfilterproxymodel.h"
#include "searchmanager.h"

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
    // Views
    void on_treeView_clicked(const QModelIndex &index);
    void on_folderView_doubleClicked(const QModelIndex &index);

    // Right click context menu
    void on_folderView_contextMenu_requested(const QPoint &pos);
    void on_new_folder();
    void on_rename();

    // Buttons
    void on_backButton_clicked();
    void on_upButton_clicked();
    void on_clearButton_clicked();

    // Details Widget
    void on_showDetails();
    void on_detailsWidget_closeRequested();

    // Search-related slots
    // void on_searchPrompt_textChanged(const QString &text);
    void onSearchResultFound(const SearchResult &result);
    void onSearchCompleted(int totalResults);
    void onSearchCancelled();
    void onSearchProgress(int fileProcessed, int directoriesProcessed);
    void clearSearch();
    void on_searchButton_clicked();
    void on_searchPrompt_returnPressed();
    void on_searchModeCombo_currentIndexChanged(int index);

private:
    void init();
    void change_dir(const QString &path);
    void setupDetailsWidget();
    void setupSearch();
    void showFileDetails(const QModelIndex &index);
    QModelIndex mapToSourceModel(const QModelIndex &proxyIndex);
    QModelIndex mapFromSourceModel(const QModelIndex &sourceIndex);
    void startSearch(const QString &searchText);
    QString formatFileSize(qint64 size);
    bool isRecentlyClicked();

    Ui::MainWindow *ui;
    QFileSystemModel model;
    DirectoryFilterProxyModel *treeProxyModel;
    QStack<QString> history_paths;
    QTime lastClickTime;
    const int DEBOUNCE_THRESHOLDMS = 100;

    // Details widget
    FileDetailsWidget *detailsWidget;
    bool detailsVisible;

    // Search-related
    SearchManager *searchManager;
    QSortFilterProxyModel *searchProxyModel;
    QStandardItemModel *searchResultsModel;
    bool isSearching;
    QModelIndex savedFolderViewRoot;
    SearchOptions currentSearchOptions;
};

#endif // MAINWINDOW_H

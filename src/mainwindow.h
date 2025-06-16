#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileSystemModel>
#include <QStack>
#include <QPoint>
#include <QStandardItemModel>
#include <QTime>
#include "widgets/filedetailswidget.h"
#include "models/directoryfilterproxymodel.h"
#include "search/searchmanager.h"

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
    void onTreeViewClicked(const QModelIndex &index);
    void onFolderViewDoubleClicked(const QModelIndex &index);
    void onFolderViewClicked(const QModelIndex &index);

    // Right click context menu
    void onFolderViewContextMenuRequested(const QPoint &pos);
    void onNewFolder();
    void onRename();

    // Buttons
    void onBackButtonClicked();
    void onUpButtonClicked();
    void onClearButtonClicked();

    // Details Widget
    void onShowDetails();
    void onDetailsWidgetCloseRequested();

    // Search-related slots
    // void on_searchPrompt_textChanged(const QString &text);
    void onSearchResultsFound(const QList<SearchResult> &results);
    void onSearchCompleted(int totalResults);
    void onSearchCancelled();
    void onSearchProgress(int fileProcessed, int directoriesProcessed);
    void clearSearch();
    void onSearchButtonClicked();
    void onSearchPromptReturnPressed();
    void onSearchModeComboCurrentIndexChanged(int index);

private:
    void init();
    void changeDir(const QString &path);
    void setupDetailsWidget();
    void setupSearch();
    void showFileDetails(const QModelIndex &index);
    QModelIndex mapToSourceModel(const QModelIndex &proxyIndex);
    QModelIndex mapFromSourceModel(const QModelIndex &sourceIndex);
    void startSearch(const QString &searchText);
    QString formatFileSize(qint64 size);

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

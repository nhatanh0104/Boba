#ifndef SEARCHTHREAD_H
#define SEARCHTHREAD_H

#include <QThread>
#include <QFileInfo>
#include <QDir>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QMutex>
#include <QAtomicInt>

struct SearchResult
{
    QString fileName;
    QString fullPath;
    qint64 fileSize;
    QString fileType;
    QString lastModified;
    bool isDirectory;
    QIcon icon;
};

class SearchThread : public QThread
{
    Q_OBJECT
public: 
    explicit SearchThread(QObject *parent = nullptr);
    ~SearchThread();

    void startSearch(const QString &searchText, const QString &rootPath);
    void stopSearch();
    bool isSearching() const;

protected:
    void run() override;

signals:
    void searchProgress(int filesProcessed, int directoriesProcessed);
    void resultFound(const SearchResult &result);
    void searchCompleted(int totalResults);
    void searchCancelled();

private:
    void searchRecursively(const QString &dirPath);
    QString formatFileSize(qint64 size);
    QString getFileType(const QFileInfo &fileInfo);

    QString m_searchText;
    QString m_rootPath;
    QAtomicInt m_shouldStop;
    QAtomicInt m_filesProcessed;
    QAtomicInt m_directoriesProcessed;
    QAtomicInt m_resultsFound;
    mutable QMutex m_mutex;
};

#endif // SEARCHTHREAD_H

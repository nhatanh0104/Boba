#ifndef SEARCHMANAGER_H
#define SEARCHMANAGER_H

#include <QObject>
#include <QFileInfo>
#include <QMutex>
#include <QAtomicInt>
#include <QThreadPool>
#include <QRunnable>
#include <QTimer>
#include <QWaitCondition>
#include <QIcon>
#include <QQueue>

enum SearchMode
{
    FileName,       // Search in file names
    FileContent,    // Search in file contents
};



struct SearchResult
{
    QString fileName;
    QString fullPath;
    qint64 fileSize;
    QString fileType;
    QString lastModified;
    bool isDirectory;
    QIcon icon;

    // For content search
    QString matchedLine;
    int lineNumber;
};




struct SearchOptions
{
    SearchMode mode = SearchMode::FileName;
    qint64 maxFileSizeBytes = 10 * 1024 * 1024;
};




class SearchManager;

// Worker task for searching a single directory
class DirectorySearchWorker : public QRunnable
{
public:
    DirectorySearchWorker(const QString &dirPath, const QString &searchText,
                          const SearchOptions &options, SearchManager *manager);
    void run() override;

private:
    bool searchInFile(const QFileInfo &fileInfo, QList<SearchResult> &results);
    QString getFileType(const QFileInfo &fileInfo);
    SearchResult createSearchResult(const QFileInfo &fileInfo, int lineNumber, const QString &matchedLine);

    QString m_dirPath;
    QString m_searchText;
    SearchOptions m_options;
    SearchManager *m_manager;
    const int BATCH_SIZE = 15;
};







class SearchManager : public QObject
{
    Q_OBJECT
public:
    explicit SearchManager(QObject *parent = nullptr);
    ~SearchManager();

    void startSearch(const QString &searchText, const QString &rootPath, const SearchOptions &options = SearchOptions());
    void stopSearch();
    bool isSearching() const;

    // Thread-safe methods for worker tasks
    void reportResults(const QList<SearchResult> &results);
    void incrementCounters(int files, int directories);
    bool shouldStop() const;
    void workerFinished();
    void addDirectoryToQueue(const QString &dirPath);  // Workers can add new directories

signals:
    void searchProgress(int filesProcessed, int directoriesProcessed);
    void resultsFound(const QList<SearchResult> &results);
    void searchCompleted(int totalResults);
    void searchCancelled();

private slots:
    void onProgressTimer();
    void performSearch();
    void finishSearch();

private:
    void startInitialSearch();


    mutable QMutex m_mutex;
    mutable QMutex m_resultMutex;
    mutable QMutex m_queueMutex;  // For thread-safe work queue
    QString m_searchText;
    QString m_rootPath;
    SearchOptions m_options;

    QAtomicInt m_shouldStop;
    QAtomicInt m_filesProcessed;
    QAtomicInt m_directoriesProcessed;
    QAtomicInt m_resultsFound;
    QAtomicInt m_activeWorkers;

    QThreadPool *m_threadPool;
    QTimer *m_progressTimer;
    QQueue<QString> m_workQueue;  // Thread-safe work queue
    QWaitCondition m_hasWork;     // Signal when work is available
};

#endif // SEARCHMANAGER_H

#ifndef SEARCHTHREAD_H
#define SEARCHTHREAD_H

#include <QThread>
#include <QFileInfo>
#include <QDir>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QMutex>
#include <QAtomicInt>

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
    QStringList textFileExtensions = {
        "txt", "cpp", "h", "hpp", "c", "cc", "java", "py", "js",
        "html", "css", "xml", "json", "md", "ini", "conf", "log"
    };
};

class SearchThread : public QThread
{
    Q_OBJECT
public:
    explicit SearchThread(QObject *parent = nullptr);
    ~SearchThread();

    void startSearch(const QString &searchText, const QString &rootPath, const SearchOptions &options = SearchOptions());
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
    bool searchInFile(const QFileInfo &fileInfo);
    bool isTextFile(const QFileInfo &fileInfo);
    QString formatFileSize(qint64 size);
    QString getFileType(const QFileInfo &fileInfo);

    mutable QMutex m_mutex;
    QString m_searchText;
    QString m_rootPath;
    SearchOptions m_options;

    QAtomicInt m_shouldStop;
    QAtomicInt m_filesProcessed;
    QAtomicInt m_directoriesProcessed;
    QAtomicInt m_resultsFound;

};

#endif // SEARCHTHREAD_H

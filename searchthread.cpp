#include "searchthread.h"
#include <QDebug>
#include <QFileIconProvider>
#include <QApplication>
#include <QDateTime>
#include <QTextStream>

SearchThread::SearchThread(QObject *parent)
    : QThread(parent)
    , m_shouldStop(0)
    , m_filesProcessed(0)
    , m_directoriesProcessed(0)
    , m_resultsFound(0)
{
}

SearchThread::~SearchThread()
{
    stopSearch();
    if (!wait(3000)) {
        terminate();
        wait();
    }
}

void SearchThread::startSearch(const QString &searchText, const QString &rootPath, const SearchOptions &options)
{
    QMutexLocker locker(&m_mutex);

    if (isRunning())
    {
        stopSearch();
        wait();
    }

    m_searchText = searchText;
    m_rootPath = rootPath;
    m_options = options;
    m_shouldStop = 0;
    m_filesProcessed = 0;
    m_directoriesProcessed = 0;
    m_resultsFound = 0;

    start();
}

void SearchThread::stopSearch()
{
    m_shouldStop = 1;
}

bool SearchThread::isSearching() const
{
    return isRunning();
}

void SearchThread::run()
{
    qDebug() << "Search thread started for:" << m_searchText << "in" << m_rootPath;
    qDebug() << "Search mode:" << (m_options.mode == SearchMode::FileName ? "FileName" : "FileContent");

    try {
        searchRecursively(m_rootPath);

        if (!m_shouldStop.loadAcquire())
        {
            emit searchCompleted(m_resultsFound.loadAcquire());
            qDebug() << "Search completed. Total results:" << m_resultsFound.loadAcquire();
        } else
        {
            emit searchCancelled();
            qDebug() << "Search was cancelled";
        }
    }
    catch (...)
    {
        qDebug() << "Exception occurred during search";
        emit searchCancelled();
    }
}

void SearchThread::searchRecursively(const QString &dirPath)
{
    if (m_shouldStop.loadAcquire()) {
        return;
    }

    QDir dir(dirPath);
    if (!dir.exists() || !dir.isReadable()) {
        return;
    }

    m_directoriesProcessed.fetchAndAddAcquire(1);

    // Emit progress every 50 processed directories
    if (m_directoriesProcessed.loadAcquire() % 50 == 0) {
        emit searchProgress(m_filesProcessed.loadAcquire(), m_directoriesProcessed.loadAcquire());
    }

    QFileInfoList entries = dir.entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden,
        QDir::Name | QDir::DirsFirst
        );

    QFileIconProvider iconProvider;

    for (const QFileInfo &fileInfo : entries) {
        if (m_shouldStop.loadAcquire()) {
            return;
        }

        m_filesProcessed.fetchAndAddAcquire(1);

        if (m_options.mode == SearchMode::FileName) {
            // Search only in filename
            QString fileName = fileInfo.fileName();

            if (fileName.contains(m_searchText, Qt::CaseInsensitive)) {
                SearchResult result;
                result.fileName = fileName;
                result.fullPath = fileInfo.absoluteFilePath();
                result.fileSize = fileInfo.isFile() ? fileInfo.size() : 0;
                result.fileType = getFileType(fileInfo);
                result.lastModified = fileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss");
                result.isDirectory = fileInfo.isDir();
                result.icon = iconProvider.icon(fileInfo);
                result.lineNumber = 0;

                emit resultFound(result);
                m_resultsFound.fetchAndAddAcquire(1);
            }
        } else if (m_options.mode == SearchMode::FileContent) {
            // Search only in file content
            if (fileInfo.isFile() && searchInFile(fileInfo)) {
                // Results are emitted from searchInFile
            }
        }

        // Recursively search subdirectories
        if (fileInfo.isDir()) {
            searchRecursively(fileInfo.absoluteFilePath());
        }

        // Yield control periodically
        if (m_filesProcessed.loadAcquire() % 100 == 0) {
            msleep(1);
        }
    }
}

bool SearchThread::searchInFile(const QFileInfo &fileInfo)
{
    if (fileInfo.size() > m_options.maxFileSizeBytes)
    {
        return false;
    }

    if (!isTextFile(fileInfo))
    {
        return false;
    }

    QFile file(fileInfo.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    int lineNumber = 0;
    bool foundAny = false;

    while (!stream.atEnd() && !m_shouldStop.loadAcquire())
    {
        QString line = stream.readLine();
        lineNumber++;

        if (line.contains(m_searchText, Qt::CaseInsensitive)) {
            SearchResult result;
            result.fileName = fileInfo.fileName();
            result.fullPath = fileInfo.absoluteFilePath();
            result.fileSize = fileInfo.size();
            result.fileType = getFileType(fileInfo);
            result.lastModified = fileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss");
            result.isDirectory = false;
            result.lineNumber = lineNumber;
            result.matchedLine = line.trimmed();

            // Truncate long lines
            if (result.matchedLine.length() > 200) {
                int pos = result.matchedLine.indexOf(m_searchText, 0, Qt::CaseInsensitive);
                if (pos > 100) {
                    result.matchedLine = "..." + result.matchedLine.mid(pos - 50, 150) + "...";
                } else {
                    result.matchedLine = result.matchedLine.left(200) + "...";
                }
            }

            QFileIconProvider iconProvider;
            result.icon = iconProvider.icon(fileInfo);

            emit resultFound(result);
            m_resultsFound.fetchAndAddAcquire(1);
            foundAny = true;

            // Optional: limit results per file to avoid too many from one file
            static const int MAX_RESULTS_PER_FILE = 5;
            if (m_resultsFound.loadAcquire() % MAX_RESULTS_PER_FILE == 0) {
                break;
            }
        }
    }

    file.close();
    return foundAny;
}

bool SearchThread::isTextFile(const QFileInfo &fileInfo)
{
    QString suffix = fileInfo.suffix().toLower();
    return m_options.textFileExtensions.contains(suffix) || suffix.isEmpty();
}

QString SearchThread::formatFileSize(qint64 size)
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

QString SearchThread::getFileType(const QFileInfo &fileInfo)
{
    if (fileInfo.isDir()) {
        return "Folder";
    } else if (fileInfo.isFile()) {
        QString suffix = fileInfo.suffix().toUpper();
        return suffix.isEmpty() ? "File" : suffix + " File";
    } else if (fileInfo.isSymLink()) {
        return "Shortcut";
    } else {
        return "Unknown";
    }
}

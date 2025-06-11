#include "searchmanager.h"
#include <QDebug>
#include <QDirIterator>
#include <QDir>
#include <QFileIconProvider>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMetaObject>
#include <QCoreApplication>
#include <algorithm>
#include <random>

DirectorySearchWorker::DirectorySearchWorker(const QString &dirPath, const QString &searchText,
                                             const SearchOptions &options, SearchManager *manager)
    : m_dirPath(dirPath), m_searchText(searchText), m_options(options), m_manager(manager)
{
    setAutoDelete(true);
}

void DirectorySearchWorker::run()
{
    if (m_manager->shouldStop()) {
        m_manager->workerFinished();
        return;
    }

    QDir dir(m_dirPath);
    if (!dir.exists() || !dir.isReadable()) {
        m_manager->workerFinished();
        return;
    }

    // Get all entries (files AND subdirectories)
    QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden, QDir::Name);

    QFileIconProvider iconProvider;
    int processedCount = 0;

    for (const QFileInfo &fileInfo : entries) {
        if (m_manager->shouldStop()) {
            break;
        }

        if (fileInfo.isDir()) {
            // Add subdirectory to work queue for other workers to process
            m_manager->addDirectoryToQueue(fileInfo.absoluteFilePath());
            QString fileName = fileInfo.fileName();
            if (fileName.contains(m_searchText, Qt::CaseInsensitive)) {
                SearchResult result;
                result.fileName = fileName;
                result.fullPath = fileInfo.absoluteFilePath();
                result.fileSize = fileInfo.size();
                result.fileType = getFileType(fileInfo);
                result.lastModified = fileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss");
                result.isDirectory = false;
                result.icon = iconProvider.icon(fileInfo);
                result.lineNumber = 0;

                m_manager->reportResult(result);
            }
        } else {
            // Process file
            processedCount++;

            if (m_options.mode == SearchMode::FileName) {
                // Search in filename
                QString fileName = fileInfo.fileName();
                if (fileName.contains(m_searchText, Qt::CaseInsensitive)) {
                    SearchResult result;
                    result.fileName = fileName;
                    result.fullPath = fileInfo.absoluteFilePath();
                    result.fileSize = fileInfo.size();
                    result.fileType = getFileType(fileInfo);
                    result.lastModified = fileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss");
                    result.isDirectory = false;
                    result.icon = iconProvider.icon(fileInfo);
                    result.lineNumber = 0;

                    m_manager->reportResult(result);
                }
            } else if (m_options.mode == SearchMode::FileContent) {
                // Search in file content
                searchInFile(fileInfo);
            }

            // Report progress every 25 files
            if (processedCount % 25 == 0) {
                m_manager->incrementCounters(25, 0);
            }
        }
    }

    // Report remaining progress
    if (processedCount % 25 != 0) {
        m_manager->incrementCounters(processedCount % 25, 1);
    }

    m_manager->workerFinished();
}

bool DirectorySearchWorker::searchInFile(const QFileInfo &fileInfo)
{
    if (fileInfo.size() > m_options.maxFileSizeBytes || !isTextFile(fileInfo)) {
        return false;
    }

    QFile file(fileInfo.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    int lineNumber = 0;
    bool foundAny = false;
    int resultsInFile = 0;
    const int MAX_RESULTS_PER_FILE = 3;

    while (!stream.atEnd() && !m_manager->shouldStop() && resultsInFile < MAX_RESULTS_PER_FILE) {
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

            // Truncate very long lines
            if (result.matchedLine.length() > 150) {
                int pos = result.matchedLine.indexOf(m_searchText, 0, Qt::CaseInsensitive);
                if (pos > 50) {
                    result.matchedLine = "..." + result.matchedLine.mid(pos - 30, 120) + "...";
                } else {
                    result.matchedLine = result.matchedLine.left(150) + "...";
                }
            }

            QFileIconProvider iconProvider;
            result.icon = iconProvider.icon(fileInfo);

            m_manager->reportResult(result);
            foundAny = true;
            resultsInFile++;
        }
    }

    file.close();
    return foundAny;
}

bool DirectorySearchWorker::isTextFile(const QFileInfo &fileInfo)
{
    QString suffix = fileInfo.suffix().toLower();
    return m_options.textFileExtensions.contains(suffix) || suffix.isEmpty();
}

QString DirectorySearchWorker::getFileType(const QFileInfo &fileInfo)
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

SearchManager::SearchManager(QObject *parent)
    : QObject(parent)
    , m_shouldStop(0)
    , m_filesProcessed(0)
    , m_directoriesProcessed(0)
    , m_resultsFound(0)
    , m_activeWorkers(0)
    , m_threadPool(nullptr)
    , m_progressTimer(nullptr)
{
    // Create thread pool
    m_threadPool = new QThreadPool(this);

    // Use a reasonable default thread count
    const int DEFAULT_THREAD_COUNT = 8;
    int threadCount = qMax(1, qMin(DEFAULT_THREAD_COUNT, QThread::idealThreadCount()));
    m_threadPool->setMaxThreadCount(threadCount);

    // Progress timer for UI updates
    m_progressTimer = new QTimer(this);
    m_progressTimer->setInterval(300); // Update every 300ms
    connect(m_progressTimer, &QTimer::timeout, this, &SearchManager::onProgressTimer);

    qDebug() << "SearchManager initialized with" << threadCount << "worker threads";
}

SearchManager::~SearchManager()
{
    stopSearch();
}

void SearchManager::startSearch(const QString &searchText, const QString &rootPath, const SearchOptions &options)
{
    QMutexLocker locker(&m_mutex);

    // Stop any existing search
    if (isSearching()) {
        stopSearch();
    }

    m_searchText = searchText;
    m_rootPath = rootPath;
    m_options = options;
    m_shouldStop = 0;
    m_filesProcessed = 0;
    m_directoriesProcessed = 0;
    m_resultsFound = 0;
    m_activeWorkers = 0;

    // Clear work queue
    {
        QMutexLocker queueLocker(&m_queueMutex);
        m_workQueue.clear();
    }

    // Start the search asynchronously using Qt's event system
    QMetaObject::invokeMethod(this, "performSearch", Qt::QueuedConnection);
}

void SearchManager::stopSearch()
{
    m_shouldStop = 1;

    if (m_threadPool) {
        m_threadPool->clear();              // Clear pending tasks
        m_threadPool->waitForDone(2000);    // Wait for active tasks
    }

    if (m_progressTimer) {
        m_progressTimer->stop();
    }
}

bool SearchManager::isSearching() const
{
    bool hasActiveWorkers = m_activeWorkers.loadAcquire() > 0;
    bool hasQueuedWork = false;

    {
        QMutexLocker queueLocker(&m_queueMutex);
        hasQueuedWork = !m_workQueue.isEmpty();
    }

    return hasActiveWorkers || hasQueuedWork ||
           (m_threadPool && m_threadPool->activeThreadCount() > 0);
}

void SearchManager::reportResult(const SearchResult &result)
{
    QMutexLocker locker(&m_resultMutex);
    if (!m_shouldStop.loadAcquire()) {
        emit resultFound(result);
        m_resultsFound.fetchAndAddAcquire(1);
    }
}

void SearchManager::incrementCounters(int files, int directories)
{
    if (files > 0) {
        m_filesProcessed.fetchAndAddAcquire(files);
    }
    if (directories > 0) {
        m_directoriesProcessed.fetchAndAddAcquire(directories);
    }
}

bool SearchManager::shouldStop() const
{
    return m_shouldStop.loadAcquire() != 0;
}

void SearchManager::workerFinished()
{
    int remaining = m_activeWorkers.fetchAndSubAcquire(1) - 1;

    // Try to start a new worker if there's more work
    QString nextDir;
    {
        QMutexLocker queueLocker(&m_queueMutex);
        if (!m_workQueue.isEmpty()) {
            nextDir = m_workQueue.dequeue();
        }
    }

    if (!nextDir.isEmpty() && !m_shouldStop.loadAcquire()) {
        // Start a new worker for the next directory
        DirectorySearchWorker *worker = new DirectorySearchWorker(
            nextDir, m_searchText, m_options, this);
        m_activeWorkers.fetchAndAddAcquire(1);
        m_threadPool->start(worker);
    } else if (remaining == 0) {
        // No more workers and no more work - finish search
        bool hasWork;
        {
            QMutexLocker queueLocker(&m_queueMutex);
            hasWork = !m_workQueue.isEmpty();
        }

        if (!hasWork) {
            QMetaObject::invokeMethod(this, "finishSearch", Qt::QueuedConnection);
        }
    }
}

void SearchManager::performSearch()
{
    qDebug() << "Multi-threaded search started:" << m_searchText << "in" << m_rootPath;

    try {
        m_progressTimer->start();
        startInitialSearch();
    }
    catch (...) {
        qDebug() << "Exception in search manager";
        emit searchCancelled();
        m_progressTimer->stop();
    }
}

void SearchManager::startInitialSearch()
{
    // Add root directory to work queue
    {
        QMutexLocker queueLocker(&m_queueMutex);
        m_workQueue.enqueue(m_rootPath);
    }

    // Start initial workers - they will create more work as they discover subdirectories
    int initialWorkers = m_threadPool->maxThreadCount();

    for (int i = 0; i < initialWorkers && !m_shouldStop.loadAcquire(); ++i) {
        QString dirPath;
        {
            QMutexLocker queueLocker(&m_queueMutex);
            if (!m_workQueue.isEmpty()) {
                dirPath = m_workQueue.dequeue();
            }
        }

        if (!dirPath.isEmpty()) {
            DirectorySearchWorker *worker = new DirectorySearchWorker(
                dirPath, m_searchText, m_options, this);
            m_activeWorkers.fetchAndAddAcquire(1);
            m_threadPool->start(worker);
        }
    }

    qDebug() << "Started initial search with" << initialWorkers << "workers";
}

void SearchManager::addDirectoryToQueue(const QString &dirPath)
{
    if (m_shouldStop.loadAcquire()) {
        return;
    }

    {
        QMutexLocker queueLocker(&m_queueMutex);
        m_workQueue.enqueue(dirPath);
    }

    // Try to start a new worker if we have work and available threads
    if (m_threadPool->activeThreadCount() < m_threadPool->maxThreadCount()) {
        QString nextDir;
        {
            QMutexLocker queueLocker(&m_queueMutex);
            if (!m_workQueue.isEmpty()) {
                nextDir = m_workQueue.dequeue();
            }
        }

        if (!nextDir.isEmpty()) {
            DirectorySearchWorker *worker = new DirectorySearchWorker(
                nextDir, m_searchText, m_options, this);
            m_activeWorkers.fetchAndAddAcquire(1);
            m_threadPool->start(worker);
        }
    }
}

void SearchManager::finishSearch()
{
    m_progressTimer->stop();

    if (!m_shouldStop.loadAcquire()) {
        emit searchCompleted(m_resultsFound.loadAcquire());
        qDebug() << "Search completed:" << m_resultsFound.loadAcquire() << "results";
    } else {
        emit searchCancelled();
        qDebug() << "Search cancelled";
    }
}

void SearchManager::onProgressTimer()
{
    emit searchProgress(m_filesProcessed.loadAcquire(), m_directoriesProcessed.loadAcquire());
}

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
    // Check if worker shouldstop
    if (m_manager->shouldStop()) {
        m_manager->workerFinished();
        return;
    }

    // Check if search dir is valid
    QDir dir(m_dirPath);
    if (!dir.exists() || !dir.isReadable()) {
        m_manager->workerFinished();
        return;
    }

    // Get all entries (files AND subdirectories)
    QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden, QDir::Name);


    QFileIconProvider iconProvider;
    int processedCount = 0;
    QList<SearchResult> resultBatch;
    resultBatch.reserve(BATCH_SIZE);

    // Iterate through all entries in the dir
    for (const QFileInfo &fileInfo : entries) {        
        if (m_manager->shouldStop()) {
            break;
        }

        // Search in file name (No different between files/dirs)
        if (m_options.mode == SearchMode::FileName) {
            QString fileName = fileInfo.fileName();
            if (fileName.contains(m_searchText, Qt::CaseInsensitive)) {
                SearchResult result = createSearchResult(fileInfo, 0, QString());
                qDebug() << "Found 1 result";
                resultBatch.append(result);
            }
        }

        // If entry is a directory, then add to queue
        if (fileInfo.isDir()) {
            m_manager->addDirectoryToQueue(fileInfo.absoluteFilePath());
        // Else entry is a file, then process
        }
        else {
            // Increment Processed file
            processedCount++;

            // If search mode is File Content, then search
            if (m_options.mode == SearchMode::FileContent) {
                // Search in file content
                searchInFile(fileInfo, resultBatch);
            }

            // Report progress every 25 files
            if (processedCount % 100 == 0) {
                m_manager->incrementCounters(25, 0);
            }
        }

        if (resultBatch.size() >= BATCH_SIZE) {
            m_manager->reportResults(resultBatch);
            resultBatch.clear();
        }
    }

    // Report remaining results
    if (resultBatch.length() % BATCH_SIZE != 0)
    {
        m_manager->reportResults(resultBatch);
    }

    // Report remaining progress
    if (processedCount % 100 != 0) {
        m_manager->incrementCounters(processedCount % 100, 1);
    }

    m_manager->workerFinished();
}

bool DirectorySearchWorker::searchInFile(const QFileInfo &fileInfo, QList<SearchResult> &results)
{
    if (fileInfo.size() > m_options.maxFileSizeBytes) {
        return false;
    }

    QFile file(fileInfo.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    const int BUFFER_SIZE = 16384;  // 16KB buffer
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    int lineNumber = 0;
    bool foundAny = false;
    int resultsInFile = 0;
    const int MAX_RESULTS_PER_FILE = 3;

    QString buffer;
    buffer.reserve(BUFFER_SIZE * 2);

    while (!stream.atEnd() && !m_manager->shouldStop() && resultsInFile < MAX_RESULTS_PER_FILE) {        
        QString chunk = stream.read(BUFFER_SIZE);
        buffer += chunk;

        QStringList lines = buffer.split('\n');
        buffer = lines.takeLast();

        for (const QString &line : lines) {
            lineNumber++;

            if (line.contains(m_searchText, Qt::CaseInsensitive)) {
                QString trimmedLine = line.trimmed();

                if (trimmedLine.length() > 150) {
                    int searchPos = trimmedLine.indexOf(m_searchText, 0, Qt::CaseInsensitive);
                    if (searchPos > 50) {
                        trimmedLine = "..." + trimmedLine.mid(searchPos - 30, 120) + "...";
                    }
                    else {
                        trimmedLine = trimmedLine.left(150) + "...";
                    }
                }

                SearchResult result = createSearchResult(fileInfo, lineNumber, trimmedLine);
                results.append(result);
                if (results.length() > BATCH_SIZE) {
                    m_manager->reportResults(results);
                    results.clear();
                }
                foundAny = true;
                resultsInFile++;
                if (resultsInFile >= MAX_RESULTS_PER_FILE) break;
            }
        }
    }

    file.close();
    return foundAny;
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

SearchResult DirectorySearchWorker::createSearchResult(const QFileInfo &fileInfo, int lineNumber, const QString &matchedLine)
{
    SearchResult result;
    result.fileName = fileInfo.fileName();
    result.fullPath = fileInfo.absoluteFilePath();
    result.fileSize = fileInfo.size();
    result.fileType = getFileType(fileInfo);
    result.lastModified = fileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss");
    result.isDirectory = fileInfo.isDir();
    result.lineNumber = lineNumber;
    result.matchedLine = matchedLine;

    QFileIconProvider iconProvider;
    result.icon = iconProvider.icon(fileInfo);

    return result;
}






















// Search Manager
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

    // Wait for thread pool
    if (m_threadPool) {
        m_threadPool->clear();
        m_threadPool->waitForDone(5000);
    }

    if (m_progressTimer) {
        m_progressTimer->stop();
    }
}

void SearchManager::startSearch(const QString &searchText, const QString &rootPath, const SearchOptions &options)
{
    QMutexLocker locker(&m_mutex);

    // Stop any existing search
    if (isSearching()) {
        stopSearch();
    }

    // Update info to new search
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

void SearchManager::reportResults(const QList<SearchResult> &results)
{
    QMutexLocker locker(&m_resultMutex);
    if (!m_shouldStop.loadAcquire()) {
        emit resultsFound(results);
        int resultsCount = results.length();
        m_resultsFound.fetchAndAddAcquire(resultsCount);
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
    // Decrement active workers
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
    // Start initial workers - they will create more work as they discover subdirectories
    DirectorySearchWorker *initialWorker = new DirectorySearchWorker(m_rootPath, m_searchText, m_options, this);
    m_activeWorkers.fetchAndAddAcquire(1);
    m_threadPool->start(initialWorker);
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

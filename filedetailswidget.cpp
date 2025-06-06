#include "filedetailswidget.h"
#include <QFileIconProvider>
#include <QDateTime>
#include <QDir>
#include <QApplication>
#include <QStyle>

FileDetailsWidget::FileDetailsWidget(QWidget *parent)
    : QWidget{parent}
{
    setupUi();
    setFixedWidth(300);
    setMinimumHeight(400);
}

void FileDetailsWidget::setupUi()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    headerLayout = new QHBoxLayout();
    titleLabel = new QLabel("Details");
    titleLabel->setStyleSheet("font-weight: bold; font-size: 14px;");

    closeButton = new QPushButton("×");
    closeButton->setFixedSize(24, 24);
    closeButton->setStyleSheet(
        "QPushButton {"
        "   border: none;"
        "   background-color: #f0f0f0;"
        "   border-radius: 12px;"
        "   font-size: 16px;"
        "   font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "   background-color: #e0e0e0;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #d0d0d0;"
        "}"
        );

    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(closeButton);

    mainLayout->addLayout(headerLayout);

    // Separator line
    separator = new QFrame();
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(separator);

    // Scrollable content area
    scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    contentWidget = new QWidget();
    contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setSpacing(8);
    contentLayout->setContentsMargins(5, 5, 5, 5);

    // File icon
    iconLabel = new QLabel();
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setFixedHeight(64);
    contentLayout->addWidget(iconLabel);

    // File details
    nameLabel = new QLabel();
    nameLabel->setWordWrap(true);
    nameLabel->setStyleSheet("font-weight: bold;");
    contentLayout->addWidget(nameLabel);

    pathLabel = new QLabel();
    pathLabel->setWordWrap(true);
    pathLabel->setStyleSheet("color: #666; font-size: 11px;");
    contentLayout->addWidget(pathLabel);

    // Add separator
    QFrame *detailsSeparator = new QFrame();
    detailsSeparator->setFrameShape(QFrame::HLine);
    detailsSeparator->setFrameShadow(QFrame::Sunken);
    contentLayout->addWidget(detailsSeparator);

    sizeLabel = new QLabel();
    typeLabel = new QLabel();
    createdLabel = new QLabel();
    modifiedLabel = new QLabel();
    accessedLabel = new QLabel();
    permissionsLabel = new QLabel();

    contentLayout->addWidget(sizeLabel);
    contentLayout->addWidget(typeLabel);
    contentLayout->addWidget(createdLabel);
    contentLayout->addWidget(modifiedLabel);
    contentLayout->addWidget(accessedLabel);
    contentLayout->addWidget(permissionsLabel);

    contentLayout->addStretch();

    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea);

    // Connect close button
    connect(closeButton, &QPushButton::clicked, this, &FileDetailsWidget::onCloseButtonClicked);

    // Initially hide the widget
    hide();
}

void FileDetailsWidget::setFileInfo(const QFileInfo &fileInfo)
{
    currentFileInfo = fileInfo;
    updateDetails();
    show();
}

void FileDetailsWidget::updateDetails()
{
    if (!currentFileInfo.exists())
    {
        clearDetails();
        return;
    }

    // File icon
    QFileIconProvider iconProvider;
    QIcon icon = iconProvider.icon(currentFileInfo);
    QPixmap pixmap = icon.pixmap(48, 48);
    iconLabel->setPixmap(pixmap);

    // File name
    nameLabel->setText(currentFileInfo.fileName());

    // File path
    pathLabel->setText(currentFileInfo.absolutePath());

    // File size
    if (currentFileInfo.isFile()) {
        sizeLabel->setText(QString("Size: %1").arg(formatFileSize(currentFileInfo.size())));
    } else {
        sizeLabel->setText("Size: -");
    }

    // File type
    typeLabel->setText(QString("Type: %1").arg(getFileTypeDescription(currentFileInfo)));

    // Timestamps
    createdLabel->setText(QString("Created: %1").arg(
        currentFileInfo.birthTime().toString("yyyy-MM-dd hh:mm:ss")));
    modifiedLabel->setText(QString("Modified: %1").arg(
        currentFileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss")));
    accessedLabel->setText(QString("Accessed: %1").arg(
        currentFileInfo.lastRead().toString("yyyy-MM-dd hh:mm:ss")));

    // Permissions
    QString permissions;
    QFile::Permissions perms = currentFileInfo.permissions();

    // Owner permissions
    permissions += (perms & QFile::ReadOwner) ? "r" : "-";
    permissions += (perms & QFile::WriteOwner) ? "w" : "-";
    permissions += (perms & QFile::ExeOwner) ? "x" : "-";

    // Group permissions
    permissions += (perms & QFile::ReadGroup) ? "r" : "-";
    permissions += (perms & QFile::WriteGroup) ? "w" : "-";
    permissions += (perms & QFile::ExeGroup) ? "x" : "-";

    // Other permissions
    permissions += (perms & QFile::ReadOther) ? "r" : "-";
    permissions += (perms & QFile::WriteOther) ? "w" : "-";
    permissions += (perms & QFile::ExeOther) ? "x" : "-";

    permissionsLabel->setText(QString("Permissions: %1").arg(permissions));
}

void FileDetailsWidget::clearDetails()
{
    iconLabel->clear();
    nameLabel->clear();
    pathLabel->clear();
    sizeLabel->clear();
    typeLabel->clear();
    createdLabel->clear();
    modifiedLabel->clear();
    accessedLabel->clear();
    permissionsLabel->clear();
    hide();
}

QString FileDetailsWidget::formatFileSize(qint64 size)
{
    const qint64 KB = 1024;
    const qint64 MB = KB * 1024;
    const qint64 GB = MB * 1024;
    const qint64 TB = GB * 1024;

    if (size >= TB) {
        return QString::number(size / TB, 'f', 2) + " TB";
    } else if (size >= GB) {
        return QString::number(size / GB, 'f', 2) + " GB";
    } else if (size >= MB) {
        return QString::number(size / MB, 'f', 2) + " MB";
    } else if (size >= KB) {
        return QString::number(size / KB, 'f', 2) + " KB";
    } else {
        return QString::number(size) + " bytes";
    }
}

QString FileDetailsWidget::getFileTypeDescription(const QFileInfo &info)
{
    if (info.isDir()) {
        return "Folder";
    } else if (info.isFile()) {
        QString suffix = info.suffix().toLower();
        if (suffix.isEmpty()) {
            return "File";
        } else {
            return QString("%1 file").arg(suffix.toUpper());
        }
    } else if (info.isSymLink()) {
        return "Symbolic Link";
    } else {
        return "Unknown";
    }
}

void FileDetailsWidget::onCloseButtonClicked()
{
    emit closeRequested();
    hide();
}


#include "filedetailswidget.h"
#include "./ui_filedetailswidget.h"
#include <QFileIconProvider>
#include <QDateTime>
#include <QDir>
#include <QApplication>
#include <QStyle>
#include <QPushButton>

FileDetailsWidget::FileDetailsWidget(QWidget *parent)
    : QWidget{parent}
    , ui(new Ui::FileDetailsWidget)
{
    ui->setupUi(this);

    connect(ui->closeButton, &QPushButton::clicked, this, &FileDetailsWidget::onCloseButtonClicked);
    hide();
}

FileDetailsWidget::~FileDetailsWidget()
{
    delete ui;
}


void FileDetailsWidget::setFileInfo(const QFileInfo &fileInfo)
{
    currentFileInfo = fileInfo;
    updateDetails();
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
    ui->iconLabel->setPixmap(pixmap);

    // File name
    ui->nameLabel->setText(currentFileInfo.fileName());

    // File path
    ui->pathLabel->setText(currentFileInfo.absolutePath());

    // File size
    if (currentFileInfo.isFile()) {
        ui->sizeLabel->setText(QString("Size: %1").arg(formatFileSize(currentFileInfo.size())));
    } else {
        ui->sizeLabel->setText("Size: -");
    }

    // File type
    QString typeText = QString("Type: %1").arg(getFileTypeDescription(currentFileInfo));
    ui->typeLabel->setText(typeText);

    // Timestamps
    ui->createdLabel->setText(QString("Created: %1").arg(
        currentFileInfo.birthTime().toString("yyyy-MM-dd hh:mm:ss")));
    ui->modifiedLabel->setText(QString("Modified: %1").arg(
        currentFileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss")));
    ui->accessedLabel->setText(QString("Accessed: %1").arg(
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

    ui->permissionsLabel->setText(QString("Permissions: %1").arg(permissions));
}

void FileDetailsWidget::clearDetails()
{
    ui->iconLabel->clear();
    ui->nameLabel->clear();
    ui->pathLabel->clear();
    ui->sizeLabel->clear();
    ui->typeLabel->clear();
    ui->createdLabel->clear();
    ui->modifiedLabel->clear();
    ui->accessedLabel->clear();
    ui->permissionsLabel->clear();
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


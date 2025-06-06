#ifndef FILEDETAILSWIDGET_H
#define FILEDETAILSWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileInfo>
#include <QPixmap>
#include <QIcon>
#include <QFrame>
#include <QScrollArea>

class FileDetailsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FileDetailsWidget(QWidget *parent = nullptr);
    void setFileInfo(const QFileInfo &fileInfo);
    void clearDetails();

private slots:
    void onCloseButtonClicked();

private:
    void setupUi();
    void updateDetails();
    QString formatFileSize(qint64 size);
    QString getFileTypeDescription(const QFileInfo &info);

    QVBoxLayout *mainLayout;
    QHBoxLayout *headerLayout;
    QLabel *titleLabel;
    QPushButton *closeButton;
    QFrame *separator;
    QScrollArea *scrollArea;
    QWidget *contentWidget;
    QVBoxLayout *contentLayout;

    // Detail labels
    QLabel *iconLabel;
    QLabel *nameLabel;
    QLabel *pathLabel;
    QLabel *sizeLabel;
    QLabel *typeLabel;
    QLabel *createdLabel;
    QLabel *modifiedLabel;
    QLabel *accessedLabel;
    QLabel *permissionsLabel;

    QFileInfo currentFileInfo;

signals:
    void closeRequested();
};

#endif // FILEDETAILSWIDGET_H

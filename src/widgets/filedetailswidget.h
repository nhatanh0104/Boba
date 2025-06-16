#ifndef FILEDETAILSWIDGET_H
#define FILEDETAILSWIDGET_H

#include <QWidget>
#include <QFileInfo>
#include <QPixmap>
#include <QIcon>

QT_BEGIN_NAMESPACE
namespace Ui {
class FileDetailsWidget;
}
QT_END_NAMESPACE

class FileDetailsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FileDetailsWidget(QWidget *parent = nullptr);
    ~FileDetailsWidget();
    void setFileInfo(const QFileInfo &fileInfo);
    void clearDetails();

private slots:
    void onCloseButtonClicked();

private:
    void updateDetails();
    QString formatFileSize(qint64 size);
    QString getFileTypeDescription(const QFileInfo &info);

    Ui::FileDetailsWidget *ui;
    QFileInfo currentFileInfo;

signals:
    void closeRequested();
};

#endif // FILEDETAILSWIDGET_H

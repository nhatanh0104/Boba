#ifndef DIRECTORYFILTERPROXYMODEL_H
#define DIRECTORYFILTERPROXYMODEL_H

#include <QSortFilterProxyModel>
#include <QFileSystemModel>

class DirectoryFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit DirectoryFilterProxyModel(QObject *parent = nullptr);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
};

#endif // DIRECTORYFILTERPROXYMODEL_H

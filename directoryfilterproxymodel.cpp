#include "directoryfilterproxymodel.h"

DirectoryFilterProxyModel::DirectoryFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{}

bool DirectoryFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QFileSystemModel *fileModel = qobject_cast<QFileSystemModel*>(sourceModel());
    if (!fileModel)
        return false;

    QModelIndex index = fileModel->index(sourceRow, 0, sourceParent);
    if (!index.isValid())
        return false;

    return fileModel->isDir(index);
}

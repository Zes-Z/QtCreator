#pragma once

#include "models/FileItem.h"

#include <QAbstractListModel>
#include <QVector>

namespace litapp {

// 将本地文件元数据暴露给 QML 的列表模型。
class FileListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        LocalPathRole,
        RemotePathRole,
        CategoryRole,
        CategoryTextRole,
        TagsRole,
        ZoneRole,
        SyncStateRole,
        FolderNameRole,
        AnnotationsPathRole
    };

    explicit FileListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setFiles(const QVector<FileItem> &files);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void reloadFromStore(const QString &databasePath, int zone);
    Q_INVOKABLE void reloadAllFromStore(const QString &databasePath);
    Q_INVOKABLE QVariantList toVariantList() const;

private:
    QVector<FileItem> m_files;
};

} // namespace litapp

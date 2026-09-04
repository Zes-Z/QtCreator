#pragma once

#include "models/FileItem.h"

#include <QString>
#include <QVector>

class QSqlDatabase;

namespace litapp {

class MetadataStore
{
public:
    MetadataStore();
    ~MetadataStore();

    bool open(const QString &databasePath);
    void close();

    bool addFile(const FileItem &item);
    bool updateFile(const FileItem &item);
    bool updateLocalPath(
        const QString &fileId,
        const QString &localPath);

    bool removeFile(
        const QString &fileId);

    bool moveToTrash(
        const QString &fileId,
        const QString &trashLocalPath,
        const QString &originalLocalPath);

    bool restoreFromTrash(
        const QString &fileId,
        const QString &localPath);

    bool permanentlyDelete(
        const QString &fileId);

    QVector<FileItem> files(
        FileSpace space) const;

    QVector<FileItem> childrenOf(
        const QString &parentId,
        FileSpace space) const;

    QVector<FileItem> rootFiles(
        FileSpace space) const;

    QVector<FileItem> trashFiles(
        FileSpace space) const;

    bool isTrashed(
        const QString &fileId) const;

    QString originalLocalPath(
        const QString &fileId) const;

    FileItem fileById(
        const QString &fileId) const;

    QVector<FileItem> filesByCategory(
        FileCategory category,
        FileSpace space) const;

    QVector<FileItem> filesByTag(
        const QString &tag,
        FileSpace space) const;

private:
    bool createTables();

    QSqlDatabase *m_db = nullptr;
};

} // namespace litapp
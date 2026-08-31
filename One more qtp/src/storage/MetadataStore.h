#pragma once

#include "models/FileItem.h"

#include <QString>
#include <QVector>

class QSqlDatabase;

namespace litapp {

// 本地元数据存储：使用 SQLite 缓存文件分类、标签、同步状态等。
class MetadataStore
{
public:
    MetadataStore();
    ~MetadataStore();

    bool open(const QString &databasePath);
    void close();

    bool addFile(const FileItem &item);
    bool updateFile(const FileItem &item);
    bool updateLocalPath(const QString &fileId, const QString &localPath);
    bool removeFile(const QString &fileId);
    QVector<FileItem> files(FileZone zone) const;
    FileItem fileById(const QString &fileId) const;

    // 分类与标签筛选
    QVector<FileItem> filesByCategory(FileCategory category, FileZone zone) const;
    QVector<FileItem> filesByTag(const QString &tag, FileZone zone) const;

private:
    bool createTables();

    QSqlDatabase *m_db = nullptr;
};

} // namespace litapp

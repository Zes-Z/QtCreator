#pragma once

#include "models/FileItem.h"

#include <QObject>
#include <QString>

namespace litapp {

// 本地文件导入与元数据管理服务，供 QML 调用。
class FileService : public QObject
{
    Q_OBJECT
public:
    explicit FileService(QObject *parent = nullptr);

    Q_INVOKABLE bool addLocalFile(const QString &filePath,
                                  int zone,
                                  int category,
                                  const QString &tagsCsv);
    Q_INVOKABLE bool removeFile(const QString &fileId);
    Q_INVOKABLE bool markDownloaded(const QString &fileId, const QString &localPath);
    Q_INVOKABLE bool renameItem(const QString &fileId, const QString &newBaseName);
    Q_INVOKABLE bool moveToTrash(const QString &fileId);
    Q_INVOKABLE bool copyItem(const QString &fileId);
    Q_INVOKABLE bool moveItem(const QString &fileId, int destZone);

private:
    bool copyDirectory(const QString &src, const QString &dst);
    QString m_databasePath = QStringLiteral("litapp_metadata.db");
};

} // namespace litapp

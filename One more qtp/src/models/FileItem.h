#pragma once

#include "FileCategory.h"

#include <QDateTime>
#include <QString>
#include <QStringList>

namespace litapp {

enum class FileZone {
    MyFiles,    // 我的文件
    GroupFiles  // 群组文件
};

enum class SyncState {
    LocalOnly,     // 仅本地
    Synced,        // 已同步
    Uploading,     // 上传中
    Downloading,   // 下载中
    Conflict,      // 冲突
    Error          // 同步错误
};

struct FileItem {
    QString id;
    QString name;
    QString localPath;
    QString remotePath;
    QString extension;          // pdf / md / ...
    QString folderName;         // 同名文件夹对象名（不含扩展名）
    QString annotationsPath;    // 批注/标注文件路径
    qint64 size = 0;
    QDateTime modifiedTime;
    FileZone zone = FileZone::MyFiles;
    FileCategory category = FileCategory::Other;
    QStringList tags;           // 自定义标签，可多个
    SyncState syncState = SyncState::LocalOnly;
    QString syncError;

    bool isValid() const { return !name.isEmpty(); }
};

} // namespace litapp

Q_DECLARE_METATYPE(litapp::FileItem)
Q_DECLARE_METATYPE(litapp::FileZone)
Q_DECLARE_METATYPE(litapp::SyncState)

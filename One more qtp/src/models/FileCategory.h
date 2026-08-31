#pragma once

#include <QString>
#include <QMetaType>

namespace litapp {

// 文件原分类：必选且只能单选
enum class FileCategory {
    Book,           // 书籍
    JournalPaper,   // 期刊论文
    AtomicKnowledge,// Markdown 原子知识
    Other           // 其他
};

inline QString fileCategoryToString(FileCategory category)
{
    switch (category) {
    case FileCategory::Book:
        return QStringLiteral("书籍");
    case FileCategory::JournalPaper:
        return QStringLiteral("期刊论文");
    case FileCategory::AtomicKnowledge:
        return QStringLiteral("Markdown 原子知识");
    case FileCategory::Other:
        return QStringLiteral("未分类");
    }
    return QStringLiteral("未分类");
}

} // namespace litapp

Q_DECLARE_METATYPE(litapp::FileCategory)

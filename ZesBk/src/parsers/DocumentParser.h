#pragma once

#include <QString>

namespace litapp {

// 文档解析器：根据扩展名选择 PDF / Markdown / 未来扩展格式的解析实现。
class DocumentParser
{
public:
    enum class Format {
        Unknown,
        Pdf,
        Markdown
    };

    static Format detectFormat(const QString &filePath);
    static QString extractText(const QString &filePath);
    static QString markdownToHtml(const QString &markdown);
};

} // namespace litapp

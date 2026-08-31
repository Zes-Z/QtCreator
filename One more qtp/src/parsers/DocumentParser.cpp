#include "parsers/DocumentParser.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace litapp {

DocumentParser::Format DocumentParser::detectFormat(const QString &filePath)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    if (suffix == QStringLiteral("pdf")) {
        return Format::Pdf;
    }
    if (suffix == QStringLiteral("md") || suffix == QStringLiteral("markdown")) {
        return Format::Markdown;
    }
    return Format::Unknown;
}

QString DocumentParser::extractText(const QString &filePath)
{
    const Format format = detectFormat(filePath);
    if (format == Format::Markdown) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return {};
        }
        QTextStream stream(&file);
        return stream.readAll();
    }

    // TODO: PDF 文本提取（可使用 QPdfDocument 或 Poppler）。
    if (format == Format::Pdf) {
        return QStringLiteral("[PDF 文本提取待实现]");
    }

    return {};
}

QString DocumentParser::markdownToHtml(const QString &markdown)
{
    // TODO: 使用成熟的 Markdown 解析器，例如 md4c / markdown-it 的 C++ 移植。
    // 当前仅做极简转义换行，便于 UI 占位显示。
    QString html = markdown.toHtmlEscaped();
    html.replace(QStringLiteral("\n"), QStringLiteral("<br/>"));
    return html;
}

} // namespace litapp

#include "recognition/request-builder.h"

#include "recognition/prompt-templates.h"

#include <QJsonArray>

namespace whilefresh::recognition {

namespace {

QString vocabularyBlock(const Vocabulary &vocabulary)
{
    QStringList lines;
    for (const auto &[category, names] : vocabulary.namesByCategory()) {
        const QString label = category.isEmpty() ? QStringLiteral("其他") : category;
        lines.append(QStringLiteral("【%1】%2").arg(label, names.join(QStringLiteral("、"))));
    }
    return lines.join(QLatin1Char('\n'));
}

QString inputParagraph(InputKind kind)
{
    switch (kind) {
    case InputKind::Screenshot:
        return QString::fromUtf8(prompt::kInputScreenshot);
    case InputKind::Receipt:
        return QString::fromUtf8(prompt::kInputReceipt);
    case InputKind::OrderText:
        return QString::fromUtf8(prompt::kInputOrderText);
    }
    return {};
}

QString imageLabel(const PreparedImage &image)
{
    QString label = QStringLiteral("第 %1 张图").arg(image.imageIndex + 1);
    if (image.sliceCount > 1)
        label += QStringLiteral("，切片 %1/%2").arg(image.sliceIndex + 1).arg(image.sliceCount);
    if (image.captureDate.isValid())
        label += QStringLiteral("，拍摄日期 %1").arg(image.captureDate.toString(Qt::ISODate));
    return QStringLiteral("[%1]").arg(label);
}

QJsonObject textBlock(const QString &text)
{
    return { { QStringLiteral("type"), QStringLiteral("text") }, { QStringLiteral("text"), text } };
}

QJsonObject imageBlock(const QByteArray &jpeg)
{
    const QString url = QStringLiteral("data:image/jpeg;base64,") + QString::fromLatin1(jpeg.toBase64());
    return { { QStringLiteral("type"), QStringLiteral("image_url") },
             { QStringLiteral("image_url"), QJsonObject{ { QStringLiteral("url"), url } } } };
}

}

QString promptVersion()
{
    return QString::fromLatin1(prompt::kPromptVersion);
}

QString buildPrompt(InputKind kind, const Vocabulary &vocabulary)
{
    QString text = QString::fromUtf8(prompt::kPromptTemplate);
    text.replace(QStringLiteral("{{INPUT}}"), inputParagraph(kind));
    text.replace(QStringLiteral("{{VOCABULARY}}"), vocabularyBlock(vocabulary));
    return text;
}

QJsonObject buildChatRequest(InputKind kind, const Vocabulary &vocabulary,
                             const QList<PreparedImage> &images, const QString &text,
                             const RequestOptions &options)
{
    QJsonArray content;
    content.append(textBlock(buildPrompt(kind, vocabulary)));

    if (kind == InputKind::OrderText) {
        content.append(textBlock(QStringLiteral("<订单文字>\n%1\n</订单文字>").arg(text)));
    } else {
        for (const PreparedImage &image : images) {
            content.append(textBlock(imageLabel(image)));
            content.append(imageBlock(image.jpeg));
        }
    }

    const QJsonObject message{ { QStringLiteral("role"), QStringLiteral("user") },
                               { QStringLiteral("content"), content } };
    QJsonObject body{ { QStringLiteral("model"), QString::fromLatin1(kDeepSeekModel) },
                      { QStringLiteral("messages"), QJsonArray{ message } },
                      { QStringLiteral("response_format"),
                        QJsonObject{ { QStringLiteral("type"), QStringLiteral("json_object") } } },
                      { QStringLiteral("max_tokens"), options.maxTokens },
                      { QStringLiteral("temperature"), 0 } };
    if (!options.thinking)
        body.insert(QStringLiteral("thinking"), QJsonObject{ { QStringLiteral("type"), QStringLiteral("disabled") } });
    return body;
}

}

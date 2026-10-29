#include "recognition/request-builder.h"

#include "recognition/prompt-templates.h"

namespace whilefresh::recognition {

namespace {

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

}

QString promptVersion()
{
    return QString::fromLatin1(prompt::kPromptVersion);
}

QString buildPrompt(InputKind kind, const core::IngredientCatalog &catalog)
{
    QString text = QString::fromUtf8(prompt::kPromptTemplate);
    text.replace(QStringLiteral("{{INPUT}}"), inputParagraph(kind));
    text.replace(QStringLiteral("{{VOCABULARY}}"), catalog.hierarchyText());
    return text;
}

ai::ChatRequest buildChatRequest(InputKind kind, const core::IngredientCatalog &catalog,
                                 const QList<PreparedImage> &images, const QString &text,
                                 const RequestOptions &options)
{
    ai::ChatMessage message;
    message.parts.append(ai::ChatPart::text(buildPrompt(kind, catalog)));

    if (kind == InputKind::OrderText) {
        message.parts.append(ai::ChatPart::text(QStringLiteral("<订单文字>\n%1\n</订单文字>").arg(text)));
    } else {
        for (const PreparedImage &image : images) {
            message.parts.append(ai::ChatPart::text(imageLabel(image)));
            message.parts.append(ai::ChatPart::jpegImage(image.jpeg));
        }
    }

    ai::ChatRequest request;
    request.messages = { message };
    request.jsonMode = true;
    request.maxTokens = options.maxTokens;
    request.temperature = 0;
    request.thinking = options.thinking;
    return request;
}

}

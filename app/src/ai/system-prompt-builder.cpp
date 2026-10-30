#include "ai/system-prompt-builder.h"

#include <QStringList>

namespace whilefresh::ai {

namespace {

QString section(const QString &title, const QString &body)
{
    return QStringLiteral("【%1】\n%2").arg(title, body);
}

}

SystemPromptBuilder &SystemPromptBuilder::addSection(const QString &title, const QString &body)
{
    if (!body.trimmed().isEmpty())
        m_sections.append({ title, body.trimmed() });
    return *this;
}

SystemPromptBuilder &SystemPromptBuilder::setPreferenceSummary(const QString &summary)
{
    m_preferenceSummary = summary.trimmed();
    return *this;
}

QString SystemPromptBuilder::build() const
{
    QStringList parts;
    if (!m_base.trimmed().isEmpty())
        parts.append(m_base.trimmed());
    for (const auto &[title, body] : m_sections)
        parts.append(section(title, body));
    if (!m_preferenceSummary.isEmpty())
        parts.append(section(QStringLiteral("用户偏好"), m_preferenceSummary));
    return parts.join(QStringLiteral("\n\n"));
}

}

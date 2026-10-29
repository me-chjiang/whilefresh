#pragma once

#include <QList>
#include <QString>

#include <utility>

namespace whilefresh::ai {

// The system prompt of an AI service: a base text, then optional sections in the order they
// were added, then the user's preference summary, which always comes last (PRD v6: appended to
// the end of the system prompt of cooking-related requests). Empty sections are left out.
class SystemPromptBuilder
{
public:
    explicit SystemPromptBuilder(QString base) : m_base(std::move(base)) {}

    SystemPromptBuilder &addSection(const QString &title, const QString &body);
    SystemPromptBuilder &setPreferenceSummary(const QString &summary);

    QString build() const;

private:
    QString m_base;
    QList<std::pair<QString, QString>> m_sections;
    QString m_preferenceSummary;
};

}

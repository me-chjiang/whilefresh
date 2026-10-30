#include "ui/settings-model.h"

namespace whilefresh::ui {

namespace {

QString failureText(ai::ErrorKind kind)
{
    switch (kind) {
    case ai::ErrorKind::NotConfigured:
        return QStringLiteral("未配置密钥");
    case ai::ErrorKind::NoNetwork:
        return QStringLiteral("网络不可用");
    case ai::ErrorKind::Timeout:
        return QStringLiteral("请求超时");
    case ai::ErrorKind::AuthFailed:
        return QStringLiteral("密钥无效");
    case ai::ErrorKind::QuotaOrRateLimit:
        return QStringLiteral("额度不足或请求过多");
    default:
        return QStringLiteral("整理失败");
    }
}

}

SettingsModel::SettingsModel(ai::ChatClient *chat, std::shared_ptr<UserCredentialProvider> credentials,
                             QObject *parent)
    : QObject(parent), m_chat(chat), m_credentials(std::move(credentials)), m_summarizer(chat)
{
    m_available = m_chat->isConfigured();
    connect(&m_summarizer, &ai::PreferenceSummarizer::summarized, this, &SettingsModel::summarized);
    connect(&m_summarizer, &ai::PreferenceSummarizer::failed, this, &SettingsModel::summaryFailed);
}

void SettingsModel::cancelSummary()
{
    if (m_request == 0)
        return;
    m_summarizer.cancel(m_request);
    m_request = 0;
    m_summaryError.clear();
    emit summarizingChanged();
}

void SettingsModel::attach(data::Store *store, core::NameMappingStore *names)
{
    cancelSummary();
    m_store = store;
    m_names = names;
    emit preferencesChanged();
    emit summaryChanged();
    emit nameMappingsChanged();
}

std::optional<ai::Service> SettingsModel::serviceFor(const QString &field)
{
    if (field == QLatin1String("deepseek"))
        return ai::Service::DeepSeek;
    if (field == QLatin1String("lexmount"))
        return ai::Service::Lexmount;
    if (field == QLatin1String("lexmountProject"))
        return ai::Service::LexmountProjectId;
    return std::nullopt;
}

bool SettingsModel::saveSecret(const QString &field, const QString &value)
{
    const auto service = serviceFor(field);
    const QString trimmed = value.trimmed();
    // The value ends up in an HTTP header or a request body: no whitespace or control characters.
    bool clean = !trimmed.isEmpty();
    for (const QChar c : trimmed)
        clean = clean && !c.isSpace() && !c.isNonCharacter() && c.category() != QChar::Other_Control;
    if (!service || !clean || !m_credentials->save(*service, trimmed))
        return false;
    emit servicesChanged();
    refreshAvailability();
    return true;
}

bool SettingsModel::clearSecret(const QString &field)
{
    const auto service = serviceFor(field);
    if (!service || !m_credentials->clear(*service))
        return false;
    emit servicesChanged();
    refreshAvailability();
    return true;
}

void SettingsModel::refresh()
{
    emit preferencesChanged();
    emit summaryChanged();
    emit nameMappingsChanged();
}

bool SettingsModel::summarizeAvailable() const { return m_chat->isConfigured(); }

void SettingsModel::refreshAvailability()
{
    const bool now = m_chat->isConfigured();
    if (now == m_available)
        return;
    m_available = now;
    emit summarizeAvailableChanged();
}

QVariantList SettingsModel::preferences() const
{
    QVariantList list;
    if (!m_store)
        return list;
    for (const core::Preference &preference : m_store->preferences())
        list.append(QVariantMap{ { QStringLiteral("id"), preference.id },
                                 { QStringLiteral("text"), preference.text },
                                 { QStringLiteral("source"), preference.source } });
    return list;
}

QStringList SettingsModel::preferenceTexts() const
{
    QStringList texts;
    if (m_store) {
        for (const core::Preference &preference : m_store->preferences())
            texts.append(preference.text);
    }
    return texts;
}

QString SettingsModel::preferenceSummary() const
{
    return m_store ? m_store->setting(QString::fromLatin1(kPreferenceSummarySetting)) : QString();
}

bool SettingsModel::summaryStale() const
{
    if (!m_store || preferenceSummary().isEmpty())
        return false;
    return m_store->setting(QString::fromLatin1(kPreferenceSummarySourceSetting))
        != preferenceTexts().join(QLatin1Char('\n'));
}

bool SettingsModel::addPreference(const QString &text)
{
    cancelSummary();
    if (!m_store || m_store->addPreference(text) == 0)
        return false;
    emit preferencesChanged();
    emit summaryChanged();
    return true;
}

bool SettingsModel::editPreference(qint64 id, const QString &text)
{
    cancelSummary();
    if (!m_store || !m_store->updatePreference(id, text))
        return false;
    emit preferencesChanged();
    emit summaryChanged();
    return true;
}

bool SettingsModel::deletePreference(qint64 id)
{
    cancelSummary();
    if (!m_store || !m_store->deletePreference(id))
        return false;
    // A summary of nothing would keep steering requests after the user removed every preference.
    if (m_store->preferences().isEmpty()) {
        m_store->setSetting(QString::fromLatin1(kPreferenceSummarySetting), QString());
        m_store->setSetting(QString::fromLatin1(kPreferenceSummarySourceSetting), QString());
    }
    emit preferencesChanged();
    emit summaryChanged();
    return true;
}

void SettingsModel::summarize()
{
    if (!m_store || m_request != 0)
        return;
    const QStringList texts = preferenceTexts();
    if (texts.isEmpty())
        return;
    if (!m_chat->isConfigured()) {
        m_summaryError = failureText(ai::ErrorKind::NotConfigured);
        emit summarizingChanged();
        return;
    }
    m_summaryError.clear();
    m_summarySource = texts.join(QLatin1Char('\n'));
    m_request = m_summarizer.summarize(texts);
    emit summarizingChanged();
}

void SettingsModel::summarized(quint64 id, const ai::PreferenceSummary &summary)
{
    if (id != m_request || !m_store)
        return;
    // The source is what was actually sent, not what the list holds now.
    const bool saved = m_store->setSetting(QString::fromLatin1(kPreferenceSummarySourceSetting), m_summarySource)
        && m_store->setSetting(QString::fromLatin1(kPreferenceSummarySetting), summary.text);
    m_request = 0;
    m_summaryError = saved ? QString() : QStringLiteral("保存失败");
    emit summarizingChanged();
    emit summaryChanged();
}

void SettingsModel::summaryFailed(quint64 id, const ai::ChatError &error)
{
    if (id != m_request)
        return;
    m_request = 0;
    m_summaryError = failureText(error.kind);
    emit summarizingChanged();
}

QString SettingsModel::cookingSystemPrompt(ai::SystemPromptBuilder builder) const
{
    return builder.setPreferenceSummary(preferenceSummary()).build();
}

QVariantList SettingsModel::nameMappings() const
{
    QVariantList list;
    if (!m_names)
        return list;
    for (const auto &[raw, canonical] : m_names->entries())
        list.append(QVariantMap{ { QStringLiteral("raw"), raw }, { QStringLiteral("canonical"), canonical } });
    return list;
}

void SettingsModel::forgetNameMapping(const QString &rawName)
{
    if (m_names && m_names->forget(rawName))
        emit nameMappingsChanged();
}

void SettingsModel::clearNameMappings()
{
    if (m_names && m_names->clear())
        emit nameMappingsChanged();
}

}

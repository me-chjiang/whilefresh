#pragma once

#include "ai/preference-summarizer.h"
#include "ai/system-prompt-builder.h"
#include "core/name-mapping.h"
#include "data/store.h"
#include "ui/user-keys.h"

#include <QObject>
#include <QVariantList>

#include <memory>

namespace whilefresh::ui {

inline constexpr char kPreferenceSummarySetting[] = "preference_summary";
// The preference texts the stored summary was made from; differs from the current list when
// preferences changed since.
inline constexpr char kPreferenceSummarySourceSetting[] = "preference_summary_source";

// The 我的 sub-pages 服务 and 偏好记忆 (R-ADR-11): the user's keys, preference memory with its
// summary, and the remembered name mappings. Exposed to QML as App.settings. Secrets are only
// ever written, cleared and reported as configured / not; they are never read back out.
// Preferences and mappings belong to the active session (real or demo), set by attach().
class SettingsModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool deepSeekConfigured READ deepSeekConfigured NOTIFY servicesChanged)
    Q_PROPERTY(bool lexmountConfigured READ lexmountConfigured NOTIFY servicesChanged)
    Q_PROPERTY(bool lexmountProjectConfigured READ lexmountProjectConfigured NOTIFY servicesChanged)
    Q_PROPERTY(QVariantList preferences READ preferences NOTIFY preferencesChanged)
    Q_PROPERTY(QString preferenceSummary READ preferenceSummary NOTIFY summaryChanged)
    Q_PROPERTY(bool summaryStale READ summaryStale NOTIFY summaryChanged)
    Q_PROPERTY(bool summarizing READ summarizing NOTIFY summarizingChanged)
    Q_PROPERTY(QString summaryError READ summaryError NOTIFY summarizingChanged)
    Q_PROPERTY(bool summarizeAvailable READ summarizeAvailable NOTIFY summarizeAvailableChanged)
    Q_PROPERTY(QVariantList nameMappings READ nameMappings NOTIFY nameMappingsChanged)

public:
    // `chat` must outlive this object.
    SettingsModel(ai::ChatClient *chat, std::shared_ptr<UserCredentialProvider> credentials, QObject *parent = nullptr);

    // Points the model at the active session's store and name mappings (either may be null).
    void attach(data::Store *store, core::NameMappingStore *names);
    const std::shared_ptr<UserCredentialProvider> &userCredentials() const { return m_credentials; }

    bool deepSeekConfigured() const { return m_credentials->isConfigured(ai::Service::DeepSeek); }
    bool lexmountConfigured() const { return m_credentials->isConfigured(ai::Service::Lexmount); }
    bool lexmountProjectConfigured() const { return m_credentials->isConfigured(ai::Service::LexmountProjectId); }
    QVariantList preferences() const;
    QString preferenceSummary() const;
    bool summaryStale() const;
    bool summarizing() const { return m_request != 0; }
    QString summaryError() const { return m_summaryError; }
    // False when the active chat client has no key; the page shows 未配置 instead of calling.
    bool summarizeAvailable() const;
    QVariantList nameMappings() const;

    // `field` is "deepseek", "lexmount" or "lexmountProject". False means 保存失败 / 清除失败.
    Q_INVOKABLE bool saveSecret(const QString &field, const QString &value);
    Q_INVOKABLE bool clearSecret(const QString &field);

    Q_INVOKABLE bool addPreference(const QString &text);
    Q_INVOKABLE bool editPreference(qint64 id, const QString &text);
    Q_INVOKABLE bool deletePreference(qint64 id);
    // The only place that calls the model; nothing summarizes automatically.
    Q_INVOKABLE void summarize();
    Q_INVOKABLE void forgetNameMapping(const QString &rawName);
    Q_INVOKABLE void clearNameMappings();

    // The system prompt of a cooking-related request: `builder` as given, with the stored
    // preference summary appended last (PRD v6).
    QString cookingSystemPrompt(ai::SystemPromptBuilder builder) const;

    // Re-reads preferences and mappings; the import flow adds mappings behind the page's back.
    Q_INVOKABLE void refresh();
    // Call after the ChatClient's key source changed.
    void refreshAvailability();

signals:
    void servicesChanged();
    void preferencesChanged();
    void summaryChanged();
    void summarizingChanged();
    void summarizeAvailableChanged();
    void nameMappingsChanged();

private:
    static std::optional<ai::Service> serviceFor(const QString &field);
    QStringList preferenceTexts() const;
    void summarized(quint64 id, const ai::PreferenceSummary &summary);
    void summaryFailed(quint64 id, const ai::ChatError &error);

    ai::ChatClient *m_chat;
    std::shared_ptr<UserCredentialProvider> m_credentials;
    ai::PreferenceSummarizer m_summarizer;
    data::Store *m_store = nullptr;
    core::NameMappingStore *m_names = nullptr;
    quint64 m_request = 0;
    QString m_summaryError;
    bool m_available = false;
};

}

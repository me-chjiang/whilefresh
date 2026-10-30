#pragma once

#include "ai/api-keys.h"
#include "ai/deepseek-chat-client.h"
#include "core/clock.h"
#include "data/builtin-data.h"
#include "data/database.h"
#include "data/store.h"
#include "platform/platform-services.h"
#include "recognition/recognition-service.h"
#include "ui/import-service.h"
#include "ui/kitchen-service.h"
#include "ui/settings-model.h"

#include <QObject>
#include <QSet>
#include <QVariantList>
#include <QVariantMap>

#include <memory>

namespace whilefresh::ui {

// Thin view model for QML: services in, plain maps and lists out. Holds one real and,
// while demo mode is on, one separate demo session (own database file and recognizer).
class AppModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList items READ items NOTIFY changed)
    Q_PROPERTY(int nearCount READ nearCount NOTIFY changed)
    Q_PROPERTY(int attentionCount READ attentionCount NOTIFY changed)
    Q_PROPERTY(QVariantList selectedIds READ selectedIds NOTIFY changed)
    Q_PROPERTY(QVariantList recipes READ recipes NOTIFY changed)
    Q_PROPERTY(QVariantList clearRecipes READ clearRecipes NOTIFY changed)
    Q_PROPERTY(QVariantList shopping READ shopping NOTIFY changed)
    Q_PROPERTY(QVariantList pantry READ pantry NOTIFY changed)
    Q_PROPERTY(QVariantList suggestions READ suggestions NOTIFY changed)
    Q_PROPERTY(QVariantList rules READ rules NOTIFY changed)
    Q_PROPERTY(QVariantList imports READ imports NOTIFY changed)
    Q_PROPERTY(bool demoMode READ demoMode NOTIFY demoModeChanged)
    Q_PROPERTY(bool recognitionConfigured READ recognitionConfigured NOTIFY recognitionConfiguredChanged)
    Q_PROPERTY(QObject *settings READ settings CONSTANT)
    Q_PROPERTY(QString startupNotice READ startupNotice CONSTANT)
    Q_PROPERTY(QString privacyNotice READ privacyNotice CONSTANT)
    Q_PROPERTY(QString importState READ importState NOTIFY importChanged)
    Q_PROPERTY(QString importError READ importError NOTIFY importChanged)
    Q_PROPERTY(QVariantList draftItems READ draftItems NOTIFY importChanged)
    Q_PROPERTY(QString draftWarning READ draftWarning NOTIFY importChanged)
    Q_PROPERTY(bool draftDuplicate READ draftDuplicate NOTIFY importChanged)
    Q_PROPERTY(int draftFoodCount READ draftFoodCount NOTIFY importChanged)
    Q_PROPERTY(bool hasUnfinishedImport READ hasUnfinishedImport NOTIFY importChanged)

public:
    // `secrets` defaults to the platform's store (Android Keystore / desktop dev file).
    AppModel(platform::PlatformServices *platform, std::shared_ptr<const data::BuiltinData> builtin,
             QObject *parent = nullptr, std::unique_ptr<platform::KeyVault> secrets = nullptr);
    ~AppModel() override;

    bool ok() const { return m_real != nullptr; }

    QVariantList items();
    int nearCount();
    int attentionCount() const;
    QVariantList selectedIds() const;
    QVariantList recipes();
    QVariantList clearRecipes();
    QVariantList shopping();
    QVariantList pantry() const;
    QVariantList suggestions() const;
    QVariantList rules() const;
    QVariantList imports() const;
    bool demoMode() const { return m_active && m_active->demo; }
    bool recognitionConfigured() const;
    // Where the keys come from (built-in or the user's); takes effect for the next request.
    // Demo mode and leaving it switch this automatically (built-in key / the user's keys).
    void setCredentialProvider(std::shared_ptr<ai::CredentialProvider> credentials);
    std::shared_ptr<const ai::CredentialProvider> credentials() const { return m_credentials; }
    SettingsModel *settings() const { return m_settings.get(); }
    QString startupNotice() const { return m_startupNotice; }
    static QString privacyNotice() { return ImportService::privacyNotice(); }
    QString importState() const { return m_importState; }
    QString importError() const { return m_importError; }
    QVariantList draftItems() const;
    QString draftWarning() const;
    bool draftDuplicate() const;
    int draftFoodCount() const;
    bool hasUnfinishedImport() const;

    Q_INVOKABLE void toggleSelected(qint64 id);
    Q_INVOKABLE QVariantMap recipeDetail(const QString &id) const;
    Q_INVOKABLE int addMissing(const QString &recipeId);
    Q_INVOKABLE void removeShopping(qint64 id);
    Q_INVOKABLE QString shoppingText() const;

    Q_INVOKABLE void useItem(qint64 id);
    Q_INVOKABLE void markAbsent(qint64 id);
    Q_INVOKABLE void discardItem(qint64 id);
    Q_INVOKABLE void snoozeItem(qint64 id);
    Q_INVOKABLE void toggleFirst(qint64 id, bool enabled);
    Q_INVOKABLE void removeItems(const QVariantList &ids);
    // Returns { ok, error, question } ; question is non-empty when a rule can be remembered.
    Q_INVOKABLE QVariantMap saveItem(qint64 id, const QVariantMap &edits);
    Q_INVOKABLE QVariantMap addManual(const QString &name, const QString &quantity, const QString &location);
    Q_INVOKABLE void confirmRememberedRules();
    Q_INVOKABLE void dismissRememberedRules();

    Q_INVOKABLE void addPantry(const QString &name);
    Q_INVOKABLE void removePantry(const QString &name);
    Q_INVOKABLE void decideSuggestion(qint64 id, bool accept);
    Q_INVOKABLE void saveRule(const QString &name, const QString &oldLocation, const QString &location,
                              const QString &days);
    Q_INVOKABLE void deleteRule(const QString &name, const QString &location, const QString &kind);
    Q_INVOKABLE void deleteImport(qint64 id);
    Q_INVOKABLE void clearImportHistory();
    Q_INVOKABLE void deleteAllData();
    Q_INVOKABLE void setDemoMode(bool enabled);

    // Gallery and camera: the user has agreed to the privacy notice before the picker opens.
    Q_INVOKABLE void beginPick(bool camera);
    Q_INVOKABLE void beginText(const QString &text);
    Q_INVOKABLE void startImport(bool allowDuplicate);
    Q_INVOKABLE void cancelImport();
    Q_INVOKABLE void skipImport();
    // Hook for the 计划 page's resume bar (T19).
    Q_INVOKABLE void resumeImport();
    Q_INVOKABLE void discardUnfinishedImport();
    Q_INVOKABLE void editDraft(int index, const QVariantMap &edits);
    Q_INVOKABLE void dropDraft(int index);
    Q_INVOKABLE void retrieveDraft(int index);
    Q_INVOKABLE void confirmImport();

signals:
    void changed();
    void demoModeChanged();
    void recognitionConfiguredChanged();
    void importChanged();
    void notice(const QString &message);
    void pickCancelled();
    void importCommitted();

private:
    struct Session {
        bool demo = false;
        std::unique_ptr<data::Database> database;
        std::unique_ptr<data::Store> store;
        std::unique_ptr<KitchenService> kitchen;
        std::unique_ptr<recognition::RecognitionService> recognizer;
        std::unique_ptr<core::NameMappingStore> names;
        std::unique_ptr<ai::IngredientClassifier> classifier;
        std::unique_ptr<ImportService> import;
    };

    std::unique_ptr<Session> openSession(const QString &dbPath, bool demo);
    void seedDemo(Session &session);
    void activate(Session *session);
    void applyModeCredentials();
    void invalidate();
    void setImportState(const QString &state, const QString &error = {});
    QVariantMap itemMap(const core::FridgeItem &item) const;
    QVariantMap summaryMap(const core::RecipeMatch &match) const;
    void applyEdits(core::FridgeItem &item, const QVariantMap &edits, bool editEntry) const;
    QString demoPath() const;
    KitchenService &kitchen() const { return *m_active->kitchen; }

    platform::PlatformServices *m_platform;
    std::shared_ptr<const data::BuiltinData> m_builtin;
    core::SystemClock m_clock;
    // Declared before the sessions so their recognizers never outlive the client.
    std::shared_ptr<ai::CredentialProvider> m_credentials = std::make_shared<ai::BuiltinCredentialProvider>();
    std::unique_ptr<ai::DeepSeekChatClient> m_chat = std::make_unique<ai::DeepSeekChatClient>(m_credentials);
    std::unique_ptr<SettingsModel> m_settings;
    std::unique_ptr<Session> m_real;
    std::unique_ptr<Session> m_demo;
    Session *m_active = nullptr;
    QSet<qint64> m_selected;
    QList<core::RuleProposal> m_pendingProposals;
    QString m_startupNotice;
    QString m_dataDir;
    QString m_importState = QStringLiteral("idle");
    QString m_importError;
    bool m_consented = false;
    bool m_lastConfigured = false;

    std::optional<QVariantList> m_items;
    std::optional<QVariantList> m_recipes;
    std::optional<QVariantList> m_clearRecipes;
    std::optional<QVariantList> m_shopping;
};

}

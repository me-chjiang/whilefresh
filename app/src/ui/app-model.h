#pragma once

#include "core/clock.h"
#include "data/builtin-data.h"
#include "data/database.h"
#include "data/store.h"
#include "platform/platform-services.h"
#include "recognition/recognition-service.h"
#include "ui/import-service.h"
#include "ui/kitchen-service.h"

#include <QObject>
#include <QSet>
#include <QVariantList>
#include <QVariantMap>

#include <memory>
#include <functional>

namespace whilefresh::ui {

// Thin view model for QML: services in, plain maps and lists out. Holds one real and,
// while demo mode is on, one separate demo session (own database file and recognizer).
class AppModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList items READ items NOTIFY changed)
    Q_PROPERTY(int nearCount READ nearCount NOTIFY changed)
    Q_PROPERTY(QVariantList selectedIds READ selectedIds NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList recipes READ recipes NOTIFY recipesChanged)
    Q_PROPERTY(QVariantList clearRecipes READ clearRecipes NOTIFY clearResultsChanged)
    Q_PROPERTY(QVariantMap clearPreview READ clearPreview NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap clearResultsSummary READ clearResultsSummary NOTIFY clearResultsChanged)
    Q_PROPERTY(bool clearResultsReady READ clearResultsReady NOTIFY clearResultsChanged)
    Q_PROPERTY(QVariantList shopping READ shopping NOTIFY shoppingChanged)
    Q_PROPERTY(QVariantList pantry READ pantry NOTIFY changed)
    Q_PROPERTY(QVariantList suggestions READ suggestions NOTIFY changed)
    Q_PROPERTY(QVariantList rules READ rules NOTIFY changed)
    Q_PROPERTY(QVariantList imports READ imports NOTIFY changed)
    Q_PROPERTY(bool demoMode READ demoMode NOTIFY demoModeChanged)
    Q_PROPERTY(bool recognitionConfigured READ recognitionConfigured CONSTANT)
    Q_PROPERTY(QString startupNotice READ startupNotice CONSTANT)
    Q_PROPERTY(QString privacyNotice READ privacyNotice CONSTANT)
    Q_PROPERTY(QString importState READ importState NOTIFY importChanged)
    Q_PROPERTY(QString importError READ importError NOTIFY importChanged)
    Q_PROPERTY(QVariantList draftItems READ draftItems NOTIFY importChanged)
    Q_PROPERTY(QString draftWarning READ draftWarning NOTIFY importChanged)
    Q_PROPERTY(bool draftDuplicate READ draftDuplicate NOTIFY importChanged)
    Q_PROPERTY(int draftFoodCount READ draftFoodCount NOTIFY importChanged)

public:
    using RecognizerFactory = std::function<std::unique_ptr<recognition::RecognitionService>()>;
    AppModel(platform::PlatformServices *platform, std::shared_ptr<const data::BuiltinData> builtin,
             const QString &dataDirectory = {}, const core::Clock *clock = nullptr,
             RecognizerFactory recognizerFactory = {}, QObject *parent = nullptr);
    ~AppModel() override;

    bool ok() const { return m_real != nullptr; }

    QVariantList items();
    int nearCount();
    QVariantList selectedIds() const;
    QVariantList recipes();
    QVariantList clearRecipes() const;
    QVariantMap clearPreview();
    QVariantMap clearResultsSummary() const { return m_clearResultsSummary; }
    bool clearResultsReady() const { return m_clearResultsReady; }
    QVariantList shopping();
    QVariantList pantry() const;
    QVariantList suggestions() const;
    QVariantList rules() const;
    QVariantList imports() const;
    bool demoMode() const { return m_active && m_active->demo; }
    bool recognitionConfigured() const;
    QString startupNotice() const { return m_startupNotice; }
    static QString privacyNotice() { return ImportService::privacyNotice(); }
    QString importState() const { return m_importState; }
    QString importError() const { return m_importError; }
    QVariantList draftItems() const;
    QString draftWarning() const;
    bool draftDuplicate() const;
    int draftFoodCount() const;

    Q_INVOKABLE void toggleSelected(qint64 id);
    Q_INVOKABLE void selectDefaults();
    Q_INVOKABLE bool findClearRecipes();
    Q_INVOKABLE QVariantMap recipeDetail(const QString &id) const;
    Q_INVOKABLE int addMissing(const QString &recipeId);
    Q_INVOKABLE void removeShopping(qint64 id);
    Q_INVOKABLE QString shoppingText() const;

    Q_INVOKABLE void useItem(qint64 id);
    Q_INVOKABLE void markAbsent(qint64 id);
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
    Q_INVOKABLE bool saveRule(const QString &name, const QString &oldLocation, const QString &location,
                              const QString &days);
    Q_INVOKABLE void deleteRule(const QString &name, const QString &location, const QString &kind);
    Q_INVOKABLE void deleteImport(qint64 id);
    Q_INVOKABLE QVariantMap deleteAllData();
    Q_INVOKABLE void setDemoMode(bool enabled);

    // Gallery and camera: the user has agreed to the privacy notice before the picker opens.
    Q_INVOKABLE void beginPick(bool camera);
    Q_INVOKABLE void beginText(const QString &text);
    Q_INVOKABLE void startImport(bool allowDuplicate);
    Q_INVOKABLE void cancelImport();
    Q_INVOKABLE void editDraft(int index, const QVariantMap &edits);
    Q_INVOKABLE void setDraftCategory(int index, const QString &category);
    Q_INVOKABLE void dropDraft(int index);
    Q_INVOKABLE void retrieveDraft(int index);
    Q_INVOKABLE void confirmImport();

signals:
    void changed();
    void selectionChanged();
    void recipesChanged();
    void clearResultsChanged();
    void shoppingChanged();
    void demoModeChanged();
    void importChanged();
    void notice(const QString &message);
    void pickCancelled();
    void importCommitted();

private slots:
    void refreshDate();

private:
    struct Session {
        bool demo = false;
        std::unique_ptr<data::Database> database;
        std::unique_ptr<data::Store> store;
        std::unique_ptr<KitchenService> kitchen;
        std::unique_ptr<recognition::RecognitionService> recognizer;
        std::unique_ptr<ImportService> import;
    };

    std::unique_ptr<Session> openSession(const QString &dbPath, bool demo);
    void seedDemo(Session &session);
    void activate(Session *session);
    void invalidate();
    void invalidateSelection();
    void setImportState(const QString &state, const QString &error = {});
    QVariantMap itemMap(const core::FridgeItem &item) const;
    QVariantMap summaryMap(const core::RecipeMatch &match) const;
    void applyEdits(core::FridgeItem &item, const QVariantMap &edits, bool editEntry) const;
    QString demoPath() const;
    KitchenService &kitchen() const { return *m_active->kitchen; }

    platform::PlatformServices *m_platform;
    std::shared_ptr<const data::BuiltinData> m_builtin;
    core::SystemClock m_systemClock;
    const core::Clock &m_clock;
    RecognizerFactory m_recognizerFactory;
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
    QDate m_lastDate;

    std::optional<QVariantList> m_items;
    std::optional<QVariantList> m_recipes;
    std::optional<QVariantList> m_clearRecipes;
    std::optional<QVariantList> m_shopping;
    std::optional<QVariantMap> m_clearPreview;
    QVariantMap m_clearResultsSummary;
    bool m_clearResultsReady = false;
};

}

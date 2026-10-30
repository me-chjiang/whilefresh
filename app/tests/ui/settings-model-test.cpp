#include "ai/recorded-chat-client.h"
#include "data/database.h"
#include "platform/desktop/file-key-vault.h"
#include "ui/app-model.h"
#include "ui/settings-model.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace whilefresh;
using namespace whilefresh::ui;
namespace ai = whilefresh::ai;

namespace {

ai::ChatRecording summaryAnswer(const QString &summary)
{
    ai::ChatExchange exchange;
    const QString content = QString::fromUtf8(QJsonDocument(QJsonObject{ { "summary", summary } }).toJson(QJsonDocument::Compact));
    exchange.response = QJsonDocument(QJsonObject{
        { "choices", QJsonArray{ QJsonObject{
            { "message", QJsonObject{ { "role", "assistant" }, { "content", content } } },
            { "finish_reason", "stop" } } } } }).toJson();
    return { { exchange } };
}

class UnconfiguredChatClient : public ai::ChatClient
{
public:
    quint64 sendEncoded(const ai::EncodedChatRequest &) override { ++sent; return nextRequestId(); }
    void cancel(quint64) override {}
    bool isConfigured() const override { return false; }
    int sent = 0;
};

}

class SettingsModelTest : public QObject
{
    Q_OBJECT

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<data::Database> m_database;
    std::unique_ptr<core::FixedClock> m_clock;
    std::unique_ptr<data::Store> m_store;
    core::MemoryNameMappingStore m_names;

    std::shared_ptr<UserCredentialProvider> credentials() const
    {
        return std::make_shared<UserCredentialProvider>(
            std::make_unique<platform::FileKeyVault>(m_dir->filePath(QStringLiteral("keys"))));
    }

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        m_database = std::make_unique<data::Database>(QStringLiteral(":memory:"));
        QVERIFY(m_database->open());
        QVERIFY(m_database->migrate());
        m_clock = std::make_unique<core::FixedClock>(QDateTime(QDate(2026, 10, 30), QTime(10, 0)));
        m_store = std::make_unique<data::Store>(*m_database, *m_clock, m_dir->filePath(QStringLiteral("imports")));
        m_names.clear();
    }

    void cleanup()
    {
        m_store.reset();
        m_database.reset();
        m_dir.reset();
    }

    void providerStoresKeysAndKeepsThemAcrossInstances()
    {
        auto provider = credentials();
        QVERIFY(!provider->isConfigured(ai::Service::DeepSeek));
        QVERIFY(provider->save(ai::Service::DeepSeek, QStringLiteral(" sk-user ")));
        QVERIFY(provider->save(ai::Service::LexmountProjectId, QStringLiteral("proj-1")));
        QCOMPARE(provider->key(ai::Service::DeepSeek), QStringLiteral("sk-user"));
        QVERIFY(!provider->isConfigured(ai::Service::Lexmount));

        auto again = credentials();
        QCOMPARE(again->key(ai::Service::DeepSeek), QStringLiteral("sk-user"));
        QCOMPARE(again->key(ai::Service::LexmountProjectId), QStringLiteral("proj-1"));
        QVERIFY(again->clear(ai::Service::DeepSeek));
        QVERIFY(!again->isConfigured(ai::Service::DeepSeek));
        QVERIFY(!credentials()->isConfigured(ai::Service::DeepSeek));
    }

    void saveSecretValidatesAndSignals()
    {
        ai::RecordedChatClient chat({});
        SettingsModel model(&chat, credentials());
        QSignalSpy changed(&model, &SettingsModel::servicesChanged);
        QVERIFY(!model.deepSeekConfigured());
        QVERIFY(!model.saveSecret(QStringLiteral("deepseek"), QStringLiteral("   ")));
        QVERIFY(!model.saveSecret(QStringLiteral("deepseek"), QStringLiteral("sk a")));
        QVERIFY(!model.saveSecret(QStringLiteral("deepseek"), QStringLiteral("sk\nx")));
        QVERIFY(!model.saveSecret(QStringLiteral("nonsense"), QStringLiteral("sk-x")));
        QCOMPARE(changed.size(), 0);
        QVERIFY(model.saveSecret(QStringLiteral("deepseek"), QStringLiteral("sk-x")));
        QVERIFY(model.saveSecret(QStringLiteral("lexmountProject"), QStringLiteral("p1")));
        QVERIFY(model.deepSeekConfigured() && model.lexmountProjectConfigured() && !model.lexmountConfigured());
        QCOMPARE(changed.size(), 2);
        QVERIFY(model.clearSecret(QStringLiteral("deepseek")));
        QVERIFY(!model.deepSeekConfigured());
        QCOMPARE(changed.size(), 3);
    }

    void preferencesSummaryLifecycle()
    {
        ai::RecordedChatClient chat(summaryAnswer(QStringLiteral("不吃香菜；少油。")));
        SettingsModel model(&chat, credentials());
        model.attach(m_store.get(), &m_names);
        QVERIFY(model.addPreference(QStringLiteral("不吃香菜")));
        QVERIFY(model.addPreference(QStringLiteral("少油")));
        QVERIFY(!model.addPreference(QStringLiteral("少油")));
        QCOMPARE(model.preferences().size(), 2);
        QVERIFY(model.preferenceSummary().isEmpty());

        QSignalSpy busy(&model, &SettingsModel::summarizingChanged);
        model.summarize();
        QVERIFY(model.summarizing());
        QTRY_VERIFY(!model.summarizing());
        QVERIFY(model.summaryError().isEmpty());
        QCOMPARE(model.preferenceSummary(), QStringLiteral("不吃香菜；少油。"));
        QCOMPARE(m_store->setting(QString::fromLatin1(kPreferenceSummarySetting)), QStringLiteral("不吃香菜；少油。"));
        QVERIFY(!model.summaryStale());
        QCOMPARE(chat.requests().size(), 1);

        const qint64 id = model.preferences().first().toMap().value(QStringLiteral("id")).toLongLong();
        QVERIFY(model.editPreference(id, QStringLiteral("不吃香菜和折耳根")));
        QVERIFY(model.summaryStale());

        for (const QVariant &preference : model.preferences())
            QVERIFY(model.deletePreference(preference.toMap().value(QStringLiteral("id")).toLongLong()));
        QVERIFY(model.preferenceSummary().isEmpty());
        QVERIFY(!model.summaryStale());
    }

    void deleteAllWhileSummarizingDropsTheResult()
    {
        ai::RecordedChatClient chat(summaryAnswer(QStringLiteral("不吃香菜。")));
        SettingsModel model(&chat, credentials());
        model.attach(m_store.get(), &m_names);
        QVERIFY(model.addPreference(QStringLiteral("不吃香菜")));
        model.summarize();
        QVERIFY(model.summarizing());
        for (const QVariant &preference : model.preferences())
            QVERIFY(model.deletePreference(preference.toMap().value(QStringLiteral("id")).toLongLong()));
        QVERIFY(!model.summarizing());
        QTest::qWait(100);
        QVERIFY(model.preferenceSummary().isEmpty());
        QVERIFY(!model.summaryStale());
    }

    void editWhileSummarizingDropsTheResult()
    {
        ai::RecordedChatClient chat(summaryAnswer(QStringLiteral("不吃香菜。")));
        SettingsModel model(&chat, credentials());
        model.attach(m_store.get(), &m_names);
        QVERIFY(model.addPreference(QStringLiteral("不吃香菜")));
        model.summarize();
        const qint64 id = model.preferences().first().toMap().value(QStringLiteral("id")).toLongLong();
        QVERIFY(model.editPreference(id, QStringLiteral("不吃葱")));
        QTest::qWait(100);
        QVERIFY(model.preferenceSummary().isEmpty());
        QVERIFY(!model.summarizing());
    }

    void summarizeNeverCallsWithoutAKeyOrPreferences()
    {
        UnconfiguredChatClient chat;
        SettingsModel model(&chat, credentials());
        model.attach(m_store.get(), &m_names);
        model.summarize(); // nothing to summarize
        QVERIFY(!model.summarizing());
        QVERIFY(model.addPreference(QStringLiteral("少油")));
        QVERIFY(!model.summarizeAvailable());
        model.summarize();
        QVERIFY(!model.summarizing());
        QCOMPARE(model.summaryError(), QStringLiteral("未配置密钥"));
        QCOMPARE(chat.sent, 0);
    }

    void failedSummaryKeepsTheOldOne()
    {
        ai::ChatExchange failure;
        failure.error = ai::ChatError{ ai::ErrorKind::NoNetwork, QStringLiteral("down"), 0 };
        ai::RecordedChatClient chat(ai::ChatRecording{ { failure } });
        SettingsModel model(&chat, credentials());
        model.attach(m_store.get(), &m_names);
        QVERIFY(m_store->setSetting(QString::fromLatin1(kPreferenceSummarySetting), QStringLiteral("旧摘要")));
        QVERIFY(model.addPreference(QStringLiteral("少油")));
        model.summarize();
        QTRY_VERIFY(!model.summarizing());
        QCOMPARE(model.summaryError(), QStringLiteral("网络不可用"));
        QCOMPARE(model.preferenceSummary(), QStringLiteral("旧摘要"));
    }

    void storedSummaryIsTheLastPartOfACookingPrompt()
    {
        ai::RecordedChatClient chat({});
        SettingsModel model(&chat, credentials());
        model.attach(m_store.get(), &m_names);
        QVERIFY(m_store->setSetting(QString::fromLatin1(kPreferenceSummarySetting), QStringLiteral("不吃香菜；少油。")));
        ai::SystemPromptBuilder builder(QStringLiteral("你是厨房助手。"));
        builder.addSection(QStringLiteral("库存"), QStringLiteral("鸡蛋"));
        const QString prompt = model.cookingSystemPrompt(builder);
        QVERIFY(prompt.endsWith(QStringLiteral("【用户偏好】\n不吃香菜；少油。")));
        QVERIFY(prompt.indexOf(QStringLiteral("【库存】")) < prompt.indexOf(QStringLiteral("【用户偏好】")));
        QVERIFY(m_store->setSetting(QString::fromLatin1(kPreferenceSummarySetting), QString()));
        QVERIFY(!model.cookingSystemPrompt(builder).contains(QStringLiteral("用户偏好")));
    }

    void nameMappingsCanBeListedForgottenAndCleared()
    {
        ai::RecordedChatClient chat({});
        SettingsModel model(&chat, credentials());
        model.attach(m_store.get(), &m_names);
        QVERIFY(model.nameMappings().isEmpty());
        m_names.remember(QStringLiteral("土猪肉"), QStringLiteral("猪肉"));
        m_names.remember(QStringLiteral("冰鲜 鸡全腿"), QStringLiteral("鸡腿"));
        QCOMPARE(model.nameMappings().size(), 2);
        QSignalSpy changed(&model, &SettingsModel::nameMappingsChanged);
        model.forgetNameMapping(QStringLiteral("土猪肉"));
        QCOMPARE(model.nameMappings().size(), 1);
        QCOMPARE(model.nameMappings().first().toMap().value(QStringLiteral("canonical")).toString(), QStringLiteral("鸡腿"));
        model.clearNameMappings();
        QVERIFY(model.nameMappings().isEmpty());
        QCOMPARE(changed.size(), 2);
    }

    void layeredProviderPrefersThePrimaryKey()
    {
        auto primary = std::make_shared<ai::StaticCredentialProvider>();
        auto fallback = std::make_shared<ai::StaticCredentialProvider>(QStringLiteral("sk-built"));
        fallback->setKey(ai::Service::Lexmount, QStringLiteral("lx-built"));
        ai::LayeredCredentialProvider layered(primary, fallback);
        QCOMPARE(layered.key(ai::Service::DeepSeek), QStringLiteral("sk-built"));
        QCOMPARE(layered.key(ai::Service::Lexmount), QStringLiteral("lx-built"));
        QVERIFY(!layered.isConfigured(ai::Service::LexmountProjectId));
        primary->setKey(ai::Service::DeepSeek, QStringLiteral("sk-user"));
        QCOMPARE(layered.key(ai::Service::DeepSeek), QStringLiteral("sk-user"));
        QCOMPARE(layered.key(ai::Service::Lexmount), QStringLiteral("lx-built"));
        primary->setKey(ai::Service::DeepSeek, QString());
        QCOMPARE(layered.key(ai::Service::DeepSeek), QStringLiteral("sk-built"));
    }

    void sourceReportsUserBuiltinOrNone()
    {
        auto builtin = std::make_shared<ai::StaticCredentialProvider>(QStringLiteral("sk-built"));
        builtin->setKey(ai::Service::Lexmount, QStringLiteral("lx-built"));
        ai::RecordedChatClient chat({});
        SettingsModel model(&chat, credentials(), builtin);
        QCOMPARE(model.deepSeekSource(), QStringLiteral("builtin"));
        QCOMPARE(model.lexmountSource(), QStringLiteral("builtin"));
        QCOMPARE(model.lexmountProjectSource(), QStringLiteral("none"));
        QVERIFY(!model.deepSeekConfigured()); // 已配置 means the user's own key
        QSignalSpy changed(&model, &SettingsModel::servicesChanged);
        QVERIFY(model.saveSecret(QStringLiteral("deepseek"), QStringLiteral("sk-user")));
        QVERIFY(model.saveSecret(QStringLiteral("lexmountProject"), QStringLiteral("p1")));
        QCOMPARE(model.deepSeekSource(), QStringLiteral("user"));
        QCOMPARE(model.lexmountProjectSource(), QStringLiteral("user"));
        QVERIFY(model.clearSecret(QStringLiteral("deepseek")));
        QCOMPARE(model.deepSeekSource(), QStringLiteral("builtin"));
        QVERIFY(model.clearSecret(QStringLiteral("lexmountProject")));
        QCOMPARE(model.lexmountProjectSource(), QStringLiteral("none"));
        QCOMPARE(changed.size(), 4);
    }

    void modeSwitchChoosesTheKeySource()
    {
        QString error;
        const auto builtin = data::BuiltinData::builtIn(&error);
        QVERIFY2(builtin, qPrintable(error));
        QStandardPaths::setTestModeEnabled(true);
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).removeRecursively();

        auto builtinKeys = std::make_shared<ai::StaticCredentialProvider>(QStringLiteral("sk-built"));
        builtinKeys->setKey(ai::Service::Lexmount, QStringLiteral("lx-built"));
        AppModel app(nullptr, builtin, nullptr,
                     std::make_unique<platform::FileKeyVault>(m_dir->filePath(QStringLiteral("app-keys"))), builtinKeys);
        QVERIFY(app.ok());
        QVERIFY(!app.demoMode());
        // No user key: the built-in one is used.
        QVERIFY(app.recognitionConfigured());
        QCOMPARE(app.credentials()->key(ai::Service::DeepSeek), QStringLiteral("sk-built"));
        QCOMPARE(app.credentials()->key(ai::Service::Lexmount), QStringLiteral("lx-built"));
        QVERIFY(!app.credentials()->isConfigured(ai::Service::LexmountProjectId));
        QSignalSpy configured(&app, &AppModel::recognitionConfiguredChanged);

        // The user's key wins; recognition stays configured, so no signal.
        QVERIFY(app.settings()->saveSecret(QStringLiteral("deepseek"), QStringLiteral("sk-user")));
        QVERIFY(app.recognitionConfigured());
        QCOMPARE(configured.size(), 0);
        QCOMPARE(app.credentials()->key(ai::Service::DeepSeek), QStringLiteral("sk-user"));

        // Demo mode ignores the user's keys.
        app.setDemoMode(true);
        QVERIFY(app.demoMode());
        QCOMPARE(app.credentials()->key(ai::Service::DeepSeek), QStringLiteral("sk-built"));
        QVERIFY(app.recognitionConfigured());
        QVERIFY(app.settings()->deepSeekConfigured()); // the user's key is untouched

        app.setDemoMode(false);
        QCOMPARE(app.credentials()->key(ai::Service::DeepSeek), QStringLiteral("sk-user"));

        // Clearing the user's key falls back to the built-in one.
        QVERIFY(app.settings()->clearSecret(QStringLiteral("deepseek")));
        QCOMPARE(app.credentials()->key(ai::Service::DeepSeek), QStringLiteral("sk-built"));
        QVERIFY(app.recognitionConfigured());
        QCOMPARE(configured.size(), 0);
    }

    void keyLessBuildNeedsTheUsersKeys()
    {
        QString error;
        const auto builtin = data::BuiltinData::builtIn(&error);
        QVERIFY2(builtin, qPrintable(error));
        QStandardPaths::setTestModeEnabled(true);
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).removeRecursively();

        AppModel app(nullptr, builtin, nullptr,
                     std::make_unique<platform::FileKeyVault>(m_dir->filePath(QStringLiteral("app-keys"))),
                     std::make_shared<ai::StaticCredentialProvider>());
        QVERIFY(app.ok());
        QVERIFY(!app.recognitionConfigured());
        QSignalSpy configured(&app, &AppModel::recognitionConfiguredChanged);
        QVERIFY(app.settings()->saveSecret(QStringLiteral("deepseek"), QStringLiteral("sk-user")));
        QVERIFY(app.recognitionConfigured());
        QCOMPARE(configured.size(), 1);
        app.setDemoMode(true);
        QVERIFY(!app.recognitionConfigured());
        app.setDemoMode(false);
        QVERIFY(app.settings()->clearSecret(QStringLiteral("deepseek")));
        QVERIFY(!app.recognitionConfigured());
    }
};

QTEST_MAIN(SettingsModelTest)

#include "settings-model-test.moc"

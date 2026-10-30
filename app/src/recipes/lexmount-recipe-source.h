#pragma once

#include "recipes/recipe-source.h"

#include <QHash>
#include <QUrl>

#include <functional>
#include <optional>

class QNetworkAccessManager;

namespace whilefresh::recipes {

class LexmountJob;

// Temporary credential source for desktop use until the settings page stores them (T16):
// WHILEFRESH_LEXMOUNT_API_KEY and WHILEFRESH_LEXMOUNT_PROJECT_ID.
LexmountCredentials lexmountCredentialsFromEnvironment();

struct LexmountConfig {
    QUrl baseUrl { QStringLiteral("https://api.lexmount.cn") };
    int httpTimeoutMs = 15000;      // one Lexmount HTTP call
    int sessionReadyTimeoutMs = 40000; // creating the browser until it reports active
    int stepTimeoutMs = 20000;      // one DevTools command or one wait poll
    int pageWaitTimeoutMs = 30000;  // navigation until the page settled
    int totalTimeoutMs = 90000;     // whole request
    int pollIntervalMs = 300;
    int cleanupTimeoutMs = 8000;
    int pageSize = 20;
};

// Reads Xiachufang through a Lexmount cloud browser: create a session, drive the page over the
// DevTools protocol, scrape the DOM, delete the session. Everything is asynchronous on the
// caller's thread (QNetworkAccessManager and a socket), so the UI thread never blocks and
// cancel() takes effect between steps. The session is deleted on success, error, timeout and
// cancel, with the same request the submodule's lexmount.py sends (DELETE <base>/instance with
// {"session_id": ...}).
//
// The scraped page is turned into the YAML contract document and parsed by
// parseXiachufangYaml, the path recorded samples take too; documentReady() carries that YAML for
// recording tools. A failed browser route is reported as such; there is no fallback to another
// Lexmount endpoint.
class LexmountRecipeSource : public RecipeSource
{
    Q_OBJECT

public:
    using CredentialsFn = std::function<LexmountCredentials()>;

    using Config = LexmountConfig;

    // The credentials are asked for when a request starts and kept only while it runs.
    explicit LexmountRecipeSource(CredentialsFn credentials, Config config = {}, QObject *parent = nullptr);
    ~LexmountRecipeSource() override;

    quint64 search(const QString &keyword, int page = 1) override;
    quint64 detail(const QString &id) override;
    void cancel(quint64 requestId) override;
    bool isConfigured() const override;

    // Browser sessions whose deletion is still under way.
    int pendingCleanups() const { return m_cleaning; }

signals:
    // The contract document a successful request was built from (for recording samples).
    void documentReady(quint64 requestId, const QByteArray &yaml);
    // The browser session of a request was deleted (ok) or deleting it failed.
    void cleanupFinished(quint64 requestId, bool ok);

private:
    friend class LexmountJob;

    quint64 start(bool isSearch, const QString &keyword, const QString &id, int page);
    void jobFinished(quint64 id, bool isSearch, const std::optional<RecipeError> &error, const QByteArray &yaml, bool silent,
        LexmountJob *job);

    CredentialsFn m_credentials;
    Config m_config;
    QNetworkAccessManager *m_network;
    QHash<quint64, LexmountJob *> m_jobs;
    int m_cleaning = 0;
};

}

#include "recipes/lexmount-recipe-source.h"

#include "recipes/cdp-connection.h"
#include "recipes/contract-document.h"
#include "recipes/recipes-log.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QUrlQuery>

namespace whilefresh::recipes {

namespace {

constexpr qsizetype kMaxHttpBytes = 4 * 1024 * 1024;
const char kSite[] = "https://www.xiachufang.com";

// Same selectors and image fallbacks as the submodule's scrape script, but it waits for the
// page content to appear (the load event alone can fire on a still empty client-rendered page)
// and reports where it ended up so a verification page can be told from a recipe page.
const char kScrapeScript[] = R"js((async () => {
  const kind = '%1';
  const text = (root) => root ? (root.textContent || '').trim() : '';
  const image = (root) => { const i = root && root.querySelector('img');
    return i ? (i.currentSrc || i.src || i.getAttribute('data-src')) : null; };
  const titleEl = () => document.querySelector('h1.page-title[itemprop="name"]') || document.querySelector('h1.page-title');
  const waitFor = async (fn, ms) => {
    const end = Date.now() + ms;
    while (Date.now() < end) { if (fn()) return true; await new Promise((r) => setTimeout(r, 150)); }
    return !!fn();
  };
  if (document.readyState !== 'complete')
    await new Promise((resolve) => window.addEventListener('load', resolve, {once: true}));
  if (kind === 'detail') await waitFor(titleEl, 8000);
  else await waitFor(() => document.querySelector('.normal-recipe-list .recipe'), 8000);
  await Promise.race([
    Promise.all([...document.images].map((img) => img.decode ? img.decode().catch(() => undefined) : undefined)),
    new Promise((r) => setTimeout(r, 3000))]);
  const diag = {title: document.title, host: location.host, path: location.pathname,
    list: !!document.querySelector('.normal-recipe-list')};
  if (kind === 'search') {
    const rows = [...document.querySelectorAll('.normal-recipe-list .recipe')].map((node) => {
      const links = [...node.querySelectorAll('a[href*="/recipe/"]')];
      const a = node.querySelector('.info .name a') || links.find((link) => text(link)) || links[0];
      const m = a && (a.getAttribute('href') || '').match(/\/recipe\/(\d+)\/?/);
      return m ? {name: text(a), url: `/recipe/${m[1]}/`, cover: image(node.querySelector('.cover'))} : null;
    }).filter(Boolean);
    return {Recipe: rows, diag};
  }
  const title = titleEl();
  if (!title) return {diag};
  const materials = [...document.querySelectorAll('div.recipe-show > div.ings > table tr')].map((row) => ({
    name: text(row.querySelector('td.name')), unit: text(row.querySelector('td.unit'))})).filter((x) => x.name);
  const steps = [...document.querySelectorAll('div.steps > ol li')].map((node, n) => ({
    step: n + 1, desc: text(node.querySelector('p')), img: image(node)})).filter((x) => x.desc || x.img);
  return {Content: [{name: text(title), cover: image(document.querySelector('div.recipe-show > div.cover')),
    grade: text(document.querySelector('.recipe-show .stats .score .number')) || null,
    materials, steps, tip: text(document.querySelector('div.tip'))}], diag};
})())js";

const char kStateScript[] = "({href: location.href, ready: document.readyState, title: document.title})";

QString findString(const QJsonObject &object, const QStringList &keys)
{
    for (const QString &key : keys) {
        const QJsonValue value = object.value(key);
        if (value.isString() && !value.toString().isEmpty()) {
            return value.toString();
        }
    }
    for (auto it = object.begin(); it != object.end(); ++it) {
        if (it.value().isObject()) {
            const QString found = findString(it.value().toObject(), keys);
            if (!found.isEmpty()) {
                return found;
            }
        }
    }
    return {};
}

QString statusOf(const QJsonObject &object)
{
    for (const char *key : { "status", "state" }) {
        if (object.value(QLatin1String(key)).isString()) {
            return object.value(QLatin1String(key)).toString().toLower();
        }
    }
    for (auto it = object.begin(); it != object.end(); ++it) {
        if (it.value().isObject()) {
            const QString found = statusOf(it.value().toObject());
            if (!found.isEmpty()) {
                return found;
            }
        }
    }
    return {};
}

}

class LexmountJob : public QObject
{
    Q_OBJECT

public:
    LexmountJob(LexmountRecipeSource *owner, QNetworkAccessManager *network, LexmountRecipeSource::Config config,
        LexmountCredentials credentials, quint64 id, ContractQuery query)
        : QObject(owner), m_owner(owner), m_network(network), m_config(std::move(config)), m_credentials(std::move(credentials)),
          m_id(id), m_query(std::move(query))
    {
        m_stepTimer.setSingleShot(true);
        m_totalTimer.setSingleShot(true);
        connect(&m_stepTimer, &QTimer::timeout, this, [this] { fail({ RecipeErrorKind::Timeout, QStringLiteral("step timed out") }); });
        connect(&m_totalTimer, &QTimer::timeout, this, [this] { fail({ RecipeErrorKind::Timeout, QStringLiteral("request timed out") }); });
    }

    bool isSearch() const { return m_query.operation == RecipeDocument::Operation::Search; }

    void begin()
    {
        if (m_done) {
            return;
        }
        m_elapsed.start();
        m_totalTimer.start(m_config.totalTimeoutMs);
        qCInfo(lcRecipes, "recipe %llu: creating browser session", m_id);
        m_creating = true;
        http("POST", QStringLiteral("/instance/v2"),
            QJsonObject { { QStringLiteral("browser_mode"), QStringLiteral("normal") },
                { QStringLiteral("enable_lightmount_layout"), true }, { QStringLiteral("enable_captcha"), false },
                { QStringLiteral("downloads"), QJsonObject { { QStringLiteral("enabled"), true } } } },
            m_config.httpTimeoutMs, [this](const QJsonObject &created) {
                m_sessionId = findString(created, { QStringLiteral("session_id"), QStringLiteral("sessionId"),
                    QStringLiteral("instance_id"), QStringLiteral("id") });
                if (m_sessionId.isEmpty()) {
                    fail({ RecipeErrorKind::Unavailable, QStringLiteral("Lexmount browser did not return a session") });
                    return;
                }
                m_sessionDeadline.start();
                pollSession();
            });
    }

    // The source is going away: nobody is left to tell, and the cleanup bookkeeping stays untouched.
    void detachOwner() { m_owner = nullptr; }

    // Stops without telling anyone; the session is still deleted.
    void cancel()
    {
        qCInfo(lcRecipes, "recipe %llu: cancelled", m_id);
        finish(std::nullopt, {}, true);
    }

private:
    struct Pending {
        int token = 0;
        bool tolerant = false;
        std::function<void(const QJsonObject &)> next;
    };

    QNetworkRequest makeRequest(const QString &path, const QUrlQuery &query = {}) const
    {
        QUrl url = m_config.baseUrl;
        url.setPath(path);
        url.setQuery(query);
        QNetworkRequest request(url);
        request.setRawHeader("x-api-key", m_credentials.apiKey.toUtf8());
        request.setRawHeader("x-project-id", m_credentials.projectId.toUtf8());
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        return request;
    }

    void armStep(int ms) { m_stepTimer.start(ms); }

    void http(const QByteArray &method, const QString &path, const QJsonObject &body, int timeoutMs,
        std::function<void(const QJsonObject &)> next, const QUrlQuery &query = {})
    {
        const int token = ++m_token;
        armStep(timeoutMs);
        const QNetworkRequest request = makeRequest(path, query);
        const QByteArray payload = body.isEmpty() ? QByteArray() : QJsonDocument(body).toJson(QJsonDocument::Compact);
        QNetworkReply *reply = method == "GET" ? m_network->get(request) : m_network->post(request, payload);
        m_reply = reply;
        connect(reply, &QNetworkReply::finished, this, [this, reply, token, next = std::move(next)] {
            reply->deleteLater();
            if (m_reply == reply) {
                m_reply = nullptr;
            }
            m_creating = false; // only the first call (session creation) sets it
            if (m_done || token != m_token) {
                return; // a stale reply must not move a newer step
            }
            std::optional<RecipeError> failure = httpFailure(reply);
            if (failure) {
                fail(*failure);
                return;
            }
            const QByteArray data = reply->readAll();
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
            if (!document.isObject()) {
                fail({ RecipeErrorKind::BadResponse, QStringLiteral("Lexmount returned invalid JSON") });
                return;
            }
            next(document.object());
        });
    }

    static std::optional<RecipeError> httpFailure(QNetworkReply *reply)
    {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 401 || status == 403) {
            return RecipeError { RecipeErrorKind::AuthFailed, QStringLiteral("Lexmount HTTP %1").arg(status) };
        }
        if (reply->error() == QNetworkReply::NoError && status >= 200 && status < 300) {
            if (reply->bytesAvailable() > kMaxHttpBytes) {
                return RecipeError { RecipeErrorKind::BadResponse, QStringLiteral("Lexmount response exceeds 4 MiB") };
            }
            return std::nullopt;
        }
        if (status >= 400) {
            return RecipeError { status == 429 ? RecipeErrorKind::Unavailable : RecipeErrorKind::Unavailable,
                QStringLiteral("Lexmount HTTP %1").arg(status) };
        }
        if (reply->error() == QNetworkReply::TimeoutError) {
            return RecipeError { RecipeErrorKind::Timeout, QStringLiteral("Lexmount request timed out") };
        }
        return RecipeError { RecipeErrorKind::NetworkError, QStringLiteral("Lexmount request failed: error %1").arg(int(reply->error())) };
    }

    void pollSession()
    {
        http("POST", QStringLiteral("/instance/session"), QJsonObject { { QStringLiteral("session_id"), m_sessionId } },
            m_config.httpTimeoutMs, [this](const QJsonObject &status) {
                const QString state = statusOf(status);
                if (state == QLatin1String("active") || state == QLatin1String("ready") || state == QLatin1String("running")) {
                    qCInfo(lcRecipes, "recipe %llu: browser ready after %lld ms", m_id, qint64(m_elapsed.elapsed()));
                    fetchVersion();
                    return;
                }
                if (state == QLatin1String("failed") || state == QLatin1String("error") || state == QLatin1String("stopped")
                    || state == QLatin1String("terminated")) {
                    fail({ RecipeErrorKind::Unavailable, QStringLiteral("Lexmount browser session failed") });
                    return;
                }
                if (m_sessionDeadline.elapsed() > m_config.sessionReadyTimeoutMs) {
                    fail({ RecipeErrorKind::Timeout, QStringLiteral("Lexmount browser session did not become ready") });
                    return;
                }
                const int token = ++m_token;
                armStep(m_config.httpTimeoutMs);
                QTimer::singleShot(m_config.pollIntervalMs, this, [this, token] {
                    if (!m_done && token == m_token) {
                        pollSession();
                    }
                });
            });
    }

    void fetchVersion()
    {
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("session_id"), m_sessionId);
        http("GET", QStringLiteral("/json/version"), {}, m_config.httpTimeoutMs, [this](const QJsonObject &version) {
            QString socketUrl = version.value(QLatin1String("webSocketDebuggerUrlTransformed")).toString();
            if (socketUrl.isEmpty()) {
                socketUrl = version.value(QLatin1String("webSocketDebuggerUrl")).toString();
            }
            if (socketUrl.isEmpty()) {
                socketUrl = version.value(QLatin1String("websocketDebuggerUrl")).toString();
            }
            if (socketUrl.isEmpty()) {
                fail({ RecipeErrorKind::Unavailable, QStringLiteral("Lexmount browser websocket URL missing") });
                return;
            }
            openCdp(QUrl(socketUrl));
        }, query);
    }

    void openCdp(const QUrl &url)
    {
        m_cdp = new CdpConnection(this);
        connect(m_cdp, &CdpConnection::response, this, &LexmountJob::onCdpResponse);
        connect(m_cdp, &CdpConnection::closed, this, [this](const QString &reason) {
            fail({ RecipeErrorKind::NetworkError, QStringLiteral("browser connection lost: %1").arg(reason) });
        });
        const int token = ++m_token;
        armStep(m_config.stepTimeoutMs);
        connect(m_cdp, &CdpConnection::opened, this, [this, token] {
            if (m_done || token != m_token) {
                return;
            }
            command(QStringLiteral("Target.getTargets"), {}, false, [this](const QJsonObject &targets) {
                QString targetId;
                const QJsonArray infos = targets.value(QLatin1String("targetInfos")).toArray();
                for (const QJsonValue &value : infos) {
                    if (value.toObject().value(QLatin1String("type")).toString() == QLatin1String("page")) {
                        targetId = value.toObject().value(QLatin1String("targetId")).toString();
                        break;
                    }
                }
                if (targetId.isEmpty() && !infos.isEmpty()) {
                    targetId = infos.first().toObject().value(QLatin1String("targetId")).toString();
                }
                if (targetId.isEmpty()) {
                    fail({ RecipeErrorKind::Unavailable, QStringLiteral("Lexmount browser page target missing") });
                    return;
                }
                command(QStringLiteral("Target.attachToTarget"),
                    QJsonObject { { QStringLiteral("targetId"), targetId }, { QStringLiteral("flatten"), true } }, false,
                    [this](const QJsonObject &attached) {
                        m_cdpSession = attached.value(QLatin1String("sessionId")).toString();
                        command(QStringLiteral("Page.enable"), {}, true, [this](const QJsonObject &) {
                            command(QStringLiteral("Runtime.enable"), {}, true, [this](const QJsonObject &) { navigate(); });
                        });
                    });
            });
        });
        m_cdp->open(url);
    }

    void command(const QString &method, const QJsonObject &params, bool inPage, std::function<void(const QJsonObject &)> next,
        bool tolerant = false)
    {
        const int token = ++m_token;
        armStep(m_config.stepTimeoutMs);
        const quint64 commandId = m_cdp->send(method, params, inPage ? m_cdpSession : QString());
        m_pending.insert(commandId, { token, tolerant, std::move(next) });
    }

    void onCdpResponse(quint64 commandId, bool ok, const QJsonObject &result, const QString &message)
    {
        const auto it = m_pending.find(commandId);
        if (it == m_pending.end()) {
            return;
        }
        const Pending pending = it.value();
        m_pending.erase(it);
        if (m_done || pending.token != m_token) {
            return;
        }
        if (!ok && !pending.tolerant) {
            fail({ RecipeErrorKind::Unavailable, QStringLiteral("browser command failed: %1").arg(message.left(120)) });
            return;
        }
        pending.next(ok ? result : QJsonObject());
    }

    QString targetPath() const
    {
        return isSearch() ? QStringLiteral("/search/") : QStringLiteral("/recipe/%1/").arg(m_query.id);
    }

    QString targetUrl() const
    {
        if (isSearch()) {
            QUrl url(QString::fromLatin1(kSite) + QStringLiteral("/search/"));
            QUrlQuery query;
            query.addQueryItem(QStringLiteral("keyword"), m_query.keyword);
            query.addQueryItem(QStringLiteral("cat"), QStringLiteral("1001"));
            query.addQueryItem(QStringLiteral("page"), QString::number(m_query.page));
            url.setQuery(query);
            return url.toString(QUrl::FullyEncoded);
        }
        return QString::fromLatin1(kSite) + targetPath();
    }

    void navigate()
    {
        qCInfo(lcRecipes, "recipe %llu: navigating", m_id);
        command(QStringLiteral("Page.navigate"), QJsonObject { { QStringLiteral("url"), targetUrl() } }, true,
            [this](const QJsonObject &result) {
                const QString errorText = result.value(QLatin1String("errorText")).toString();
                if (!errorText.isEmpty()) {
                    fail({ RecipeErrorKind::NetworkError, QStringLiteral("navigation failed: %1").arg(errorText.left(80)) });
                    return;
                }
                m_pageWait.start();
                m_lastPath.clear();
                m_stablePolls = 0;
                waitForPage();
            });
    }

    // The page the browser starts on (about:blank) also reports "complete", so readiness means:
    // the target path is loaded, or some other http(s) page has stayed put across two polls
    // (a redirect to a verification page), in which case the scrape reports what it found.
    void waitForPage()
    {
        command(QStringLiteral("Runtime.evaluate"),
            QJsonObject { { QStringLiteral("expression"), QString::fromLatin1(kStateScript) },
                { QStringLiteral("returnByValue"), true } },
            true,
            [this](const QJsonObject &result) {
                const QJsonObject state = result.value(QLatin1String("result")).toObject().value(QLatin1String("value")).toObject();
                const QUrl href(state.value(QLatin1String("href")).toString());
                const bool complete = state.value(QLatin1String("ready")).toString() == QLatin1String("complete");
                const bool http = href.scheme() == QLatin1String("http") || href.scheme() == QLatin1String("https");
                m_lastPath = QStringLiteral("%1%2").arg(href.host(), href.path());
                m_lastTitle = state.value(QLatin1String("title")).toString();
                if (complete && http) {
                    if (href.path() == targetPath()) {
                        scrape();
                        return;
                    }
                    if (++m_stablePolls >= 2) {
                        scrape();
                        return;
                    }
                } else {
                    m_stablePolls = 0;
                }
                if (m_pageWait.elapsed() > m_config.pageWaitTimeoutMs) {
                    fail({ RecipeErrorKind::Timeout, QStringLiteral("page did not load (%1, title=\"%2\")").arg(m_lastPath, m_lastTitle.left(60)) });
                    return;
                }
                const int token = ++m_token;
                armStep(m_config.stepTimeoutMs);
                QTimer::singleShot(m_config.pollIntervalMs, this, [this, token] {
                    if (!m_done && token == m_token) {
                        waitForPage();
                    }
                });
            },
            true);
    }

    void scrape()
    {
        qCInfo(lcRecipes, "recipe %llu: scraping after %lld ms", m_id, qint64(m_elapsed.elapsed()));
        command(QStringLiteral("Runtime.evaluate"),
            QJsonObject { { QStringLiteral("expression"), QString::fromLatin1(kScrapeScript).arg(isSearch() ? QStringLiteral("search") : QStringLiteral("detail")) },
                { QStringLiteral("returnByValue"), true }, { QStringLiteral("awaitPromise"), true } },
            true, [this](const QJsonObject &result) {
                if (result.contains(QLatin1String("exceptionDetails"))) {
                    fail({ RecipeErrorKind::Unavailable, QStringLiteral("page script failed") });
                    return;
                }
                const QJsonObject value = result.value(QLatin1String("result")).toObject().value(QLatin1String("value")).toObject();
                onScraped(value);
            });
    }

    void onScraped(const QJsonObject &value)
    {
        const QJsonObject diag = value.value(QLatin1String("diag")).toObject();
        const QString title = diag.value(QLatin1String("title")).toString().left(60);
        const QString where = diag.value(QLatin1String("host")).toString() + diag.value(QLatin1String("path")).toString();
        static const QRegularExpression verify(QStringLiteral("验证|安全|拦截|captcha|denied|forbidden"),
            QRegularExpression::CaseInsensitiveOption);
        bool blocked = false;
        if (isSearch()) {
            blocked = !diag.value(QLatin1String("list")).toBool()
                && (diag.value(QLatin1String("path")).toString() != QLatin1String("/search/") || verify.match(title).hasMatch());
        } else {
            blocked = !value.contains(QLatin1String("Content"));
        }
        if (blocked) {
            fail({ RecipeErrorKind::Blocked, QStringLiteral("not a recipe page: title=\"%1\" at %2").arg(title, where) });
            return;
        }
        QString error;
        const QByteArray yaml = buildContractDocument(m_query, value, double(m_elapsed.elapsed()), &error);
        if (yaml.isEmpty()) {
            fail({ RecipeErrorKind::BadResponse, error });
            return;
        }
        finish(std::nullopt, yaml, false);
    }

    void fail(const RecipeError &error)
    {
        qCWarning(lcRecipes, "recipe %llu failed: kind=%s %s", m_id, qPrintable(enumKey(error.kind)), qPrintable(error.detail.left(160)));
        finish(error, {}, false);
    }

    void finish(const std::optional<RecipeError> &error, const QByteArray &yaml, bool silent)
    {
        if (m_done) {
            return;
        }
        m_done = true;
        ++m_token;
        m_stepTimer.stop();
        m_totalTimer.stop();
        bool adopted = false;
        if (m_reply) {
            m_reply->disconnect(this);
            if (m_creating) {
                // The server may already have created the session; let the reply arrive and delete it.
                adoptCreateReply(m_reply);
                adopted = true;
            } else {
                m_reply->abort();
                m_reply->deleteLater();
            }
            m_reply = nullptr;
        }
        if (m_cdp) {
            m_cdp->disconnect(this);
            m_cdp->close();
        }
        m_pending.clear();
        if (!adopted) {
            startCleanup(m_sessionId);
        }
        if (m_owner) {
            m_owner->jobFinished(m_id, isSearch(), silent ? std::optional<RecipeError>() : error, yaml, silent, this);
        }
    }

    // A cancelled or timed-out POST /instance/v2 is not aborted: its reply tells us the session id.
    void adoptCreateReply(QNetworkReply *reply)
    {
        if (m_owner) {
            ++m_owner->m_cleaning;
        }
        auto *timer = new QTimer(reply);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, reply, &QNetworkReply::abort);
        timer->start(m_config.cleanupTimeoutMs);
        connect(reply, &QNetworkReply::finished, this, [this, reply] {
            reply->deleteLater();
            QString sessionId;
            if (reply->error() == QNetworkReply::NoError) {
                sessionId = findString(QJsonDocument::fromJson(reply->readAll()).object(),
                    { QStringLiteral("session_id"), QStringLiteral("sessionId"), QStringLiteral("instance_id"), QStringLiteral("id") });
            }
            if (!sessionId.isEmpty()) {
                qCInfo(lcRecipes, "recipe %llu: removing the session of an abandoned create call", m_id);
                startCleanup(sessionId); // counts itself before this one is released
            } else {
                qCWarning(lcRecipes, "recipe %llu: abandoned create call returned no session (error %d)", m_id, int(reply->error()));
                deleteLater();
            }
            if (m_owner) {
                --m_owner->m_cleaning;
                if (sessionId.isEmpty()) {
                    emit m_owner->cleanupFinished(m_id, false);
                }
            }
        });
    }

    void startCleanup(const QString &sessionId)
    {
        if (sessionId.isEmpty()) {
            deleteLater();
            return;
        }
        if (m_owner) {
            ++m_owner->m_cleaning;
        }
        // Exactly the request lexmount.py sends: DELETE <base>/instance with {"session_id": ...} as JSON.
        const QNetworkRequest request = makeRequest(QStringLiteral("/instance"));
        const QByteArray body = QJsonDocument(QJsonObject { { QStringLiteral("session_id"), sessionId } }).toJson(QJsonDocument::Compact);
        QNetworkReply *reply = m_network->sendCustomRequest(request, "DELETE", body);
        auto *timer = new QTimer(reply);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, reply, &QNetworkReply::abort);
        timer->start(m_config.cleanupTimeoutMs);
        connect(reply, &QNetworkReply::finished, this, [this, reply] {
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const bool deleted = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300;
            if (deleted) {
                qCInfo(lcRecipes, "recipe %llu: browser session deleted (HTTP %d)", m_id, status);
            } else {
                qCWarning(lcRecipes, "recipe %llu: deleting the browser session failed (HTTP %d, error %d)", m_id, status, int(reply->error()));
            }
            reply->deleteLater();
            if (m_owner) {
                --m_owner->m_cleaning;
                emit m_owner->cleanupFinished(m_id, deleted);
            }
            deleteLater();
        });
    }

    LexmountRecipeSource *m_owner;
    QNetworkAccessManager *m_network;
    LexmountRecipeSource::Config m_config;
    LexmountCredentials m_credentials;
    quint64 m_id;
    ContractQuery m_query;

    bool m_done = false;
    bool m_creating = false;
    int m_token = 0;
    QString m_sessionId;
    QString m_cdpSession;
    CdpConnection *m_cdp = nullptr;
    QNetworkReply *m_reply = nullptr;
    QHash<quint64, Pending> m_pending;
    QTimer m_stepTimer;
    QTimer m_totalTimer;
    QElapsedTimer m_elapsed;
    QElapsedTimer m_sessionDeadline;
    QElapsedTimer m_pageWait;
    QString m_lastPath;
    QString m_lastTitle;
    int m_stablePolls = 0;
};

LexmountCredentials lexmountCredentialsFromEnvironment()
{
    return { qEnvironmentVariable("WHILEFRESH_LEXMOUNT_API_KEY"), qEnvironmentVariable("WHILEFRESH_LEXMOUNT_PROJECT_ID") };
}

LexmountRecipeSource::LexmountRecipeSource(CredentialsFn credentials, Config config, QObject *parent)
    : RecipeSource(parent), m_credentials(std::move(credentials)), m_config(std::move(config)), m_network(new QNetworkAccessManager(this))
{
}

LexmountRecipeSource::~LexmountRecipeSource()
{
    for (LexmountJob *job : findChildren<LexmountJob *>()) {
        job->detachOwner();
    }
}

bool LexmountRecipeSource::isConfigured() const
{
    return m_credentials && m_credentials().isComplete();
}

quint64 LexmountRecipeSource::search(const QString &keyword, int page)
{
    return start(true, keyword, {}, page);
}

quint64 LexmountRecipeSource::detail(const QString &id)
{
    return start(false, {}, id, 1);
}

quint64 LexmountRecipeSource::start(bool isSearch, const QString &keyword, const QString &id, int page)
{
    const quint64 requestId = nextRequestId();
    const LexmountCredentials credentials = m_credentials ? m_credentials() : LexmountCredentials();
    static const QRegularExpression digits(QStringLiteral("^\\d{1,20}$"));
    std::optional<RecipeError> early;
    if (!credentials.isComplete()) {
        early = RecipeError { RecipeErrorKind::NotConfigured, QStringLiteral("no Lexmount credentials") };
    } else if (isSearch && (keyword.trimmed().isEmpty() || keyword.size() > 200 || page < 1)) {
        early = RecipeError { RecipeErrorKind::InvalidQuery, QStringLiteral("keyword must be 1-200 characters") };
    } else if (!isSearch && !digits.match(id).hasMatch()) {
        early = RecipeError { RecipeErrorKind::InvalidQuery, QStringLiteral("id must contain 1-20 digits") };
    }
    if (early) {
        QTimer::singleShot(0, this, [this, requestId, early] { emit failed(requestId, *early); });
        return requestId;
    }
    ContractQuery query;
    query.operation = isSearch ? RecipeDocument::Operation::Search : RecipeDocument::Operation::Detail;
    query.keyword = keyword.trimmed();
    query.id = id;
    query.page = page;
    query.pageSize = m_config.pageSize;
    auto *job = new LexmountJob(this, m_network, m_config, credentials, requestId, query);
    m_jobs.insert(requestId, job);
    // Begin from the event loop so the request id is returned before anything can be emitted.
    QTimer::singleShot(0, job, [job] { job->begin(); });
    return requestId;
}

void LexmountRecipeSource::cancel(quint64 requestId)
{
    if (LexmountJob *job = m_jobs.take(requestId)) {
        job->cancel();
    }
}

void LexmountRecipeSource::jobFinished(quint64 id, bool isSearch, const std::optional<RecipeError> &error, const QByteArray &yaml,
    bool silent, LexmountJob *)
{
    m_jobs.remove(id);
    if (silent) {
        return;
    }
    if (error) {
        emit failed(id, *error);
        return;
    }
    const ParsedRecipeDocument parsed = parseXiachufangYaml(yaml);
    if (!parsed.ok()) {
        emit failed(id, parsed.error);
        return;
    }
    emit documentReady(id, yaml);
    if (isSearch) {
        emit searchSucceeded(id, parsed.document->search);
    } else {
        emit detailSucceeded(id, *parsed.document->recipe);
    }
}

}

#include "lexmount-recipe-source.moc"

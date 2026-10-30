#pragma once

#include "recipes/recipe-source.h"

#include <QHash>
#include <QStringList>

#include <functional>

class QProcess;

namespace whilefresh::recipes {

// Lexmount credentials for the source's cloud-browser provider; both empty means direct access.
struct LexmountCredentials {
    QString apiKey;
    QString projectId;
};

// Runs the external recipe-source CLI (recipe-sources-xiachufang-api, desktop only, built with
// WHILEFRESH_XIACHUFANG_ADAPTER) and parses the YAML it prints. The credentials are asked for
// when a request starts and are neither stored nor logged; the CLI takes them as arguments, so
// they are visible to other processes of the same user while it runs.
class ProcessRecipeSource : public RecipeSource
{
    Q_OBJECT

public:
    struct Config {
        // e.g. program "uv", leadingArguments {"run", "--project", <submodule dir>, "python",
        // "-m", "xiachufang_source"}; the query arguments are appended.
        QString program;
        QStringList leadingArguments;
        int timeoutMs = 20000;
    };

    using CredentialsFn = std::function<LexmountCredentials()>;

    explicit ProcessRecipeSource(Config config, CredentialsFn credentials = {}, QObject *parent = nullptr);

    quint64 search(const QString &keyword, int page = 1) override;
    quint64 detail(const QString &id) override;
    void cancel(quint64 requestId) override;
    bool isConfigured() const override { return !m_config.program.isEmpty(); }

private:
    quint64 start(const QStringList &queryArguments, bool isSearch);
    void finish(quint64 id, bool isSearch, int exitCode, bool crashed, const QByteArray &output);

    Config m_config;
    CredentialsFn m_credentials;
    QHash<quint64, QProcess *> m_running;
};

}

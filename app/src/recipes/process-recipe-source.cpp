#include "recipes/process-recipe-source.h"

#include "recipes/recipes-log.h"
#include "recipes/xiachufang-yaml-parser.h"

#include <QProcess>
#include <QTimer>

namespace whilefresh::recipes {

ProcessRecipeSource::ProcessRecipeSource(Config config, CredentialsFn credentials, QObject *parent)
    : RecipeSource(parent), m_config(std::move(config)), m_credentials(std::move(credentials))
{
}

quint64 ProcessRecipeSource::search(const QString &keyword, int page)
{
    return start({ QStringLiteral("--operation"), QStringLiteral("search"), QStringLiteral("--keyword"), keyword,
                     QStringLiteral("--page"), QString::number(page) },
        true);
}

quint64 ProcessRecipeSource::detail(const QString &id)
{
    return start({ QStringLiteral("--operation"), QStringLiteral("detail"), QStringLiteral("--id"), id }, false);
}

quint64 ProcessRecipeSource::start(const QStringList &queryArguments, bool isSearch)
{
    const quint64 id = nextRequestId();
    if (!isConfigured()) {
        QTimer::singleShot(0, this, [this, id] {
            emit failed(id, { RecipeErrorKind::NotConfigured, QStringLiteral("no recipe source program configured") });
        });
        return id;
    }

    QStringList arguments = m_config.leadingArguments + queryArguments;
    const LexmountCredentials credentials = m_credentials ? m_credentials() : LexmountCredentials();
    if (credentials.apiKey.isEmpty() || credentials.projectId.isEmpty()) {
        arguments << QStringLiteral("--provider") << QStringLiteral("direct");
    } else {
        arguments << QStringLiteral("--provider") << QStringLiteral("auto") << QStringLiteral("--api-key")
                  << credentials.apiKey << QStringLiteral("--project-id") << credentials.projectId;
        if (!isSearch) {
            arguments << QStringLiteral("--search-type") << QStringLiteral("detail");
        }
    }

    auto *process = new QProcess(this);
    m_running.insert(id, process);
    connect(process, &QProcess::finished, this, [this, id, isSearch, process](int exitCode, QProcess::ExitStatus status) {
        const QByteArray output = process->readAllStandardOutput();
        finish(id, isSearch, exitCode, status == QProcess::CrashExit, output);
    });
    connect(process, &QProcess::errorOccurred, this, [this, id, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) {
            return;
        }
        if (m_running.take(id)) {
            emit failed(id, { RecipeErrorKind::NotConfigured, QStringLiteral("recipe source program failed to start") });
        }
        process->deleteLater();
    });
    auto *timer = new QTimer(process);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, [this, id, process] {
        if (m_running.take(id)) {
            process->disconnect(this);
            process->kill();
            process->deleteLater();
            emit failed(id, { RecipeErrorKind::Timeout, QStringLiteral("recipe source did not answer in time") });
        }
    });
    timer->start(m_config.timeoutMs);

    process->setProcessChannelMode(QProcess::SeparateChannels);
    process->start(m_config.program, arguments);
    qCInfo(lcRecipes, "recipe request %llu started (%s)", id, isSearch ? "search" : "detail");
    return id;
}

void ProcessRecipeSource::cancel(quint64 requestId)
{
    if (QProcess *process = m_running.take(requestId)) {
        process->disconnect(this);
        process->kill();
        process->deleteLater();
    }
}

void ProcessRecipeSource::finish(quint64 id, bool isSearch, int exitCode, bool crashed, const QByteArray &output)
{
    QProcess *process = m_running.take(id);
    if (!process) {
        return;
    }
    process->deleteLater();

    // The CLI prints the document even for source errors and exits 1 then.
    const ParsedRecipeDocument parsed = parseXiachufangYaml(output);
    qCInfo(lcRecipes, "recipe request %llu finished: exit=%d ok=%d", id, exitCode, int(parsed.ok()));
    if (crashed || (!parsed.ok() && output.isEmpty())) {
        emit failed(id, { RecipeErrorKind::Internal, QStringLiteral("recipe source process failed, exit code %1").arg(exitCode) });
    } else if (!parsed.ok()) {
        emit failed(id, parsed.error);
    } else if (isSearch != (parsed.document->operation == RecipeDocument::Operation::Search)) {
        emit failed(id, { RecipeErrorKind::BadResponse, QStringLiteral("unexpected operation in document") });
    } else if (isSearch) {
        emit searchSucceeded(id, parsed.document->search);
    } else {
        emit detailSucceeded(id, *parsed.document->recipe);
    }
}

}

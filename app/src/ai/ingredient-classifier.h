#pragma once

#include "ai/chat-client.h"
#include "core/catalog.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>

#include <memory>

namespace whilefresh::ai {

inline constexpr int kClassifierMaxNames = 80;
inline constexpr int kClassifierIdleTimeoutMs = 30000;
inline constexpr int kClassifierDeadlineMs = 60000;

struct Classification {
    // Sent raw name -> catalog standard name. Names the model left as "none" are absent.
    QHash<QString, QString> canonical;
    // Answers that are not in the catalog; dropped, counted here for logs and tests.
    int rejected = 0;
    TokenUsage usage;
    qint64 elapsedMs = 0;
};

// Puts product names the vocabulary could not resolve locally into the vocabulary, all in one
// text-only JSON-mode call through the shared ChatClient (PRD v6: 词表外的名称批量请求 DeepSeek
// 归类). Every answer is checked against the catalog; anything else is dropped, so the result
// only ever holds catalog names. At most kClassifierMaxNames names are sent; the rest stay
// unresolved.
class IngredientClassifier : public QObject
{
    Q_OBJECT

public:
    // The client must outlive this object.
    IngredientClassifier(ChatClient *client, std::shared_ptr<const core::IngredientCatalog> catalog,
                         QObject *parent = nullptr);

    bool isConfigured() const { return m_client->isConfigured(); }
    // Returns a request id; exactly one of classified() / failed() follows unless cancelled.
    quint64 classify(const QStringList &rawNames);
    void cancel(quint64 requestId);

    static ChatRequest buildRequest(const QStringList &rawNames, const core::IngredientCatalog &catalog);
    // `answer` is the JSON object of the reply; `rawNames` what was sent, in order.
    static Classification parseAnswer(const QJsonObject &answer, const QStringList &rawNames,
                                      const core::IngredientCatalog &catalog);
    // What buildRequest() actually sends: trimmed, whitespace collapsed, deduplicated, capped.
    static QStringList namesToSend(const QStringList &rawNames);

signals:
    void classified(quint64 requestId, const whilefresh::ai::Classification &result);
    void failed(quint64 requestId, const whilefresh::ai::ChatError &error);

private:
    void chatSucceeded(quint64 id, const ChatResult &result);
    void chatFailed(quint64 id, const ChatError &error);

    ChatClient *m_client;
    std::shared_ptr<const core::IngredientCatalog> m_catalog;
    QHash<quint64, QStringList> m_pending; // id -> names sent
};

}

Q_DECLARE_METATYPE(whilefresh::ai::Classification)

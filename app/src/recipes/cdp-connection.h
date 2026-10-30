#pragma once

#include "recipes/websocket-codec.h"

#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QUrl>

class QSslSocket;

namespace whilefresh::recipes {

// A Chrome DevTools Protocol connection over a WebSocket, fully asynchronous. Replies are matched
// by command id; the owner decides what a late reply means (see LexmountRecipeSource).
class CdpConnection : public QObject
{
    Q_OBJECT

public:
    explicit CdpConnection(QObject *parent = nullptr);
    ~CdpConnection() override;

    void open(const QUrl &url);
    quint64 send(const QString &method, const QJsonObject &params = {}, const QString &sessionId = {});
    // Closing handshake, then the socket is dropped after a short grace period.
    void close();
    bool isOpen() const { return m_state == State::Open; }

signals:
    void opened();
    void response(quint64 id, bool ok, const QJsonObject &result, const QString &errorMessage);
    void event(const QString &method, const QJsonObject &params, const QString &sessionId);
    // Exactly once per connection; reason is empty after our own close().
    void closed(const QString &reason);

private:
    enum class State { Idle, Connecting, Handshaking, Open, Closing, Closed };

    void onConnected();
    void onReadyRead();
    void handleMessage(const WebSocketCodec::Message &message);
    void finish(const QString &reason);

    QSslSocket *m_socket = nullptr;
    WebSocketCodec m_codec;
    QByteArray m_key;
    QByteArray m_handshake;
    QUrl m_url;
    State m_state = State::Idle;
    quint64 m_lastId = 0;
    QTimer m_closeTimer;
};

}

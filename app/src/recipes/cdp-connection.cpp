#include "recipes/cdp-connection.h"

#include "recipes/recipes-log.h"

#include <QJsonDocument>
#include <QSslSocket>

namespace whilefresh::recipes {

CdpConnection::CdpConnection(QObject *parent) : QObject(parent)
{
    m_closeTimer.setSingleShot(true);
    m_closeTimer.setInterval(1000);
    connect(&m_closeTimer, &QTimer::timeout, this, [this] { finish(QString()); });
}

CdpConnection::~CdpConnection()
{
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->abort();
    }
}

void CdpConnection::open(const QUrl &url)
{
    if (m_state != State::Idle) {
        return;
    }
    const bool secure = url.scheme() == QLatin1String("wss");
    if (!secure && url.scheme() != QLatin1String("ws")) {
        finish(QStringLiteral("websocket URL must be ws or wss"));
        return;
    }
    m_url = url;
    m_state = State::Connecting;
    m_socket = new QSslSocket(this);
    connect(m_socket, &QSslSocket::readyRead, this, &CdpConnection::onReadyRead);
    connect(m_socket, &QSslSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        finish(QStringLiteral("websocket connection error"));
    });
    connect(m_socket, &QSslSocket::disconnected, this, [this] {
        finish(m_state == State::Closing ? QString() : QStringLiteral("websocket closed by peer"));
    });
    const int port = url.port(secure ? 443 : 80);
    if (secure) {
        connect(m_socket, &QSslSocket::encrypted, this, &CdpConnection::onConnected);
        m_socket->connectToHostEncrypted(url.host(), quint16(port));
    } else {
        connect(m_socket, &QSslSocket::connected, this, &CdpConnection::onConnected);
        m_socket->connectToHost(url.host(), quint16(port));
    }
}

void CdpConnection::onConnected()
{
    m_state = State::Handshaking;
    m_key = WebSocketCodec::newKey();
    QString path = m_url.path(QUrl::FullyEncoded);
    if (path.isEmpty()) {
        path = QStringLiteral("/");
    }
    if (m_url.hasQuery()) {
        path += QLatin1Char('?') + m_url.query(QUrl::FullyEncoded);
    }
    const bool secure = m_url.scheme() == QLatin1String("wss");
    m_socket->write(WebSocketCodec::upgradeRequest(m_url.host(), m_url.port(secure ? 443 : 80), path, m_key));
}

void CdpConnection::onReadyRead()
{
    if (m_state == State::Closed) {
        return;
    }
    QByteArray data = m_socket->readAll();
    if (m_state == State::Handshaking) {
        m_handshake.append(data);
        const qsizetype used = m_codec.checkUpgradeResponse(m_handshake, m_key);
        if (used < 0) {
            finish(m_codec.error());
            return;
        }
        if (used == 0) {
            return;
        }
        data = m_handshake.mid(used);
        m_handshake.clear();
        m_state = State::Open;
        emit opened();
        if (m_state != State::Open) {
            return;
        }
    }
    if (!m_codec.feed(data)) {
        finish(m_codec.error());
        return;
    }
    while (m_state == State::Open || m_state == State::Closing) {
        const std::optional<WebSocketCodec::Message> message = m_codec.next();
        if (!message) {
            break;
        }
        handleMessage(*message);
    }
}

void CdpConnection::handleMessage(const WebSocketCodec::Message &message)
{
    using Opcode = WebSocketCodec::Opcode;
    switch (message.opcode) {
    case Opcode::Ping:
        m_socket->write(WebSocketCodec::encode(Opcode::Pong, message.payload));
        break;
    case Opcode::Close:
        if (m_state == State::Open) {
            m_socket->write(WebSocketCodec::encode(Opcode::Close, message.payload.left(2)));
            m_socket->flush();
            finish(QStringLiteral("websocket closed by peer"));
        } else {
            finish(QString());
        }
        break;
    case Opcode::Text: {
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(message.payload, &parseError);
        if (!doc.isObject()) {
            break;
        }
        const QJsonObject object = doc.object();
        if (object.contains(QLatin1String("id"))) {
            const QJsonObject error = object.value(QLatin1String("error")).toObject();
            emit response(quint64(object.value(QLatin1String("id")).toDouble()), error.isEmpty(),
                object.value(QLatin1String("result")).toObject(), error.value(QLatin1String("message")).toString());
        } else if (object.contains(QLatin1String("method"))) {
            emit event(object.value(QLatin1String("method")).toString(), object.value(QLatin1String("params")).toObject(),
                object.value(QLatin1String("sessionId")).toString());
        }
        break;
    }
    default:
        break;
    }
}

quint64 CdpConnection::send(const QString &method, const QJsonObject &params, const QString &sessionId)
{
    const quint64 id = ++m_lastId;
    if (m_state != State::Open) {
        return id;
    }
    QJsonObject message { { QStringLiteral("id"), double(id) }, { QStringLiteral("method"), method } };
    if (!params.isEmpty()) {
        message.insert(QStringLiteral("params"), params);
    }
    if (!sessionId.isEmpty()) {
        message.insert(QStringLiteral("sessionId"), sessionId);
    }
    m_socket->write(WebSocketCodec::encode(WebSocketCodec::Opcode::Text, QJsonDocument(message).toJson(QJsonDocument::Compact)));
    return id;
}

void CdpConnection::close()
{
    if (m_state == State::Closed || m_state == State::Closing) {
        return;
    }
    if (m_state != State::Open) {
        finish(QString());
        return;
    }
    m_state = State::Closing;
    m_socket->write(WebSocketCodec::encodeClose(1000));
    m_closeTimer.start();
}

void CdpConnection::finish(const QString &reason)
{
    if (m_state == State::Closed) {
        return;
    }
    m_state = State::Closed;
    m_closeTimer.stop();
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    emit closed(reason);
}

}

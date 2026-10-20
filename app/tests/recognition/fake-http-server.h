#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>

// Minimal local HTTP/1.1 server for client tests. Serves scripted replies in order, one per
// request, and records what it received.
class FakeHttpServer : public QObject
{
    Q_OBJECT

public:
    struct Reply {
        int status = 200;
        QByteArray body;
        bool hang = false; // accept the request, never answer
    };
    struct Received {
        QByteArray requestLine;
        QHash<QByteArray, QByteArray> headers; // lower-case names
        QByteArray body;
    };

    explicit FakeHttpServer(QObject *parent = nullptr) : QObject(parent)
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *socket = m_server.nextPendingConnection()) {
                m_sockets.append(socket);
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] { onReadyRead(socket); });
            }
        });
    }

    bool listen() { return m_server.listen(QHostAddress::LocalHost); }
    quint16 port() const { return m_server.serverPort(); }
    QUrl url() const { return QUrl(QStringLiteral("http://127.0.0.1:%1/chat/completions").arg(port())); }
    void enqueue(const Reply &reply) { m_replies.append(reply); }
    const QList<Received> &received() const { return m_received; }

private:
    void onReadyRead(QTcpSocket *socket)
    {
        QByteArray &buffer = m_buffers[socket];
        buffer += socket->readAll();
        const qsizetype headerEnd = buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0)
            return;

        Received request;
        const QList<QByteArray> lines = buffer.left(headerEnd).split('\n');
        request.requestLine = lines.first().trimmed();
        for (qsizetype i = 1; i < lines.size(); ++i) {
            const qsizetype colon = lines.at(i).indexOf(':');
            if (colon > 0)
                request.headers.insert(lines.at(i).left(colon).trimmed().toLower(), lines.at(i).mid(colon + 1).trimmed());
        }
        const qsizetype length = request.headers.value("content-length").toLongLong();
        if (buffer.size() < headerEnd + 4 + length)
            return;
        request.body = buffer.mid(headerEnd + 4, length);
        m_buffers.remove(socket);
        m_received.append(request);

        const Reply reply = m_replies.isEmpty() ? Reply{ 500, "no reply scripted", false } : m_replies.takeFirst();
        if (reply.hang)
            return;
        const QByteArray head = "HTTP/1.1 " + QByteArray::number(reply.status) + " X\r\n"
            "Content-Type: application/json\r\nConnection: close\r\nContent-Length: "
            + QByteArray::number(reply.body.size()) + "\r\n\r\n";
        socket->write(head + reply.body);
        socket->disconnectFromHost();
    }

    QTcpServer m_server;
    QList<QTcpSocket *> m_sockets;
    QHash<QTcpSocket *, QByteArray> m_buffers;
    QList<Reply> m_replies;
    QList<Received> m_received;
};

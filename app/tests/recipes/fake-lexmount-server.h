#pragma once

#include "recipes/websocket-codec.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>

// A local stand-in for the Lexmount HTTP API plus the DevTools WebSocket it hands out, speaking
// just enough of both for LexmountRecipeSource. Every HTTP request and every DevTools command is
// recorded in order.
class FakeLexmountServer : public QObject
{
public:
    struct HttpRecord {
        QByteArray method;
        QByteArray path;
        QByteArray body;
        QHash<QByteArray, QByteArray> headers; // lower-case names
    };

    explicit FakeLexmountServer(QObject *parent = nullptr) : QObject(parent)
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *socket = m_server.nextPendingConnection()) {
                m_connections.insert(socket, {});
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] { onReadyRead(socket); });
                connect(socket, &QTcpSocket::disconnected, this, [this, socket] {
                    m_connections.remove(socket);
                    socket->deleteLater();
                });
            }
        });
        m_server.listen(QHostAddress::LocalHost, 0);
    }

    ~FakeLexmountServer() override
    {
        // The sockets are children of m_server and outlive m_connections; stop their slots first.
        for (QTcpSocket *socket : m_connections.keys()) {
            socket->disconnect(this);
        }
    }

    QUrl baseUrl() const { return QUrl(QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort())); }

    // Behaviour switches, set before the request under test.
    int createStatus = 200;
    int versionStatus = 200;
    int sessionPollsBeforeActive = 1;
    int staleStatePolls = 2;          // polls that still see about:blank after Page.navigate
    bool stallNavigate = false;       // never answer Page.navigate
    bool pingBeforeReplies = false;   // send a 200-byte ping before each CDP reply
    QJsonObject searchScrape;
    QJsonObject detailScrape;
    QString landedPath;               // path the state script reports; empty means the requested one

    QList<HttpRecord> http;
    QStringList commands;             // DevTools methods in arrival order, "Runtime.evaluate:state|scrape"
    int deletes() const
    {
        int n = 0;
        for (const HttpRecord &r : http) {
            n += r.method == "DELETE";
        }
        return n;
    }
    int created = 0;

private:
    struct Connection {
        QByteArray buffer;
        bool websocket = false;
        QString navigatedUrl;
        int staleLeft = 0;
        bool navigated = false;
    };

    static QByteArray statusLine(int code)
    {
        return QByteArray("HTTP/1.1 ") + QByteArray::number(code) + (code == 200 ? " OK" : " Error") + "\r\n";
    }

    void reply(QTcpSocket *socket, int code, const QJsonObject &object)
    {
        const QByteArray body = QJsonDocument(object).toJson(QJsonDocument::Compact);
        socket->write(statusLine(code) + "Content-Type: application/json\r\nContent-Length: " + QByteArray::number(body.size())
            + "\r\nConnection: close\r\n\r\n" + body);
        socket->disconnectFromHost();
    }

    void onReadyRead(QTcpSocket *socket)
    {
        Connection &conn = m_connections[socket];
        conn.buffer.append(socket->readAll());
        if (conn.websocket) {
            readFrames(socket, conn);
            return;
        }
        const qsizetype headerEnd = conn.buffer.indexOf("\r\n\r\n");
        if (headerEnd < 0) {
            return;
        }
        const QList<QByteArray> lines = conn.buffer.left(headerEnd).split('\n');
        const QList<QByteArray> requestLine = lines.first().trimmed().split(' ');
        HttpRecord record;
        record.method = requestLine.value(0);
        record.path = requestLine.value(1);
        for (qsizetype i = 1; i < lines.size(); ++i) {
            const qsizetype colon = lines.at(i).indexOf(':');
            if (colon > 0) {
                record.headers.insert(lines.at(i).left(colon).trimmed().toLower(), lines.at(i).mid(colon + 1).trimmed());
            }
        }
        const qsizetype length = record.headers.value("content-length", "0").toInt();
        if (conn.buffer.size() < headerEnd + 4 + length) {
            return;
        }
        record.body = conn.buffer.mid(headerEnd + 4, length);
        conn.buffer.remove(0, headerEnd + 4 + length);

        if (record.headers.value("upgrade").toLower() == "websocket") {
            conn.websocket = true;
            const QByteArray accept = whilefresh::recipes::WebSocketCodec::acceptFor(record.headers.value("sec-websocket-key"));
            socket->write("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: "
                + accept + "\r\n\r\n");
            return;
        }
        http.append(record);
        route(socket, record);
    }

    void route(QTcpSocket *socket, const HttpRecord &record)
    {
        const QJsonObject body = QJsonDocument::fromJson(record.body).object();
        if (record.method == "POST" && record.path == "/instance/v2") {
            if (createStatus != 200) {
                reply(socket, createStatus, {});
                return;
            }
            const QString id = QStringLiteral("s%1").arg(++created);
            m_polls.insert(id, 0);
            reply(socket, 200, { { QStringLiteral("session_id"), id } });
        } else if (record.method == "POST" && record.path == "/instance/session") {
            const QString id = body.value(QLatin1String("session_id")).toString();
            const bool active = ++m_polls[id] > sessionPollsBeforeActive;
            reply(socket, 200, { { QStringLiteral("status"), active ? QStringLiteral("active") : QStringLiteral("creating") } });
        } else if (record.method == "GET" && record.path.startsWith("/json/version")) {
            if (versionStatus != 200) {
                reply(socket, versionStatus, {});
                return;
            }
            reply(socket, 200,
                { { QStringLiteral("webSocketDebuggerUrl"),
                    QStringLiteral("ws://127.0.0.1:%1/devtools/browser/x").arg(m_server.serverPort()) } });
        } else if (record.method == "DELETE" && record.path == "/instance") {
            reply(socket, 200, {});
        } else {
            reply(socket, 404, {});
        }
    }

    static QByteArray serverFrame(quint8 opcode, const QByteArray &payload)
    {
        QByteArray frame;
        frame.append(char(0x80 | opcode));
        if (payload.size() < 126) {
            frame.append(char(payload.size()));
        } else if (payload.size() <= 0xFFFF) {
            frame.append(char(126));
            frame.append(char(payload.size() >> 8));
            frame.append(char(payload.size() & 0xFF));
        } else {
            frame.append(char(127));
            for (int shift = 56; shift >= 0; shift -= 8) {
                frame.append(char((quint64(payload.size()) >> shift) & 0xFF));
            }
        }
        return frame + payload;
    }

    void sendJson(QTcpSocket *socket, const QJsonObject &object)
    {
        if (pingBeforeReplies) {
            socket->write(serverFrame(0x9, QByteArray(200, 'p')));
        }
        socket->write(serverFrame(0x1, QJsonDocument(object).toJson(QJsonDocument::Compact)));
    }

    void readFrames(QTcpSocket *socket, Connection &conn)
    {
        for (;;) {
            if (conn.buffer.size() < 2) {
                return;
            }
            const quint8 opcode = quint8(conn.buffer.at(0)) & 0x0F;
            quint64 length = quint8(conn.buffer.at(1)) & 0x7F;
            qsizetype header = 2;
            if (length == 126) {
                if (conn.buffer.size() < 4) {
                    return;
                }
                length = (quint64(quint8(conn.buffer.at(2))) << 8) | quint8(conn.buffer.at(3));
                header = 4;
            } else if (length == 127) {
                if (conn.buffer.size() < 10) {
                    return;
                }
                length = 0;
                for (int i = 0; i < 8; ++i) {
                    length = (length << 8) | quint8(conn.buffer.at(2 + i));
                }
                header = 10;
            }
            if (conn.buffer.size() < header + 4 + qsizetype(length)) {
                return;
            }
            const QByteArray mask = conn.buffer.mid(header, 4);
            QByteArray payload = conn.buffer.mid(header + 4, qsizetype(length));
            for (qsizetype i = 0; i < payload.size(); ++i) {
                payload[i] = char(payload[i] ^ mask[i % 4]);
            }
            conn.buffer.remove(0, header + 4 + qsizetype(length));
            if (opcode == 0x8) {
                socket->write(serverFrame(0x8, payload.left(2)));
                socket->disconnectFromHost();
                return;
            }
            if (opcode == 0x1) {
                onCommand(socket, conn, QJsonDocument::fromJson(payload).object());
            }
        }
    }

    void onCommand(QTcpSocket *socket, Connection &conn, const QJsonObject &message)
    {
        const double id = message.value(QLatin1String("id")).toDouble();
        const QString method = message.value(QLatin1String("method")).toString();
        const QJsonObject params = message.value(QLatin1String("params")).toObject();
        QJsonObject result;
        QString recorded = method;
        if (method == QLatin1String("Target.getTargets")) {
            result = { { QStringLiteral("targetInfos"),
                QJsonArray { QJsonObject { { QStringLiteral("type"), QStringLiteral("page") }, { QStringLiteral("targetId"), QStringLiteral("t1") } } } } };
        } else if (method == QLatin1String("Target.attachToTarget")) {
            result = { { QStringLiteral("sessionId"), QStringLiteral("cdp1") } };
        } else if (method == QLatin1String("Page.navigate")) {
            conn.navigatedUrl = params.value(QLatin1String("url")).toString();
            conn.navigated = true;
            conn.staleLeft = staleStatePolls;
            if (stallNavigate) {
                commands.append(recorded);
                return;
            }
            result = { { QStringLiteral("frameId"), QStringLiteral("f1") } };
        } else if (method == QLatin1String("Runtime.evaluate")) {
            const QString expression = params.value(QLatin1String("expression")).toString();
            if (expression.contains(QLatin1String("location.href"))) {
                recorded += QStringLiteral(":state");
                QJsonObject state;
                if (!conn.navigated || conn.staleLeft-- > 0) {
                    state = { { QStringLiteral("href"), QStringLiteral("about:blank") }, { QStringLiteral("ready"), QStringLiteral("complete") },
                        { QStringLiteral("title"), QString() } };
                } else {
                    const QUrl url(conn.navigatedUrl);
                    const QString path = landedPath.isEmpty() ? url.path() : landedPath;
                    state = { { QStringLiteral("href"), QStringLiteral("https://www.xiachufang.com") + path },
                        { QStringLiteral("ready"), QStringLiteral("complete") }, { QStringLiteral("title"), QStringLiteral("下厨房") } };
                }
                result = { { QStringLiteral("result"), QJsonObject { { QStringLiteral("type"), QStringLiteral("object") }, { QStringLiteral("value"), state } } } };
            } else {
                recorded += QStringLiteral(":scrape");
                const bool search = expression.contains(QLatin1String("const kind = 'search'"));
                result = { { QStringLiteral("result"),
                    QJsonObject { { QStringLiteral("type"), QStringLiteral("object") },
                        { QStringLiteral("value"), search ? searchScrape : detailScrape } } } };
            }
        }
        commands.append(recorded);
        QJsonObject reply { { QStringLiteral("id"), id }, { QStringLiteral("result"), result } };
        if (message.contains(QLatin1String("sessionId"))) {
            reply.insert(QStringLiteral("sessionId"), message.value(QLatin1String("sessionId")));
        }
        sendJson(socket, reply);
    }

    QTcpServer m_server;
    QHash<QTcpSocket *, Connection> m_connections;
    QHash<QString, int> m_polls;
};

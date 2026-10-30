#include "recipes/websocket-codec.h"

#include <QCryptographicHash>
#include <QRandomGenerator>

#include <cstring>

namespace whilefresh::recipes {

namespace {

constexpr char kGuid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

}

QByteArray WebSocketCodec::newKey()
{
    QByteArray raw(16, Qt::Uninitialized);
    for (int i = 0; i < 16; i += 4) {
        const quint32 value = QRandomGenerator::system()->generate();
        std::memcpy(raw.data() + i, &value, 4);
    }
    return raw.toBase64();
}

QByteArray WebSocketCodec::acceptFor(const QByteArray &key)
{
    return QCryptographicHash::hash(key + kGuid, QCryptographicHash::Sha1).toBase64();
}

QByteArray WebSocketCodec::upgradeRequest(const QString &host, int port, const QString &pathAndQuery, const QByteArray &key)
{
    QByteArray request = "GET " + pathAndQuery.toUtf8() + " HTTP/1.1\r\nHost: " + host.toUtf8() + ':'
        + QByteArray::number(port) + "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: " + key
        + "\r\nSec-WebSocket-Version: 13\r\n\r\n";
    return request;
}

qsizetype WebSocketCodec::checkUpgradeResponse(const QByteArray &response, const QByteArray &key)
{
    const qsizetype end = response.indexOf("\r\n\r\n");
    if (end < 0) {
        if (response.size() > 16 * 1024) {
            m_error = QStringLiteral("websocket handshake response too large");
            return -1;
        }
        return 0;
    }
    const QList<QByteArray> lines = response.left(end).split('\n');
    if (lines.isEmpty() || !lines.first().startsWith("HTTP/1.1 101")) {
        m_error = QStringLiteral("websocket upgrade refused: %1").arg(QString::fromLatin1(lines.value(0).trimmed().left(40)));
        return -1;
    }
    QByteArray accept;
    bool upgrade = false;
    bool connection = false;
    for (qsizetype i = 1; i < lines.size(); ++i) {
        const qsizetype colon = lines.at(i).indexOf(':');
        if (colon < 0) {
            continue;
        }
        const QByteArray name = lines.at(i).left(colon).trimmed().toLower();
        const QByteArray value = lines.at(i).mid(colon + 1).trimmed();
        if (name == "sec-websocket-accept") {
            accept = value;
        } else if (name == "upgrade") {
            upgrade = value.toLower() == "websocket";
        } else if (name == "connection") {
            connection = value.toLower().contains("upgrade");
        }
    }
    if (!upgrade || !connection || accept != acceptFor(key)) {
        m_error = QStringLiteral("websocket handshake validation failed");
        return -1;
    }
    return end + 4;
}

QByteArray WebSocketCodec::encode(Opcode opcode, const QByteArray &payload)
{
    QByteArray frame;
    frame.append(char(0x80 | quint8(opcode)));
    const quint64 length = quint64(payload.size());
    if (length < 126) {
        frame.append(char(0x80 | length));
    } else if (length <= 0xFFFF) {
        frame.append(char(0x80 | 126));
        frame.append(char(length >> 8));
        frame.append(char(length & 0xFF));
    } else {
        frame.append(char(0x80 | 127));
        for (int shift = 56; shift >= 0; shift -= 8) {
            frame.append(char((length >> shift) & 0xFF));
        }
    }
    const quint32 maskValue = QRandomGenerator::system()->generate();
    char mask[4];
    std::memcpy(mask, &maskValue, 4);
    frame.append(mask, 4);
    const qsizetype start = frame.size();
    frame.append(payload);
    for (qsizetype i = 0; i < payload.size(); ++i) {
        frame[start + i] = char(frame[start + i] ^ mask[i % 4]);
    }
    return frame;
}

QByteArray WebSocketCodec::encodeClose(quint16 code, const QByteArray &reason)
{
    QByteArray payload;
    payload.append(char(code >> 8));
    payload.append(char(code & 0xFF));
    payload.append(reason);
    return encode(Opcode::Close, payload);
}

bool WebSocketCodec::fail(const QString &why)
{
    m_error = why;
    m_buffer.clear();
    return false;
}

bool WebSocketCodec::feed(const QByteArray &data)
{
    if (!m_error.isEmpty()) {
        return false;
    }
    m_buffer.append(data);
    return parse();
}

std::optional<WebSocketCodec::Message> WebSocketCodec::next()
{
    if (m_ready.isEmpty()) {
        return std::nullopt;
    }
    return m_ready.takeFirst();
}

bool WebSocketCodec::parse()
{
    for (;;) {
        if (m_buffer.size() < 2) {
            return true;
        }
        const quint8 b0 = quint8(m_buffer.at(0));
        const quint8 b1 = quint8(m_buffer.at(1));
        const bool fin = b0 & 0x80;
        const Opcode opcode = Opcode(b0 & 0x0F);
        if (b0 & 0x70) {
            return fail(QStringLiteral("websocket reserved bits set"));
        }
        if (b1 & 0x80) {
            return fail(QStringLiteral("websocket server frame is masked"));
        }
        quint64 length = b1 & 0x7F;
        qsizetype header = 2;
        if (length == 126) {
            if (m_buffer.size() < 4) {
                return true;
            }
            length = (quint64(quint8(m_buffer.at(2))) << 8) | quint8(m_buffer.at(3));
            header = 4;
        } else if (length == 127) {
            if (m_buffer.size() < 10) {
                return true;
            }
            length = 0;
            for (int i = 0; i < 8; ++i) {
                length = (length << 8) | quint8(m_buffer.at(2 + i));
            }
            header = 10;
        }
        if (length > quint64(m_max)) {
            return fail(QStringLiteral("websocket frame exceeds the size cap"));
        }
        if (m_buffer.size() < header + qsizetype(length)) {
            return true;
        }
        const QByteArray payload = m_buffer.mid(header, qsizetype(length));
        m_buffer.remove(0, header + qsizetype(length));

        switch (opcode) {
        case Opcode::Continuation:
            if (!m_inFragment) {
                return fail(QStringLiteral("websocket continuation without a start"));
            }
            if (m_fragment.size() + payload.size() > m_max) {
                return fail(QStringLiteral("websocket message exceeds the size cap"));
            }
            m_fragment.append(payload);
            if (fin) {
                m_ready.append({ m_fragmentOpcode, m_fragment });
                m_fragment.clear();
                m_inFragment = false;
            }
            break;
        case Opcode::Text:
        case Opcode::Binary:
            if (m_inFragment) {
                return fail(QStringLiteral("websocket data frame inside a fragmented message"));
            }
            if (fin) {
                m_ready.append({ opcode, payload });
            } else {
                m_inFragment = true;
                m_fragmentOpcode = opcode;
                m_fragment = payload;
            }
            break;
        case Opcode::Close:
        case Opcode::Ping:
        case Opcode::Pong:
            if (!fin) {
                return fail(QStringLiteral("websocket control frame is fragmented"));
            }
            m_ready.append({ opcode, payload });
            break;
        default:
            return fail(QStringLiteral("websocket unknown opcode"));
        }
    }
}

}

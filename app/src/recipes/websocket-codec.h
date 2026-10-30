#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

#include <optional>

namespace whilefresh::recipes {

// RFC 6455 framing for a client, without any I/O. Qt6::WebSockets is not installed in the
// Android kits, so the CDP connection brings its own small implementation.
class WebSocketCodec
{
public:
    enum class Opcode : quint8 { Continuation = 0x0, Text = 0x1, Binary = 0x2, Close = 0x8, Ping = 0x9, Pong = 0xA };

    struct Message {
        Opcode opcode = Opcode::Text; // Continuation never appears: fragments are reassembled
        QByteArray payload;
    };

    // maxMessageBytes caps a reassembled message and any single frame.
    explicit WebSocketCodec(qsizetype maxMessageBytes = 4 * 1024 * 1024) : m_max(maxMessageBytes) {}

    static QByteArray newKey();
    static QByteArray acceptFor(const QByteArray &key);
    static QByteArray upgradeRequest(const QString &host, int port, const QString &pathAndQuery, const QByteArray &key);
    // Whole-response check: status 101, Upgrade/Connection headers and a matching
    // Sec-WebSocket-Accept. Returns the number of bytes consumed through the blank line, 0 when
    // the response is still incomplete, -1 when it is invalid (error() says why).
    qsizetype checkUpgradeResponse(const QByteArray &response, const QByteArray &key);

    // A single final frame, masked as clients must; any payload size.
    static QByteArray encode(Opcode opcode, const QByteArray &payload);
    static QByteArray encodeClose(quint16 code, const QByteArray &reason = {});

    // Appends bytes from the wire. False on a protocol violation; the codec is then dead.
    bool feed(const QByteArray &data);
    std::optional<Message> next();
    const QString &error() const { return m_error; }

private:
    bool parse();
    bool fail(const QString &why);

    qsizetype m_max;
    QByteArray m_buffer;
    QList<Message> m_ready;
    Opcode m_fragmentOpcode = Opcode::Text;
    QByteArray m_fragment;
    bool m_inFragment = false;
    QString m_error;
};

}

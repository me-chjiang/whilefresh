#include "recipes/websocket-codec.h"

#include <QTest>

using namespace whilefresh::recipes;
using Opcode = WebSocketCodec::Opcode;

namespace {

// Server frames are unmasked.
QByteArray frame(quint8 firstByte, const QByteArray &payload)
{
    QByteArray out;
    out.append(char(firstByte));
    if (payload.size() < 126) {
        out.append(char(payload.size()));
    } else if (payload.size() <= 0xFFFF) {
        out.append(char(126));
        out.append(char(payload.size() >> 8));
        out.append(char(payload.size() & 0xFF));
    } else {
        out.append(char(127));
        for (int shift = 56; shift >= 0; shift -= 8) {
            out.append(char((quint64(payload.size()) >> shift) & 0xFF));
        }
    }
    return out + payload;
}

}

class WebSocketCodecTest : public QObject
{
    Q_OBJECT

private slots:
    void acceptMatchesTheRfcExample()
    {
        QCOMPARE(WebSocketCodec::acceptFor("dGhlIHNhbXBsZSBub25jZQ=="), QByteArray("s3pPLMBiTxaQ9kYGzzhZRbK+xOo="));
    }

    void upgradeResponseIsChecked()
    {
        const QByteArray key = WebSocketCodec::newKey();
        const QByteArray good = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: "
            + WebSocketCodec::acceptFor(key) + "\r\n\r\n";
        WebSocketCodec codec;
        QCOMPARE(codec.checkUpgradeResponse(good.left(20), key), 0);
        QCOMPARE(codec.checkUpgradeResponse(good + "extra", key), good.size());

        WebSocketCodec wrongAccept;
        const QByteArray bad = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: AAAA\r\n\r\n";
        QCOMPARE(wrongAccept.checkUpgradeResponse(bad, key), -1);

        WebSocketCodec refused;
        QCOMPARE(refused.checkUpgradeResponse("HTTP/1.1 403 Forbidden\r\n\r\n", key), -1);
        QVERIFY(refused.error().contains(QStringLiteral("403")));
    }

    void clientFramesAreMaskedWithAllLengthForms_data()
    {
        QTest::addColumn<int>("size");
        QTest::newRow("short") << 5;
        QTest::newRow("125") << 125;
        QTest::newRow("126") << 126;
        QTest::newRow("16 bit") << 70000 % 65536 + 1000;
        QTest::newRow("65535") << 65535;
        QTest::newRow("64 bit") << 65536;
    }

    void clientFramesAreMaskedWithAllLengthForms()
    {
        QFETCH(int, size);
        const QByteArray payload(size, 'x');
        const QByteArray out = WebSocketCodec::encode(Opcode::Text, payload);
        QCOMPARE(quint8(out.at(0)), quint8(0x81));
        QVERIFY(quint8(out.at(1)) & 0x80);
        const quint8 marker = quint8(out.at(1)) & 0x7F;
        qsizetype header = 2;
        quint64 length = marker;
        if (marker == 126) {
            length = (quint64(quint8(out.at(2))) << 8) | quint8(out.at(3));
            header = 4;
        } else if (marker == 127) {
            length = 0;
            for (int i = 0; i < 8; ++i) {
                length = (length << 8) | quint8(out.at(2 + i));
            }
            header = 10;
        }
        QCOMPARE(length, quint64(size));
        QCOMPARE(out.size(), header + 4 + size);
        QByteArray unmasked = out.mid(header + 4);
        for (qsizetype i = 0; i < unmasked.size(); ++i) {
            unmasked[i] = char(unmasked[i] ^ out.at(header + i % 4));
        }
        QCOMPARE(unmasked, payload);
    }

    void serverFramesAreDecodedAcrossChunks()
    {
        const QByteArray medium(300, 'm');
        const QByteArray large(70000, 'l');
        const QByteArray wire = frame(0x81, "hi") + frame(0x82, medium) + frame(0x81, large);
        WebSocketCodec codec;
        for (qsizetype i = 0; i < wire.size(); i += 7) {
            QVERIFY(codec.feed(wire.mid(i, 7)));
        }
        const auto a = codec.next();
        const auto b = codec.next();
        const auto c = codec.next();
        QVERIFY(a && b && c);
        QCOMPARE(a->opcode, Opcode::Text);
        QCOMPARE(a->payload, QByteArray("hi"));
        QCOMPARE(b->opcode, Opcode::Binary);
        QCOMPARE(b->payload, medium);
        QCOMPARE(c->payload, large);
        QVERIFY(!codec.next());
    }

    void fragmentsAreReassembledAndControlFramesMayInterleave()
    {
        WebSocketCodec codec;
        QVERIFY(codec.feed(frame(0x01, "Hel") + frame(0x89, "ping") + frame(0x00, "lo, ") + frame(0x80, "world")));
        const auto ping = codec.next();
        const auto text = codec.next();
        QVERIFY(ping && text);
        QCOMPARE(ping->opcode, Opcode::Ping);
        QCOMPARE(ping->payload, QByteArray("ping"));
        QCOMPARE(text->opcode, Opcode::Text);
        QCOMPARE(text->payload, QByteArray("Hello, world"));
    }

    void pingsOfAnySizeAreReported()
    {
        WebSocketCodec codec;
        const QByteArray big(1000, 'p');
        QVERIFY(codec.feed(frame(0x89, big) + frame(0x88, QByteArray("\x03\xe8", 2))));
        const auto ping = codec.next();
        QVERIFY(ping);
        QCOMPARE(ping->payload, big);
        const auto close = codec.next();
        QVERIFY(close);
        QCOMPARE(close->opcode, Opcode::Close);
        QCOMPARE(close->payload.size(), 2);
    }

    void violationsKillTheCodec_data()
    {
        QTest::addColumn<QByteArray>("wire");
        QTest::newRow("masked server frame") << QByteArray("\x81\x81xxxxy", 7);
        QTest::newRow("reserved bit") << frame(0xC1, "x");
        QTest::newRow("orphan continuation") << frame(0x80, "x");
        QTest::newRow("data inside fragments") << (frame(0x01, "a") + frame(0x81, "b"));
        QTest::newRow("fragmented control") << frame(0x09, "x");
        QTest::newRow("unknown opcode") << frame(0x83, "x");
    }

    void violationsKillTheCodec()
    {
        QFETCH(QByteArray, wire);
        WebSocketCodec codec;
        QVERIFY(!codec.feed(wire));
        QVERIFY(!codec.error().isEmpty());
        QVERIFY(!codec.feed("more"));
    }

    void sizeCapIsEnforced()
    {
        WebSocketCodec codec(1000);
        QVERIFY(!codec.feed(frame(0x81, QByteArray(1001, 'x'))));

        WebSocketCodec fragments(1000);
        QVERIFY(fragments.feed(frame(0x01, QByteArray(600, 'x'))));
        QVERIFY(!fragments.feed(frame(0x80, QByteArray(600, 'x'))));

        WebSocketCodec header(1000); // a declared 4 GiB frame is refused before its payload arrives
        QVERIFY(!header.feed(QByteArray("\x81\x7f\x00\x00\x00\x01\x00\x00\x00\x00", 10)));
    }
};

QTEST_APPLESS_MAIN(WebSocketCodecTest)
#include "websocket-codec-test.moc"

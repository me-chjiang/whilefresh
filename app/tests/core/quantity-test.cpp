#include "core/quantity.h"

#include <QTest>

using namespace whilefresh::core;

class QuantityTest : public QObject
{
    Q_OBJECT

private slots:
    void parse_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<double>("value");
        QTest::addColumn<QString>("unit");

        QTest::newRow("grams") << "500g" << 500.0 << "g";
        QTest::newRow("kilograms") << "1.5kg" << 1500.0 << "g";
        QTest::newRow("upper case") << "500G" << 500.0 << "g";
        QTest::newRow("chinese grams with prefix") << "约350克" << 350.0 << "g";
        QTest::newRow("gongjin") << "2公斤" << 2000.0 << "g";
        QTest::newRow("jin") << "2斤" << 1000.0 << "g";
        QTest::newRow("half a jin") << "半斤" << 250.0 << "g";
        QTest::newRow("full width") << "５００ｇ" << 500.0 << "g";
        QTest::newRow("label before") << "净含量：1kg" << 1000.0 << "g";
        QTest::newRow("ml times count") << "500ml×2" << 1000.0 << "ml";
        QTest::newRow("ml star packs") << "250ml*6盒" << 1500.0 << "ml";
        QTest::newRow("count times grams") << "2袋x500g" << 1000.0 << "g";
        QTest::newRow("litre") << "1L" << 1000.0 << "ml";
        QTest::newRow("sheng") << "1.5升" << 1500.0 << "ml";
        QTest::newRow("boxes") << "2盒" << 2.0 << "盒";
        QTest::newRow("pieces") << "3个" << 3.0 << "个";
        QTest::newRow("bunch") << "1把" << 1.0 << "把";
        QTest::newRow("eggs") << "10枚" << 10.0 << "枚";
        QTest::newRow("chinese numeral") << "两盒" << 2.0 << "盒";
        QTest::newRow("measure wins over count") << "500g（约3个）" << 500.0 << "g";
        QTest::newRow("unrelated number not a multiplier") << "500g 2件起售" << 500.0 << "g";
        QTest::newRow("empty") << "" << 0.0 << "";
        QTest::newRow("no number") << "适量" << 0.0 << "";
        QTest::newRow("bare number") << "500" << 0.0 << "";
        QTest::newRow("unknown unit") << "3人份" << 0.0 << "";
    }

    void parse()
    {
        QFETCH(QString, text);
        QFETCH(double, value);
        QFETCH(QString, unit);
        const Amount amount = parseAmount(text);
        QCOMPARE(amount.value, value);
        QCOMPARE(amount.unit, unit);
        QCOMPARE(amount.isKnown(), value > 0);
    }

    void totalMultipliesByPackages()
    {
        QCOMPARE(totalAmount(QStringLiteral("500g"), 2), (Amount{ 1000, QStringLiteral("g") }));
        QCOMPARE(totalAmount(QStringLiteral("3个"), 2), (Amount{ 6, QStringLiteral("个") }));
        QCOMPARE(totalAmount(QStringLiteral("适量"), 2), Amount{});
        QCOMPARE(totalAmount(QStringLiteral("1把"), 0), (Amount{ 1, QStringLiteral("把") }));
    }
};

QTEST_GUILESS_MAIN(QuantityTest)
#include "quantity-test.moc"

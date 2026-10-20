#include "core/app-version.h"

#include <QTest>
#include <QVersionNumber>

class AppVersionTest : public QObject
{
    Q_OBJECT

private slots:
    void matchesProjectVersion()
    {
        QCOMPARE(whilefresh::core::appVersion(), QStringLiteral(WHILEFRESH_EXPECTED_VERSION));
    }

    void isThreePartVersion()
    {
        const QVersionNumber version = QVersionNumber::fromString(whilefresh::core::appVersion());
        QCOMPARE(version.segmentCount(), 3);
    }
};

QTEST_APPLESS_MAIN(AppVersionTest)

#include "app-version-test.moc"

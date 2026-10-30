#include "platform/desktop/file-key-vault.h"

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

using namespace whilefresh::platform;

class FileKeyVaultTest : public QObject
{
    Q_OBJECT

private slots:
    void roundTripSurvivesANewInstance()
    {
        QTemporaryDir dir;
        const QString root = dir.filePath(QStringLiteral("data"));
        {
            FileKeyVault vault(root);
            QVERIFY(vault.value(QString::fromLatin1(kSecretDeepSeekKey)).isEmpty());
            QVERIFY(vault.setValue(QString::fromLatin1(kSecretDeepSeekKey), QStringLiteral("sk-test-1")));
            QVERIFY(vault.setValue(QString::fromLatin1(kSecretLexmountProjectId), QStringLiteral("proj-中")));
        }
        FileKeyVault again(root);
        QCOMPARE(again.value(QString::fromLatin1(kSecretDeepSeekKey)), QStringLiteral("sk-test-1"));
        QCOMPARE(again.value(QString::fromLatin1(kSecretLexmountProjectId)), QStringLiteral("proj-中"));
        QVERIFY(again.value(QString::fromLatin1(kSecretLexmountKey)).isEmpty());
    }

    void overwriteAndRemove()
    {
        QTemporaryDir dir;
        FileKeyVault vault(dir.path());
        const QString name = QString::fromLatin1(kSecretLexmountKey);
        QVERIFY(vault.setValue(name, QStringLiteral("a")));
        QVERIFY(vault.setValue(name, QStringLiteral("b")));
        QCOMPARE(vault.value(name), QStringLiteral("b"));
        QVERIFY(vault.remove(name));
        QVERIFY(vault.value(name).isEmpty());
        QVERIFY(vault.remove(name)); // already gone
        QVERIFY(vault.setValue(name, QStringLiteral("c")));
        QVERIFY(vault.setValue(name, QString())); // empty removes
        QVERIFY(vault.value(name).isEmpty());
    }

    void fileIsOwnerOnly()
    {
        QTemporaryDir dir;
        FileKeyVault vault(dir.filePath(QStringLiteral("data")));
        QVERIFY(vault.setValue(QString::fromLatin1(kSecretDeepSeekKey), QStringLiteral("sk-test")));
        const QFileInfo info(vault.filePath());
        QVERIFY(info.exists());
        const auto others = QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup
            | QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther;
        QVERIFY(info.permissions() & QFileDevice::ReadOwner);
        QVERIFY(!(info.permissions() & others));
        QVERIFY(!(QFileInfo(info.absolutePath()).permissions() & others));
    }

    void badNamesAreRefused()
    {
        QTemporaryDir dir;
        FileKeyVault vault(dir.path());
        QVERIFY(!vault.setValue(QStringLiteral("../x"), QStringLiteral("v")));
        QVERIFY(!vault.setValue(QString(), QStringLiteral("v")));
        QVERIFY(vault.value(QStringLiteral("../x")).isEmpty());
    }
};

QTEST_APPLESS_MAIN(FileKeyVaultTest)

#include "file-key-vault-test.moc"

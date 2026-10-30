#include "data/builtin-data.h"

#include <QDir>
#include <QFile>
#include <QMutex>

namespace whilefresh::data {

namespace {

std::optional<QByteArray> readFile(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("cannot read %1: %2").arg(path, file.errorString());
        return std::nullopt;
    }
    return file.readAll();
}

}

std::shared_ptr<const BuiltinData> BuiltinData::load(const QString &directory, QString *error)
{
    const QDir dir(directory);
    auto data = std::make_shared<BuiltinData>();

    const auto vocabulary = readFile(dir.filePath(QStringLiteral("vocabulary.json")), error);
    if (!vocabulary)
        return nullptr;
    auto catalog = core::IngredientCatalog::fromJson(*vocabulary, error);
    if (!catalog)
        return nullptr;
    data->catalog = std::make_shared<const core::IngredientCatalog>(std::move(*catalog));

    // The reference table is optional: without it every lookup falls through to the AI estimate.
    const QString shelfLifePath = dir.filePath(QStringLiteral("shelf-life.json"));
    core::ShelfLifeTable table;
    if (QFile::exists(shelfLifePath)) {
        const auto json = readFile(shelfLifePath, error);
        if (!json)
            return nullptr;
        auto parsed = core::ShelfLifeTable::fromJson(*json, error);
        if (!parsed)
            return nullptr;
        table = std::move(*parsed);
    }
    data->shelfLife = std::make_shared<const core::ShelfLifeTable>(std::move(table));
    return data;
}

std::shared_ptr<const BuiltinData> BuiltinData::builtIn(QString *error)
{
    static QMutex mutex;
    static std::shared_ptr<const BuiltinData> cached;
    const QMutexLocker locker(&mutex);
    if (!cached)
        cached = load(builtInDirectory(), error);
    return cached;
}

}

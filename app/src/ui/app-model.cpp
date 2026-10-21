#include "ui/app-model.h"

#include "core/date-text.h"
#include "platform/import-files.h"
#include "recognition/api-key.h"
#include "recognition/deepseek-recognizer.h"
#include "recognition/demo-recognizer.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace whilefresh::ui {

namespace {

namespace rec = recognition;

QString dateText(const QDate &date) { return date.isValid() ? date.toString(Qt::ISODate) : QString(); }

std::optional<core::Storage> storageFromName(const QString &name)
{
    for (auto storage : core::allStorages) {
        if (core::storageDisplayName(storage) == name)
            return storage;
    }
    return std::nullopt;
}

QString freshnessName(core::Freshness freshness)
{
    switch (freshness) {
    case core::Freshness::Fresh: return QStringLiteral("fresh");
    case core::Freshness::NearExpiry: return QStringLiteral("nearing");
    case core::Freshness::BeyondEstimate: return QStringLiteral("overdue");
    default: return QStringLiteral("unknown");
    }
}

QString sourceName(core::ShelfLifeSource source)
{
    switch (source) {
    case core::ShelfLifeSource::Package: return QStringLiteral("package");
    case core::ShelfLifeSource::Manual:
    case core::ShelfLifeSource::LocalRule: return QStringLiteral("user");
    case core::ShelfLifeSource::Reference: return QStringLiteral("reference");
    case core::ShelfLifeSource::AiEstimate: return QStringLiteral("ai");
    default: return QStringLiteral("none");
    }
}

void setDate(core::RecordedDate &recorded, const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        recorded = {};
        return;
    }
    const QDate date = QDate::fromString(trimmed, Qt::ISODate);
    if (date.isValid() && date != recorded.date) {
        recorded.date = date;
        recorded.origin = core::DateOrigin::User;
        recorded.text = trimmed;
    }
}

}

AppModel::AppModel(platform::PlatformServices *platform, std::shared_ptr<const data::BuiltinData> builtin,
                   QObject *parent)
    : QObject(parent), m_platform(platform), m_builtin(std::move(builtin))
{
    m_dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(m_dataDir);
    m_real = openSession(QDir(m_dataDir).filePath(QStringLiteral("whilefresh.db")), false);
    if (!m_real) {
        m_startupNotice = QStringLiteral("无法打开本机数据库，数据无法保存");
        return;
    }
    if (!m_real->store->retryFileCleanup())
        m_startupNotice = QStringLiteral("部分导入图片尚未清理，稍后会自动重试");
    m_active = m_real.get();
    if (m_real->kitchen->demoMode()) {
        m_demo = openSession(demoPath(), true);
        if (m_demo) {
            seedDemo(*m_demo);
            m_active = m_demo.get();
        }
    }
    m_selected = kitchen().defaultSelection();

    if (m_platform) {
        connect(m_platform, &platform::PlatformServices::importReceived, this,
                [this](const platform::ImportPayload &payload) { m_active->import->receive(payload); });
        connect(m_platform, &platform::PlatformServices::importFailed, this,
                [this](platform::ImportPayload::Origin, const QString &message) { emit notice(message); });
        connect(m_platform, &platform::PlatformServices::importCancelled, this,
                [this](platform::ImportPayload::Origin) {
                    if (m_importState == QLatin1String("idle")) {
                        m_consented = false;
                        emit pickCancelled();
                    }
                });
    }
}

AppModel::~AppModel() = default;

bool AppModel::recognitionConfigured() const { return rec::isDeepSeekApiKeyConfigured(); }

QString AppModel::demoPath() const
{
    const QString path = QDir(m_dataDir).filePath(QStringLiteral("demo.db"));
    QFile::remove(path);
    return path;
}

std::unique_ptr<AppModel::Session> AppModel::openSession(const QString &dbPath, bool demo)
{
    auto session = std::make_unique<Session>();
    session->demo = demo;
    session->database = std::make_unique<data::Database>(dbPath);
    QString error;
    if (!session->database->open(&error) || !session->database->migrate(&error)) {
        qWarning("database %s: %s", qUtf8Printable(dbPath), qUtf8Printable(error));
        return nullptr;
    }
    session->store = std::make_unique<data::Store>(*session->database, m_clock, platform::importsDirectory());
    session->kitchen = std::make_unique<KitchenService>(*session->store, m_builtin, m_clock);
    if (demo)
        session->recognizer = std::make_unique<rec::DemoRecognizer>();
    else
        session->recognizer = std::make_unique<rec::DeepSeekRecognizer>();
    session->import = std::make_unique<ImportService>(*session->store, m_builtin, m_clock, *session->recognizer, demo);

    Session *raw = session.get();
    auto *k = raw->kitchen.get();
    auto *imp = raw->import.get();
    connect(k, &KitchenService::changed, this, [this, raw] { if (raw == m_active) invalidate(); });
    connect(k, &KitchenService::aboutToDeleteAllData, imp, &ImportService::discard);
    connect(imp, &ImportService::prepared, this, [this, raw] {
        if (raw != m_active) return;
        if (m_consented) {
            m_consented = false;
            setImportState(QStringLiteral("recognizing"));
            raw->import->start(true, false);
            if (m_importState == QLatin1String("recognizing") && !raw->import->busy())
                setImportState(QStringLiteral("prepared"));
        } else {
            setImportState(QStringLiteral("prepared"));
        }
    });
    connect(imp, &ImportService::duplicateDetected, this, [this, raw] {
        if (raw == m_active) setImportState(QStringLiteral("duplicate"));
    });
    connect(imp, &ImportService::draftReady, this, [this, raw] {
        if (raw == m_active) setImportState(QStringLiteral("draft"));
    });
    connect(imp, &ImportService::draftChanged, this, [this, raw] { if (raw == m_active) emit importChanged(); });
    connect(imp, &ImportService::failed, this, [this, raw](const rec::RecognitionError &error) {
        if (raw != m_active) return;
        if (m_importState == QLatin1String("recognizing") || m_importState == QLatin1String("prepared"))
            setImportState(QStringLiteral("failed"), error.userMessage);
        else
            emit notice(error.userMessage);
    });
    connect(imp, &ImportService::committed, this, [this, raw] {
        if (raw != m_active) return;
        setImportState(QStringLiteral("idle"));
        invalidate();
        emit importCommitted();
    });
    return session;
}

void AppModel::seedDemo(Session &session)
{
    struct Seed { const char *name; const char *quantity; core::Storage storage; };
    const Seed seeds[] = {
        { "五花肉", "500g", core::Storage::Fridge }, { "鸡蛋", "10枚", core::Storage::Fridge },
        { "茭白", "2根", core::Storage::Fridge }, { "白蘑菇", "200g", core::Storage::Fridge },
        { "长尖椒", "150g", core::Storage::Fridge }, { "蓝莓", "125g", core::Storage::Fridge },
        { "冻虾仁", "300g", core::Storage::Freezer }, { "山药", "1根", core::Storage::Room },
    };
    for (const auto &seed : seeds) {
        core::FridgeItem item;
        item.displayName = QString::fromUtf8(seed.name);
        item.rawName = item.displayName;
        item.canonicalName = m_builtin->catalog->resolve(item.displayName);
        if (item.canonicalName.isEmpty())
            item.canonicalName = rec::uncategorizedName();
        item.category = m_builtin->catalog->categoryOf(item.canonicalName);
        item.quantityText = QString::fromUtf8(seed.quantity);
        item.storage = seed.storage;
        item.purchaseDate.date = m_clock.today().addDays(-3);
        item.purchaseDate.origin = core::DateOrigin::User;
        session.kitchen->saveItem(item);
    }
}

void AppModel::activate(Session *session)
{
    m_active = session;
    m_consented = false;
    m_pendingProposals.clear();
    m_selected = kitchen().defaultSelection();
    setImportState(QStringLiteral("idle"));
    invalidate();
    emit demoModeChanged();
}

void AppModel::invalidate()
{
    m_items.reset();
    m_recipes.reset();
    m_clearRecipes.reset();
    m_shopping.reset();
    QSet<qint64> existing;
    for (const auto &item : kitchen().fridge())
        existing.insert(item.id);
    m_selected.intersect(existing);
    emit changed();
}

void AppModel::setImportState(const QString &state, const QString &error)
{
    m_importState = state;
    m_importError = error;
    emit importChanged();
}

QVariantMap AppModel::itemMap(const core::FridgeItem &item) const
{
    const auto state = kitchen().freshness(item);
    QString days;
    if (item.shelfLifeSource == core::ShelfLifeSource::Package && item.expiryDate.isValid())
        days = item.expiryDate.toString(QStringLiteral("MM-dd")) + QStringLiteral(" 到期");
    else if (state.freshness == core::Freshness::BeyondEstimate)
        days = QStringLiteral("请查看后决定");
    else if (state.remainingDays)
        days = QStringLiteral("约剩 %1 天").arg(*state.remainingDays);
    else
        days = QStringLiteral("保质期未知");
    const QString name = item.displayName.isEmpty() ? item.canonicalName : item.displayName;
    return {
        { "id", item.id }, { "name", name }, { "quantity", item.quantityText },
        { "category", item.category }, { "location", core::storageDisplayName(item.storage) },
        { "freshness", freshnessName(state.freshness) }, { "source", sourceName(item.shelfLifeSource) },
        { "days", days }, { "first", item.useFirst },
        { "bulk", kitchen().pantryRules().isBulk(item.canonicalName) },
        { "purchasedAt", dateText(item.purchaseDate.date) }, { "storedAt", dateText(item.entryDate) },
        { "producedAt", dateText(item.productionDate.date) },
        { "packageExpiresAt", dateText(item.packageExpiry.date) },
        { "shelfDays", item.shelfLifeDays ? QString::number(*item.shelfLifeDays) : QString() },
    };
}

QVariantMap AppModel::summaryMap(const core::RecipeMatch &match) const
{
    const auto &r = *match.recipe;
    return { { "id", r.id }, { "name", r.name }, { "difficulty", r.difficulty.value_or(1) },
             { "haveCount", match.haveCount }, { "missingCount", match.missingCount }, { "sourceUrl", r.sourceUrl } };
}

QVariantList AppModel::items()
{
    if (!m_items) {
        QVariantList list;
        for (const auto &item : kitchen().fridge())
            list.append(itemMap(item));
        m_items = list;
    }
    return *m_items;
}

int AppModel::nearCount()
{
    int count = 0;
    for (const auto &value : items())
        count += value.toMap().value(QStringLiteral("freshness")) == QLatin1String("nearing");
    return count;
}

QVariantList AppModel::selectedIds() const
{
    QVariantList list;
    for (qint64 id : m_selected)
        list.append(id);
    return list;
}

void AppModel::toggleSelected(qint64 id)
{
    if (!m_selected.remove(id))
        m_selected.insert(id);
    m_clearRecipes.reset();
    emit changed();
}

QVariantList AppModel::recipes()
{
    if (!m_recipes) {
        QVariantList list;
        for (const auto &match : kitchen().recipes())
            list.append(summaryMap(match));
        m_recipes = list;
    }
    return *m_recipes;
}

QVariantList AppModel::clearRecipes()
{
    if (!m_clearRecipes) {
        QVariantList list;
        if (!m_selected.isEmpty()) {
            for (const auto &match : kitchen().clearFridge(m_selected))
                list.append(summaryMap(match));
        }
        m_clearRecipes = list;
    }
    return *m_clearRecipes;
}

QVariantMap AppModel::recipeDetail(const QString &id) const
{
    const auto match = kitchen().recipe(id);
    if (!match || !match->recipe)
        return {};
    QVariantMap map = summaryMap(*match);
    QVariantList ingredients;
    for (const auto &group : match->groups)
        ingredients.append(QVariantMap { { "raw", group.ingredientRaw.isEmpty() ? group.group.join(QStringLiteral(" 或 ")) : group.ingredientRaw },
                                         { "has", group.satisfied() }, { "optional", group.optional } });
    map.insert(QStringLiteral("ingredients"), ingredients);
    map.insert(QStringLiteral("steps"), match->recipe->steps);
    return map;
}

int AppModel::addMissing(const QString &recipeId) { return kitchen().addMissingToShopping(recipeId); }

QVariantList AppModel::shopping()
{
    if (!m_shopping) {
        QVariantList list;
        for (const auto &entry : kitchen().shopping())
            list.append(QVariantMap { { "id", entry.id }, { "name", entry.displayName() }, { "category", entry.category } });
        m_shopping = list;
    }
    return *m_shopping;
}

void AppModel::removeShopping(qint64 id) { kitchen().removeShopping(id); }
QString AppModel::shoppingText() const { return kitchen().shoppingText(); }

void AppModel::useItem(qint64 id) { kitchen().useItem(id); }
void AppModel::markAbsent(qint64 id) { kitchen().markAbsent(id); }
void AppModel::toggleFirst(qint64 id, bool enabled) { kitchen().setUseFirst(id, enabled); }

void AppModel::removeItems(const QVariantList &ids)
{
    for (const auto &id : ids)
        kitchen().markAbsent(id.toLongLong());
}

void AppModel::applyEdits(core::FridgeItem &item, const QVariantMap &edits, bool editEntry) const
{
    if (edits.contains(QStringLiteral("name"))) {
        const QString name = edits.value(QStringLiteral("name")).toString().trimmed();
        if (!name.isEmpty() && name != item.displayName) {
            QString canonical = m_builtin->catalog->resolve(name);
            if (canonical.isEmpty())
                canonical = rec::uncategorizedName();
            item.displayName = name;
            if (canonical != item.canonicalName) {
                item.canonicalName = canonical;
                item.aiDays = {};
                item.referenceConditionsConfirmed = false;
            }
            item.category = m_builtin->catalog->categoryOf(canonical);
        }
    }
    if (edits.contains(QStringLiteral("quantity")))
        item.quantityText = edits.value(QStringLiteral("quantity")).toString().trimmed();
    if (const auto storage = storageFromName(edits.value(QStringLiteral("location")).toString());
        storage && *storage != item.storage) {
        item.storage = *storage;
        item.manualDays.reset();
        item.referenceConditionsConfirmed = false;
    }
    if (edits.contains(QStringLiteral("shelfDays"))) {
        const QString text = edits.value(QStringLiteral("shelfDays")).toString().trimmed();
        const int days = text.toInt();
        if (text.isEmpty())
            item.manualDays.reset();
        else if (days > 0 && std::optional<int>(days) != item.shelfLifeDays && std::optional<int>(days) != item.manualDays)
            item.manualDays = days;
    }
    if (edits.contains(QStringLiteral("purchasedAt")))
        setDate(item.purchaseDate, edits.value(QStringLiteral("purchasedAt")).toString());
    if (edits.contains(QStringLiteral("producedAt")))
        setDate(item.productionDate, edits.value(QStringLiteral("producedAt")).toString());
    if (edits.contains(QStringLiteral("packageExpiresAt"))) {
        const auto before = item.packageExpiry.date;
        setDate(item.packageExpiry, edits.value(QStringLiteral("packageExpiresAt")).toString());
        if (item.packageExpiry.date != before)
            item.manualDays.reset();
    }
    if (editEntry && edits.contains(QStringLiteral("storedAt"))) {
        const QDate date = QDate::fromString(edits.value(QStringLiteral("storedAt")).toString().trimmed(), Qt::ISODate);
        if (date.isValid())
            item.entryDate = date;
    }
}

QVariantMap AppModel::saveItem(qint64 id, const QVariantMap &edits)
{
    core::FridgeItem before;
    for (const auto &item : kitchen().fridge()) {
        if (item.id == id)
            before = item;
    }
    core::FridgeItem item = before;
    applyEdits(item, edits, true);
    QString error;
    const bool ok = kitchen().saveItem(item, &error);
    m_pendingProposals.clear();
    QString question;
    if (ok && id != 0) {
        m_pendingProposals = kitchen().proposalsForEdit(before, item);
        if (!m_pendingProposals.isEmpty())
            question = m_pendingProposals.first().question;
    }
    return { { "ok", ok }, { "error", error }, { "question", question } };
}

QVariantMap AppModel::addManual(const QString &name, const QString &quantity, const QString &location)
{
    core::FridgeItem item;
    item.rawName = name.trimmed();
    item.canonicalName = rec::uncategorizedName();
    applyEdits(item, { { "name", name }, { "quantity", quantity }, { "location", location } }, false);
    QString error;
    const bool ok = kitchen().saveItem(item, &error);
    return { { "ok", ok }, { "error", error } };
}

void AppModel::confirmRememberedRules()
{
    for (const auto &proposal : std::as_const(m_pendingProposals))
        kitchen().confirmRule(proposal);
    m_pendingProposals.clear();
}

void AppModel::dismissRememberedRules() { m_pendingProposals.clear(); }

QVariantList AppModel::pantry() const
{
    QVariantList list;
    const auto rules = kitchen().pantryRules();
    for (const auto &name : rules.namesAtHome())
        list.append(QVariantMap { { "name", name }, { "category", m_builtin->catalog->categoryOf(name) },
                                  { "bulk", rules.isBulk(name) } });
    return list;
}

void AppModel::addPantry(const QString &name)
{
    QString resolved = m_builtin->catalog->resolve(name);
    if (resolved.isEmpty())
        resolved = name.trimmed();
    auto override = kitchen().pantryRules().overrideFor(resolved);
    override.name = resolved;
    override.pantry = true;
    override.out = false;
    kitchen().setPantryOverride(override);
}

void AppModel::removePantry(const QString &name)
{
    auto override = kitchen().pantryRules().overrideFor(name);
    override.name = name;
    override.pantry = false;
    kitchen().setPantryOverride(override);
}

QVariantList AppModel::suggestions() const
{
    QVariantList list;
    for (const auto &s : kitchen().suggestions())
        list.append(QVariantMap { { "id", s.id }, { "name", s.name }, { "note", s.message } });
    return list;
}

void AppModel::decideSuggestion(qint64 id, bool accept) { kitchen().decideSuggestion(id, accept); }

QVariantList AppModel::rules() const
{
    QVariantList list;
    for (const auto &r : kitchen().shelfLifeRules())
        list.append(QVariantMap { { "name", r.name }, { "location", core::storageDisplayName(r.storage) },
                                  { "days", QString::number(r.days) }, { "kind", "shelf" }, { "source", "你的设置" } });
    for (const auto &r : kitchen().storageRules())
        list.append(QVariantMap { { "name", r.name }, { "location", core::storageDisplayName(r.storage) },
                                  { "days", QString() }, { "kind", "storage" }, { "source", "你的设置" } });
    return list;
}

void AppModel::saveRule(const QString &name, const QString &oldLocation, const QString &location, const QString &days)
{
    const auto storage = storageFromName(location);
    const auto old = storageFromName(oldLocation);
    if (!storage)
        return;
    const int n = days.trimmed().toInt();
    if (n > 0) {
        if (old && *old != *storage)
            kitchen().deleteShelfLifeRule(name, *old);
        if (const auto proposal = core::proposeShelfLifeRule(name, *storage, n))
            kitchen().confirmRule(*proposal);
    } else if (const auto proposal = core::proposeStorageRule(name, *storage)) {
        kitchen().confirmRule(*proposal);
    }
}

void AppModel::deleteRule(const QString &name, const QString &location, const QString &kind)
{
    if (kind == QLatin1String("storage"))
        kitchen().deleteStorageRule(name);
    else if (const auto storage = storageFromName(location))
        kitchen().deleteShelfLifeRule(name, *storage);
}

QVariantList AppModel::imports() const
{
    QVariantList list;
    for (const auto &r : kitchen().imports()) {
        QString kind = r.inputKind == QLatin1String("receipt") ? QStringLiteral("小票")
            : r.inputKind == QLatin1String("order-text") ? QStringLiteral("订单文字")
            : r.inputKind == QLatin1String("screenshot") ? QStringLiteral("截图") : QStringLiteral("导入");
        if (!r.images.isEmpty())
            kind += QStringLiteral(" · %1 张").arg(r.images.size());
        if (r.isDemo)
            kind = QStringLiteral("演示 · ") + kind;
        list.append(QVariantMap { { "id", r.id }, { "name", kind },
                                  { "date", r.createdAt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd")) } });
    }
    return list;
}

void AppModel::deleteImport(qint64 id)
{
    if (!kitchen().deleteImport(id))
        emit notice(QStringLiteral("部分图片暂未清理，稍后会自动重试"));
}

void AppModel::deleteAllData()
{
    if (!kitchen().deleteAllData())
        emit notice(QStringLiteral("部分图片暂未清理，稍后会自动重试"));
    m_selected.clear();
    invalidate();
}

void AppModel::setDemoMode(bool enabled)
{
    if (!m_real || enabled == demoMode())
        return;
    m_active->import->discard();
    if (enabled) {
        m_demo.reset();
        m_demo = openSession(demoPath(), true);
        if (!m_demo) {
            emit notice(QStringLiteral("无法创建演示数据"));
            return;
        }
        seedDemo(*m_demo);
        m_real->kitchen->setDemoMode(true);
        activate(m_demo.get());
    } else {
        m_real->kitchen->setDemoMode(false);
        activate(m_real.get());
        m_demo.reset();
    }
}

void AppModel::beginPick(bool camera)
{
    m_consented = true;
    if (camera)
        m_platform->captureReceipt();
    else
        m_platform->pickImages();
}

void AppModel::beginText(const QString &text)
{
    platform::ImportPayload payload;
    payload.kind = platform::ImportPayload::Kind::Text;
    payload.origin = platform::ImportPayload::Origin::Share;
    payload.text = text;
    m_consented = true;
    if (!m_active->import->receive(payload))
        m_consented = false;
}

void AppModel::startImport(bool allowDuplicate)
{
    setImportState(QStringLiteral("recognizing"));
    if (!m_active->import->start(true, allowDuplicate) && m_importState == QLatin1String("recognizing"))
        setImportState(QStringLiteral("prepared"));
}

void AppModel::cancelImport()
{
    m_consented = false;
    if (!m_active->import->discard())
        emit notice(QStringLiteral("部分图片暂未清理，稍后会自动重试"));
    setImportState(QStringLiteral("idle"));
}

QVariantList AppModel::draftItems() const
{
    QVariantList list;
    if (!m_active)
        return list;
    const auto &draft = m_active->import->draft();
    for (int i = 0; i < draft.items.size(); ++i) {
        const auto &d = draft.items.at(i);
        const auto &item = d.item;
        QString group = QStringLiteral("food");
        if (!d.selected || d.category == rec::ItemCategory::Irrelevant)
            group = QStringLiteral("ignored");
        else if (d.category == rec::ItemCategory::Seasoning)
            group = QStringLiteral("seasoning");
        const QString name = item.displayName.isEmpty() ? item.canonicalName : item.displayName;
        list.append(QVariantMap {
            { "index", i }, { "name", name }, { "quantity", item.quantityText }, { "category", item.category },
            { "location", core::storageDisplayName(item.storage) }, { "source", sourceName(item.shelfLifeSource) },
            { "shelfDays", item.shelfLifeDays ? QString::number(*item.shelfLifeDays) : QString() },
            { "purchasedAt", dateText(item.purchaseDate.date) }, { "storedAt", dateText(item.entryDate) },
            { "producedAt", dateText(item.productionDate.date) },
            { "packageExpiresAt", dateText(item.packageExpiry.date) },
            { "group", group }, { "needsDate", d.needsDateConfirmation() } });
    }
    return list;
}

QString AppModel::draftWarning() const { return m_active ? m_active->import->draft().warning : QString(); }
bool AppModel::draftDuplicate() const { return m_active && m_active->import->draft().duplicate.isDuplicate(); }

int AppModel::draftFoodCount() const
{
    int count = 0;
    if (!m_active)
        return count;
    for (const auto &d : m_active->import->draft().items)
        count += d.selected && d.category == rec::ItemCategory::Food;
    return count;
}

void AppModel::editDraft(int index, const QVariantMap &edits)
{
    const auto &items = m_active->import->draft().items;
    if (index < 0 || index >= items.size())
        return;
    DraftItem draft = items.at(index);
    applyEdits(draft.item, edits, false);
    m_active->import->updateItem(index, draft);
}

void AppModel::dropDraft(int index)
{
    const auto &items = m_active->import->draft().items;
    if (index < 0 || index >= items.size())
        return;
    DraftItem draft = items.at(index);
    draft.selected = false;
    m_active->import->updateItem(index, draft);
}

void AppModel::retrieveDraft(int index)
{
    const auto &items = m_active->import->draft().items;
    if (index < 0 || index >= items.size())
        return;
    DraftItem draft = items.at(index);
    if (draft.category == rec::ItemCategory::Irrelevant)
        draft.category = rec::ItemCategory::Food;
    draft.selected = true;
    m_active->import->updateItem(index, draft);
}

void AppModel::confirmImport()
{
    m_active->import->confirm(draftDuplicate());
}

}

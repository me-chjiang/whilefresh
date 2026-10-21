# 核心领域与应用服务（T5）

## 分层与生命周期

- `core` 只链接 QtCore，不依赖识别、SQL、GUI。`IngredientCatalog` 读取领域需要的 parent/default 字段；识别器的 `Vocabulary` 保持原接口，由应用服务从 catalog 构造。
- `BuiltinData::builtIn()` 懒加载 qrc 中的 recipes/vocabulary/shelf-life，一次加载、只读共享；`load(directory)` 可用于测试，参考表缺失不是错误，损坏则报错。
- `Database` 是一个具名 SQLite 连接；先 `open()`，再 `migrate()`。数据库父目录由入口创建。`Store`、服务只在同一线程使用，不做后台数据库访问。
- 生命周期顺序：时钟 / 内置数据 / 数据库 → Store → 识别器 → 应用服务。服务依赖的对象必须比服务活得久；退出时先销毁 ImportService，让它取消请求并清理未确认图片。
- 正式 QML 页面不在本任务范围内，`main.cpp` 保留原平台调试入口。视图模型 AppModel 已组合下面的服务。

## 给 T8 的接口

### ImportService

头文件：`app/src/ui/import-service.h`。

```cpp
ImportService(Store &, shared_ptr<const BuiltinData>, const Clock &,
              RecognitionService &, bool demo = false, QObject *parent = nullptr);
```

一个服务只处理一个活动导入，识别器应专供该服务使用。

1. `receive(ImportPayload)` 接收 **T9 实际类型**，接管 App 私有 `imports/` 中的图片，计算 SHA-256、检查重复；不联网、不入库。文字原文只留在活动载荷中，不写数据库。
2. 页面展示 `privacyNotice()`，用户同意后调用 `start(true, allowDuplicate)`。每次重试都须明确传入同意；无同意或未确认重复时不调用识别器。相机映射 Receipt，其他图片映射 Screenshot，文字映射 OrderText。
3. `draftReady()` 后用 `draft()` 读取草稿，用 `updateItem(index, DraftItem)` 修改选中/分类/名称/数量/存放位置/日期/天数，用 `setCaptureDate(imageIndex, date)` 补拍摄日期。
4. `confirm(allowDuplicate)` 事务内写入导入记录、食材、缓存与常备建议；成功发 `committed(importId)` 并释放草稿。失败保留草稿供重试，不留下半次导入。
5. `discard()` 可在上传中、失败后或草稿页调用：取消当前请求并删除私有图片，不删除已被其他导入引用的文件；清理失败返回 false，文件进入可重试队列。析构也调用此方法。

信号：`prepared()`、`draftReady()`、`draftChanged()`、`duplicateDetected(DuplicateCheck)`、`failed(RecognitionError)`、`committed(qint64)`。识别器返回的 `ErrorKind` 与中文 `userMessage` 原样透传；不要把 `detail` 展示给用户。

载荷忙时再收到新载荷：保留当前草稿，清理新的未引用私有图片并提示重新导入。**平台的 `importFailed` 不等于当前草稿失败**：T9 部分成功会先发 `importReceived` 再发 `importFailed`，重复打开选图也会报错。因此平台错误只展示提示，不据此调用 `discard()`；`importCancelled` 同理不要取消另一个活动草稿。

`ImportDraft`：

- `record`：导入时间、输入方式、平台 origin、识别 sourceType、图片路径/SHA/MIME/拍摄时间及来源、isDemo、truncated、文字长度。
- `items[]`：原 `RecognizedItem`、可编辑 `FridgeItem`、分类、selected、三种 `DateParse`；`needsDateConfirmation()` 提示缺基准。
- `duplicate`、`ignoredSummary`、`warning`、`ready`；无关项默认不选，`ignoredCount()` 给“已忽略”组计数。找回无关项须改为食材/调料后选中。
- 截断时 warning 固定“可能有遗漏，请检查”。入库后的演示标识从物资的 importId 关联导入记录获得，不把演示当真识别。
- 调料进入待确认的常备建议，不制造额外冰箱物资；已知常备且本来在家时无需重复建议。未归类调料必须先选择标准名或改成普通食材，避免静默丢掉。

### KitchenService

头文件：`app/src/ui/kitchen-service.h`。持有 Store / BuiltinData / Clock，变更成功发 `changed()`。

| 用途 | 接口 |
| --- | --- |
| 冰箱、时间刷新 | `fridge()`、`freshness(item)`、`defaultSelection()` |
| 手动添加/编辑 | `saveItem(item, error)`；新物资不调用 AI，自动设入库日，已有物资保留所属导入与入库日 |
| 常用动作 | `useItem(id)`、`markAbsent(id)`、`setUseFirst(id, bool)`、`changeStorage(id, Storage)` |
| 来源链 | `resolve(item)`；只计算当前物资，不偷偷写规则 |
| 本地记忆 | `proposalsForEdit(before, after)` → 单独弹出确认 → `confirmRule(proposal)`；`shelfLifeRules()` / `storageRules()` / 两类 delete 方法；修改规则也调用 confirmRule |
| 常备/存量 | `pantryRules()`、`setPantryOverride()`、`markPantryAbsent(name)`、`suggestions()`、`decideSuggestion(id, accept)` |
| 菜谱 | `recipes(search)`、`recipe(id)`、`recipeSource()`、`clearFridge(selectedIds)` |
| 待买 | `addMissingToShopping(recipeId)`、`addShopping(names)`、`shopping()` / `shoppingSections()` / `shoppingText()`、`removeShopping(id)` |
| 数据和设置 | `imports()`、`deleteImport(id)`、`deleteAllData()`、`demoMode()`、`setDemoMode(bool)` |

`RecipeMatch::recipe` 是内置 RecipeBook 中的只读指针，视图模型持有服务/内置数据期间有效。待买纯文本交平台 `copyText()` 或 `shareText()`。

**必须的接线**（防止“删除全部”后异步识别回写）：

```cpp
QObject::connect(platformServices, &platform::PlatformServices::importReceived,
                 importService, &ui::ImportService::receive);
QObject::connect(kitchenService, &ui::KitchenService::aboutToDeleteAllData,
                 importService, &ui::ImportService::discard);
QObject::connect(importService, &ui::ImportService::committed,
                 viewModel, &YourViewModel::reload);
```

多种识别器/多个 ImportService 时，删除全部信号连接到所有活动导入服务。切换演示模式时也先放弃当前草稿，再换专用识别器/服务；设置开关本身不会替换正在上传的识别器。初始化 Store 后调用 `retryFileCleanup()`，将失败清理的提示呈现给用户。文件删除失败可能发生在 SQL 已提交之后，因此 KitchenService 仍发 changed，让页面刷新，同时 false 表示还有图片待清理。

### FridgeItem 字段

`id/importId`、`canonicalName/displayName/rawName/category`、`quantityText/count`、`storage/storageSource`、`useFirst/status/statusChangedAt`；`purchaseDate`、`productionDate`、`packageExpiry` 为 `RecordedDate{date, origin(None/Visible/User), text}`，`entryDate` 自动；`labelledDays/manualDays/aiDays`、`referenceConditionsConfirmed`；解析结果 `shelfLifeSource/shelfLifeDays/expiryDate/startBasis/shelfLifeNote`，以及创建/修改时间。新鲜度不持久化，前台恢复与跨日时重新查询。

## 日期与来源链

- 显式年月日无需拍摄基准；相对日期与无年份月日必须有原图拍摄日期。单图可用唯一原图作为回退，多图未给 imageIndex 时不任意取第一张。缺基准返回 NeedsBaseDate，所选食材的此类日期未确认前不能入库。
- 无年份月日统一取不晚于拍摄日的最近一次，**到期日期也不擅自推到未来**；无法解析保留原文。生产日期永远不由入库日补齐。用户填的日期不会被后续拍摄日修正覆盖。
- 单项用户修改天数优先（是明确纠错，不冒充本地记忆），其后是包装到期/标注天数、本地规则、参考表、首次 AI 值。除包装标注外均作为估计，UI 可用 `isEstimate()`/来源枚举与集中中文常量展示。
- 包装到期日直接使用；非参考天数的起算顺序为生产 > 购买 > 入库。
- **协调者恢复指示修正了早期 ADR 的泛化起算规则**：FoodKeeper DOP 是购买后的期限，不是生产后的寿命。参考表天数从生产日期 > 购买日期 > 入库日期起算；`referenceConditionsConfirmed` 字段已不再使用。
- T3 `basis`（包括 manual、DOP_*、ripe）只是元信息，不能当程序规则。由于表没有结构化的开封/成熟度条件，确认适用性是显式用户操作，默认 false。存放方式/标准名改变后需重新确认，不能自动沿用。
- 参考表查标准名，未收录才沿 parent 找最近收录行；最近行某存放方式为 null 时直接用 AI，不越过该行继续找父项。
- AI 缓存按标准名+存放位置，**首次确认入库**时记全部有效存放方式，即使当时包装/参考来源胜出。草稿中临时缓存保证同次导入一致，放弃草稿不污染持久缓存。“未归类”不共享缓存。换存放位置只查本地，不调用 AI；修改标准名时清掉旧名称的 AI 天数。
- 新鲜度阈值集中在 FreshnessThresholds：剩余 ≤0 超出估计期限；≤max(1,total×25%) 临期；其他新鲜。到期日未知则未知。文案无食品安全结论。

## 匹配、常备与待买

- 需求组内 OR、组间 AND，忽略空组列表及工具；可选未满足不计缺，满足的可选组计入“有”。重复的同一备选集合只算一次，必需优先于可选。
- 库存子孙满足需求父项，父项不满足子项；常备使用完全相同的方向规则。匹配结果保留每组被哪些物资/常备满足。
- 清冰箱：所选物资命中数降序、缺料升序、临期物资命中数降序、菜名稳定排序；不命中所选物资的菜谱不显示。一般菜谱：缺少数、库存命中数、菜名。物资数按独立 itemId 去重，不按需求重复计数。
- pantry/bulk 默认来自词表、用户覆盖可空以表示沿用默认。“用掉”批量物资保留在库，“家里没有”才移出。仍有同名物资时不提出“全用完”的建议。
- 系统只生成建议，accept 才改常备/待买；用户主动“家里没有”会改变在家状态，但加入待买仍须确认。重复待确认建议按 kind+name 复用。
- 待买按**整个备选集合**去重（顺序无关），不擅自把“A 或 B”缩成 A。显示“或”，按原组第一项的品类分组，品类顺序沿词表，纯文本不含价格。

## 数据库与图片所有权

`Database::migrations()` 只追加，`PRAGMA user_version` 当前 **4**，每个迁移独立事务：

| 版本 | 内容 |
| --- | --- |
| 1 | imports、import_images、items、shelf_life_rules、storage_rules、ai_shelf_life_cache、pantry_overrides、suggestions、shopping_items |
| 2 | settings 与 items/suggestions 查询索引 |
| 3 | import_products（不可变购买指纹）、file_cleanup（提交后文件删除队列） |
| 4 | items.reference_conditions_confirmed；imports.source_type |

- `imports`：时间、输入方式、平台来源、识别来源类型、演示/截断、文字长度。`import_images`：所属导入、顺序、私有路径、SHA-256、MIME、拍摄日期与来源。
- `items`：上述物资与日期字段，import_id 外键级联删除，manual item 的 import_id 为 NULL；日期按 ISO 存储，时间为 UTC。
- 保质期规则与 AI 缓存主键为 canonical_name+storage；存放规则与 pantry_overrides 以 canonical_name 为键。AI INSERT OR IGNORE，首值不覆盖。
- `suggestions` 保存 pending/accepted/dismissed 与来源导入；`shopping_items` 保存 JSON 备选名称、唯一 dedupe_key、品类、来源、备注；`settings` 为字符串键值。
- `import_products` 存确认时的标准名+数量原文+件数，之后编辑/用掉物资不会改变购买指纹，调料也纳入。早期库缺快照的记录按现有物资回退。
- 同 SHA 不受时间窗口限制；近 72 小时内商品多重集合交集/较大集合 ≥80% 才“可能重复”，至少两项；未知标准名不用于相似判断。只是提示，允许用户明确继续。
- 删除一次导入：删除其物资、原图元数据、购买指纹与未确认建议；已明确确认的用户规则/常备变更不自动撤销。删除全部清掉所有用户表数据并排队清理 imports 内文件，不删 schema 或内置只读数据。
- SQL 删除与 `file_cleanup` 入队同事务，提交后删除文件；失败保留队列供 `retryFileCleanup()` 重试，不误报彻底删除成功。只删除指定 imports 根目录的直接文件，其他导入还引用的文件暂不删除；不得把图库原件路径交给 Store 当导入私有副本。

## 验证范围与边界

九个 T5 Qt Test 套件：日期、来源链、新鲜度、匹配、家庭规则、迁移、Store、真实内置数据、应用服务。包含旧版本升级、真实鸡胸肉冷藏 2 天/空心菜 4 天/土豆冷藏 null、372/544 数据集、西红柿炒鸡蛋缺料为零且前十、Replay/Demo 全流程、截断、重复、取消、事务失败重试与规则确认。均离线；Android 用 `scripts/build-android.sh x86_64 --no-key` 编译验证，不等于正式 UI/真机端到端验收。

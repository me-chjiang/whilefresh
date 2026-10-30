# 食材模型与导入续传（T15）

依据 [PRD v6](../../todo/prd-v6.md) 与 [第二轮计划](../../todo/round-2-plan.md) 的 R-ADR-8 / R-ADR-9、迁移 v5。

## 迁移 v5

只追加，`PRAGMA user_version` = 5。

| 表 | 变化 |
| --- | --- |
| `items` | `kind`（`food` / `staple_seasoning` / `staple_food`，默认 `food`）、`amount REAL`（默认 0）、`unit`（默认空）、`snoozed_until`（日期，可空）；`status` 新增 `discarded` |
| `imports` | `status`（`confirmed` / `skipped_duplicate`，默认 `confirmed`）、`duplicate_of`（指向被重复的导入，删除时置空）、`deleted_at`（软删除时间） |
| `name_mappings` | `raw_name` 主键 → `canonical_name`、`confirmed_at`；T14 使用 |
| `preferences` | 偏好记忆条目 `id`、`text`、`source`、时间；T16 使用。偏好摘要存 `settings.preference_summary`，无需建表 |
| `import_jobs` | 续传任务：`state`、`payload`（JSON）、时间 |
| `import_job_batches` | `(job_id, batch_index)` 主键、`state`、`result`（BLOB）；随任务级联删除 |
| `cleanup_sessions` | 下厨会话占位：`state`、`payload`、时间、`committed_at`；只有结构，T18 使用 |

## 删除导入与清空历史（R-ADR-8）

- `Store::deleteImport(id)`：同一事务里删除该导入仍在库（`in_stock`）的食材、图片元数据、购买指纹和未确认建议，设置 `deleted_at`；`used` / `discarded` / `absent` 的食材保留 `import_id`。手动添加的食材（`import_id` 为空）不受影响。导入记录不物理删除，`status` 不变。
- `Store::clearImportHistory()`：隐藏全部导入并删除其图片与购买指纹，**不删除任何食材**，避免"删历史"清空冰箱。
- 已删除的导入不出现在 `imports()`，也不参与疑似重复判断（重新导入同一张图不会再提示）。`imports(true)` 可查到全部。
- 图片仍走 `file_cleanup` 队列：SQL 提交后删除，失败可重试。

## 跳过疑似重复

检测规则不变（同一张图，或 3 天内与某次导入商品重合 ≥ 80%）。用户点"跳过"时 `ImportService::skipDuplicate()` 写一条 `status = skipped_duplicate`、`duplicate_of = 被重复的导入` 的记录，不写食材、图片和购买指纹，然后放弃草稿并删除图片。识别前（同图）和核对页（商品相似）都可以跳过。历史列表标"已跳过"。

## 种类判定（常备调料 / 常备食材）

`core::classifyItemKind`，在核对页每次重算、入库和手动添加时使用：

1. 识别为调料，或词表品类为 `seasoning` → `staple_seasoning`（常备调料）。
2. 词表品类为 `staple`（主食粮油）时：
   - `pantry_default` 或 `bulk_default` 为真（大米、面粉、食用油、淀粉等）→ `staple_food`（常备食材），与存放位置无关；
   - 否则常温存放、且参考表常温天数不短于 30 天（或没有参考值）→ `staple_food`（挂面、意面、杂粮、干豆、方便面）；
   - 其他（鲜面条、饺子皮、馒头等冷藏冷冻的，面包、吐司等常温不足 30 天的）→ `food`。
3. 其余 → `food`。干货里的坚果、枸杞等在 `other` 品类，本轮不算常备食材。

常备类保质期未知时不丢弃、不只转成常备建议：照常入库，`needsShelfLife()` 为真，界面标"待录保质期"。调料的常备建议流程照旧（只对词表内的标准名提出）；未归类调料也直接入库，不再要求先选标准名。

## 用量解析

`core::parseAmount(text)`：取第一个质量 / 体积（g、克、kg、千克、公斤、斤、两 → g；ml、毫升、L、升 → ml），没有时取第一个计数单位（个、件、枚、盒、袋、包、瓶、把、根……原样保留）。紧挨着的乘号（×、x、*）连接的件数相乘：`500ml×2` → 1000 ml，`2袋x500g` → 1000 g。支持全角字符和一至十、两、半。没有单位的数字或不认识的单位 → 0 / 空。

入库存的是总量：`totalAmount(quantityText, count)` = 解析值 × 件数。确认导入时写入；手动添加或修改数量 / 件数时重算。

## 冰箱分组与排序

`core::sortForFridge`：先按组（冷藏、冷冻、常温、常备调料、常备食材；常备类按种类，其余按存放位置），组内按剩余天数升序（过期的为负数，自然在前），未知在后，再按名称、id 稳定排序。`KitchenService::fridge()` 返回此顺序，冰箱页按组显示，行内只显示"剩余x天" / "过期x天" / "待录保质期"。

## 扔掉与忽略

- `KitchenService::discardItem(id)`：在库 → `discarded`。
- `KitchenService::snoozeItem(id)`：只对临期食材，`snoozed_until` = 明天；过期的只能改保质期或扔掉。
- `KitchenService::attention()`：临期或过期、且未被忽略的食材；`AppModel.nearCount` / `attentionCount` 不计被忽略的。

## 导入续传（R-ADR-9）

`core::ResumableJob` 不做 I/O：`pending → running →（逐批 completeBatch）→ done`，`running → failed → running` 从第一个未完成批次继续。批次数在规划后（切片分批后）才知道，批次必须按顺序完成，每批带一个不透明的结果。落库时若状态是 `running`，恢复后视为 `failed`（进程死时请求已丢）。

`ImportService` 只为**图片导入**建任务：

1. `start()` 建任务（`payload` 记录导入时间、输入方式、来源、图片路径 / SHA / 拍摄日期），每次重试都从 `nextBatch()` 开始。
2. 识别器新增 `RecognitionRequest::firstBatch` 与信号 `batchSucceeded(id, index, count, part)`。`DeepSeekRecognizer` 从第 `firstBatch` 批开始发送，每批成功即报告；回放 / 演示识别器不报告批次、忽略 `firstBatch`，原有整单接口不变。
3. 每批成功立即保存（原始响应 + 覆盖的图片范围）。全部完成后从任务里重新解析、合并全部批次生成草稿，与一次跑完的结果相同。若本次规划出的批次数与已存的不同（如更新后批次上限变了），丢弃已存批次从头开始。
4. 析构 `ImportService` 时保留任务和图片，只取消在途请求；`confirm()` 在入库事务中删除任务，`discard()` / `skipDuplicate()` 删除任务和图片。
5. 下次启动：`hasUnfinishedJob()` → `resume()`。全部批次已完成（草稿未确认时被关掉）就直接重建草稿，不发请求；否则只发未完成的批次。`discardUnfinishedJob()` 放弃。没人继续的旧任务在下一次导入时被替换（图片一并删除）。
6. `AppModel` 暴露 `hasUnfinishedImport`、`resumeImport()`、`discardUnfinishedImport()`，计划页的继续条在 T19 接入。

**文字导入不续传**：沿用 T5 的约定，粘贴的原文不写数据库；文字只有一批，中断后重新粘贴。草稿上的修改不保存，续传后重新核对。

## 验证

`quantity-test`、`fridge-groups-test`、`resumable-job-test`（core），`database-test`（v4 升级与默认值）、`store-test`（软删除、清空历史、跳过记录、任务读写），`application-service-test`（删除导入保留已用 / 扔掉的、清空历史保留冰箱、两种跳过、调料与常备食材入库及用量、扔掉与忽略、第 2 批失败后换新服务只重发第 2、3 批且请求体与原第 2 批相同、草稿阶段关闭后续传不发请求、放弃与替换旧任务、文字导入不落库）。

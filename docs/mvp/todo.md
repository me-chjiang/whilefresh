# 趁鲜 MVP 问题与待办

> 2026-10-21 整理。已完成的事项见 [cycle-state](cycle-state.md) 与 [审查记录](review-log.md)；本文件只列**尚未完成**的事项。完成后从这里删除，并在 cycle-state 决策日志中记一行。

## 发布前必须做

- [ ] **T17 未完成：真实详情样例待子模块 Lexmount 路线修复**（录制于 2026-10-30，子模块 `recipe-sources-xiachufang-api` 版本 0.1.0，依赖以 `uv.lock` 为准，其版本库尚无提交）。直连搜索可用，直连详情被站点拦截（`unrecognized detail page (blocked or upstream markup changed)`，id 107722218，`--provider direct`）。`--provider lexmount` 的搜索和详情都在子模块内报错，因此 `test-data/recipes/xiachufang/detail-synthetic-*.yaml` 是用子模块引擎 + 固定数据合成的。
  - 复现命令（在子模块目录用 `uv run`；凭据读自 `~/.config/lexmount/config.yaml`，此处已脱敏；配置里有 `base-url`，当时以 `--lexmount-base-url <base-url>` 一并传入）：
    1. `python -m xiachufang_source --provider lexmount --api-key <api-key> --project-id <project-id> --operation detail --id 104493058 --search-type detail` → `status.code: error`，`message: xiachufang-api response requires one Content recipe`，`incomplete: [source_request]`，耗时约 2237 ms。
    2. 同上但省略 `--search-type`，id 104493058 → 同样错误，约 3133 ms。
    3. 同 2，id 103468406 → 同样错误，约 3583 ms。
    4. 同 2，id 101829462（子模块 README 的示例 id）→ 同样错误，约 3156 ms。
    5. `python -m xiachufang_source --provider lexmount --api-key <api-key> --project-id <project-id> --keyword 虾仁`（搜索，省略 `--search-type`）→ `message: source recipe must have a name`，约 2811 ms。
  - 路径：以上都是 Lexmount 云浏览器 DOM 提取（`engine.py` 在 provider 为 lexmount 时选 `LexmountBrowserClient`；搜索在 `search_type` 为空时被 `LexmountAPI.search` 强制为 browser）；没有走 Web Fetch 文本路线，该路线未单独测试。
  - 报错位置（只读推断，未改子模块）：
    - `response requires one Content recipe`：`src/xiachufang_source/engine.py` `RecipeEngine.run` 的 detail 分支（`rows = payload.get("Content")`，不是长度为 1 的列表就抛出）。浏览器抓取脚本（`lexmount.py` 的 `_scrape_script`，约 179–195 行）在找不到 `h1.page-title[itemprop="name"]` 时返回 `{Recipe: [...]}` 而不是 `{Content: [...]}`，而 `lexmount.py` 第 249 行只检查两者之一存在，所以拦截页、验证页或改版页面会走到这里。疑似原因：云浏览器打开的详情页不是预期的菜谱页，或 `Page.navigate` 后立即 evaluate、页面尚未加载完。
    - `source recipe must have a name`：`engine.py` `normalize()` 开头（`row["name"]` 为空即抛）。搜索结果名由 `_scrape_script` 的 `text(a)` 取得，选择器 `.info .name a, a[href*="/recipe/"]` 可能命中无文字的图片链接，得到空名。
    - 可能相关：`engine.py` 把 `budget` 限制在 2.5 秒，而云浏览器会话创建与轮询（`lexmount.py` `extract`，`while time.monotonic() < deadline`）本身就要数秒；实测耗时 2.2–3.6 s。上面几次返回的是解析错误而非 deadline 错误。
  - 无法从输出判断云浏览器取回的页面实际内容；修复时建议先在子模块里把原始 JSON / 页面标题打到本地调试输出（不含凭据）。
  - 子模块修复后 T17 剩余：① 用 `--provider lexmount`（或直连可用时直连）录制 2–3 个真实详情，存入 `test-data/recipes/xiachufang/`（同样只留必要内容）；② 删除 `detail-synthetic-full.yaml`、`detail-synthetic-partial.yaml`，改 `app/tests/recipes/` 三个测试中引用 900000001/900000002 和合成步骤文字的断言（`xiachufang-yaml-parser-test.cpp`、`recorded-recipe-source-test.cpp`、`process-recipe-source-test.cpp`），并保留一个 partial 详情样例（真实或经子模块引擎合成）；③ 更新 `SOURCE.md` 的文件表和生成方式；④ 用 `rg -i 'api.?key|project.?id|credential|token'` 确认样例无凭据；⑤ 把 cycle-state 中 T17 改回已完成，并更新 review-log。
- [ ] **arm64 真机实测**：目前只在 x86_64 模拟器上走查过。在一台真实手机上安装 `whilefresh-arm64-v8a-release.apk`，走一遍：分享截图入库 → 核对 → 清冰箱 → 菜谱 → 待买。
- [ ] **备份签名材料**：`~/.config/whilefresh/release.keystore` 与钥匙串条目 `landing-project-android-keystore`。丢失后无法覆盖升级。
- [ ] **DeepSeek 密钥设额度上限**：试用包内嵌密钥，可被提取；在 DeepSeek 控制台设消费上限，试用结束后作废。
- [ ] **推送远端需用户确认**：当前功能分支仅合并到本地 `main`，未推送（来源：cycle-state.md 第 72、81 行）。
- [ ] **Qt LGPL 义务正式核对**：试用包已在"我的 → 来源与许可"中告知，分发前核对义务（非法律意见）（来源：cycle-state.md 第 130 行；trial-release.md 第 45 行）。

## Phase 3 测试迭代（用户约定 3 轮，尚未开始）

- [ ] 补做代码审查：T5 核心领域与数据层、T7 合成测试数据、T8a/T8b 界面与接线（为赶进度未审查即合并）。
- [ ] 用 `private/bill/` 及新增样例覆盖盒马、叮咚、美团、淘宝 / 京东 / 拼多多；目前只有美团小象一张真实样例。
- [ ] 小票拍照与订单文字的真实识别验证（目前只验证了截图）。
- [ ] 无网络、识别超时、重复导入、删除导入、删除全部数据的界面路径走查。
- [ ] 时间推进 N 天后新鲜度变化的界面验证（单元测试已覆盖逻辑）。
- [ ] 验收 1 的真机路径补覆盖"复制 / 分享待买、标记用掉"（来源：mvp-requirements.md 第 175 行）。

## 已知问题

| 优先级 | 问题 | 位置 / 说明 |
| --- | --- | --- |
| 中 | 入库确认页"入库日期"固定为当天，不能在草稿里改 | `ImportService::updateItem` 强制当天；入库后可在编辑页改 |
| 中 | 演示数据中茭白、山药等显示"保质期未知" | 演示种子缺 AI 估计天数；`AppModel::seedDemo` |
| 中 | 参考表在不知道购买日期时按入库日期起算，可能偏晚 | 已按批准规则实现，界面标"估计" |
| 低 | 粘贴的订单文字在导入记录中来源显示为"分享" | 平台载荷没有"粘贴"来源 |
| 低 | 冰箱物资行右侧有细小的角标残影 | `PantryRow.qml` 拟物装饰 |
| 低 | 按钮文字偏上，未垂直居中 | `InkButton.qml` |
| 低 | "拍购物小票"使用了铅笔图标 | 入库方式选择页 |
| 低 | 移除常备品走 `setPantryOverride`，未调用 `markPantryAbsent` | `ProfilePage` / `AppModel` |
| 低 | 发布版仍编译进调试面板（运行时关闭） | 可改为 Release 不打包 |
| 低 | `referenceConditionsConfirmed` 字段与数据库列已不再使用 | 下次数据库迁移时清理 |
| 低 | 桌面版使用本机另装的 OpenSSL 3.6.3，来源未核实 | 不影响 Android |
| 低 | Android 关闭了着色器磁盘缓存以避开模拟器卡死，真机冷启动可能略慢 | `main.cpp`；真机实测后再决定是否只对模拟器关闭 |
| 低 | 分享进来的"图文"只取图片，同时带的文字被丢弃 | notes/android-integration.md 第 70 行附近 |
| 低 | `allowBackup=false` 不覆盖 Android 12+ 设备间迁移 | 需另配 `dataExtractionRules`；notes/android-integration.md 第 71 行附近 |

## 数据质量待复核

- [ ] 识别基线：样本 receipt-03 的"鲜切面条"被识别为"面条"（应为"鲜面条"）（来源：notes/recognition-baseline.md 第 52–69 行）。
- [ ] 识别基线：样本 order-mall-list-01 的"藤椒油"被识别为"未归类"（应为"花椒油"）（来源：notes/recognition-baseline.md 第 52–69 行）。
- [ ] 识别基线：4 个样本来源类型误判（text-darkstore-01、text-delivery-02、text-mall-03 识别为 order，text-share-04 识别为 cart，均应为 text）（来源：notes/recognition-baseline.md 第 52–69 行）。
- [ ] 保质期存疑：鲜面条冷藏 2 天、猪肝等内脏与肉末冷藏 2 天、全谷物常温 60–120 天可能偏短（来源：notes/shelf-life-review.md "存疑条目"）。
- [ ] 保质期存疑：酱油 / 生抽 / 老抽、醋按未开封 730 天；淡奶油只保留冷藏 10 天，国内常温 UHT 款未开封远长（来源：notes/shelf-life-review.md "存疑条目"）。
- [ ] 保质期存疑：牛奶、韭菜、猪油、火腿肠、豆腐干、皮蛋、粉丝、莲藕、莴笋等高频食材仍无参考值（来源：notes/shelf-life-review.md "存疑条目"）。
- [ ] 词表上位关系待收紧：五花肉 / 里脊 / 前腿肉 → 猪肉、西芹 → 芹菜、青椒 / 小米辣 → 辣椒、圣女果 → 西红柿等 `parent` 取舍偏宽时优先收紧（来源：notes/vocabulary-review.md 第 57、72 行附近）。
- [ ] 词表偏差映射：米酒 → 料酒、酒 → 白酒、冷饭 → 大米、桂圆 → 龙眼、酸笋 → 竹笋、日本豆腐 → 豆腐、棉花糖 → 糖果（来源：notes/vocabulary-review.md 第 70 行附近）。
- [ ] 词表映射未逐一核对原菜谱：凤尾（水煮肉片）→ 莴笋叶、黄花（生汆丸子汤）→ 黄花菜、葱头（山西过油肉）→ 葱（来源：notes/vocabulary-review.md 第 71 行附近）。

## 后续增强（PRD v5 已列，首版不做）

- 临期提醒通知、上传前自动裁掉地址栏。
- 附近门店（高德 POI）、淘宝订单 API。
- 识别服务切换（备选阿里云百炼 Qwen-VL）。
- 多人 / 家庭协作、iOS 等其他平台。

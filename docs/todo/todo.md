# 趁鲜 问题与待办

> 2026-10-30 更新（第二轮视角）。已完成的事项见 [cycle-state](cycle-state.md) 与 [审查记录](../done/review-log.md)；本文件只列**尚未完成**的事项。完成后从这里删除，并在 cycle-state 决策日志中记一行。

## 第二轮未实现

- [ ] T18 `feat/meal-plan`：MealPlanner、PlanCoverage、换菜、RecipeAdapter、扣减、下厨会话续传与幂等提交（[第二轮计划](round-2-plan.md)）。
- [ ] T19 `feat/home-shell`：新导航（计划 / 冰箱 / ＋ / 下厨 / 我的）、计划页、我的各页、每日一次弹窗、拆分 AppModel（[第二轮计划](round-2-plan.md)）。
- [ ] T20 `feat/reminders`：通知调度、快照、权限（[第二轮计划](round-2-plan.md)）。
- [ ] T21 `spike/a11y-bubble`：无障碍气泡可行性验证与报告（[第二轮计划](round-2-plan.md)）。

## 发布前必须做

- [ ] **arm64 真机实测**：目前只在 x86_64 模拟器上走查过。在一台真实手机上安装 `whilefresh-arm64-v8a-release.apk`，走一遍：分享截图入库 → 核对 → 清冰箱 → 菜谱 → 待买。
- [ ] **备份签名材料**：`~/.config/whilefresh/release.keystore` 与钥匙串条目 `landing-project-android-keystore`。丢失后无法覆盖升级。
- [ ] **DeepSeek 密钥设额度上限**：试用包内嵌密钥，可被提取；在 DeepSeek 控制台设消费上限，试用结束后作废。
- [ ] **Qt LGPL 义务正式核对**：试用包已在"我的 → 来源与许可"中告知，分发前核对义务（非法律意见）（来源：cycle-state.md「待解决问题」；docs/done/round-1/trial-release.md 第 46 行）。

## 待验证

- [ ] 补做代码审查：T5 核心领域与数据层、T7 合成测试数据、T8a/T8b 界面与接线（为赶进度未审查即合并）。
- [ ] 用 `private/bill/` 及新增样例覆盖盒马、叮咚、美团、淘宝 / 京东 / 拼多多；目前只有美团小象一张真实样例。
- [ ] 小票拍照与订单文字的真实识别验证（目前只验证了截图）。
- [ ] 无网络、识别超时、重复导入、删除导入、删除全部数据的界面路径走查。
- [ ] 时间推进 N 天后新鲜度变化的界面验证（单元测试已覆盖逻辑）。
- [ ] 验收 1 的真机路径补覆盖"复制 / 分享待买、标记用掉"（来源：docs/done/round-1/mvp-requirements.md 第 175 行）。

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
| 低 | 分享进来的"图文"只取图片，同时带的文字被丢弃 | docs/done/notes/android-integration.md 第 70 行附近 |
| 低 | `allowBackup=false` 不覆盖 Android 12+ 设备间迁移 | 需另配 `dataExtractionRules`；docs/done/notes/android-integration.md 第 71 行附近 |

## 数据质量待复核

- [ ] 识别基线：样本 receipt-03 的"鲜切面条"被识别为"面条"（应为"鲜面条"）（来源：docs/done/notes/recognition-baseline.md 第 52–69 行）。
- [ ] 识别基线：样本 order-mall-list-01 的"藤椒油"被识别为"未归类"（应为"花椒油"）（来源：docs/done/notes/recognition-baseline.md 第 52–69 行）。
- [ ] 识别基线：4 个样本来源类型误判（text-darkstore-01、text-delivery-02、text-mall-03 识别为 order，text-share-04 识别为 cart，均应为 text）（来源：docs/done/notes/recognition-baseline.md 第 52–69 行）。
- [ ] 保质期存疑：鲜面条冷藏 2 天、猪肝等内脏与肉末冷藏 2 天、全谷物常温 60–120 天可能偏短（来源：docs/done/notes/shelf-life-review.md "存疑条目"）。
- [ ] 保质期存疑：酱油 / 生抽 / 老抽、醋按未开封 730 天；淡奶油只保留冷藏 10 天，国内常温 UHT 款未开封远长（来源：docs/done/notes/shelf-life-review.md "存疑条目"）。
- [ ] 保质期存疑：牛奶、韭菜、猪油、火腿肠、豆腐干、皮蛋、粉丝、莲藕、莴笋等高频食材仍无参考值（来源：docs/done/notes/shelf-life-review.md "存疑条目"）。
- [ ] 词表上位关系待收紧：五花肉 / 里脊 / 前腿肉 → 猪肉、西芹 → 芹菜、青椒 / 小米辣 → 辣椒、圣女果 → 西红柿等 `parent` 取舍偏宽时优先收紧（来源：docs/done/notes/vocabulary-review.md 第 57、72 行附近）。
- [ ] 词表偏差映射：米酒 → 料酒、酒 → 白酒、冷饭 → 大米、桂圆 → 龙眼、酸笋 → 竹笋、日本豆腐 → 豆腐、棉花糖 → 糖果（来源：docs/done/notes/vocabulary-review.md 第 70 行附近）。

## 后续增强（尚未排期）

- 上传前自动裁掉地址栏。
- 附近门店（高德 POI）、淘宝订单 API。
- 识别服务切换（备选阿里云百炼 Qwen-VL）。
- 多人 / 家庭协作、iOS 等其他平台。

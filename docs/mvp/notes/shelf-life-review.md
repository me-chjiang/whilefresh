# 保质期参考表复核（T3）

分支 `feat/shelf-life-table`。数据源 USDA FoodKeeper v128（快照与说明见 `tools/sources/foodkeeper/`），生成物 `app/assets/data/shelf-life.json`，人工映射源数据 `tools/whilefresh_tools/shelf_life/foodkeeper-map.json`。本文件中的"覆盖率""高频无参考值""抽样对照""人工修正值""成熟果""校验警告"六节由 `python -m whilefresh_tools.shelf_life report` 生成，其余为人工整理。

## 概要

- 参考表 290 个标准名（词表 546 个）；沿 `parent` 回退后覆盖 318 个，占 58.2%；按菜谱用料次数加权为 71.1%。
- 取值规则（完整版见 `tools/sources/foodkeeper/SOURCE.md`）：
  - 每种存放方式取"从购买日起"的最短值（`DOP_*_Min`）；最短值换算后不超过 2 天且 `_Max` 更长时改取最大值（`DOP_*_Max`）。
  - 没有 `DOP_*` 数据时退到无前缀字段，但 tips 写明 cooked / mashed / sliced / balled / not recommended / 烹饪后 / 开封后的不用，"速冻购买食品"类的冷藏期限也不用。
  - 非数值单位（成熟时、包装日期、无限期、不推荐、小时）一律留空。
  - 换算：周 × 7，月 × 30，年 × 365。
  - 人工修正值写在映射条目的 `override`（必填 `reason`），生成物里该存放方式的 `basis` 为 `manual`，`source` 仍为 `foodkeeper`。
- DeepSeek 初稿：`deepseek-flash`（关闭思考）7 次请求，prompt 71587 + completion 11806 = 83393 tokens；初稿给出 322 个对应，222 个留空。之后逐条人工复核。初稿和响应缓存在 `~/.cache/whilefresh/shelf-life/`，不入库；第二轮修订没有再调用 DeepSeek，构建和测试不联网。
- 校验：所有键在词表中；天数为 1 到 3650 的整数；FoodKeeper ID 存在；冷冻不短于冷藏、常温不长于冷藏（有意例外用 `confirmed` 确认，目前 3 个）；生成物与源数据一致（`validate` 检查陈旧）。

## 第二轮修订（审查后）

起因：审查指出数据口径问题。改动涉及 97 个标准名：91 个数值变化，4 个仅 `basis` 加成熟果标记，西红柿新增，果汁删除。

| 指标 | 修订前 | 修订后 |
| --- | --- | --- |
| 冷藏 ≤ 2 天的条目 | 55 | 35 |
| 常温 ≤ 2 天的条目 | 11 | 1 |

修订后仍 ≤ 2 天的 35 项冷藏条目主要是禽肉、鱼、肉末、内脏、玉米、鲜面条（FoodKeeper 的 1 到 2 天取到最大值 2 天），以及生蚝 / 蛤蜊 / 贻贝（人工取 2）和螃蟹；常温仍 ≤ 2 天的只剩葡萄（1 天）。

1. **取值策略**：最短值 ≤ 2 天且 `_Max` 更长时取最大值。例：绿叶菜 1 到 4 天 → 4；禽肉、鱼 1 到 2 天 → 2；虾、鱿鱼、扇贝 1 → 3；其余条目仍取最短值。
2. **tips 排除**：无前缀字段的 tips 含 cooked / mashed / sliced / balled / not recommended 时不用。因此土豆（mashed）、苹果、李子、桃子、梨、甜瓜的冷冻值消失；FoodKeeper 的这些冷冻数字指煮熟、切片或做成泥之后。
3. **人工修正值**：见下文"人工修正值"表（28 个标准名），每项带理由。
4. **skip**：土豆、芋头不用冷藏（土豆常温 30 天，芋头常温 7 天）；苹果、李子、甜瓜不用冷冻；开心果只保留常温；淡奶油只保留冷藏 10 天（国内常温 UHT 款以包装日期为准）；圣女果不用常温。
5. **删除 / 新增**：果汁删除（国内常温 UHT 与 FoodKeeper 口径不同，交给 AI 或包装日期）；西红柿借用圣女果条目的冷藏 5 天（FoodKeeper 的 Tomatoes 只有冷冻数字），`basis` 为 `manual`，理由见表；牛奶仍留空（FoodKeeper 的答案是包装日期）。
6. **保留不动**：整粒香料、味噌、枫糖浆。
7. **成熟果**：杏、李子、桃子、梨、火龙果、牛油果、猕猴桃、菠萝、香蕉的冷藏值在 FoodKeeper 中针对成熟果，数字保留，`basis` 带 `(ripe)` 标记，界面上这些水果的"估计"含义是"买回后已成熟"。
8. **新增警告** `room_gt_fridge`（常温期限长于冷藏）：西瓜（整个常温 7 天、切开冷藏 3 天）、香菜（常温 7 天、冷藏取 5 天）、枫糖浆（常温 730 天、冷藏 540 天）已用 `confirmed` 确认；其余在修订中解决（土豆、芋头去掉冷藏，圣女果去掉常温，葫芦瓜 / 西葫芦常温改 3 天）。
9. **新增测试**：冷藏最短值取最大值的边界（绿叶菜 1 到 4 取 4，禽肉 1 到 2 取 2，3 天以上不替换）；tips 排除；成熟果标记；`override` 校验与 `basis` 标记；`test_committed_table_has_no_extreme_values`（常温 ≤ 1095、生鲜冷藏 ≥ 2、土豆和芋头无冷藏值）；名字含 干 / 粉 / 罐 / 腌 / 酱 / 汁 / 油 的标准名若由 parent 回退得到值，必须在白名单（土豆淀粉、红薯淀粉、木薯粉、蒸鱼豉油）里。

### 覆盖率

| 品类 | 标准名数 | 直接有参考值 | 直接占比 | 按 parent 回退后 | 回退后占比 |
| --- | --- | --- | --- | --- | --- |
| 蔬菜（vegetable） | 96 | 77 | 80.2% | 77 | 80.2% |
| 水果（fruit） | 42 | 31 | 73.8% | 31 | 73.8% |
| 肉类（meat） | 57 | 30 | 52.6% | 36 | 63.2% |
| 水产海鲜（seafood） | 58 | 23 | 39.7% | 40 | 69.0% |
| 蛋奶（egg_dairy） | 18 | 13 | 72.2% | 13 | 72.2% |
| 豆制品（soy） | 10 | 1 | 10.0% | 1 | 10.0% |
| 主食粮油（staple） | 56 | 31 | 55.4% | 34 | 60.7% |
| 调料（seasoning） | 97 | 53 | 54.6% | 55 | 56.7% |
| 加工/速食（processed） | 62 | 15 | 24.2% | 15 | 24.2% |
| 其他（other） | 48 | 16 | 33.3% | 16 | 33.3% |
| 合计 | 546 | 290 | 53.1% | 318 | 58.2% |

按菜谱用料出现次数加权（不含默认常备项，回退后）：1114 / 1567，71.1%。

仅靠 parent 回退才有参考值的标准名 28 个：北极甜虾→虾、土豆淀粉→淀粉、基围虾→虾、多宝鱼→鱼、巴沙鱼→鱼、带鱼→鱼、明虾→虾、木薯粉→淀粉、白虾→虾、秋刀鱼→鱼、红薯淀粉→淀粉、罗非鱼→鱼、羊肉卷→羊肉、羊腩→羊肉、肥牛→牛肉、草鱼→鱼、蒸鱼豉油→酱油、青虾→虾、香醋→醋、鲅鱼→鱼、鲈鱼→鱼、鲢鱼→鱼、鲤鱼→鱼、鲳鱼→鱼、鳗鱼→鱼、鸡翅→鸡肉、鸭翅→鸭肉、鸭腿→鸭肉

### 无参考值的高频食材（前 50，按菜谱出现次数）

口径：回退后仍无参考值、且不是默认常备的标准名，共 220 个（另有 6 个默认常备项无参考值，不列入）。次数为用到该标准名的菜谱数。

| # | 标准名 | 品类 | 菜谱数 |
| --- | --- | --- | --- |
| 1 | 干辣椒 | seasoning | 30 |
| 2 | 豆瓣酱 | seasoning | 30 |
| 3 | 香叶 | seasoning | 30 |
| 4 | 牛奶 | egg_dairy | 25 |
| 5 | 冰糖 | seasoning | 24 |
| 6 | 啤酒 | other | 11 |
| 7 | 辣椒油 | seasoning | 9 |
| 8 | 猪油 | staple | 8 |
| 9 | 汤底 | seasoning | 7 |
| 10 | 气泡水 | other | 6 |
| 11 | 火腿肠 | processed | 6 |
| 12 | 白芷 | seasoning | 6 |
| 13 | 花椒油 | seasoning | 6 |
| 14 | 豆腐乳 | soy | 6 |
| 15 | 豆豉 | seasoning | 6 |
| 16 | 方便面 | staple | 5 |
| 17 | 朗姆酒 | other | 5 |
| 18 | 枣 | fruit | 5 |
| 19 | 白豆蔻 | seasoning | 5 |
| 20 | 白酒 | other | 5 |
| 21 | 皮蛋 | egg_dairy | 5 |
| 22 | 粉丝 | staple | 5 |
| 23 | 香肠 | processed | 5 |
| 24 | 黄豆酱 | seasoning | 5 |
| 25 | 可乐 | other | 4 |
| 26 | 咖喱 | seasoning | 4 |
| 27 | 木耳 | vegetable | 4 |
| 28 | 泡椒 | processed | 4 |
| 29 | 火腿 | processed | 4 |
| 30 | 甜面酱 | seasoning | 4 |
| 31 | 芥末 | seasoning | 4 |
| 32 | 利口酒 | other | 3 |
| 33 | 奶酪 | egg_dairy | 3 |
| 34 | 年糕 | processed | 3 |
| 35 | 米粉 | staple | 3 |
| 36 | 糖浆 | other | 3 |
| 37 | 紫菜 | seafood | 3 |
| 38 | 草果 | seasoning | 3 |
| 39 | 豆皮 | soy | 3 |
| 40 | 金酒 | other | 3 |
| 41 | 陈皮 | seasoning | 3 |
| 42 | 韭菜 | vegetable | 3 |
| 43 | 饼 | staple | 3 |
| 44 | 黑椒酱 | seasoning | 3 |
| 45 | 伏特加 | other | 2 |
| 46 | 可可粉 | other | 2 |
| 47 | 味淋 | seasoning | 2 |
| 48 | 坚果 | other | 2 |
| 49 | 山奈 | seasoning | 2 |
| 50 | 山楂 | fruit | 2 |

### 抽样对照（40 条，种子 20261021）

| 标准名 | FoodKeeper 条目 | 冷藏 | 冷冻 | 常温 | 取值字段 |
| --- | --- | --- | --- | --- | --- |
| 丁香 | 237 Spice/spices (whole) | - | - | 1095 | room:DOP_Pantry_Min |
| 五香粉 | 233 Seasoning blends | - | - | 365 | room:DOP_Pantry_Min |
| 兔肉 | 595 Rabbit (whole, fresh) | 2 | 270 | - | fridge:Refrigerate_Min, freezer:Freeze_Min |
| 前腿肉 | 74 Pork (shoulder) | 3 | 120 | - | fridge:DOP_Refrigerate_Min, freezer:DOP_Freeze_Min |
| 大葱 | 295 Onions (spring or green) | 7 | 300 | - | fridge:DOP_Refrigerate_Min, freezer:DOP_Freeze_Min |
| 奶油奶酪 | 10 Cream cheese | 14 | - | - | fridge:DOP_Refrigerate_Min |
| 奶粉 | 465 Powdered milk | - | - | 365 | room:manual |
| 姜黄粉 | 238 Spice/spices (ground) | - | - | 730 | room:DOP_Pantry_Min |
| 尖椒 | 296 Peppers | 4 | 180 | - | fridge:DOP_Refrigerate_Min, freezer:DOP_Freeze_Min |
| 干木耳 | 385 Mushrooms (dried) | - | - | 365 | room:DOP_Pantry_Min |
| 意面酱 | 359 Spaghetti sauce (in jars) | - | - | 540 | room:DOP_Pantry_Min |
| 披萨 | 571 Pizza (frozen) | - | 360 | - | freezer:Freeze_Min |
| 挂面 | 335 Pasta (dry, without eggs) | - | - | 730 | room:DOP_Pantry_Min |
| 木瓜 | 265 Papaya, mango, feijoa, passionfruit, casaha melon | 7 | 180 | 3 | fridge:DOP_Refrigerate_Min, freezer:Freeze_Min, room:DOP_Pantry_Min |
| 杏 | 249 Apricots | 5 | - | - | fridge:Refrigerate_Max(ripe) |
| 杏仁 | 438 Almonds (no shell) | 240 | 300 | 90 | fridge:DOP_Refrigerate_Min, freezer:DOP_Freeze_Min, room:manual |
| 核桃 | 446 Walnuts (shell or no shell) | 270 | 720 | 90 | fridge:DOP_Refrigerate_Min, freezer:DOP_Freeze_Min, room:manual |
| 椰子 | 258 Coconuts (fresh) | 14 | - | 7 | fridge:DOP_Refrigerate_Min, room:DOP_Pantry_Min |
| 沃柑 | 256 Citrus fruit (lemon, lime, orange, grapefruit, tangerines, clementines) | 10 | - | 10 | fridge:DOP_Refrigerate_Min, room:DOP_Pantry_Min |
| 油麦菜 | 291 Lettuce (leaf, spinach) | 3 | - | - | fridge:DOP_Refrigerate_Min |
| 牛腩 | 39 Beef (chuck roast, brisket) | 3 | 120 | - | fridge:DOP_Refrigerate_Min, freezer:DOP_Freeze_Min |
| 猪肝 | 77 Variety meats (liver, tongue, chitterlings, etc.) | 2 | 90 | - | fridge:DOP_Refrigerate_Max, freezer:DOP_Freeze_Min |
| 番茄罐头 | 373 Canned goods (high acid (such as juices, fruit, pickles, sauerkraut, tomato soup, and foods in vinegar-based sauce)) | - | - | 360 | room:DOP_Pantry_Min |
| 白芝麻 | 613 Sesame seeds | - | - | 365 | room:manual |
| 紫甘蓝 | 278 Cabbage | 7 | 300 | - | fridge:DOP_Refrigerate_Min, freezer:DOP_Freeze_Min |
| 肉豆蔻 | 476 Nutmeg (ground, dried, commercially bottled or purchased in bulk) | - | - | 1095 | room:DOP_Pantry_Min |
| 胡萝卜 | 279 Carrots, parsnips | 14 | 300 | - | fridge:DOP_Refrigerate_Min, freezer:DOP_Freeze_Min |
| 芋头 | 305 Taro | - | - | 7 | room:DOP_Pantry_Min |
| 草莓 | 481 Strawberries | 3 | 240 | - | fridge:DOP_Refrigerate_Max, freezer:Freeze_Min |
| 荷兰豆 | 273 Beans and peas (green, fava, lima, soybean, wax, snow, sugar snap) | 3 | 240 | - | fridge:DOP_Refrigerate_Min, freezer:DOP_Freeze_Min |
| 葱 | 295 Onions (spring or green) | 7 | 300 | - | fridge:DOP_Refrigerate_Min, freezer:DOP_Freeze_Min |
| 蒜 | 285 Garlic | - | 30 | 30 | freezer:DOP_Freeze_Min, room:DOP_Pantry_Min |
| 藜麦 | 425 Quinoa (uncooked) | - | - | 730 | room:DOP_Pantry_Min |
| 虾仁 | 151 Shrimp, crayfish | 3 | 180 | - | fridge:DOP_Refrigerate_Max, freezer:DOP_Freeze_Min |
| 蚝油 | 434 Oyster sauce | - | - | 540 | room:DOP_Pantry_Min |
| 醋 | 361 Vinegar | - | - | 730 | room:DOP_Pantry_Min |
| 食用油 | 227 Oils (olive or vegetable) | - | - | 180 | room:DOP_Pantry_Min |
| 鸡腿 | 118 Chicken parts (legs or thighs) | 2 | 270 | - | fridge:DOP_Refrigerate_Max, freezer:DOP_Freeze_Min |
| 黄瓜 | 283 Cucumbers | 4 | - | - | fridge:DOP_Refrigerate_Min |
| 黑芝麻 | 613 Sesame seeds | - | - | 365 | room:manual |

### 人工修正值（28 个标准名）

| 标准名 | 修正的存放方式与天数 | 理由 |
| --- | --- | --- |
| 午餐肉 | room 730 | FoodKeeper 罐头肉制品为 5 年；国内罐头午餐肉标注多为 2 到 3 年，取 2 年 |
| 吐司 | room 7 | FoodKeeper 为 14 天；商超包装面包常温约一周，取 7 天 |
| 奶粉 | room 365 | FoodKeeper 为 3 年；国内罐装奶粉未开封标注多为 18 到 24 个月，取 1 年 |
| 姜 | room 7 | FoodKeeper 的 5 天偏短，取 7 天 |
| 嫩姜 | room 7 | FoodKeeper 的 5 天偏短，取 7 天 |
| 开心果 | room 90 | 坚果常温期限在 FoodKeeper 各条目间从 2 周到 6 个月不等，统一取 3 个月 |
| 杏仁 | room 90 | 坚果常温期限在 FoodKeeper 各条目间从 2 周到 6 个月不等，统一取 3 个月 |
| 松子 | room 90 | 坚果常温期限在 FoodKeeper 各条目间从 2 周到 6 个月不等，统一取 3 个月 |
| 核桃 | room 90 | 坚果常温期限在 FoodKeeper 各条目间从 2 周到 6 个月不等，统一取 3 个月 |
| 生抽 | room 730 | FoodKeeper 为 3 年；国内瓶装酱油未开封标注多为 2 年 |
| 生蚝 | fridge 2 | FoodKeeper 的 5 天对应刚买回的活贝；家庭冷藏取 2 天 |
| 白芝麻 | room 365 | FoodKeeper 为 5 年；芝麻含油易哈败，取 1 年 |
| 秋葵 | room 3 | 统一取 3 天 |
| 老抽 | room 730 | FoodKeeper 为 3 年；国内瓶装酱油未开封标注多为 2 年 |
| 腰果 | room 90 | 坚果常温期限在 FoodKeeper 各条目间从 2 周到 6 个月不等，统一取 3 个月 |
| 芝麻 | room 365 | FoodKeeper 为 5 年；芝麻含油易哈败，取 1 年 |
| 花生 | room 90 | 坚果常温期限在 FoodKeeper 各条目间从 2 周到 6 个月不等，统一取 3 个月 |
| 茄子 | room 3 | 统一取 3 天 |
| 葫芦瓜 | room 3 | 统一取 3 天 |
| 蛤蜊 | fridge 2 | FoodKeeper 的 5 天对应刚买回的活贝；家庭冷藏取 2 天 |
| 西瓜 | room 7 | 整个西瓜常温约一周 |
| 西红柿 | fridge 5 | 借用 Cherry tomatoes 的冷藏 5 天；西红柿本身在 FoodKeeper 中没有冷藏数字 |
| 西葫芦 | room 3 | 统一取 3 天 |
| 贻贝 | fridge 2 | FoodKeeper 的 5 天对应刚买回的活贝；家庭冷藏取 2 天 |
| 酱油 | room 730 | FoodKeeper 为 3 年；国内瓶装酱油未开封标注多为 2 年 |
| 面包 | room 7 | FoodKeeper 为 14 天；商超包装面包常温约一周，取 7 天 |
| 香菜 | fridge 5 | FoodKeeper 的 2 周偏长，取 5 天 |
| 黑芝麻 | room 365 | FoodKeeper 为 5 年；芝麻含油易哈败，取 1 年 |

### 仅适用于成熟果的冷藏值（9 个）

李子、杏、桃子、梨、火龙果、牛油果、猕猴桃、菠萝、香蕉

### 校验警告

无。

第一轮清单已被第二轮修订取代，见 git 历史。

## 存疑条目（审查依据）

以下记录当时的争议与适用性限制；后续复核统一见 [项目待办](../todo.md)，不在本报告维护执行状态。

**数字可能偏短**

- 小米 / 燕麦 / 荞麦常温 60 到 120 天：FoodKeeper 全谷物条目的常温期限很短。
- 鲜面条冷藏 2 天：FoodKeeper 的 Fresh pasta 面向店内现做；超市包装鲜面通常以包装日期为准，识别结果会覆盖。
- 猪心、猪肝、牛肚等内脏、肉末、禽肉冷藏 2 天：FoodKeeper 的 1 到 2 天取最大值，仍偏保守。

**数字可能偏长或口径不同**

- 酱油 / 生抽 / 老抽（730 天）、醋（730 天）、整粒香料（1095 天）、味噌（冷藏 365 天）、枫糖浆：均为未开封期限；开封后不使用（首版不处理），界面标"估计"可缓冲。
- 淡奶油只保留冷藏 10 天：FoodKeeper 的 Cream (heavy) 针对冷藏鲜奶油；国内常见的常温 UHT 款未开封远长于此，以包装日期为准。
- 九种水果的冷藏值针对成熟果（见上文），未成熟的要先常温催熟。

**近似对应**

- 肉类部位：五花肉 / 前腿肉 / 梅花肉 / 猪肉 → Pork (shoulder)（生猪肉各部位冷藏 3 天、冷冻 4 个月一致）；牛肉 → Beef (steaks)；羊肉 → Lamb (shoulder)；乌鸡 / 鸡肉 / 整鸡 → Chicken (whole)。
- 鱼种：鲫鱼、鳊鱼、鳜鱼、黑鱼、黄花鱼、鲷鱼、鳕鱼、龙利鱼 → 瘦鱼；三文鱼、金枪鱼、鲶鱼、黄颡鱼 → 肥鱼。国内淡水鱼不在 FoodKeeper 名单内，仅按脂肪含量近似；冷藏同为 2 天，只影响冷冻天数。
- 鸭蛋 / 鹅蛋 / 鹌鹑蛋 → Eggs (in shell) 21 天：同为带壳鲜蛋，按鸡蛋近似。
- 冬瓜 → Squash (winter)：切开的冬瓜明显短于整个。
- 芝麻酱 → Tahini：质地和用途相近，非同一产品。
- 干货香辛料：胡椒粉、姜粉等 → Spice (ground) 730 天；五香粉、十三香、椒盐、烧烤料 → Seasoning blends 365 天。

**词表问题（本任务不改词表，仅记录）**

- `木耳` 是干 / 鲜共用的泛称，而 `干木耳<木耳`、`干香菇<香菇` 的 parent 关系把干品挂在鲜品之下：回退时 `干X → X` 会得到鲜品期限（现在 干木耳、干香菇都有直接映射，不受影响）。建议词表里干品不要以鲜品为 parent，或回退时跳过。
- `芽菜` 含义有歧义（豆芽类嫩芽 / 宜宾芽菜），`豆芽` 已单列，建议明确。
- `面条` 泛称下有干面（挂面）和鲜面（鲜面条、拉面），回退时无法区分，所以参考表里 `面条` 留空。
- `奶酪` 泛称同理留空；`牛奶` 的包装日期是 FoodKeeper 的答案，参考表无数字，应由识别结果或 AI 给出。
- 高频食材中仍无参考值且值得 AI 补充的：牛奶、韭菜、猪油、火腿肠 / 香肠 / 火腿、豆腐乳 / 豆皮 / 豆腐干、皮蛋、粉丝 / 米粉、莲藕、莴笋。

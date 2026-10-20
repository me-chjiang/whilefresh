# 保质期参考表复核（T3）

分支 `feat/shelf-life-table`。数据源 USDA FoodKeeper v128（快照与说明见 `tools/sources/foodkeeper/`），生成物 `app/assets/data/shelf-life.json`，人工映射源数据 `tools/whilefresh_tools/shelf_life/foodkeeper-map.json`。本文件中的"覆盖率""高频无参考值""抽样对照""校验警告"四节由 `python -m whilefresh_tools.shelf_life report` 生成，其余为人工整理。

## 概要

- 参考表 290 个标准名（词表 544 个）；沿 `parent` 回退后覆盖 318 个，占 58.5%；按菜谱用料次数加权为 69.8%。
- 取值规则：每种存放方式取"从购买日起"的最短值（`DOP_*_Min`）；没有 `DOP_*` 数据时退到无前缀字段（记录在 `basis`），但不用"烹饪后 / 开封后"的期限，也不用"速冻购买食品"类的冷藏期限（那是解冻后）。非数值单位（成熟时、包装日期、无限期、不推荐、小时）一律留空。换算：周 × 7，月 × 30，年 × 365。
- DeepSeek 初稿：`deepseek-flash`（关闭思考）7 次请求，prompt 71587 + completion 11806 = 83393 tokens；初稿给出 322 个对应，222 个留空。之后逐条人工复核，最终 290 个。初稿和响应缓存在 `~/.cache/whilefresh/shelf-life/`，不入库；构建和测试不联网。
- 校验：所有键在词表中；天数为 1 到 3650 的整数；FoodKeeper ID 存在；冷冻天数不小于冷藏天数（当前无违反）；生成物与源数据一致（`validate` 检查陈旧）。

### 覆盖率

| 品类 | 标准名数 | 直接有参考值 | 直接占比 | 按 parent 回退后 | 回退后占比 |
| --- | --- | --- | --- | --- | --- |
| 蔬菜（vegetable） | 96 | 76 | 79.2% | 76 | 79.2% |
| 水果（fruit） | 42 | 31 | 73.8% | 31 | 73.8% |
| 肉类（meat） | 57 | 30 | 52.6% | 36 | 63.2% |
| 水产海鲜（seafood） | 58 | 23 | 39.7% | 40 | 69.0% |
| 蛋奶（egg_dairy） | 18 | 13 | 72.2% | 13 | 72.2% |
| 豆制品（soy） | 10 | 1 | 10.0% | 1 | 10.0% |
| 主食粮油（staple） | 56 | 31 | 55.4% | 34 | 60.7% |
| 调料（seasoning） | 97 | 53 | 54.6% | 55 | 56.7% |
| 加工/速食（processed） | 62 | 15 | 24.2% | 15 | 24.2% |
| 其他（other） | 48 | 17 | 35.4% | 17 | 35.4% |
| 合计 | 544 | 290 | 53.3% | 318 | 58.5% |

按菜谱用料出现次数加权（不含默认常备项，回退后）：1094 / 1567，69.8%。

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
| 6 | 西红柿 | vegetable | 22 |
| 7 | 啤酒 | other | 11 |
| 8 | 辣椒油 | seasoning | 9 |
| 9 | 猪油 | staple | 8 |
| 10 | 汤底 | seasoning | 7 |
| 11 | 气泡水 | other | 6 |
| 12 | 火腿肠 | processed | 6 |
| 13 | 白芷 | seasoning | 6 |
| 14 | 花椒油 | seasoning | 6 |
| 15 | 豆腐乳 | soy | 6 |
| 16 | 豆豉 | seasoning | 6 |
| 17 | 方便面 | staple | 5 |
| 18 | 朗姆酒 | other | 5 |
| 19 | 枣 | fruit | 5 |
| 20 | 白豆蔻 | seasoning | 5 |
| 21 | 白酒 | other | 5 |
| 22 | 皮蛋 | egg_dairy | 5 |
| 23 | 粉丝 | staple | 5 |
| 24 | 香肠 | processed | 5 |
| 25 | 黄豆酱 | seasoning | 5 |
| 26 | 可乐 | other | 4 |
| 27 | 咖喱 | seasoning | 4 |
| 28 | 木耳 | vegetable | 4 |
| 29 | 泡椒 | processed | 4 |
| 30 | 火腿 | processed | 4 |
| 31 | 甜面酱 | seasoning | 4 |
| 32 | 芥末 | seasoning | 4 |
| 33 | 利口酒 | other | 3 |
| 34 | 奶酪 | egg_dairy | 3 |
| 35 | 年糕 | processed | 3 |
| 36 | 米粉 | staple | 3 |
| 37 | 糖浆 | other | 3 |
| 38 | 紫菜 | seafood | 3 |
| 39 | 草果 | seasoning | 3 |
| 40 | 豆皮 | soy | 3 |
| 41 | 金酒 | other | 3 |
| 42 | 陈皮 | seasoning | 3 |
| 43 | 韭菜 | vegetable | 3 |
| 44 | 饼 | staple | 3 |
| 45 | 黑椒酱 | seasoning | 3 |
| 46 | 伏特加 | other | 2 |
| 47 | 可可粉 | other | 2 |
| 48 | 味淋 | seasoning | 2 |
| 49 | 坚果 | other | 2 |
| 50 | 山奈 | seasoning | 2 |

### 抽样对照（40 条，种子 20261021）

| 标准名 | FoodKeeper 条目 | 冷藏 | 冷冻 | 常温 | 取值字段 |
| --- | --- | --- | --- | --- | --- |
| 丁香 | 237 Spice/spices (whole) | - | - | 1095 | room:DOP_Pantry_Min |
| 五香粉 | 233 Seasoning blends | - | - | 365 | room:DOP_Pantry_Min |
| 兔肉 | 595 Rabbit (whole, fresh) | 2 | 270 | - | fridge:Refrigerate_Min, freezer:Freeze_Min |
| 前腿肉 | 74 Pork (shoulder) | 3 | 120 | - | fridge:DOP_Refrigerate_Min, freezer:DOP_Freeze_Min |
| 大葱 | 295 Onions (spring or green) | 7 | 300 | - | fridge:DOP_Refrigerate_Min, freezer:DOP_Freeze_Min |
| 奶油奶酪 | 10 Cream cheese | 14 | - | - | fridge:DOP_Refrigerate_Min |
| 奶粉 | 465 Powdered milk | - | - | 1095 | room:Pantry_Min |
| 姜黄粉 | 238 Spice/spices (ground) | - | - | 730 | room:DOP_Pantry_Min |
| 尖椒 | 296 Peppers | 4 | 180 | - | fridge:DOP_Refrigerate_Min, freezer:DOP_Freeze_Min |
| 干木耳 | 385 Mushrooms (dried) | - | - | 365 | room:DOP_Pantry_Min |
| 意面酱 | 359 Spaghetti sauce (in jars) | - | - | 540 | room:DOP_Pantry_Min |
| 披萨 | 571 Pizza (frozen) | - | 360 | - | freezer:Freeze_Min |
| 挂面 | 335 Pasta (dry, without eggs) | - | - | 730 | room:DOP_Pantry_Min |
| 木瓜 | 265 Papaya, mango, feijoa, passionfruit, casaha melon | 7 | 180 | 3 | fridge:DOP_Refrigerate_Min, freezer:Freeze_Min, room:DOP_Pantry_Min |
| 杏 | 249 Apricots | 2 | - | - | fridge:Refrigerate_Min |
| 杏仁 | 438 Almonds (no shell) | 240 | 300 | 120 | fridge:DOP_Refrigerate_Min, freezer:DOP_Freeze_Min, room:DOP_Pantry_Min |
| 树莓 | 482 Raspberries | 2 | 240 | - | fridge:DOP_Refrigerate_Min, freezer:Freeze_Min |
| 椒盐 | 233 Seasoning blends | - | - | 365 | room:DOP_Pantry_Min |
| 毛豆 | 273 Beans and peas (green, fava, lima, soybean, wax, snow, sugar snap) | 3 | 240 | - | fridge:DOP_Refrigerate_Min, freezer:DOP_Freeze_Min |
| 油菜 | 275 Bok choy | 2 | 300 | - | fridge:DOP_Refrigerate_Min, freezer:DOP_Freeze_Min |
| 牛肚 | 77 Variety meats (liver, tongue, chitterlings, etc.) | 1 | 90 | - | fridge:DOP_Refrigerate_Min, freezer:DOP_Freeze_Min |
| 猪肉末 | 73 Pork (ground) | 1 | 90 | - | fridge:DOP_Refrigerate_Min, freezer:DOP_Freeze_Min |
| 生蚝 | 160 Fresh clams, mussels, oysters | 5 | - | - | fridge:DOP_Refrigerate_Min |
| 白胡椒 | 238 Spice/spices (ground) | - | - | 730 | room:DOP_Pantry_Min |
| 糯米 | 338 Rice (white or wild) | - | - | 730 | room:DOP_Pantry_Min |
| 老抽 | 360 Soy sauce or teriyaki sauce | - | - | 1095 | room:DOP_Pantry_Min |
| 胡椒粉 | 238 Spice/spices (ground) | - | - | 730 | room:DOP_Pantry_Min |
| 腰果 | 439 Cashews (no shell) | 180 | 360 | 14 | fridge:DOP_Refrigerate_Min, freezer:DOP_Freeze_Min, room:DOP_Pantry_Min |
| 茼蒿 | 287 Greens | 1 | 300 | - | fridge:DOP_Refrigerate_Min, freezer:DOP_Freeze_Min |
| 荠菜 | 287 Greens | 1 | 300 | - | fridge:DOP_Refrigerate_Min, freezer:DOP_Freeze_Min |
| 葫芦瓜 | 302 Squash (summer and zucchini) | 4 | 300 | 1 | fridge:DOP_Refrigerate_Min, freezer:DOP_Freeze_Min, room:DOP_Pantry_Min |
| 葵花籽油 | 522 Sunflower oil | - | - | 365 | room:DOP_Pantry_Min |
| 薯片 | 392 Potato chips | - | - | 60 | room:DOP_Pantry_Min |
| 虾 | 151 Shrimp, crayfish | 1 | 180 | - | fridge:DOP_Refrigerate_Min, freezer:DOP_Freeze_Min |
| 蚕豆 | 273 Beans and peas (green, fava, lima, soybean, wax, snow, sugar snap) | 3 | 240 | - | fridge:DOP_Refrigerate_Min, freezer:DOP_Freeze_Min |
| 醋 | 361 Vinegar | - | - | 730 | room:DOP_Pantry_Min |
| 食用油 | 227 Oils (olive or vegetable) | - | - | 180 | room:DOP_Pantry_Min |
| 鸡腿 | 118 Chicken parts (legs or thighs) | 1 | 270 | - | fridge:DOP_Refrigerate_Min, freezer:DOP_Freeze_Min |
| 黄瓜 | 283 Cucumbers | 4 | - | - | fridge:DOP_Refrigerate_Min |
| 黑芝麻 | 613 Sesame seeds | - | - | 1825 | room:Pantry_Min |

### 校验警告

无。

## 人工修正清单

相对 DeepSeek 初稿（322 个对应）的改动：删除 37 个，新增 5 个，改选 1 个，限制取值 5 个。

**删除（对应不合理，留空交给 AI）**

- 品类或加工方式不同：冻豆腐（冷冻豆腐 ≠ 豆腐）、酸菜（≠ 韩式泡菜）、泡椒（腌制品，被套成鲜辣椒）、茶树菇（多为干品）、木耳（词表泛称，常指干木耳；干木耳已单列）、坚果（泛称，品种间常温期限从 2 周到 6 个月，回退不可靠）、奶酪（泛称，硬质奶酪 180 天不能代表奶酪棒等软质或再制品）、汤底（可能是液态高汤）、芥末（≠ 美式调制芥末酱）。
- 物种或制法不同：山药（≠ 红薯类 Yams）、丝瓜、苦瓜（≠ 夏南瓜）、粘米粉、糯米粉（≠ 白面粉）、土豆淀粉、红薯淀粉、木薯粉（≠ 玉米淀粉 / 珍珠西米）、麦芽糖（≠ 糖蜜）、糖浆（泛称）、香醋（≠ 意大利香醋）、葡萄酒（酒类未开封期限与本应用无关）。
- 面条类：面条、刀削面、拉面、乌冬面。干面与鲜面保质期相差极大，泛称无法代表；保留挂面、意面（干）和鲜面条（鲜）。
- 鱼类：鲳鱼、鳗鱼、黄鳝不在 FoodKeeper 的瘦鱼 / 肥鱼名单内且鱼种差异大；它们仍可通过 `parent` 回退到"鱼"。
- 其他：西红柿（FoodKeeper 只有冷冻 2 个月，冷藏和常温均为"成熟时"）、牛奶（冷藏为"包装日期"，只有冷冻数字）、芥蓝（≠ 叶菜类 Greens 的 1 天）、芽菜（可能是宜宾芽菜这类腌菜）、白芷、山奈、砂仁、白豆蔻、草果（药食香料，"整粒香料"条目只适合明确的香辛料）。

**新增**

- 鱼 → 146 Fatty fish：泛称，仅作各鱼种的回退。冷藏各类鱼都是 1 天，冷冻取较短的肥鱼条目（保守）。
- 虾仁 → 151 Shrimp：去壳生虾仁与带壳生虾冷藏期限相同。
- 黑米 → 339 Rice (brown)：带麸皮的全谷米近似。
- 鹰嘴豆 → 333 Beans (dried)：干豆。
- 午餐肉 → 111 Meat products (canned)：罐头肉制品；包装有到期日时以包装为准。

**改选**

- 枫糖浆：由玻璃瓶（533，4 年）改选塑料瓶（534，2 年），玻璃与塑料不分时取较短者。

**限制取值（`skip`）**

- 葱、大葱、小葱、蒜苗：不用常温期限。FoodKeeper 的"葱 常温 1 个月"不符合常识。
- 蒜：不用冷藏期限。FoodKeeper 的 3 天对应去皮或切好的蒜；整头蒜常温保存。

## 存疑条目（需要主会话或用户判断）

**数字可能偏短（偏保守，可能让用户过早看到"临期"）**

- 绿叶菜类（空心菜、茼蒿、荠菜、苦菊、芥菜、芝麻菜、莴笋叶）冷藏 1 天：FoodKeeper "Greens" 为 1–4 天，取最短值。
- 鲜面条 1 天：FoodKeeper 的 Fresh pasta 面向店内现做；超市包装鲜面通常以包装日期为准，实际识别结果会覆盖。
- 玉米 1 天、淡奶油 10 天（国内常见为常温 UHT 淡奶油，未开封远长于此）。
- 小米 / 燕麦 / 荞麦常温 60–120 天：FoodKeeper 全谷物条目的常温期限很短。
- 盒装果汁常温 21 天：同样出自 FoodKeeper，国内常温盒装果汁未开封一般更长。

**数字可能偏长**

- 白芝麻、黑芝麻、芝麻常温 1825 天，午餐肉 1825 天：按 FoodKeeper 原值。芝麻在高温环境下会哈败。
- 酱油 / 生抽 / 老抽 1095 天、醋 730 天：均为未开封期限；开封后不使用（首版不处理），界面标"估计"可缓冲。

**近似对应**

- 水果的冷藏期限（杏、牛油果、猕猴桃、桃、梨、李、香蕉、菠萝等）在 FoodKeeper 中针对"成熟果"，未成熟的果子要先常温催熟，此时冷藏期限偏短不适用。
- 肉类部位：五花肉 / 前腿肉 / 梅花肉 / 猪肉 → Pork (shoulder)（生猪肉各部位冷藏 3 天、冷冻 4 个月一致）；牛肉 → Beef (steaks)；羊肉 → Lamb (shoulder)；乌鸡 / 鸡肉 / 整鸡 → Chicken (whole)。猪心、猪肝、牛肚 → Variety meats（1 天，偏短）。
- 鱼种：鲫鱼、鳊鱼、鳜鱼、黑鱼、黄花鱼、鲷鱼、鳕鱼、龙利鱼 → 瘦鱼；三文鱼、金枪鱼、鲶鱼、黄颡鱼 → 肥鱼。国内淡水鱼不在 FoodKeeper 名单内，仅按脂肪含量近似；冷藏同为 1 天，只影响冷冻天数。
- 鸭蛋 / 鹅蛋 / 鹌鹑蛋 → Eggs (in shell) 21 天：同为带壳鲜蛋，按鸡蛋近似。
- 冬瓜 → Squash (winter)：切开的冬瓜明显短于整个。
- 芝麻酱 → Tahini：质地和用途相近，非同一产品。
- 香辛料：八角、桂皮、花椒、孜然等 → Spice (whole) 1095 天；胡椒粉、姜粉等 → Spice (ground) 730 天；五香粉、十三香、椒盐、烧烤料 → Seasoning blends 365 天。均为干货未开封。

**词表问题（本任务不改词表，仅记录）**

- `木耳` 是干 / 鲜共用的泛称，而 `干木耳<木耳`、`干香菇<香菇` 的 parent 关系把干品挂在鲜品之下：回退时 `干X → X` 会得到鲜品期限（现在 干木耳、干香菇都有直接映射，不受影响）。建议词表里干品不要以鲜品为 parent，或回退时跳过。
- `芽菜` 含义有歧义（豆芽类嫩芽 / 宜宾芽菜），`豆芽` 已单列，建议明确。
- `面条` 泛称下有干面（挂面）和鲜面（鲜面条、拉面），回退时无法区分，所以参考表里 `面条` 留空。
- `奶酪` 泛称同理留空；`牛奶` 的包装日期是 FoodKeeper 的答案，参考表无数字，应由识别结果或 AI 给出。
- 高频食材中仍无参考值且值得 AI 补充的：西红柿、牛奶、韭菜、猪油、火腿肠 / 香肠 / 火腿、豆腐乳 / 豆皮 / 豆腐干、皮蛋、粉丝 / 米粉、莲藕、莴笋。
- 抽样检查（见上表）未发现数字与条目不符；`椒盐 → Seasoning blends`、`黑芝麻 → Sesame seeds 1825` 是上面已列的近似。

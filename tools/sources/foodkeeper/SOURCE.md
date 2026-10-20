# USDA FoodKeeper 数据快照

保质期参考表（`app/assets/data/shelf-life.json`）的数据源。快照入库是因为官网对部分网络环境返回 403，且需要固定版本。

| 项目 | 内容 |
| --- | --- |
| 名称 | FSIS FoodKeeper Data（美国农业部食品安全检验局） |
| 数据目录页 | https://catalog.data.gov/dataset/fsis-foodkeeper-data |
| 版本 | v128（`Version` 表中 `Current_Version` 为 Yes，修改日期 2018-09-06） |
| 原始文件 | `FMA-Data-v128.xlsx` |
| 本目录文件 | `foodkeeper-v128.json`：该 xlsx 的 JSON 导出，`fileName` 字段保留原文件名 |
| 获取方式 | 2026-10-20 经 Lexmount Web Fetch API 取得（直接访问官网返回 403） |
| 获取日期 | 2026-10-20 |
| sha256 | `04874864cad0629cbd50b017514d4099a25597f2aa3c14e7585dc83147a815fa` |
| 许可 | CC0 1.0 公有领域（https://creativecommons.org/publicdomain/zero/1.0/），可随 APK 再分发 |

文件内容未做任何修改。重新核对：`shasum -a 256 foodkeeper-v128.json`。

## 结构

`sheets[]` 含 `Version`、`Category`、`Product`（661 行）、`CookingTips`、`CookingMethods`、`Data Dictionary`。每行是一个单键对象的列表（`[{"ID": 1.0}, {"Name": "Butter"}, ...]`），值为空的列通常整列省略。

`Product` 表主要列：

- `ID`、`Category_ID`（对应 `Category` 表）、`Name`、`Name_subtitle`、`Keywords`。
- 三种存放方式，每种有一组 `_Min`、`_Max`、`_Metric`（时间单位）、`_tips`：
  - 常温：`Pantry_*`、`DOP_Pantry_*`、`Pantry_After_Opening_*`
  - 冷藏：`Refrigerate_*`、`DOP_Refrigerate_*`、`Refrigerate_After_Opening_*`、`Refrigerate_After_Thawing_*`
  - 冷冻：`Freeze_*`、`DOP_Freeze_*`
- `DOP_` 前缀（Date Of Purchase）表示"从购买日起"的期限；无前缀的是 FoodKeeper 的通用期限，没有注明起算点。`After_Opening`、`After_Thawing` 为开封后、解冻后的期限。
- `_Metric` 取值：`Days`、`Weeks`、`Months`、`Years`、`Hours`，以及非数值的 `When Ripe`、`Package use-by date`、`Indefinitely`、`Not Recommended`。

## 参考表取值规则

构建脚本 `tools/whilefresh_tools/shelf_life/`（命令见仓库 README）按以下规则取值，规则以代码和测试为准：

1. 对冷藏、冷冻、常温分别取值。优先 `DOP_<存放方式>_Min`（从购买日起的最短值，偏保守）。
2. 该存放方式没有任何 `DOP_` 数据时，退到无前缀的 `<存放方式>_Min`，并在生成物 `basis` 中记录实际字段。以下情况不退：冷藏 `Refrigerate_tips` 注明"烹饪后 / 开封后 / 饮用前"的期限；"速冻购买食品"类（`Category_ID` = 8）的冷藏期限（指解冻或烹饪后）。
3. `_Metric` 不是数值单位（`When Ripe`、`Package use-by date`、`Indefinitely`、`Not Recommended`、`Hours`）或缺 `_Min` 时，该存放方式留空。
4. 开封后、解冻后的字段不使用。
5. 换算为天：Days × 1，Weeks × 7，Months × 30，Years × 365。

美式条目与中国食材的对应关系由人工维护的 `foodkeeper-map.json` 决定，快照本身不含中文。

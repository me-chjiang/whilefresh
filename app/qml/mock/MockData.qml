pragma Singleton
import QtQuick

QtObject {
    id: mock

    readonly property string previewLabel: "演示 · 界面预览（模拟数据）"
    property bool demoMode: true
    property var items: [
        { id: 1, name: "五花肉", quantity: "500g", category: "meat", location: "冷藏", freshness: "nearing", source: "reference", days: "约剩 1 天", first: true, purchasedAt: "2026-10-20", storedAt: "2026-10-20", producedAt: "", packageExpiresAt: "", shelfDays: "5" },
        { id: 2, name: "羊肉火锅片", quantity: "300g", category: "meat", location: "冷藏", freshness: "fresh", source: "reference", days: "约剩 3 天", first: false, purchasedAt: "2026-10-20", storedAt: "2026-10-20", producedAt: "", packageExpiresAt: "", shelfDays: "5" },
        { id: 3, name: "鸡蛋", quantity: "10枚", category: "egg_dairy", location: "冷藏", freshness: "fresh", source: "package", days: "11-02 到期", first: true, purchasedAt: "2026-10-20", storedAt: "2026-10-20", producedAt: "", packageExpiresAt: "2026-11-02", shelfDays: "" },
        { id: 4, name: "茭白", quantity: "2根", category: "vegetable", location: "冷藏", freshness: "nearing", source: "reference", days: "约剩 2 天", first: false, purchasedAt: "2026-10-20", storedAt: "2026-10-20", producedAt: "", packageExpiresAt: "", shelfDays: "5" },
        { id: 5, name: "白蘑菇", quantity: "200g", category: "vegetable", location: "冷藏", freshness: "overdue", source: "ai", days: "请查看后决定", first: false, purchasedAt: "2026-10-20", storedAt: "2026-10-20", producedAt: "", packageExpiresAt: "", shelfDays: "3" },
        { id: 6, name: "长尖椒", quantity: "150g", category: "vegetable", location: "冷藏", freshness: "fresh", source: "reference", days: "约剩 6 天", first: true, purchasedAt: "2026-10-20", storedAt: "2026-10-20", producedAt: "", packageExpiresAt: "", shelfDays: "7" },
        { id: 7, name: "蓝莓", quantity: "125g", category: "fruit", location: "冷藏", freshness: "nearing", source: "reference", days: "约剩 2 天", first: false, purchasedAt: "2026-10-20", storedAt: "2026-10-20", producedAt: "", packageExpiresAt: "", shelfDays: "5" },
        { id: 8, name: "冻虾仁", quantity: "300g", category: "seafood", location: "冷冻", freshness: "fresh", source: "reference", days: "约剩 80 天", first: false, purchasedAt: "2026-10-20", storedAt: "2026-10-20", producedAt: "", packageExpiresAt: "", shelfDays: "90" },
        { id: 9, name: "掌中宝", quantity: "500g", category: "meat", location: "冷冻", freshness: "fresh", source: "reference", days: "约剩 60 天", first: false, purchasedAt: "2026-10-20", storedAt: "2026-10-20", producedAt: "", packageExpiresAt: "", shelfDays: "90" },
        { id: 10, name: "山药", quantity: "1根", category: "staple", location: "常温", freshness: "fresh", source: "reference", days: "约剩 12 天", first: false, purchasedAt: "2026-10-20", storedAt: "2026-10-20", producedAt: "", packageExpiresAt: "", shelfDays: "15" }
    ]
    property var selectedIds: [1, 3, 4, 6]
    property var shopping: []
    property var pantry: [
        { name: "盐", category: "seasoning", bulk: false }, { name: "生抽", category: "seasoning", bulk: false },
        { name: "菜籽油", category: "seasoning", bulk: false }, { name: "葱", category: "vegetable", bulk: false },
        { name: "姜", category: "vegetable", bulk: false }, { name: "蒜", category: "vegetable", bulk: false },
        { name: "鸡蛋", category: "egg_dairy", bulk: true }
    ]
    property var suggestions: [{ name: "生抽", note: "识别到新购入，是否更新常备状态" }]
    property var rules: [{ name: "五花肉", location: "冷藏", days: "5", source: "你的设置" }]
    property var candidates: [
        { name: "五花肉", quantity: "500g", category: "meat", location: "冷藏", source: "reference", shelfDays: "5", purchasedAt: "2026-10-20", storedAt: "2026-10-20", producedAt: "", packageExpiresAt: "", group: "food" },
        { name: "羊肉火锅片", quantity: "300g", category: "meat", location: "冷冻", source: "reference", shelfDays: "90", purchasedAt: "2026-10-20", storedAt: "2026-10-20", producedAt: "", packageExpiresAt: "", group: "food" },
        { name: "鸡蛋", quantity: "10枚", category: "egg_dairy", location: "冷藏", source: "package", shelfDays: "", purchasedAt: "2026-10-20", storedAt: "2026-10-20", producedAt: "", packageExpiresAt: "2026-11-02", group: "food" },
        { name: "西兰花", quantity: "1颗", category: "vegetable", location: "冷藏", source: "ai", shelfDays: "5", purchasedAt: "", storedAt: "2026-10-20", producedAt: "", packageExpiresAt: "", group: "food" },
        { name: "冻虾仁", quantity: "300g", category: "seafood", location: "冷冻", source: "reference", shelfDays: "90", purchasedAt: "2026-10-20", storedAt: "2026-10-20", producedAt: "", packageExpiresAt: "", group: "food" },
        { name: "掌中宝", quantity: "500g", category: "meat", location: "冷冻", source: "reference", shelfDays: "90", purchasedAt: "", storedAt: "2026-10-20", producedAt: "", packageExpiresAt: "", group: "food" },
        { name: "生抽", quantity: "1瓶", category: "seasoning", group: "seasoning" },
        { name: "蚝油", quantity: "1瓶", category: "seasoning", group: "seasoning" },
        { name: "白砂糖", quantity: "1袋", category: "seasoning", group: "seasoning" },
        { name: "纸巾", group: "ignored" }, { name: "配送费", group: "ignored" },
        { name: "推荐商品", group: "ignored" }, { name: "优惠", group: "ignored" },
        { name: "日用品", group: "ignored" }, { name: "满减提示", group: "ignored" }
    ]
    property var imports: [{ id: 1, name: "演示订单截图 · 2 张", date: "2026-10-20" }]
    property var recipes: [{"id": "meat_dish/辣椒炒肉", "name": "辣椒炒肉", "sourceUrl": "https://github.com/Anduin2017/HowToCook/blob/0e106eea348c734fe1586f781202994439cf98a5/dishes/meat_dish/%E8%BE%A3%E6%A4%92%E7%82%92%E8%82%89.md", "ingredients": [{"raw": "青椒（吃辣的话推荐用杭椒，螺丝椒，不吃辣的用尖椒，甜椒）", "has": true, "optional": false, "category": "other"}, {"raw": "猪瘦肉", "has": false, "optional": false, "category": "other"}, {"raw": "盐", "has": true, "optional": false, "category": "other"}, {"raw": "生抽", "has": true, "optional": false, "category": "other"}, {"raw": "蚝油", "has": false, "optional": false, "category": "other"}, {"raw": "大蒜", "has": true, "optional": false, "category": "other"}, {"raw": "姜", "has": true, "optional": false, "category": "other"}, {"raw": "酱油（可选）", "has": false, "optional": true, "category": "other"}, {"raw": "豆豉（可选）", "has": false, "optional": true, "category": "other"}], "steps": ["1. 将`青椒`洗净，去除`青椒把`以及`青椒籽`，再用`滚刀手法`切好备用。", "2. `大蒜`用刀拍一下，再横切成`蒜瓣`，`生姜`切碎成`姜末`。", "3. 将`猪瘦肉`切成`肉片`（顺着猪肉的纹理切，即刀和肉的纹理呈水平线，出来的肉片，纹路呈“川”字）。", "4. 将切好的`猪肉`洗净，放入空碗，再加入计算好的`生抽`、`蚝油`、`盐`搅拌均匀，腌制 10 分钟。", "5. 热锅，不用倒油，把`切好的青椒`放入锅中，大火干煸至虎皮状后，再加 2g`盐`继续翻炒 1 分钟 后捞起。", "6. 不用洗锅，大火热锅，加入份数 * 8ml`油`，等待 30s，加入`蒜瓣`、`姜末`翻炒 15s。", "7. 加入腌制好的`猪肉`倒入锅内翻炒 2 分钟，再加入干煸过的`青椒`翻炒 1 分钟。", "8. 根据个人口味喜好加入`豆豉`，最后加入`酱油`，继续翻炒 30s。", "9. 出锅，盛盘。"], "difficulty": 3, "haveCount": 5, "missingCount": 2}, {"id": "meat_dish/茭白炒肉/茭白炒肉", "name": "茭白炒肉", "sourceUrl": "https://github.com/Anduin2017/HowToCook/blob/0e106eea348c734fe1586f781202994439cf98a5/dishes/meat_dish/%E8%8C%AD%E7%99%BD%E7%82%92%E8%82%89/%E8%8C%AD%E7%99%BD%E7%82%92%E8%82%89.md", "ingredients": [{"raw": "茭白", "has": true, "optional": false, "category": "other"}, {"raw": "瘦肉", "has": false, "optional": false, "category": "other"}, {"raw": "淀粉", "has": false, "optional": false, "category": "other"}, {"raw": "食用油", "has": true, "optional": false, "category": "other"}, {"raw": "鸡精", "has": false, "optional": false, "category": "other"}, {"raw": "姜", "has": true, "optional": false, "category": "other"}, {"raw": "蒜", "has": true, "optional": false, "category": "other"}, {"raw": "料酒", "has": false, "optional": false, "category": "other"}, {"raw": "盐", "has": true, "optional": false, "category": "other"}], "steps": ["### 准备", "1. 茭白切片，每片厚度 0.5 cm", "2. 瘦肉切条，厚度 0.3-0.5 cm，加入料酒、生粉、盐、水搅拌", "3. 姜切片、蒜头剁碎", "### 制作", "1. 起锅水烧开，放入茭白，水煮 60-90 S 后取出沥干", "2. 起锅，倒入 15 ml 油，倒入瘦肉，反复翻炒 60 S 取出", "3. 起锅，倒入 15 ml 油，倒入姜、蒜翻炒 30S，加入茭白继续翻炒 30 S", "4. 继续加入瘦肉翻炒 60 S，加入 20 ml 水，加入盐、鸡精后翻炒 60S 出锅"], "difficulty": 3, "haveCount": 5, "missingCount": 4}, {"id": "meat_dish/尖椒炒牛肉", "name": "尖椒炒牛肉", "sourceUrl": "https://github.com/Anduin2017/HowToCook/blob/0e106eea348c734fe1586f781202994439cf98a5/dishes/meat_dish/%E5%B0%96%E6%A4%92%E7%82%92%E7%89%9B%E8%82%89.md", "ingredients": [{"raw": "牛肉", "has": false, "optional": false, "category": "other"}, {"raw": "葱", "has": true, "optional": false, "category": "other"}, {"raw": "姜", "has": true, "optional": false, "category": "other"}, {"raw": "蒜", "has": true, "optional": false, "category": "other"}, {"raw": "尖椒", "has": false, "optional": false, "category": "other"}, {"raw": "酱油", "has": false, "optional": false, "category": "other"}, {"raw": "盐", "has": true, "optional": false, "category": "other"}, {"raw": "糖", "has": false, "optional": false, "category": "other"}], "steps": ["### 配菜处理", "1. 蒜剁成蒜泥", "2. 葱切段", "3. 姜切成姜片", "4. 尖椒切成段", "### 牛肉的预处理", "1. 牛肉放入碗中", "2. 加姜、盐、酱油、糖进行腌制 30-40 分钟", "3. 腌制完姜可以去掉", "### 炒熟", "1. 冷油下锅，待油变热至偶有气泡", "2. 加入蒜泥", "3. 蒜泥变金黄后加入尖椒", "4. 待尖椒表皮微皱，加入腌制好的牛肉翻炒", "5. 翻炒变熟之前加入葱，继续翻炒", "6. 翻炒至牛肉变熟，关火出锅"], "difficulty": 3, "haveCount": 4, "missingCount": 4}, {"id": "breakfast/蒸水蛋", "name": "蒸水蛋", "sourceUrl": "https://github.com/Anduin2017/HowToCook/blob/0e106eea348c734fe1586f781202994439cf98a5/dishes/breakfast/%E8%92%B8%E6%B0%B4%E8%9B%8B.md", "ingredients": [{"raw": "新鲜鸡蛋", "has": true, "optional": false, "category": "other"}, {"raw": "热水", "has": false, "optional": false, "category": "other"}, {"raw": "锡纸或保鲜膜", "has": false, "optional": false, "category": "other"}], "steps": ["1. 鸡蛋打入碗中，打散", "2. 取其他容器，倒入 1.5 倍（半个蛋壳为 0.5 倍水）于蛋液的温水（温度 20~30），将盐倒入水中化开", "3. 将盐水倒入鸡蛋液中，顺时针或逆时针单方向搅拌均匀，气泡之类的可以用舀出丢弃，过筛则口感更加。", "4. 使用锡纸包裹盛蛋液的碗（或用盘子盖住），置入提前带盖并加入大约 3cm 深度水的锅中", "5. 中火烧至水开，转最小的火继续蒸 4 分钟"], "difficulty": 2, "haveCount": 1, "missingCount": 2}, {"id": "staple/蛋炒饭", "name": "蛋炒饭", "sourceUrl": "https://github.com/Anduin2017/HowToCook/blob/0e106eea348c734fe1586f781202994439cf98a5/dishes/staple/%E8%9B%8B%E7%82%92%E9%A5%AD.md", "ingredients": [{"raw": "冷饭", "has": false, "optional": false, "category": "other"}, {"raw": "鸡蛋", "has": true, "optional": false, "category": "other"}, {"raw": "火腿", "has": false, "optional": false, "category": "other"}, {"raw": "黄瓜", "has": false, "optional": false, "category": "other"}, {"raw": "胡萝卜", "has": false, "optional": false, "category": "other"}, {"raw": "油", "has": true, "optional": false, "category": "other"}, {"raw": "盐", "has": true, "optional": false, "category": "other"}, {"raw": "胡椒粉", "has": false, "optional": false, "category": "other"}, {"raw": "生抽", "has": true, "optional": false, "category": "other"}, {"raw": "香葱", "has": false, "optional": false, "category": "other"}, {"raw": "灯影牛肉丝/午餐肉/腊肠/卤肉...等熟肉（备选）", "has": false, "optional": false, "category": "other"}], "steps": ["1. 米饭提前用铲子铲成小块", "2. 火腿肠、胡萝卜、黄瓜等根据需求切片或者块状", "3. 如果家里有熟肉 准备好味道更佳", "4. 将蛋白，蛋黄分开，分别打入一个大碗里，各自搅匀。注意，不要在这一步加盐。", "5. 大火热锅，待锅里冒烟放入食用油，放入蛋白，待主体凝固后盛出备用。", "6. 如果油够，则直接放入蛋黄，如果油不够则放入食用油并等其升温到大火热锅", "7. 待主体凝固后，将火调至中小火，倒入火腿肠、熟肉，胡萝卜、黄瓜等备料、翻炒 10 秒钟（到爆香）", "8. 重新倒入蛋白，翻炒 5s 钟，迅速倒入米饭大火翻炒，为的就是每一粒饭都裹上鸡蛋。", "9. 翻炒过程中将米饭的块状捣碎、这一步过程会比较长、待米饭全部捣碎再翻炒均匀即可", "10. 调至小火、加盐、胡椒粉、生抽", "11. 进一步翻炒均匀，能看到一些米饭在锅里有“跳起来”的时候其实就已经差不多了", "12. 最后倒入香葱再翻炒 10s", "13. 关火、盛入碗中"], "difficulty": 3, "haveCount": 4, "missingCount": 7}, {"id": "vegetable_dish/韭菜炒蛋/韭菜炒蛋", "name": "韭菜炒蛋", "sourceUrl": "https://github.com/Anduin2017/HowToCook/blob/0e106eea348c734fe1586f781202994439cf98a5/dishes/vegetable_dish/%E9%9F%AD%E8%8F%9C%E7%82%92%E8%9B%8B/%E9%9F%AD%E8%8F%9C%E7%82%92%E8%9B%8B.md", "ingredients": [{"raw": "韭菜", "has": false, "optional": false, "category": "other"}, {"raw": "鸡蛋", "has": true, "optional": false, "category": "other"}, {"raw": "食用油", "has": true, "optional": false, "category": "other"}, {"raw": "盐", "has": true, "optional": false, "category": "other"}], "steps": ["1. 韭菜洗净，沥干水分", "2. 韭菜切成约 3cm 长的小段，即为 `韭菜段`", "3. 将鸡蛋打入碗中，搅匀，即为 `鸡蛋液`", "4. 热锅，加入食用油", "5. 油热后，倒入 `鸡蛋液`，快速翻炒至鸡蛋凝固成块，盛出备用，即为 `炒好的鸡蛋`", "6. 锅中再加 5ml 食用油，放入 `韭菜段`，大火快速翻炒 30 秒", "7. 向锅中加入 `炒好的鸡蛋`，翻炒均匀", "8. 加入盐，翻炒均匀", "9. 关火，盛盘"], "difficulty": 2, "haveCount": 3, "missingCount": 1}]

    function updateCandidate(index, patch) {
        candidates = candidates.map(function(c, i) { return i === index ? Object.assign({}, c, patch) : c })
    }
    function confirmCandidates() {
        addItems(candidates.filter(function(c) { return c.group === "food" }).map(function(c) {
            return Object.assign({}, c, { freshness: "fresh", days: c.packageExpiresAt || "请查看后决定", first: false })
        }))
        imports = imports.concat([{ id: Date.now(), name: "模拟识别导入", date: "2026-10-21" }])
    }
    function setItem(id, patch) {
        items = items.map(function (item) { return item.id === id ? Object.assign({}, item, patch) : item })
    }
    function removeItem(id) {
        items = items.filter(function (item) { return item.id !== id })
        selectedIds = selectedIds.filter(function (value) { return value !== id })
    }
    function toggleSelected(id) {
        selectedIds = selectedIds.indexOf(id) >= 0 ? selectedIds.filter(function (x) { return x !== id }) : selectedIds.concat([id])
    }
    function addItems(newItems) {
        items = items.concat(newItems.map(function (item, index) {
            return Object.assign({}, item, { id: Date.now() + index })
        }))
    }
    function addMissing(recipe) {
        const names = recipe.ingredients.filter(function (ingredient) { return !ingredient.has && !ingredient.optional })
        shopping = shopping.concat(names.filter(function (ingredient) {
            return !shopping.some(function (entry) { return entry.name === ingredient.raw })
        }).map(function (ingredient) { return { name: ingredient.raw, category: ingredient.category || "other" } }))
    }
    function removeShopping(name) {
        shopping = shopping.filter(function (entry) { return entry.name !== name })
    }
    function shoppingText() {
        return shopping.map(function (item) { return item.name }).join("、")
    }
    function removeImport(id) {
        imports = imports.filter(function (entry) { return entry.id !== id })
    }
    function clearAll() {
        items = []
        selectedIds = []
        shopping = []
        rules = []
        imports = []
        suggestions = []
    }
}

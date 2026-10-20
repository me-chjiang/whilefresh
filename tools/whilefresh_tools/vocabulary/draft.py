"""One-off AI draft of the vocabulary and the recipe-ingredient map.

Output is a *draft* for human review: after it is copied into
``vocabulary-source.json`` / ``ingredient-map.json`` and edited, rebuilding never
calls the AI again.
"""

import json

from .deepseek import Client
from .schema import CATEGORIES, normalize_item, recipe_ingredient_names

BATCH_SIZE = 80

SYSTEM_A = """你在为中国家庭做饭的 App 整理“标准食材词表”。只输出一个 json 对象，不要任何解释。

词表用途：买菜平台订单/超市小票里的商品、菜谱用料、保质期参考表都要落到同一个标准名上。
规则：
1. name 用中国家庭最常用的叫法，例如“五花肉”“鸡蛋”“西红柿”“土豆”。一个食材只出现一次。
2. 粒度以家庭做饭和菜谱匹配为准：能决定做哪道菜的区别要单列（五花肉、里脊肉、排骨；小白菜、大白菜），商品规格、品牌、口味、包装、产地（500g 装、麻辣味、有机、东北大米）不单列，写进 aliases。
3. aliases 是别名和超市常见商品叫法（西红柿 ← 番茄、洋柿子；香肠 ← 麻辣香肠、川味香肠、广式腊肠）。别名要具体、常见，不要泛称；别名不能与任何标准名相同，也不能被两个标准名共用。
4. parent 是更宽泛的上位标准名，表示“持有子项就能满足要求父项的菜谱”：五花肉→猪肉、牛腩→牛肉、小葱→葱、鸡胸肉→鸡肉、冬瓜→不设。只在做菜时子项确实可以顶替父项时才设，否则为 null。parent 必须也是已有名称，或在本次 items 中同时输出（需要新的上位名时请把它也作为一项输出，它的 parent 为 null）。不要形成环。
5. pantry_default：中国家庭默认长期常备、不需要购买提醒的项，只限盐、食用油、生抽、老抽、糖、醋、料酒、葱、姜、蒜、水、米这类极少数；其余一律 false。
6. bulk_default：用掉一次不会让冰箱里这样东西消失的大存量项（大米、面粉、食用油、白糖），保守起见多数为 false。
7. 不要输出已有名称列表里的项，也不要输出与它们重复的别名。

输出格式：{"items":[{"name":"","aliases":[""],"parent":null,"pantry_default":false,"bulk_default":false}]}"""

# (category, topic hint, rough count)
TOPICS = [
    ("vegetable", "叶菜与花菜类：白菜、菠菜、生菜、油麦菜、西兰花、芹菜、韭菜、香菜、即食沙拉菜/什锦沙拉菜等", 45),
    ("vegetable", "根茎、瓜果、茄果类：土豆、红薯、山药、萝卜、胡萝卜、洋葱、黄瓜、冬瓜、南瓜、茄子、西红柿、各种辣椒/青椒/尖椒、玉米、莲藕、茭白等", 60),
    ("vegetable", "葱姜蒜等辛香蔬菜，豆荚鲜豆类（豆角、毛豆、豌豆）、笋、豆芽、菌菇（香菇、金针菇、蘑菇/白蘑菇、杏鲍菇、木耳等）以及常见干货蔬菜（干香菇、干木耳、干辣椒不放这里）", 60),
    ("fruit", "常见水果：苹果、梨、柑橘橙柚、桃李杏、葡萄、西瓜甜瓜、香蕉、柿子、山楂、枣等", 50),
    ("fruit", "浆果、热带与进口水果：蓝莓、草莓、树莓、芒果、菠萝、榴莲、牛油果、猕猴桃、车厘子、火龙果，以及柠檬、百香果、椰子等", 40),
    ("meat", "猪肉及部位：猪肉、五花肉、里脊、梅花肉、前腿肉、排骨/猪肋排/小排、猪蹄、猪肝等内脏、肉馅/肉末、猪油肉；不含香肠火腿等加工品", 40),
    ("meat", "牛肉与羊肉及部位：牛肉、牛腩、牛腱、牛排、肥牛卷/牛肉火锅片、牛肉末、羊肉、羊排、羊肉卷/羊肉火锅片、羊蝎子；不含加工品", 35),
    ("meat", "禽肉及部位：鸡肉、鸡胸肉、鸡腿、鸡翅（翅中、翅根）、鸡爪、鸡脆骨/掌中宝、整鸡、鸭肉、鸭腿、鸭胗、鹅肉、鸽子肉、鸡鸭内脏、血制品（鸭血）等；不含加工品", 35),
    ("seafood", "鱼、虾、蟹、贝、鱿鱼章鱼、海参、鱼籽、海带紫菜等水产海鲜，包含鲜活与冰鲜、常见淡水鱼和海鱼", 60),
    ("egg_dairy", "蛋类（鸡蛋、鸭蛋、鹌鹑蛋、咸蛋、皮蛋）与乳制品（牛奶、酸奶、奶酪/芝士、黄油、奶油、炼乳、奶粉等）", 30),
    ("soy", "豆制品：豆腐（嫩、老、冻、干）、豆干、豆皮/腐竹、豆浆、豆腐乳、豆豉、油豆腐、素鸡等", 25),
    ("staple", "米面粮谷：大米、糯米、小米、燕麦、杂粮、各类面粉、玉米粉、淀粉（土豆淀粉、玉米淀粉、红薯淀粉）、豆类杂粮（红豆、绿豆、黄豆）", 45),
    ("staple", "面条粉丝与面点：挂面、鲜面条、意大利面、方便面、米粉、河粉、粉丝粉条、饺子皮馄饨皮、面包、吐司、馒头、年糕、饼、春卷皮、土豆泥、红薯干等", 40),
    ("staple", "食用油与油脂：食用油、菜籽油、花生油、橄榄油、芝麻香油、猪油、牛油、椰子油等（食用油、植物油这类常见通称要有）", 15),
    ("seasoning", "基础调味：盐、白糖、冰糖、红糖、生抽、老抽、酱油、醋（陈醋、香醋、米醋）、料酒、蚝油、鸡精、味精、豆瓣酱、料酒等", 40),
    ("seasoning", "香辛料与粉料：八角、桂皮、香叶、花椒、胡椒粉、五香粉、孜然、辣椒粉、干辣椒、咖喱块、白芝麻、芥末、小苏打、泡打粉以外的粉类等", 50),
    ("seasoning", "酱料与复合调味：番茄酱、沙拉酱、甜面酱、黄豆酱、火锅底料、蘸料、辣椒酱/老干妈、芝麻酱、花生酱、果酱、蜂蜜、枫糖浆、各类汤底/调料包、料酒外的酒（白酒、黄酒、啤酒）", 50),
    ("processed", "加工肉与即食熟食：香肠、腊肠、火腿、培根、午餐肉、肉松、腊肉、咸肉、卤味、烤鸭、牛肉干、鱼丸肉丸虾滑、蟹棒、熟食", 35),
    ("processed", "速冻与半成品：速冻水饺/馄饨/汤圆、速冻薯条、速冻包子、披萨、冷冻鸡块、预制菜、方便速食、速冻蔬菜", 35),
    ("processed", "罐头、即食沙拉、豆制酱菜、腌制品、泡菜榨菜、方便米饭、速食汤、面包糕点、零食类食品（饼干、薯片）、饮品以外的包装食品", 45),
    ("other", "饮用水与饮品（水、可乐、果汁、茶、咖啡、酒类）、坚果干果与种子（花生、核桃、腰果、葡萄干、红枣、枸杞）、烘焙配料（酵母、吉利丁、巧克力、可可粉）、冰块、其他无法归入前面类别的食材", 60),
]

SYSTEM_B = """你在为中国家庭做饭的 App 把菜谱里的“原料行”映射到标准食材词表。只输出一个 json 对象，不要任何解释。

规则：
1. 每个原料行给出一个标准名列表，名称必须逐字取自下面的词表（可以通过括号里的别名找到标准名，但输出只写标准名）。
2. 去掉数量、规格、品牌、做法描述、备注：“红花椒 150 g”→花椒；“蒜片”→蒜；“125ml 淡奶油”→淡奶油。
3. 并列写法表示全部需要：“葱姜蒜”→[葱,姜,蒜]；“葱姜末”→[葱,姜]。
4. “A 或 B”“A/B”“A、B 二选一”这类备选，只取第一个；泛指而不是具体食材的（“蔬菜类”“其他调味料”“喜欢的酱”）→[]。
5. 选词表中最贴切的标准名；“鸡腿”类写法选更具体的“鸡腿”，不要退到“鸡肉”。
6. 水（清水、开水、热水、温水、凉白开）→[水]。冰块、包装、非食物、纯描述 → []。
7. 只有确实是食物、词表里又没有合适项时，才在 new_items 里提出新标准名（规则同词表：家庭常用叫法、粒度适中，规格口味写进别名，category 取自 vegetable/fruit/meat/seafood/egg_dairy/soy/staple/seasoning/processed/other，可有 parent、aliases、pantry_default、bulk_default），并在对应映射里使用它。尽量少新增。
8. 每个输入原料行都必须在 mappings 中出现一次，key 与输入逐字相同。

输出格式：{"mappings":{"原料行":["标准名"]},"new_items":[{"name":"","category":"","aliases":[],"parent":null,"pantry_default":false,"bulk_default":false}]}"""


class Vocab:
    """Ordered name -> item, with alias index; merges AI output conservatively."""

    def __init__(self):
        self.items: dict[str, dict] = {}
        self.alias_owner: dict[str, str] = {}
        self.log: list[str] = []

    def resolve(self, term: str) -> str | None:
        if term in self.items:
            return term
        return self.alias_owner.get(term)

    def add(self, raw: dict, category: str) -> bool:
        name = str(raw.get("name", "")).strip()
        if not name:
            return False
        if self.resolve(name):
            self.log.append(f"skip duplicate item {name!r}")
            return False
        aliases: list[str] = []
        for a in raw.get("aliases") or []:
            a = str(a).strip()
            if not a or a == name or a in aliases:
                continue
            if self.resolve(a):
                self.log.append(f"drop alias {a!r} of {name!r}: already used")
                continue
            aliases.append(a)
        parent = raw.get("parent")
        item = {
            "name": name,
            "category": category,
            "aliases": aliases,
            "parent": str(parent).strip() if parent else None,
            "pantry_default": raw.get("pantry_default") is True,
            "bulk_default": raw.get("bulk_default") is True,
        }
        self.items[name] = item
        for a in aliases:
            self.alias_owner[a] = name
        return True

    def names_prompt(self) -> str:
        return "、".join(self.items) if self.items else "(空)"

    def listing(self) -> str:
        by_cat: dict[str, list[str]] = {}
        for it in self.items.values():
            label = it["name"]
            if it["aliases"]:
                label += "(" + "/".join(it["aliases"][:3]) + ")"
            by_cat.setdefault(it["category"], []).append(label)
        return "\n".join(f"{zh}: " + "、".join(by_cat.get(code, [])) for code, zh in CATEGORIES)

    def finish(self) -> list[dict]:
        """Items for the source file; unresolved parents are logged and kept for review."""
        out = []
        for it in self.items.values():
            p = it["parent"]
            if p and p not in self.items:
                owner = self.alias_owner.get(p)
                if owner:
                    it["parent"] = owner
                else:
                    self.log.append(f"unresolved parent {p!r} of {it['name']!r}")
            out.append(normalize_item(it))
        return out


def draft_vocabulary(client: Client, vocab: Vocab) -> None:
    for category, hint, count in TOPICS:
        user = (
            f"品类：{category}\n本次范围：{hint}\n"
            f"请列出约 {count} 个标准名（不超过 {count + 20} 个），宁可常见、不要生僻。\n"
            f"已有名称（不要重复，可作为 parent）：{vocab.names_prompt()}"
        )
        reply = client.complete_json(SYSTEM_A, user)
        added = sum(vocab.add(raw, category) for raw in reply.get("items") or [] if isinstance(raw, dict))
        vocab.log.append(f"topic {category}: +{added} ({hint[:12]}…)")


def draft_map(client: Client, vocab: Vocab, recipes_doc: dict) -> dict[str, list[str]]:
    names = sorted(recipe_ingredient_names(recipes_doc))
    mapping: dict[str, list[str]] = {}
    valid_categories = {c for c, _ in CATEGORIES}
    for start in range(0, len(names), BATCH_SIZE):
        batch = names[start : start + BATCH_SIZE]
        user = "标准食材词表：\n" + vocab.listing() + "\n\n原料行（json 数组）：\n" + _json_list(batch)
        reply = client.complete_json(SYSTEM_B, user, max_tokens=8000)
        for raw in reply.get("new_items") or []:
            if isinstance(raw, dict) and raw.get("category") in valid_categories:
                if vocab.add(raw, raw["category"]):
                    vocab.log.append(f"new item {raw.get('name')!r} (from recipes)")
        got = reply.get("mappings") or {}
        for name in batch:
            targets = got.get(name)
            if not isinstance(targets, list):
                vocab.log.append(f"no mapping returned for {name!r}")
                mapping[name] = []
                continue
            resolved: list[str] = []
            for t in targets:
                std = vocab.resolve(str(t).strip())
                if std is None:
                    vocab.log.append(f"unknown target {t!r} for {name!r}")
                elif std not in resolved:
                    resolved.append(std)
            mapping[name] = resolved
    return mapping


def _json_list(values: list[str]) -> str:
    return json.dumps(values, ensure_ascii=False)


def run_draft(client: Client, recipes_doc: dict) -> tuple[list[dict], dict[str, list[str]], list[str]]:
    vocab = Vocab()
    draft_vocabulary(client, vocab)
    mapping = draft_map(client, vocab, recipes_doc)
    return vocab.finish(), mapping, vocab.log

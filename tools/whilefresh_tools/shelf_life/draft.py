"""One-off AI draft of the standard name -> FoodKeeper entry map.

The output is a draft for human review: after it is copied over
``foodkeeper-map.json`` and edited, rebuilding never calls the AI again.
"""

from ..vocabulary.deepseek import Client
from .foodkeeper import product_label, product_shelf_life
from .schema import FOODKEEPER_VERSION

BATCH_SIZE = 80

SYSTEM = """你在为中国家庭做饭的 App 建立“标准食材名 → USDA FoodKeeper 条目”的对应表，用来查冰箱里食材的保质期参考值。只输出一个 json 对象，不要任何解释。

输入有两部分：FoodKeeper 候选条目（格式 ID|名称|说明|分类），和需要对应的标准食材名列表（括号内是别名，仅帮助理解）。

规则：
1. 每个标准名选出一个最能代表“中国家庭买回来的同一种食物”的条目 ID；没有合适条目就给 null。宁可 null，也不要硬套。
2. 以下情况一律 null：条目是另一种食物（腊肉不是新鲜培根、豆腐干不是奶酪）；加工方式不同导致保质期明显不同（鲜的对干的、生的对熟的、新鲜对罐头）；中国特有、候选里没有对应物的食材（腐竹、螺蛳粉、麻辣香肠、酱牛肉等）；调料中保质期差异很大、无法代表的品类。
3. 鲜品选生的、未加工的条目；包装食品选“商业包装、未开封”的条目；不要选“自制”“烹饪后”“剩菜”“开封后”性质的条目。
4. 有明确子类时选子类（如“Pork - ground”对猪肉末；“Chicken - whole”对整鸡）；泛称标准名（鱼、虾、肉丸、坚果、速冻蔬菜等）只在某个条目确实能代表这一整类时才给，否则 null。
5. 肉类和水产的部位、品种只在 FoodKeeper 的条目确实能代表时才对应（例如猪排骨与“Pork - chops”部位不同，拿不准就 null；但猪肉、牛肉这类通称可以对应生鲜肉的通用条目）。
6. reason 用一句简短中文说明依据，或为什么是 null。
7. 输入的每个标准名都必须在 mappings 中出现一次，key 与输入的标准名逐字相同（不含括号别名）。

输出格式：{"mappings":{"标准名":{"id":123,"reason":"..."},"另一个标准名":{"id":null,"reason":"..."}}}"""


def candidates(products: dict[int, dict]) -> list[tuple[int, str]]:
    """Entries that carry at least one usable number (an entry without any cannot back a map row)."""
    out = []
    for pid in sorted(products):
        life = product_shelf_life(products[pid])
        if any(life[k] for k in ("fridge_days", "freezer_days", "room_days")):
            out.append((pid, product_label(products[pid])))
    return out


def candidate_text(products: dict[int, dict]) -> str:
    lines = []
    for pid, label in candidates(products):
        name, _, sub = label.partition(" (")
        lines.append(f"{pid}|{name}|{sub.rstrip(')')}|{products[pid].get('Category_Name')}")
    return "\n".join(lines)


def user_message(cand_text: str, names: list[dict]) -> str:
    rows = []
    for it in names:
        aliases = it.get("aliases") or []
        shown = "、".join(aliases[:6])
        rows.append(f"{it['name']}（{shown}）" if shown else it["name"])
    return f"FoodKeeper {FOODKEEPER_VERSION} 候选条目：\n{cand_text}\n\n标准食材名（{len(names)} 个）：\n" + "\n".join(rows)


def run_draft(client: Client, products: dict[int, dict], vocab_items: list[dict]):
    """Returns (map entries, log lines). ``entries`` omits names the model left null."""
    valid_ids = {pid for pid, _ in candidates(products)}
    cand_text = candidate_text(products)
    entries: dict[str, dict] = {}
    log: list[str] = []
    for start in range(0, len(vocab_items), BATCH_SIZE):
        batch = vocab_items[start : start + BATCH_SIZE]
        answer = client.complete_json(SYSTEM, user_message(cand_text, batch)).get("mappings")
        if not isinstance(answer, dict):
            raise ValueError(f"batch starting at {start}: no 'mappings' object in the response")
        for it in batch:
            name = it["name"]
            got = answer.get(name)
            if not isinstance(got, dict):
                log.append(f"{name}: missing from the response")
                continue
            reason = str(got.get("reason") or "").strip()
            pid = got.get("id")
            if pid is None:
                log.append(f"{name}: null ({reason})")
            elif not isinstance(pid, int) or pid not in valid_ids:
                log.append(f"{name}: dropped invalid id {pid!r} ({reason})")
            else:
                entries[name] = {"id": pid, "note": reason or product_label(products[pid])}
                log.append(f"{name}: {pid} {product_label(products[pid])} ({reason})")
        extras = sorted(set(answer) - {it["name"] for it in batch})
        if extras:
            log.append(f"batch {start}: ignored unrequested names {extras}")
    return entries, log

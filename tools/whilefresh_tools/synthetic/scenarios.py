"""The synthetic sample catalogue.

Everything here is invented: store, brand and product-line names are fictional, there are no
people, phone numbers or real addresses, and no real platform name or logo appears. Standard
names (`canonical`) come from app/assets/data/vocabulary.json; the drawn name is a
commercialised variant of it.
"""

from __future__ import annotations

from datetime import date

from .model import UNCATEGORIZED, Group, Noise, Product, Rec, Sample


def P(canonical: str, raw: str, spec: str | None, price: float, count: int = 1, **kw) -> Product:
    return Product(canonical, raw, spec, price, count, **kw)


def SEAS(canonical: str, raw: str, spec: str | None, price: float, count: int = 1, **kw) -> Product:
    return Product(canonical, raw, spec, price, count, category="seasoning", **kw)


def JUNK(raw: str, spec: str | None, price: float, count: int = 1, **kw) -> Product:
    return Product(UNCATEGORIZED, raw, spec, price, count, category="irrelevant", **kw)


def GIFT(canonical: str, raw: str, spec: str | None, **kw) -> Product:
    return Product(canonical, raw, spec, 0.0, 1, gift=True, **kw)


def R(name: str, spec: str, price: float, emoji: str = "🛒") -> Rec:
    return Rec(name, spec, price, emoji)


D = date

# --------------------------------------------------------------------------------------
# Style A: fresh-supermarket app cart ("青禾鲜市").  Selected / unselected rows, gifts,
# upsell and hot-sale blocks, coupon banner, delivery-fee hint.
# --------------------------------------------------------------------------------------

CART_FRESH_LONG_01 = Sample(
    id="cart-fresh-long-01",
    kind="screenshot",
    style="cart-fresh",
    source_type="cart",
    capture_date=D(2026, 10, 20),
    seed=101,
    extra={"address": "某某小区 3 号楼", "eta": "预计 30 分钟内送达", "min_height": 780},
    groups=(
        Group(
            "青禾鲜市 · 自营仓",
            (
                P("鸡蛋", "【产地直发】鲜鸡蛋 无抗生素", "10枚530g", 15.9, 2, production="昨天生产"),
                P("西红柿", "新鲜沙瓤西红柿", "500g±20g", 6.8),
                P("黄瓜", "水果黄瓜 脆甜", "约300g", 5.5, production="今天生产"),
                P("肥牛", "火锅肥牛片 精选", "250g", 24.9, production="今天生产"),
                P("上海青", "【当季】上海青 嫩叶", "300g", 3.9, production="今天生产"),
                P("牛奶", "乐牧纯牛奶 全脂灭菌", "250ml*12盒", 39.9, expiry="保质期至2027-03-12"),
                P("豆腐", "老豆腐 现做", "400g", 3.5, production="今天生产"),
                P("草莓", "红颜草莓 单果30g起", "300g", 29.9, orig_price=39.9),
                P("洋葱", "紫皮洋葱", "500g", 4.5),
                P("虾仁", "鲜虾仁 大号", "125g(18mm)", 19.9, production="10月18日生产"),
                P("鸡腿", "琵琶腿 带骨 冷鲜", "2只约450g", 17.8, production="今天生产"),
                SEAS("生抽", "鼎味轩 特级生抽", "500ml", 9.9, expiry="保质期至2028-04-02"),
                SEAS("蚝油", "锦厨 蚝油", "510g", 8.9),
                GIFT("柠檬", "赠品 小青柠 3个", "3个"),
                GIFT("香菜", "赠 香菜 一小把", "约50g", production="今天生产"),
                JUNK("柔韧抽纸 3层*10包", "10包", 19.9),
                P("苹果", "烟台红富士 脆甜", "1kg", 12.9, selected=False),
                P("吐司", "全麦吐司 切片", "400g", 9.9, selected=False, production="今天生产"),
                JUNK("清爽洗洁精", "1.2kg", 12.5, selected=False),
            ),
        ),
    ),
    noise=(
        Noise("广告横幅", "新人专享 · 领 20 元红包"),
        Noise("优惠券", "满 99 减 10 优惠券"),
        Noise("凑单提示", "还差 ¥12.6 免配送费，去凑单"),
        Noise(
            "换购推荐",
            "换购专区 · 满 59 元可换购",
            recs=(
                R("福瑞桶装水", "5L", 4.9, "🚰"),
                R("金谷东北大米", "1kg", 9.9, "🌾"),
                R("甜心小番茄", "250g", 5.9, "🍅"),
            ),
        ),
        Noise(
            "热卖推荐",
            "热卖推荐",
            recs=(
                R("海泊冷冻带鱼段", "500g", 29.9, "🐟"),
                R("锦厨火锅底料", "150g", 12.9, "🍲"),
                R("鲜榨橙汁", "300ml", 9.9, "🍊"),
                R("蓝莓 125g*2盒", "250g", 25.8, "🫐"),
                R("手撕面包", "380g", 11.9, "🍞"),
                R("乐牧酸奶 6杯装", "6x100g", 16.9, "🥛"),
            ),
        ),
        Noise("配送费", "配送费", 3.0),
    ),
)

CART_FRESH_LONG_02 = Sample(
    id="cart-fresh-long-02",
    kind="screenshot",
    style="cart-fresh",
    source_type="cart",
    capture_date=D(2026, 10, 19),
    seed=102,
    extra={"address": None, "eta": "预计 45 分钟内送达", "min_height": 780},
    groups=(
        Group(
            "青禾鲜市 · 自营仓",
            (
                P("五花肉", "【黑猪】精选五花肉 切块", "500g±20g", 28.8, production="今天生产"),
                P("排骨", "肋排 小段 新鲜", "约600g", 36.0, production="今天生产"),
                P("土豆", "黄心土豆 现挖", "1kg", 5.9, 2),
                P("胡萝卜", "水灵胡萝卜", "500g", 3.5),
                P("青椒", "薄皮青椒", "250g", 4.2),
                P("大白菜", "胶州大白菜", "约1.2kg", 7.8),
                P("金针菇", "金针菇 新鲜", "150g", 2.9),
                P("香菇", "鲜香菇 肉厚", "200g", 8.8),
                P("鲜面条", "鲜切面条 细", "500g", 5.9, production="昨天生产", visual_key="面条"),
                P("鲈鱼", "活杀鲈鱼 现杀", "约500g", 26.8),
                P("香蕉", "高山香蕉", "约1kg", 8.5),
                P("橙子", "脐橙 甜橙", "2.5kg", 25.9, orig_price=32.9),
                SEAS("料酒", "锦厨 葱姜料酒", "500ml", 5.9),
                SEAS("白糖", "禾田 白砂糖", "400g", 4.9),
                SEAS("盐", "禾田 加碘精制盐", "500g", 1.9),
                P("酸奶", "乐牧原味酸奶", "100g*8杯", 16.8, selected=False, expiry="保质期至2026-11-02"),
                GIFT("小葱", "【赠品】小葱", "100g"),
                JUNK("5号碱性电池", "8粒", 14.9),
                JUNK("厨房纸巾", "2卷", 9.9, selected=False),
            ),
        ),
        Group(
            "青禾鲜市 · 水果专区",
            (
                P("葡萄", "阳光玫瑰葡萄 大串", "约700g", 39.9, production="今天生产"),
                P("牛油果", "墨西哥牛油果", "2个约400g", 14.9, selected=False),
            ),
        ),
    ),
    noise=(
        Noise("广告横幅", "周三会员日 · 全场鲜果 8 折"),
        Noise("优惠券", "满 69 减 8 优惠券"),
        Noise(
            "换购推荐",
            "超值换购",
            recs=(
                R("小包装卫生纸", "6卷", 6.9, "🧻"),
                R("海泊虾皮", "50g", 5.9, "🦐"),
            ),
        ),
        Noise(
            "热卖推荐",
            "大家都在买",
            recs=(
                R("现烤蛋挞", "6个", 16.8, "🥧"),
                R("山楂条", "100g", 6.9, "🍬"),
                R("红糖馒头", "6个", 7.9, "🍞"),
                R("乐牧牛奶 早餐装", "200ml*10", 29.9, "🥛"),
            ),
        ),
        Noise("凑单提示", "再买 ¥8.0 可减免运费"),
        Noise("配送费", "配送费", 3.0),
    ),
)

CART_FRESH_01 = Sample(
    id="cart-fresh-01",
    kind="screenshot",
    style="cart-fresh",
    source_type="cart",
    capture_date=D(2026, 10, 18),
    seed=103,
    extra={"address": "某某小区 8 号楼", "eta": "预计 30 分钟内送达", "min_height": 780},
    groups=(
        Group(
            "青禾鲜市 · 自营仓",
            (
                P("鸡胸肉", "鸡胸肉 冷鲜", "400g", 12.9, production="今天生产"),
                P("西兰花", "西兰花 新鲜", "约350g", 6.9),
                P("鸡蛋", "鲜鸡蛋 土鸡蛋", "10枚530g", 15.9, 2),
                P("牛奶", "乐牧高钙牛奶", "250ml*10盒", 36.9),
                P("秋葵", "绿秋葵 嫩", "250g", 7.9, production="今天生产"),
                SEAS("酱油", "鼎味轩 酿造酱油", "500ml", 7.9),
            ),
        ),
    ),
    noise=(
        Noise("优惠券", "满 59 减 5 优惠券"),
        Noise("换购推荐", "满 39 元换购", recs=(R("福瑞矿泉水", "550ml", 0.9, "💧"), R("乐牧酸奶", "200g", 3.9, "🥛"))),
        Noise("配送费", "配送费", 0.0),
    ),
)

CART_FRESH_02 = Sample(
    id="cart-fresh-02",
    kind="screenshot",
    style="cart-fresh",
    source_type="cart",
    capture_date=D(2026, 10, 21),
    seed=104,
    extra={"address": None, "eta": "明日 08:00-10:00 送达", "min_height": 780},
    groups=(
        Group(
            "青禾鲜市 · 自营仓",
            (
                P("牛腩", "牛腩 带筋 冷冻", "500g", 39.9, selected=False),
                P("羊肉卷", "内蒙羊肉卷", "300g", 28.0, selected=False),
                P("西芹", "西芹 嫩", "300g", 4.5, selected=False, visual_key="芹菜"),
                P("莲藕", "九孔莲藕", "约500g", 9.9, selected=False),
                P("生菜", "球生菜", "1个约350g", 4.9),
                P("豆腐干", "五香豆腐干", "200g", 5.5),
                P("面包", "德式碱水面包", "6个", 14.9, selected=False, production="今天生产"),
                JUNK("洗衣液 薰衣草香", "2kg", 29.9),
                SEAS("花椒", "麻椒 特麻", "50g", 8.8, selected=False),
            ),
        ),
    ),
    noise=(
        Noise("广告横幅", "秋日暖胃季 · 汤锅食材满减"),
        Noise("热卖推荐", "热卖推荐", recs=(R("锦厨酸汤肥牛", "350g", 22.9, "🍲"), R("海泊鲜虾", "300g", 24.9, "🦐"))),
        Noise("凑单提示", "还差 ¥20.0 起送，去凑单"),
    ),
)

# --------------------------------------------------------------------------------------
# Style B: dark-store grocery order detail ("晨露买菜").
# --------------------------------------------------------------------------------------

ORDER_DARKSTORE_01 = Sample(
    id="order-darkstore-01",
    kind="screenshot",
    style="order-darkstore",
    source_type="order",
    capture_date=D(2026, 10, 19),
    seed=201,
    extra={"order_no": "2026101918432571", "pay": "在线支付", "address": "某某小区 5 号楼", "status": "已送达", "min_height": 780},
    groups=(
        Group(
            "晨露买菜 · 东区前置仓",
            (
                P("菠菜", "有机菠菜", "250g", 5.9),
                P("牛里脊", "牛里脊 嫩切", "300g", 32.9, production="今天生产"),
                P("鸡蛋", "鲜鸡蛋", "10枚530g", 14.9, 2),
                P("豆芽", "绿豆芽", "300g", 2.5),
                P("三文鱼", "冰鲜三文鱼 刺身级", "200g", 49.9, production="今天生产"),
                SEAS("蒸鱼豉油", "鼎味轩 蒸鱼豉油", "400ml", 11.9),
                GIFT("大葱", "赠品 大葱 1根", "1根", visual_key="葱"),
                JUNK("洗洁精 柠檬香", "500g", 8.9),
            ),
            order_time="2026-10-19 18:43:25",
            status="已送达",
        ),
    ),
    noise=(
        Noise("配送费", "配送费", 2.0),
        Noise("包装费", "包装费", 0.5),
        Noise("优惠券", "新客立减券", -5.0),
        Noise("满减", "满 60 减 6", -6.0),
    ),
)

ORDER_DARKSTORE_02 = Sample(
    id="order-darkstore-02",
    kind="screenshot",
    style="order-darkstore",
    source_type="order",
    capture_date=D(2026, 10, 20),
    seed=202,
    extra={"order_no": "2026102009051188", "pay": "在线支付", "address": None, "status": "已完成", "min_height": 780},
    groups=(
        Group(
            "晨露买菜 · 北区前置仓",
            (
                P("小白菜", "【产地直发】小白菜", "300g", 3.5, production="今天生产"),
                P("油麦菜", "油麦菜 脆嫩", "300g", 3.9),
                P("茄子", "圆茄子", "约500g", 5.9),
                P("冬瓜", "冬瓜 切块", "约800g", 6.8),
                P("南瓜", "板栗小南瓜", "约1kg", 9.9),
                P("玉米", "鲜甜玉米 真空装", "2根", 8.8),
                P("红薯", "蜜薯 小个", "约1kg", 10.9),
                P("猪肉末", "猪肉馅 肥三瘦七", "400g", 15.8, production="今天生产"),
                P("猪蹄", "猪蹄 切块", "约800g", 29.9),
                P("鸭腿", "卤鸭腿", "2只", 21.9),
                P("牛奶", "乐牧纯牛奶", "250ml*12盒", 39.9),
                P("豆腐", "嫩豆腐 盒装", "300g", 2.9, 3),
                P("面粉", "禾田 中筋面粉", "1kg", 5.9),
                P("大米", "金谷五常风味大米", "2.5kg", 29.9, orig_price=36.9),
                SEAS("白醋", "鼎味轩 白醋", "500ml", 3.9),
                SEAS("白胡椒", "锦厨 白胡椒粉", "30g", 7.5, visual_key="胡椒粉"),
                GIFT("柠檬", "赠 黄柠檬 2个", "2个"),
                JUNK("垃圾袋 加厚", "100只", 6.9),
                JUNK("纸巾 抽取式", "6包", 12.9),
                P("苹果", "红富士苹果", "6个约1.2kg", 16.8),
                P("橙子", "赣南脐橙", "2kg", 22.8),
                P("葡萄", "夏黑葡萄", "约500g", 19.9),
            ),
            order_time="2026-10-20 09:05:11",
            status="已完成",
        ),
    ),
    noise=(
        Noise("配送费", "配送费", 0.0),
        Noise("包装费", "包装费", 1.0),
        Noise("优惠券", "满 150 减 15 券", -15.0),
        Noise("广告横幅", "邀请好友得 30 元"),
        Noise("猜你喜欢", "猜你喜欢", recs=(R("海泊鲜虾仁", "200g", 22.9, "🦐"), R("乐牧黄油", "100g", 15.9, "🧈"), R("手工年糕", "500g", 9.9, "🍡"))),
    ),
)

ORDER_DARKSTORE_03 = Sample(
    id="order-darkstore-03",
    kind="screenshot",
    style="order-darkstore",
    source_type="order",
    capture_date=D(2026, 10, 17),
    seed=203,
    extra={"order_no": "2026101719220934", "pay": "在线支付", "address": "某某小区 1 号楼", "status": "已送达", "min_height": 780},
    groups=(
        Group(
            "晨露买菜 · 西区前置仓",
            (
                P("小葱", "葱 小香葱", "100g", 2.5, visual_key="葱"),
                P("姜", "老姜", "200g", 3.2),
                P("蒜", "紫皮大蒜", "250g", 3.9),
                SEAS("生抽", "鼎味轩 头道生抽", "1L", 14.9),
                SEAS("蚝油", "锦厨 蚝油", "700g", 12.5),
                SEAS("老抽", "鼎味轩 老抽", "500ml", 7.9),
                SEAS("花椒", "四川红花椒", "100g", 9.9),
                SEAS("八角", "八角 大料", "50g", 4.9),
                P("洋葱", "黄皮洋葱", "1kg", 4.9),
                P("鸡翅", "奥尔良鸡翅中", "500g", 25.9),
                JUNK("5号电池 碱性", "4粒", 9.9),
                JUNK("洗洁精", "1kg", 7.9, 2),
            ),
            order_time="2026-10-17 19:22:09",
            status="已送达",
        ),
    ),
    noise=(
        Noise("配送费", "配送费", 3.0),
        Noise("优惠券", "红包抵扣", -3.0),
    ),
)

# --------------------------------------------------------------------------------------
# Style C: food-delivery platform supermarket channel order ("闪达超市").
# --------------------------------------------------------------------------------------

ORDER_DELIVERY_01 = Sample(
    id="order-delivery-01",
    kind="screenshot",
    style="order-delivery",
    source_type="order",
    capture_date=D(2026, 10, 20),
    seed=301,
    extra={"order_no": "7261020103300219", "pay": "在线支付", "address": "某某小区 2 号楼", "min_height": 780},
    groups=(
        Group(
            "小满便利 · 超市",
            (
                P("牛奶", "乐牧鲜牛奶 巴氏杀菌", "950ml", 18.9, production="今天生产"),
                P("吐司", "切片吐司 原味", "350g", 7.9, expiry="到期日 2026-10-24"),
                P("火腿", "鲜切火腿片", "150g", 9.9),
                P("鸡蛋", "鲜鸡蛋 30枚装", "30枚1.5kg", 29.9),
                P("苹果", "烟台红富士", "约1kg", 11.9),
                JUNK("一次性湿厕纸", "40片", 7.9),
            ),
            order_time="2026-10-20 10:33:00",
            status="已完成",
        ),
    ),
    noise=(
        Noise("包装费", "包装费", 1.0),
        Noise("配送费", "配送费", 4.0),
        Noise("优惠券", "商家满减", -3.0),
        Noise("满减", "平台红包", -2.0),
        Noise("广告横幅", "会员免配送费，立即开通"),
    ),
)

ORDER_DELIVERY_02 = Sample(
    id="order-delivery-02",
    kind="screenshot",
    style="order-delivery",
    source_type="order",
    capture_date=D(2026, 10, 18),
    seed=302,
    extra={"order_no": "7261018205104476", "pay": "在线支付", "address": None, "min_height": 780},
    groups=(
        Group(
            "小满便利 · 生鲜超市",
            (
                P("香蕉", "进口香蕉", "约500g", 6.9),
                P("牛排", "家庭牛排 腌制", "150g*3片", 39.9, production="10月18日生产"),
                P("意面", "意大利面 直管", "500g", 8.9),
                SEAS("番茄酱", "味之庄 番茄酱", "340g", 6.5),
                SEAS("沙拉酱", "味之庄 沙拉酱", "200g", 5.9),
                P("罗马生菜", "罗马生菜", "2颗", 6.5, visual_key="生菜"),
                P("圣女果", "千禧圣女果", "500g", 12.8),
                JUNK("保鲜袋 大号", "100只", 8.9),
            ),
            order_time="2026-10-18 20:51:04",
            status="已完成",
        ),
    ),
    noise=(
        Noise("配送费", "配送费", 3.5),
        Noise("包装费", "包装费", 1.5),
        Noise("满减", "满 50 减 5", -5.0),
        Noise("凑单提示", "再买 ¥6 可享满减，去凑单"),
    ),
)

ORDER_DELIVERY_03 = Sample(
    id="order-delivery-03",
    kind="screenshot",
    style="order-delivery",
    source_type="order",
    capture_date=D(2026, 10, 21),
    seed=303,
    extra={"order_no": "7261021081502930", "pay": "在线支付", "address": "某某小区 6 号楼", "min_height": 780},
    groups=(
        Group(
            "小满便利 · 社区大卖场",
            (
                P("鲫鱼", "活鲫鱼 现杀", "约600g", 18.8),
                P("基围虾", "活基围虾", "500g", 49.9),
                P("蛤蜊", "花蛤", "500g", 12.8),
                P("鱿鱼", "鲜鱿鱼 整只", "约400g", 24.8),
                P("带鱼", "东海带鱼段", "500g", 29.9),
                P("螃蟹", "梭子蟹 鲜活", "2只约500g", 58.0),
                P("牛腱", "酱卤牛腱子", "500g", 62.0),
                P("鸡爪", "鸡爪 冷冻", "500g", 14.9),
                P("豆皮", "手撕豆皮", "250g", 6.9),
                P("粉丝", "龙口粉丝", "200g", 5.5),
                P("海带", "海带结", "300g", 4.9),
                P("芹菜", "香芹", "300g", 3.9),
                P("四季豆", "四季豆 嫩", "300g", 4.9),
                P("荷兰豆", "荷兰豆", "250g", 7.9),
                P("芥蓝", "芥蓝", "300g", 5.5),
                P("茼蒿", "茼蒿", "250g", 4.5),
                P("紫甘蓝", "紫甘蓝", "约500g", 5.9),
                P("山药", "铁棍山药", "约400g", 12.9),
                P("牛奶", "乐牧纯牛奶", "250ml*12盒", 39.9),
                P("酸奶", "乐牧老酸奶", "150g*6杯", 18.9),
                SEAS("孜然", "孜然粒", "50g", 4.9),
                SEAS("十三香", "锦厨 十三香", "45g", 3.5),
                JUNK("次氯酸消毒液", "500ml", 9.9),
                JUNK("牙线棒", "50支", 6.9),
            ),
            order_time="2026-10-21 08:15:02",
            status="已完成",
        ),
    ),
    noise=(
        Noise("包装费", "包装费", 2.0),
        Noise("配送费", "配送费", 0.0),
        Noise("优惠券", "商家优惠券", -8.0),
        Noise("广告横幅", "再来一单，享 8 折"),
        Noise("猜你喜欢", "买了又买", recs=(R("味之庄 拌饭酱", "200g", 9.9, "🥫"), R("海泊虾滑", "200g", 16.9, "🍤"), R("手撕鸡", "150g", 12.9, "🍗"))),
    ),
)

# --------------------------------------------------------------------------------------
# Style D: general e-commerce orders ("好集商城"): order list and order detail.
# --------------------------------------------------------------------------------------

ORDER_MALL_LIST_01 = Sample(
    id="order-mall-list-01",
    kind="screenshot",
    style="order-mall-list",
    source_type="order",
    capture_date=D(2026, 10, 19),
    seed=401,
    extra={"min_height": 780},
    groups=(
        Group(
            "禾田粮油旗舰店",
            (
                P("大米", "【产地直发】禾田东北大米 新米", "5kg", 49.9, orig_price=69.9),
                P("菜籽油", "禾田 压榨一级菜籽油", "5L", 89.0, visual_key="食用油"),
            ),
            order_time="2026-10-14 21:05:33",
            status="交易成功",
        ),
        Group(
            "海泊水产专营店",
            (
                P("三文鱼", "【冷链配送】冰鲜三文鱼中段", "500g", 79.0, production="10月16日生产"),
                P("白虾", "冷冻大虾 白虾", "500g", 36.8, 2),
            ),
            order_time="2026-10-17 12:40:18",
            status="交易成功",
        ),
        Group(
            "乐牧乳品旗舰店",
            (
                P("牛奶", "乐牧纯牛奶 整箱", "250ml*24盒", 59.9),
                P("芝士片", "乐牧芝士片 儿童", "200g", 24.9),
                P("酸奶", "乐牧希腊酸奶", "135g*6杯", 39.9),
            ),
            order_time="2026-10-18 08:22:47",
            status="待收货",
        ),
        Group(
            "鼎味轩调味品旗舰店",
            (
                SEAS("生抽", "鼎味轩 特级酿造生抽", "1.28L", 21.9),
                SEAS("陈醋", "鼎味轩 陈醋 五年", "500ml", 8.9, 2),
                SEAS("芝麻香油", "纯芝麻香油", "300ml", 26.8),
                SEAS("花椒油", "藤椒油", "250ml", 15.8),
                JUNK("调料收纳盒 套装", "6件套", 19.9),
            ),
            order_time="2026-10-18 20:15:09",
            status="交易成功",
        ),
        Group(
            "山野坚果旗舰店",
            (
                P("核桃", "每日坚果 薄壳核桃", "500g", 39.9),
                P("枣", "新疆灰枣 一等", "1kg", 32.9),
                P("枸杞", "宁夏枸杞 特优", "250g", 29.9),
            ),
            order_time="2026-10-19 09:30:55",
            status="已发货",
        ),
        Group(
            "鲜农生鲜旗舰店",
            (
                P("榴莲", "【产地直发】金枕榴莲 果肉", "400g", 59.0),
                P("鸡翅", "新奥尔良烤翅", "500g", 29.9),
            ),
            order_time="2026-10-19 11:02:10",
            status="待收货",
        ),
    ),
    noise=(
        Noise("广告横幅", "双十一预售开启 · 先领券后下单"),
        Noise("优惠券", "店铺优惠券 满 100 减 10"),
        Noise("猜你喜欢", "猜你喜欢", recs=(R("锦厨火锅底料", "200g", 12.9, "🍲"), R("海泊虾皮", "100g", 12.9, "🦐"))),
    ),
)

ORDER_MALL_DETAIL_01 = Sample(
    id="order-mall-detail-01",
    kind="screenshot",
    style="order-mall-detail",
    source_type="order",
    capture_date=D(2026, 10, 20),
    seed=402,
    extra={"order_no": "326010202010113", "pay": "在线支付", "address": "某某小区 4 号楼", "min_height": 780},
    groups=(
        Group(
            "鲜农生鲜旗舰店",
            (
                P("鸡蛋", "【产地直发】鲜鸡蛋 整箱", "40枚2kg", 39.9),
                P("牛腩", "草饲牛腩 切块 冷冻", "1kg", 69.0),
                P("猪肉", "黑猪后腿肉", "500g±20g", 23.9, production="今天生产"),
                P("苹果", "烟台红富士 一级", "5斤", 29.9),
                GIFT("橙子", "赠品 脐橙 2个", "2个"),
            ),
            order_time="2026-10-20 15:48:21",
            status="待收货",
        ),
    ),
    noise=(
        Noise("配送费", "运费", 0.0),
        Noise("优惠券", "店铺优惠券", -10.0),
        Noise("满减", "平台立减", -6.0),
        Noise("猜你喜欢", "为你推荐", recs=(R("海泊虾仁", "200g", 22.9, "🦐"), R("山野核桃", "500g", 39.9, "🥜"))),
    ),
)

ORDER_MALL_DETAIL_02 = Sample(
    id="order-mall-detail-02",
    kind="screenshot",
    style="order-mall-detail",
    source_type="order",
    capture_date=D(2026, 10, 18),
    seed=403,
    extra={"order_no": "326010183450218", "pay": "在线支付", "address": None, "min_height": 780},
    groups=(
        Group(
            "万味调味品专营店",
            (
                SEAS("生抽", "鼎味轩 零添加生抽", "500ml", 12.9),
                SEAS("老抽", "鼎味轩 红烧老抽", "500ml", 9.9),
                SEAS("料酒", "锦厨 黄酒料酒", "1L", 12.9),
                SEAS("香醋", "鼎味轩 香醋", "500ml", 6.9, visual_key="米醋"),
                SEAS("盐", "禾田 海盐", "350g", 3.9),
                SEAS("白糖", "禾田 绵白糖", "500g", 5.9),
                SEAS("冰糖", "单晶冰糖", "400g", 9.9),
                SEAS("八角", "八角", "100g", 7.9),
                SEAS("干辣椒", "二荆条辣椒干", "100g", 8.9),
                SEAS("花椒", "汉源花椒", "100g", 14.9),
                SEAS("白胡椒", "锦厨 白胡椒粉", "50g", 9.9, visual_key="胡椒粉"),
                SEAS("芝麻酱", "纯芝麻酱", "300g", 13.9),
                SEAS("甜面酱", "古法甜面酱", "300g", 6.9),
                SEAS("蒜蓉酱", "蒜蓉辣酱", "230g", 7.9),
                SEAS("咖喱", "日式咖喱块", "100g", 10.9),
                P("干香菇", "东北干香菇 冬菇", "150g", 19.9),
                P("干木耳", "黑木耳 干货", "200g", 17.9, visual_key="木耳"),
                P("腐竹", "手工腐竹", "250g", 12.9),
                P("紫菜", "无沙紫菜", "50g", 6.9),
                JUNK("调料瓶 玻璃 6件套", "6件", 24.9),
                JUNK("厨房一次性手套", "100只", 5.9),
            ),
            order_time="2026-10-18 03:45:02",
            status="交易成功",
        ),
    ),
    noise=(
        Noise("配送费", "运费", 0.0),
        Noise("优惠券", "店铺满减", -12.0),
        Noise("广告横幅", "确认收货后评价领红包"),
        Noise("猜你喜欢", "猜你喜欢", recs=(R("鼎味轩 蚝油", "500g", 9.9, "🥫"), R("锦厨 火锅蘸料", "200g", 6.9, "🥣"), R("禾田 淀粉", "500g", 4.9, "🌽"))),
    ),
)

# --------------------------------------------------------------------------------------
# Receipt photos ("鲜里来生活超市" thermal receipts), three contents x two photo variants.
# --------------------------------------------------------------------------------------

_RECEIPT_A = (
    Group(
        "鲜里来生活超市 某某路店",
        (
            P("鸡蛋", "鲜鸡蛋10枚", "10枚", 15.9, 2),
            P("西红柿", "西红柿", "0.536kg", 6.8),
            P("黄瓜", "水果黄瓜", "0.402kg", 5.5),
            P("牛奶", "乐牧纯牛奶250ml*12", "250ml*12", 39.9),
            P("五花肉", "五花肉(散称)", "0.618kg", 28.8),
            P("豆腐", "老豆腐400g", "400g", 3.5),
            P("大白菜", "白菜", "1.25kg", 2.9),
            SEAS("生抽", "鼎味轩生抽500ml", "500ml", 9.9),
            JUNK("柔韧抽纸3层10包", "10包", 19.9),
            P("香蕉", "香蕉(散称)", "0.84kg", 5.6),
        ),
        order_time="2026-10-19 17:42:08",
    ),
)

_RECEIPT_B = (
    Group(
        "好邻里生鲜便利 一店",
        (
            P("吐司", "全麦吐司片", "400g", 9.9),
            P("火腿", "火腿切片", "150g", 8.5, 2),
            P("酸奶", "乐牧原味酸奶8杯", "100g*8", 16.8),
            P("橙子", "脐橙(散称)", "1.12kg", 7.8),
            P("洋葱", "紫洋葱", "0.55kg", 4.5),
            P("鸡胸肉", "冷鲜鸡胸肉", "420g", 12.9),
            SEAS("蚝油", "锦厨蚝油510g", "510g", 8.9),
            JUNK("5号电池8粒", "8粒", 14.9),
            P("鲜面条", "鲜切面条", "500g", 5.9),
        ),
        order_time="2026-10-20 19:05:41",
    ),
)

_RECEIPT_C = (
    Group(
        "百味园菜市 某某街店",
        (
            P("土豆", "土豆", "1.34kg", 3.9),
            P("胡萝卜", "胡萝卜", "0.62kg", 3.5),
            P("青椒", "青椒", "0.31kg", 7.8),
            P("鲈鱼", "鲈鱼(现杀)", "0.72kg", 38.0),
            P("基围虾", "基围虾", "0.45kg", 98.0),
            P("豆芽", "绿豆芽", "0.30kg", 6.0),
            P("韭菜", "韭菜", "0.28kg", 8.0, optional=True),
            P("葱", "小香葱", "0.10kg", 20.0, optional=True),
            SEAS("盐", "加碘盐500g", "500g", 1.9),
            JUNK("洗洁精1kg", "1kg", 7.9),
        ),
        order_time="2026-10-21 08:12:37",
    ),
)


def _receipt(id_: str, groups, variant: str, capture: date, seed: int) -> Sample:
    return Sample(
        id=id_,
        kind="receipt",
        style="receipt",
        source_type="receipt",
        capture_date=capture,
        groups=groups,
        seed=seed,
        extra={"variant": variant},
        noise=(Noise("优惠券", "会员优惠", -2.0),) if variant in ("flat", "rotated") else (),
    )


RECEIPT_01 = _receipt("receipt-01-flat", _RECEIPT_A, "flat", D(2026, 10, 19), 501)
RECEIPT_02 = _receipt("receipt-02-rotated-shadow", _RECEIPT_A, "rotated", D(2026, 10, 19), 502)
RECEIPT_03 = _receipt("receipt-03-perspective-blur", _RECEIPT_B, "perspective", D(2026, 10, 20), 503)
RECEIPT_04 = _receipt("receipt-04-wrinkled", _RECEIPT_B, "wrinkled", D(2026, 10, 20), 504)
RECEIPT_05 = _receipt("receipt-05-occluded", _RECEIPT_C, "occluded", D(2026, 10, 21), 505)
RECEIPT_06 = _receipt("receipt-06-dim-uneven", _RECEIPT_C, "dim", D(2026, 10, 21), 506)

# --------------------------------------------------------------------------------------
# Pasted order text, four platform flavours.
# --------------------------------------------------------------------------------------

TEXT_01 = Sample(
    id="text-darkstore-01",
    kind="text",
    style="text-darkstore",
    source_type="text",
    capture_date=D(2026, 10, 19),
    seed=601,
    extra={"order_no": "2026101918432571"},
    groups=(
        Group(
            "晨露买菜 · 东区前置仓",
            (
                P("西红柿", "新鲜沙瓤西红柿", "500g±20g", 6.8),
                P("肥牛", "火锅肥牛片 精选", "250g", 24.9, 2),
                P("鸡蛋", "【产地直发】鲜鸡蛋", "10枚530g", 15.9),
                P("上海青", "上海青 嫩叶", "300g", 3.9),
                P("豆腐", "老豆腐 现做", "400g", 3.5),
                SEAS("蚝油", "锦厨 蚝油", "510g", 8.9),
                GIFT("香菜", "赠品 香菜", "约50g"),
                JUNK("抽取式纸巾", "6包", 12.9),
            ),
            order_time="2026-10-19 18:43:25",
        ),
    ),
    noise=(Noise("配送费", "配送费", 2.0), Noise("优惠券", "新客立减券", -5.0)),
)

TEXT_02 = Sample(
    id="text-delivery-02",
    kind="text",
    style="text-delivery",
    source_type="text",
    capture_date=D(2026, 10, 20),
    seed=602,
    extra={"order_no": "7261020103300219"},
    groups=(
        Group(
            "小满便利 · 超市",
            (
                P("牛奶", "乐牧鲜牛奶 巴氏杀菌", "950ml", 18.9),
                P("吐司", "切片吐司 原味", "350g", 7.9),
                P("火腿", "鲜切火腿片", "150g", 9.9),
                P("鸡蛋", "鲜鸡蛋 30枚装", "30枚1.5kg", 29.9),
                P("苹果", "烟台红富士", "约1kg", 11.9, 2),
                JUNK("一次性湿厕纸", "40片", 7.9),
            ),
            order_time="今天 10:33",
        ),
    ),
    noise=(Noise("包装费", "包装费", 1.0), Noise("配送费", "配送费", 4.0), Noise("优惠券", "商家满减", -3.0)),
)

TEXT_03 = Sample(
    id="text-mall-03",
    kind="text",
    style="text-mall",
    source_type="text",
    capture_date=D(2026, 10, 18),
    seed=603,
    extra={"order_no": "326010183450218"},
    groups=(
        Group(
            "鲜农生鲜旗舰店",
            (
                P("鸡蛋", "【产地直发】鲜鸡蛋 整箱", "40枚2kg", 39.9),
                P("牛腩", "草饲牛腩 切块 冷冻", "1kg", 69.0),
                P("苹果", "烟台红富士 一级", "5斤", 29.9, 2),
                GIFT("橙子", "赠品 脐橙 2个", "2个"),
                SEAS("生抽", "鼎味轩 特级酿造生抽", "1.28L", 21.9),
                JUNK("调料收纳盒 套装", "6件套", 19.9),
            ),
            order_time="2026年10月18日 03:45",
        ),
    ),
    noise=(Noise("配送费", "运费", 0.0), Noise("优惠券", "店铺优惠券", -10.0)),
)

TEXT_04 = Sample(
    id="text-share-04",
    kind="text",
    style="text-share",
    source_type="text",
    capture_date=D(2026, 10, 21),
    seed=604,
    extra={},
    groups=(
        Group(
            "青禾鲜市",
            (
                P("鸡胸肉", "鸡胸肉 冷鲜", "400g", 12.9),
                P("西兰花", "西兰花 新鲜", "约350g", 6.9),
                P("秋葵", "绿秋葵 嫩", "250g", 7.9),
                P("牛奶", "乐牧高钙牛奶", "250ml*10盒", 36.9),
                P("鸡蛋", "鲜鸡蛋 土鸡蛋", "10枚530g", 15.9, 2),
                SEAS("酱油", "鼎味轩 酿造酱油", "500ml", 7.9),
                P("玉米", "糯玉米 真空装", "4根", 12.9, selected=True),
            ),
            order_time=None,
        ),
    ),
    noise=(Noise("配送费", "配送费", 3.0),),
)

SAMPLES: tuple[Sample, ...] = (
    CART_FRESH_LONG_01,
    CART_FRESH_LONG_02,
    CART_FRESH_01,
    CART_FRESH_02,
    ORDER_DARKSTORE_01,
    ORDER_DARKSTORE_02,
    ORDER_DARKSTORE_03,
    ORDER_DELIVERY_01,
    ORDER_DELIVERY_02,
    ORDER_DELIVERY_03,
    ORDER_MALL_LIST_01,
    ORDER_MALL_DETAIL_01,
    ORDER_MALL_DETAIL_02,
    RECEIPT_01,
    RECEIPT_02,
    RECEIPT_03,
    RECEIPT_04,
    RECEIPT_05,
    RECEIPT_06,
    TEXT_01,
    TEXT_02,
    TEXT_03,
    TEXT_04,
)

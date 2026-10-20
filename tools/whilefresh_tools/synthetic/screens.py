"""HTML renderers for the four app-screenshot styles.

All brand names are fictional ("青禾鲜市", "晨露买菜", "闪达超市", "好集商城") and there is no
logo or trademark; the looks only borrow generic layout conventions of each platform type.
"""

from __future__ import annotations

from .html_common import checkbox, document, emoji_for, esc, money, thumb
from .model import Noise, Product, Sample


def _noise(sample: Sample, *kinds: str) -> list[Noise]:
    return [n for n in sample.noise if n.kind in kinds]


def _line_total(p: Product) -> float:
    return p.price * p.count


def _selected_total(sample: Sample) -> float:
    return sum(_line_total(p) for p in sample.products if p.selected and not p.gift)


def _fees(sample: Sample) -> float:
    return sum(n.amount for n in sample.noise if n.amount is not None and n.kind in ("配送费", "包装费", "优惠券", "满减"))


def _paid(sample: Sample) -> float:
    return _selected_total(sample) + _fees(sample)


def _date_tags(p: Product, cls: str) -> str:
    out = ""
    if p.production:
        out += f'<span class="{cls} prod">{esc(p.production)}</span>'
    if p.expiry:
        out += f'<span class="{cls} exp">{esc(p.expiry)}</span>'
    return out


def _signed(amount: float) -> str:
    return f"-¥{money(-amount)}" if amount < 0 else f"¥{money(amount)}"


def _status_bar(time_text: str = "09:41") -> str:
    return (
        '<div class="status"><span>' + time_text + '</span><span class="sb-right">5G  ▂▄▆█  🔋</span></div>'
    )


_STATUS_CSS = """
.status{height:26px;padding:6px 14px 0;display:flex;justify-content:space-between;font-size:12px;font-weight:600}
"""


# ------------------------------------------------------------------ A: fresh-supermarket cart

_CART_CSS = (
    _STATUS_CSS
    + """
.screen{background:#f4f6f5;position:relative}
.nav{height:44px;display:flex;align-items:center;justify-content:space-between;padding:0 14px;background:#fff;font-size:17px;font-weight:600}
.nav small{font-size:13px;font-weight:400;color:#666}
.addr{background:#fff;padding:6px 14px 10px;font-size:12px;color:#555;border-bottom:1px solid #eee}
.addr b{color:#1faa6b}
.ad{margin:10px 10px 0;border-radius:10px;padding:14px 16px;color:#fff;font-weight:700;font-size:16px;background:linear-gradient(100deg,#1faa6b,#6fd29c)}
.ad small{display:block;font-weight:400;font-size:11px;margin-top:3px;opacity:.9}
.coupon{margin:10px 10px 0;padding:9px 12px;border-radius:8px;background:#fff7e6;color:#c77700;font-size:12px;display:flex;justify-content:space-between}
.card{margin:10px 10px 0;background:#fff;border-radius:10px;padding:0 10px}
.ghead{display:flex;align-items:center;gap:8px;padding:12px 0 8px;font-size:14px;font-weight:600}
.item{display:flex;gap:8px;align-items:center;padding:7px 0;border-top:1px solid #f1f1f1}
.item .info{flex:1;min-width:0}
.item .name{font-size:13px;line-height:1.3;color:#222;max-height:34px;overflow:hidden}
.item .spec{display:inline-block;margin-top:3px;font-size:11px;color:#888;background:#f3f3f3;border-radius:3px;padding:1px 5px}
.tag{display:inline-block;font-size:10px;border-radius:3px;padding:0 4px;margin-left:4px;vertical-align:1px}
.tag.prod{color:#1a9a5c;border:1px solid #1a9a5c}
.tag.exp{color:#888;border:1px solid #ccc}
.tag.gift{color:#fff;background:#f5503a}
.price-row{display:flex;justify-content:space-between;align-items:center;margin-top:2px}
.price{color:#f5503a;font-weight:700;font-size:15px}
.price small{font-size:11px}
.orig{color:#aaa;text-decoration:line-through;font-size:11px;margin-left:5px;font-weight:400}
.step{display:flex;align-items:center;border:1px solid #ddd;border-radius:12px;font-size:13px;overflow:hidden}
.step span{width:22px;text-align:center;line-height:20px}
.step span.n{width:26px;color:#222;font-weight:600}
.tip{margin:10px 10px 0;padding:9px 12px;border-radius:8px;background:#e9f8ef;color:#13824d;font-size:12px;display:flex;justify-content:space-between}
.sechead{padding:12px 0 6px;font-size:14px;font-weight:600}
.swap{display:flex;gap:8px;padding-bottom:12px}
.swap .c{flex:1;text-align:center;font-size:11px;color:#444}
.swap .c .t{margin:0 auto 4px}
.swap .c b{color:#f5503a;display:block;margin-top:2px}
.swap .c i{display:inline-block;font-style:normal;margin-top:3px;border:1px solid #f5503a;color:#f5503a;border-radius:9px;padding:0 6px;font-size:10px}
.grid{display:flex;flex-wrap:wrap;gap:8px;padding-bottom:10px}
.grid .g{width:calc(50% - 4px);border:1px solid #f0f0f0;border-radius:8px;padding:8px}
.grid .g .nm{font-size:12px;margin-top:6px;height:32px;overflow:hidden}
.grid .g .sp{font-size:10px;color:#999}
.grid .g .pr{display:flex;justify-content:space-between;align-items:center;margin-top:4px;color:#f5503a;font-weight:700;font-size:14px}
.grid .g .pr i{font-style:normal;width:20px;height:20px;border-radius:50%;background:#1faa6b;color:#fff;text-align:center;line-height:19px}
.feebar{margin:10px 10px 0;padding:10px 12px;border-radius:8px;background:#fff;font-size:12px;color:#666;display:flex;justify-content:space-between}
.bottom{margin-top:14px;height:54px;background:#fff;border-top:1px solid #eee;display:flex;align-items:center;padding:0 12px;gap:8px;font-size:13px}
.bottom .sum{margin-left:auto;text-align:right;font-size:12px;color:#666}
.bottom .sum b{color:#f5503a;font-size:16px}
.bottom .go{background:#1faa6b;color:#fff;border-radius:18px;padding:9px 16px;font-weight:600;font-size:14px}
"""
)


def _cart_item(p: Product) -> str:
    qty = (
        '<span style="color:#888;font-size:12px">x1</span>'
        if p.gift
        else f'<div class="step"><span>−</span><span class="n">{p.count}</span><span>＋</span></div>'
    )
    price = "¥0.00" if p.gift else f"¥{money(p.price)}"
    orig = f'<span class="orig">¥{money(p.orig_price)}</span>' if p.orig_price else ""
    gift_tag = '<span class="tag gift">赠品</span>' if p.gift else ""
    return (
        '<div class="item">'
        + checkbox(p.selected, "#1faa6b")
        + thumb(emoji_for(p), 56, (p.visual_key or p.canonical) + p.raw)
        + '<div class="info">'
        + f'<div class="name">{esc(p.raw)}{gift_tag}{_date_tags(p, "tag")}</div>'
        + (f'<span class="spec">{esc(p.spec)}</span>' if p.spec else "")
        + f'<div class="price-row"><div class="price">{price}{orig}</div>{qty}</div>'
        + "</div></div>"
    )


def render_cart_fresh(sample: Sample) -> tuple[str, int]:
    extra = sample.extra
    parts = [_status_bar(), f'<div class="nav"><span></span><span>购物车 ({len(sample.products)})</span><small>管理</small></div>']
    addr = extra.get("address")
    eta = extra.get("eta", "")
    parts.append(f'<div class="addr">{"配送至 <b>" + esc(addr) + "</b> ▾　" if addr else ""}{esc(eta)}</div>')
    for n in _noise(sample, "广告横幅"):
        parts.append(f'<div class="ad">{esc(n.title)}<small>限时领取，立即使用 ›</small></div>')
    for n in _noise(sample, "优惠券"):
        parts.append(f'<div class="coupon"><span>🎫 {esc(n.title)}</span><span>去领取 ›</span></div>')
    for g in sample.groups:
        rows = "".join(_cart_item(p) for p in g.products)
        parts.append(
            f'<div class="card"><div class="ghead">{checkbox(True, "#1faa6b")}<span>{esc(g.name)}</span></div>{rows}</div>'
        )
    for n in _noise(sample, "凑单提示"):
        parts.append(f'<div class="tip"><span>🚚 {esc(n.title)}</span><span>去凑单 ›</span></div>')
    for n in _noise(sample, "换购推荐"):
        cells = "".join(
            f'<div class="c"><div class="t">{thumb(r.emoji, 56, r.name)}</div>{esc(r.name)}<b>¥{money(r.price)}</b><i>换购</i></div>'
            for r in n.recs
        )
        parts.append(f'<div class="card"><div class="sechead">{esc(n.title)}</div><div class="swap">{cells}</div></div>')
    for n in _noise(sample, "配送费"):
        parts.append(
            f'<div class="feebar"><span>{esc(n.title)}</span><span>¥{money(n.amount or 0)}（满 39 元免配送费）</span></div>'
        )
    for n in _noise(sample, "热卖推荐"):
        cells = "".join(
            f'<div class="g">{thumb(r.emoji, 64, r.name)}<div class="nm">{esc(r.name)}</div><div class="sp">{esc(r.spec)}</div>'
            f'<div class="pr"><span>¥{money(r.price)}</span><i>＋</i></div></div>'
            for r in n.recs
        )
        parts.append(f'<div class="card"><div class="sechead">🔥 {esc(n.title)}</div><div class="grid">{cells}</div></div>')
    n_sel = sum(p.count for p in sample.products if p.selected)
    parts.append(
        f'<div class="bottom">{checkbox(False, "#1faa6b")}<span>全选</span>'
        f'<div class="sum">合计 <b>¥{money(_selected_total(sample))}</b><br>不含配送费</div>'
        f'<div class="go">去结算({n_sel})</div></div>'
    )
    return document("\n".join(parts), _CART_CSS, 360, int(extra.get("min_height", 780)), sample.id), 360


# ------------------------------------------------------------------ B: dark-store order detail

_DARK_CSS = (
    _STATUS_CSS
    + """
.screen{background:#f5f5f5}
.hero{background:linear-gradient(160deg,#ff8a2b,#ff6a00);color:#fff;padding:0 16px 22px}
.hero .nav2{height:40px;display:flex;align-items:center;font-size:16px;font-weight:600;justify-content:center;position:relative}
.hero .nav2 i{position:absolute;left:0;font-style:normal;font-size:20px}
.hero h2{font-size:22px;margin-top:6px}
.hero p{font-size:12px;opacity:.9;margin-top:5px}
.ad{margin:10px 12px 0;padding:11px 14px;border-radius:10px;background:#fff1e3;color:#d9600a;font-size:13px;font-weight:600}
.card{margin:10px 12px 0;background:#fff;border-radius:10px;padding:2px 12px}
.shop{padding:12px 0 8px;font-size:14px;font-weight:600;border-bottom:1px solid #f0f0f0}
.line{display:flex;gap:10px;padding:12px 0;border-bottom:1px solid #f6f6f6}
.line .info{flex:1;min-width:0}
.line .nm{font-size:13.5px;color:#222;line-height:1.35}
.line .sp{font-size:11.5px;color:#999;margin-top:3px}
.line .rt{text-align:right;font-size:13px;white-space:nowrap}
.line .rt b{display:block;font-size:14px}
.line .rt span{color:#999;font-size:12px}
.tag{display:inline-block;font-size:10px;border-radius:3px;padding:0 4px;margin-left:4px;vertical-align:1px}
.tag.prod{color:#e8710a;background:#fff1e3}
.tag.exp{color:#777;background:#f0f0f0}
.tag.gift{color:#fff;background:#ff6a00}
.sum{padding:10px 0;font-size:13px}
.sum .r{display:flex;justify-content:space-between;padding:4px 0;color:#555}
.sum .r.neg span:last-child{color:#ff6a00}
.sum .total{border-top:1px solid #f3f3f3;margin-top:6px;padding-top:10px;text-align:right;font-size:13px;color:#222}
.sum .total b{font-size:18px;color:#ff6a00}
.info-box{padding:10px 0;font-size:12.5px;color:#666}
.info-box .r{display:flex;justify-content:space-between;padding:5px 0}
.info-box .r span:last-child{color:#222}
.grid{display:flex;gap:8px;padding-bottom:12px}
.grid .g{flex:1;text-align:center;font-size:11px}
.grid .g b{display:block;color:#ff6a00;margin-top:2px}
.sechead{padding:12px 0 8px;font-size:14px;font-weight:600}
.btns{display:flex;justify-content:flex-end;gap:8px;padding:6px 0 12px}
.btns span{border:1px solid #ccc;border-radius:14px;padding:5px 14px;font-size:12px}
.btns span.p{border-color:#ff6a00;color:#ff6a00}
"""
)


def render_order_darkstore(sample: Sample) -> tuple[str, int]:
    extra = sample.extra
    g = sample.groups[0]
    parts = [
        '<div class="hero">' + _status_bar("18:46") +
        '<div class="nav2"><i>‹</i>订单详情</div>'
        f'<h2>{esc(extra.get("status", "已送达"))}</h2>'
        + (f'<p>配送至 {esc(extra["address"])}</p>' if extra.get("address") else "<p>感谢您使用晨露买菜</p>")
        + "</div>"
    ]
    for n in _noise(sample, "广告横幅"):
        parts.append(f'<div class="ad">🎁 {esc(n.title)}</div>')
    rows = []
    for p in g.products:
        gift_tag = '<span class="tag gift">赠品</span>' if p.gift else ""
        price = "¥0.00" if p.gift else f"¥{money(p.price)}"
        rows.append(
            '<div class="line">'
            + thumb(emoji_for(p), 58, (p.visual_key or p.canonical) + p.raw)
            + f'<div class="info"><div class="nm">{esc(p.raw)}{gift_tag}{_date_tags(p, "tag")}</div>'
            + (f'<div class="sp">{esc(p.spec)}</div>' if p.spec else "")
            + f'</div><div class="rt"><b>{price}</b><span>x{p.count}</span></div></div>'
        )
    parts.append(f'<div class="card"><div class="shop">{esc(g.name)} ›</div>{"".join(rows)}')
    sums = [f'<div class="r"><span>商品总额</span><span>¥{money(_selected_total(sample))}</span></div>']
    for n in _noise(sample, "配送费", "包装费", "优惠券", "满减"):
        label = "满减优惠" if n.kind == "满减" else n.title
        neg = ' neg' if (n.amount or 0) < 0 else ""
        sums.append(f'<div class="r{neg}"><span>{esc(label)}</span><span>{_signed(n.amount or 0)}</span></div>')
    parts.append(
        f'<div class="sum">{"".join(sums)}<div class="total">实付 <b>¥{money(_paid(sample))}</b></div></div></div>'
    )
    for n in _noise(sample, "猜你喜欢", "热卖推荐"):
        cells = "".join(
            f'<div class="g">{thumb(r.emoji, 80, r.name)}<div>{esc(r.name)}</div><b>¥{money(r.price)}</b></div>'
            for r in n.recs
        )
        parts.append(f'<div class="card"><div class="sechead">{esc(n.title)}</div><div class="grid">{cells}</div></div>')
    info = [f'<div class="r"><span>订单编号</span><span>{esc(extra["order_no"])}</span></div>']
    if g.order_time:
        info.append(f'<div class="r"><span>下单时间</span><span>{esc(g.order_time)}</span></div>')
    info.append(f'<div class="r"><span>支付方式</span><span>{esc(extra.get("pay", "在线支付"))}</span></div>')
    parts.append(f'<div class="card"><div class="info-box">{"".join(info)}</div></div>')
    parts.append('<div class="card"><div class="btns"><span>联系客服</span><span class="p">再来一单</span></div></div>')
    parts.append('<div style="height:20px"></div>')
    return document("\n".join(parts), _DARK_CSS, 375, int(extra.get("min_height", 780)), sample.id), 375


# ------------------------------------------------------------------ C: delivery-platform supermarket order

_DELIVERY_CSS = (
    _STATUS_CSS
    + """
.screen{background:#f6f6f6}
.top{background:#ffd21e;padding-bottom:30px}
.top .nav2{height:40px;display:flex;align-items:center;padding:0 14px;font-size:16px;font-weight:600}
.top .nav2 i{font-style:normal;margin-right:auto}
.top h2{padding:0 14px;font-size:20px}
.top p{padding:2px 14px 0;font-size:12px;color:#5a4a00}
.panel{margin:-18px 10px 0;background:#fff;border-radius:12px;padding:12px}
.store{font-size:15px;font-weight:700;padding-bottom:8px;border-bottom:1px solid #f1f1f1}
.it{display:flex;gap:10px;align-items:center;padding:10px 0}
.it .mid{flex:1;min-width:0}
.it .nm{font-size:13.5px;font-weight:600;line-height:1.3}
.it .sp{font-size:11px;color:#999;margin-top:3px}
.it .q{font-size:13px;color:#666;width:30px;text-align:center}
.it .pr{font-size:14px;font-weight:600;width:62px;text-align:right}
.tg{display:inline-block;font-size:10px;margin-left:4px;padding:0 4px;border-radius:3px;font-weight:400;vertical-align:1px}
.tg.prod{background:#fff6cc;color:#8a6d00}
.tg.exp{background:#eee;color:#666}
.tg.gift{background:#ff4d4f;color:#fff}
.fee{display:flex;justify-content:space-between;font-size:13px;color:#555;padding:5px 0}
.fee .neg{color:#ff4d4f}
.paid{text-align:right;padding-top:8px;border-top:1px solid #f1f1f1;font-size:12px;color:#777}
.paid b{font-size:17px;color:#222}
.pill{margin:10px 10px 0;background:#fff9e0;border:1px dashed #f0c419;border-radius:10px;padding:10px 12px;font-size:13px;color:#8a6d00;display:flex;justify-content:space-between}
.pill b{background:#ffd21e;color:#222;border-radius:12px;padding:2px 10px;font-size:12px}
.card{margin:10px 10px 0;background:#fff;border-radius:12px;padding:12px}
.meta .r{display:flex;justify-content:space-between;font-size:12.5px;padding:5px 0;color:#777}
.meta .r span:last-child{color:#222}
.sec{font-size:14px;font-weight:700;margin-bottom:8px}
.rec{display:flex;gap:8px}
.rec .g{flex:1;text-align:center;font-size:11px}
.rec .g b{display:block;margin-top:2px}
"""
)


def render_order_delivery(sample: Sample) -> tuple[str, int]:
    extra = sample.extra
    g = sample.groups[0]
    parts = [
        '<div class="top">' + _status_bar("10:36")
        + '<div class="nav2"><i>‹</i>订单详情</div><h2>订单已完成</h2>'
        + (f'<p>已送达 {esc(extra["address"])}</p>' if extra.get("address") else "<p>感谢您的支持，期待再次光临</p>")
        + "</div>"
    ]
    rows = []
    for p in g.products:
        gift = '<span class="tg gift">赠品</span>' if p.gift else ""
        total = "¥0" if p.gift else f"¥{money(_line_total(p))}"
        rows.append(
            '<div class="it">' + thumb(emoji_for(p), 48, (p.visual_key or p.canonical) + p.raw, 8)
            + f'<div class="mid"><div class="nm">{esc(p.raw)}{gift}{_date_tags(p, "tg")}</div>'
            + (f'<div class="sp">{esc(p.spec)}</div>' if p.spec else "")
            + f'</div><div class="q">×{p.count}</div><div class="pr">{total}</div></div>'
        )
    fees = []
    for n in _noise(sample, "包装费", "配送费", "优惠券", "满减"):
        neg = n.amount is not None and n.amount < 0
        fees.append(
            f'<div class="fee"><span>{esc(n.title)}</span><span class="{"neg" if neg else ""}">{_signed(n.amount or 0)}</span></div>'
        )
    saved = -sum(n.amount for n in sample.noise if n.amount is not None and n.amount < 0)
    parts.append(
        f'<div class="panel"><div class="store">🏪 {esc(g.name)} ›</div>{"".join(rows)}{"".join(fees)}'
        f'<div class="paid">已优惠 ¥{money(saved)}　合计 <b>¥{money(_paid(sample))}</b></div></div>'
    )
    for n in _noise(sample, "广告横幅"):
        parts.append(f'<div class="pill"><span>👑 {esc(n.title)}</span><b>开通</b></div>')
    for n in _noise(sample, "猜你喜欢", "热卖推荐"):
        cells = "".join(
            f'<div class="g">{thumb(r.emoji, 70, r.name, 10)}<div>{esc(r.name)}</div><b>¥{money(r.price)}</b></div>'
            for r in n.recs
        )
        parts.append(f'<div class="card"><div class="sec">{esc(n.title)}</div><div class="rec">{cells}</div></div>')
    meta = [f'<div class="r"><span>订单号</span><span>{esc(extra["order_no"])}</span></div>']
    if g.order_time:
        meta.append(f'<div class="r"><span>下单时间</span><span>{esc(g.order_time)}</span></div>')
    meta.append(f'<div class="r"><span>支付方式</span><span>{esc(extra.get("pay", "在线支付"))}</span></div>')
    parts.append(f'<div class="card meta"><div class="sec">订单信息</div>{"".join(meta)}</div>')
    parts.append('<div style="height:20px"></div>')
    return document("\n".join(parts), _DELIVERY_CSS, 360, int(extra.get("min_height", 780)), sample.id), 360


# ------------------------------------------------------------------ D: e-commerce order list / detail

_MALL_CSS = (
    _STATUS_CSS
    + """
.screen{background:#f2f2f2}
.nav{height:44px;background:#fff;display:flex;align-items:center;justify-content:center;font-size:17px;font-weight:600;position:relative}
.nav i{position:absolute;left:14px;font-style:normal;font-size:20px;font-weight:400}
.tabs{display:flex;background:#fff;padding:0 6px;font-size:13px;color:#555;border-bottom:1px solid #eee}
.tabs span{flex:1;text-align:center;padding:10px 0}
.tabs span.on{color:#ff3b30;font-weight:700;border-bottom:2px solid #ff3b30}
.search{background:#fff;padding:8px 12px}
.search div{background:#f2f2f2;border-radius:16px;padding:7px 14px;font-size:12px;color:#999}
.ad{margin:10px 10px 0;border-radius:10px;padding:14px;background:linear-gradient(100deg,#ff3b30,#ff8a5c);color:#fff;font-weight:700;font-size:15px}
.coupon{margin:8px 10px 0;padding:8px 12px;border-radius:8px;background:#fff1ef;color:#d92f24;font-size:12px}
.order{margin:10px 10px 0;background:#fff;border-radius:10px;padding:0 12px}
.oh{display:flex;justify-content:space-between;padding:12px 0 6px;font-size:13px;font-weight:600}
.oh span:last-child{color:#ff3b30;font-weight:400}
.ot{font-size:11px;color:#999;padding-bottom:6px}
.pi{display:flex;gap:10px;padding:7px 0}
.pi .info{flex:1;min-width:0}
.pi .t{font-size:13px;line-height:1.35;height:35px;overflow:hidden;color:#222}
.pi .s{display:inline-block;margin-top:4px;font-size:11px;color:#888;background:#f5f5f5;border-radius:3px;padding:1px 5px}
.pi .rt{text-align:right;font-size:13px;white-space:nowrap}
.pi .rt b{display:block;font-weight:600}
.pi .rt span{color:#999;font-size:12px}
.pi .rt s{color:#bbb;font-size:11px;display:block}
.tag{display:inline-block;font-size:10px;border-radius:2px;padding:0 4px;margin-left:4px;vertical-align:1px}
.tag.prod{color:#d92f24;border:1px solid #d92f24}
.tag.exp{color:#888;border:1px solid #ccc}
.tag.gift{color:#fff;background:#ff3b30}
.of{text-align:right;font-size:12px;color:#666;padding:6px 0}
.of b{font-size:14px;color:#222}
.btns{display:flex;justify-content:flex-end;gap:8px;padding:2px 0 12px}
.btns span{border:1px solid #ccc;border-radius:14px;padding:5px 12px;font-size:12px;color:#444}
.btns span.p{border-color:#ff3b30;color:#ff3b30}
.rec{margin:10px 10px 0;background:#fff;border-radius:10px;padding:12px}
.rec .h{font-size:14px;font-weight:700;margin-bottom:8px}
.rec .grid{display:flex;flex-wrap:wrap;gap:8px}
.rec .g{width:calc(50% - 4px);font-size:12px}
.rec .g b{color:#ff3b30;display:block;margin-top:3px}
.hero{background:linear-gradient(160deg,#ff3b30,#ff6a4d);color:#fff;padding:14px 16px;font-size:17px;font-weight:700}
.hero small{display:block;font-weight:400;font-size:12px;margin-top:4px;opacity:.9}
.box{margin:10px 10px 0;background:#fff;border-radius:10px;padding:2px 12px}
.sumr{display:flex;justify-content:space-between;padding:6px 0;font-size:13px;color:#555}
.sumr .neg{color:#ff3b30}
.paid{text-align:right;padding:8px 0 10px;border-top:1px solid #f3f3f3;font-size:12px;color:#666}
.paid b{font-size:17px;color:#ff3b30}
.kv{display:flex;justify-content:space-between;padding:5px 0;font-size:12.5px;color:#777}
.kv span:last-child{color:#222}
"""
)


def _mall_item(p: Product) -> str:
    gift = '<span class="tag gift">赠品</span>' if p.gift else ""
    price = "¥0.00" if p.gift else f"¥{money(p.price)}"
    orig = f"<s>¥{money(p.orig_price)}</s>" if p.orig_price else ""
    return (
        '<div class="pi">' + thumb(emoji_for(p), 64, (p.visual_key or p.canonical) + p.raw, 6)
        + f'<div class="info"><div class="t">{esc(p.raw)}{gift}{_date_tags(p, "tag")}</div>'
        + (f'<span class="s">规格:{esc(p.spec)}</span>' if p.spec else "")
        + f'</div><div class="rt"><b>{price}</b>{orig}<span>x{p.count}</span></div></div>'
    )


def _mall_recs(sample: Sample) -> list[str]:
    out = []
    for n in _noise(sample, "猜你喜欢", "热卖推荐"):
        cells = "".join(
            f'<div class="g">{thumb(r.emoji, 120, r.name, 8)}<div>{esc(r.name)}</div><b>¥{money(r.price)}</b></div>'
            for r in n.recs
        )
        out.append(f'<div class="rec"><div class="h">{esc(n.title)}</div><div class="grid">{cells}</div></div>')
    return out


def render_order_mall_list(sample: Sample) -> tuple[str, int]:
    extra = sample.extra
    parts = [
        _status_bar("20:11"),
        '<div class="nav"><i>‹</i>我的订单</div>',
        '<div class="tabs"><span class="on">全部</span><span>待付款</span><span>待发货</span><span>待收货</span><span>待评价</span></div>',
        '<div class="search"><div>🔍 搜索我的订单</div></div>',
    ]
    for n in _noise(sample, "广告横幅"):
        parts.append(f'<div class="ad">{esc(n.title)}</div>')
    for n in _noise(sample, "优惠券"):
        parts.append(f'<div class="coupon">🎟 {esc(n.title)}　去使用 ›</div>')
    for g in sample.groups:
        total = sum(_line_total(p) for p in g.products if not p.gift)
        n_items = sum(p.count for p in g.products)
        action = "再次购买" if g.status == "交易成功" else "查看物流"
        parts.append(
            f'<div class="order"><div class="oh"><span>🏬 {esc(g.name)} ›</span><span>{esc(g.status or "")}</span></div>'
            f'<div class="ot">下单时间 {esc(g.order_time)}</div>'
            + "".join(_mall_item(p) for p in g.products)
            + f'<div class="of">共 {n_items} 件商品　实付款 <b>¥{money(total)}</b></div>'
            f'<div class="btns"><span>删除订单</span><span class="p">{action}</span></div></div>'
        )
    parts.extend(_mall_recs(sample))
    parts.append('<div style="height:24px"></div>')
    return document("\n".join(parts), _MALL_CSS, 390, int(extra.get("min_height", 780)), sample.id), 390


def render_order_mall_detail(sample: Sample) -> tuple[str, int]:
    extra = sample.extra
    g = sample.groups[0]
    parts = [
        _status_bar("15:52"),
        '<div class="nav"><i>‹</i>订单详情</div>',
        f'<div class="hero">{esc(g.status or "交易成功")}<small>'
        + ("卖家已发货，请注意查收" if g.status == "待收货" else "感谢您在好集商城购物")
        + "</small></div>",
    ]
    if extra.get("address"):
        parts.append(f'<div class="box"><div class="kv"><span>收货地址</span><span>{esc(extra["address"])}</span></div></div>')
    for n in _noise(sample, "广告横幅"):
        parts.append(f'<div class="ad">{esc(n.title)}</div>')
    parts.append(f'<div class="order"><div class="oh"><span>🏬 {esc(g.name)} ›</span><span></span></div>')
    parts.extend(_mall_item(p) for p in g.products)
    parts.append("</div>")
    sums = [f'<div class="sumr"><span>商品总价</span><span>¥{money(_selected_total(sample))}</span></div>']
    for n in _noise(sample, "配送费", "包装费", "优惠券", "满减"):
        neg = ' class="neg"' if (n.amount or 0) < 0 else ""
        sums.append(f'<div class="sumr"><span>{esc(n.title)}</span><span{neg}>{_signed(n.amount or 0)}</span></div>')
    parts.append(
        f'<div class="box">{"".join(sums)}<div class="paid">实付款 <b>¥{money(_paid(sample))}</b></div></div>'
    )
    kv = [f'<div class="kv"><span>订单编号</span><span>{esc(extra["order_no"])}</span></div>']
    if g.order_time:
        kv.append(f'<div class="kv"><span>下单时间</span><span>{esc(g.order_time)}</span></div>')
    kv.append(f'<div class="kv"><span>支付方式</span><span>{esc(extra.get("pay", "在线支付"))}</span></div>')
    parts.append(f'<div class="box">{"".join(kv)}</div>')
    parts.extend(_mall_recs(sample))
    parts.append('<div style="height:24px"></div>')
    return document("\n".join(parts), _MALL_CSS, 390, int(extra.get("min_height", 780)), sample.id), 390


RENDERERS = {
    "cart-fresh": render_cart_fresh,
    "order-darkstore": render_order_darkstore,
    "order-delivery": render_order_delivery,
    "order-mall-list": render_order_mall_list,
    "order-mall-detail": render_order_mall_detail,
}

"""Four distinct, invented pasted-order-text formats."""

from __future__ import annotations

from .model import Product, Sample


def _cost(p: Product) -> str:
    return "0.00" if p.gift else f"{p.price:.2f}"


def render_text(sample: Sample) -> str:
    group = sample.groups[0]
    products = group.products
    style = sample.style
    if style == "text-darkstore":
        lines = [
            "晨露买菜　订单详情",
            f"下单时间：{group.order_time}",
            f"订单编号：{sample.extra['order_no']}",
            f"商家：{group.name}",
            "订单商品：",
        ]
        for p in products:
            lines.append(f"  {p.raw}   {p.spec or ''}   ×{p.count}   ¥{_cost(p)}")
        lines.extend(f"{n.title}：{'-' if (n.amount or 0) < 0 else ''}¥{abs(n.amount or 0):.2f}" for n in sample.noise)
        lines.append("状态：已送达")
    elif style == "text-delivery":
        lines = [
            "【闪达超市 · 订单已完成】",
            f"{group.name}  ·  {group.order_time}",
            "商品明细 / 件数 / 价格",
        ]
        for p in products:
            lines.append(f"{p.raw}\n规格 {p.spec or '-'}\t{p.count} 件　¥{_cost(p)}")
        lines.extend(f"{n.title}　{'-' if (n.amount or 0) < 0 else ''}¥{abs(n.amount or 0):.2f}" for n in sample.noise)
        lines.append(f"订单号 {sample.extra['order_no']}")
    elif style == "text-mall":
        lines = [
            "好集商城 > 我的订单 > 订单详情",
            f"商家：{group.name}",
            "[商品列表]",
        ]
        for p in products:
            lines.append(f"{p.raw}\t规格：{p.spec or '默认'}\n单价 ¥{_cost(p)}　　数量 ×{p.count}")
        lines.extend(f"{n.title}　{'-' if (n.amount or 0) < 0 else ''}¥{abs(n.amount or 0):.2f}" for n in sample.noise)
        lines.extend([f"下单时间：{group.order_time}", f"订单编号：{sample.extra['order_no']}", "状态：待收货"])
    elif style == "text-share":
        lines = ["青禾鲜市 · 分享购物清单", f"{group.name}　已选商品"]
        for idx, p in enumerate(products, 1):
            lines.append(f"{idx}. {p.raw}（{p.spec or '默认规格'}） x{p.count}  ¥{_cost(p)}")
        lines.extend(f"{n.title}：¥{n.amount:.2f}" for n in sample.noise if n.amount is not None)
        lines.append("来自青禾鲜市的商品清单")
    else:
        raise ValueError(f"unknown text style: {style}")
    return "\n".join(lines) + "\n"

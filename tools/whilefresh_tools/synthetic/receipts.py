"""Thermal-receipt HTML: fictional stores, product rows, amounts and timestamps."""

from __future__ import annotations

from .html_common import document, esc, money
from .model import Product, Sample

CSS = """
.screen{width:320px;background:#fff;color:#252525;font-family:"Courier New","PingFang SC",monospace;
  padding:18px 15px 20px;line-height:1.5;font-size:12px}
.head{text-align:center;font-weight:bold;font-size:17px;letter-spacing:1px;margin:0 0 8px}
.sub{text-align:center;font-size:10px;margin-bottom:10px}
.rule{border-top:1px dashed #444;height:1px;margin:7px 0}
.bar{height:28px;background:repeating-linear-gradient(90deg,#222 0,#222 2px,#fff 2px,#fff 5px,#222 5px,#222 6px,#fff 6px,#fff 9px);margin:14px 33px 4px}
.row{display:flex;justify-content:space-between;gap:4px;white-space:nowrap}
.row .left{overflow:hidden;text-overflow:ellipsis;max-width:245px}
.row .right{text-align:right;flex:none}
.item{padding:5px 0 4px;border-bottom:1px dotted #aaa}
.item .name{font-size:12px;font-weight:bold;line-height:1.25;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.item .row{font-size:10.5px;margin-top:2px;color:#444}
.total{font-size:15px;font-weight:bold;margin:6px 0 2px}
.footer{text-align:center;font-size:10.5px;margin-top:12px}
"""


def _line_total(p: Product) -> float:
    if p.spec and p.spec.endswith("kg") and p.spec[:-2].replace(".", "", 1).isdigit():
        return p.price * float(p.spec[:-2]) * p.count
    return p.price * p.count


def render_receipt(sample: Sample) -> tuple[str, int]:
    group = sample.groups[0]
    parts = [
        f'<div class="head">{esc(group.name)}</div>',
        '<div class="sub">欢迎光临 · 电子消费凭证 · 请保留小票</div>',
        f'<div>交易时间: {esc(group.order_time)}</div>',
        '<div>收银台: 03　　　　方式: 支付</div>',
        f'<div>流水号: XH{sample.seed}87</div>',
        '<div class="rule"></div>',
        '<div class="row"><span class="left">商品</span><span class="right">金额(元)</span></div>',
        '<div class="rule"></div>',
    ]
    for idx, p in enumerate(group.products, start=1):
        spec = esc(p.spec or "-")
        unit = "kg" if p.spec and p.spec.endswith("kg") and p.spec[:-2].replace(".", "", 1).isdigit() else "件"
        qty = f"{spec} ×{p.count}" if unit == "件" else f"{spec} ×{p.count}"
        parts.append(
            f'<div class="item"><div class="name">{idx:02d} {esc(p.raw)}</div>'
            f'<div class="row"><span class="left">{qty}　单价 {money(p.price)}/{unit}</span>'
            f'<span class="right">{money(_line_total(p))}</span></div></div>'
        )
    total = sum(_line_total(p) for p in group.products)
    for n in sample.noise:
        if n.amount is not None:
            parts.append(f'<div class="row"><span>{esc(n.title)}</span><span>{money(n.amount)}</span></div>')
            total += n.amount
    parts.extend(
        [
            f'<div class="total row"><span>实收合计</span><span>¥{money(total)}</span></div>',
            f'<div class="row"><span>商品数量</span><span>{sum(p.count for p in group.products)}件</span></div>',
            '<div class="bar"></div>',
            '<div class="footer">谢谢惠顾　购物愉快<br>小票仅作购物凭证 · 请核对商品</div>',
        ]
    )
    return document("\n".join(parts), CSS, 320, 0, sample.id), 320

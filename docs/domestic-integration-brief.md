# 菜谱与买菜接入能力速查

> AI 辅助独立核查（GPT-6 Luna 起草），主会话对照官方资料修正接口链接与状态。2026-10-20。供 Codex/Claude 快速读取；详细证据见[完整报告](domestic-recipe-grocery-integration-research.md)。独立核查时未以完整报告为输入。

## 状态定义

- **匿名已调用**：本轮无需账号或 key，请求返回业务数据。
- **需凭证**：官方有请求文档，但必须申请 key、应用或用户授权；本轮没有成功获取业务数据。
- **仅社区声明**：第三方项目宣称可用，本轮未端到端验证，也无平台授权。
- **公开未提供**：本轮核查的官方公开资料没有对应消费者接口；不推断内部商务接口不存在。

## 可调用接口

| 用途 | 请求或工具 | 当前状态 | 对产品能提供什么 |
| --- | --- | --- | --- |
| 菜谱索引 | `GET https://proj.kitchen/api/recipes` | **匿名已调用**；本轮 HTTP 200、341 条，含 `id/name/category/difficulty` | 可检索候选菜谱；[官网](https://proj.kitchen/)与[返回示例](https://proj.kitchen/api/recipes) |
| 菜谱详情 | `GET https://proj.kitchen/api/recipes/{id}`，本轮以 [`hongshaorou`](https://proj.kitchen/api/recipes/hongshaorou) 实测 | **匿名已调用**；含 `ingredients/steps/tools/tips` | 可读取做法；详情无逐条 `source/author/license`，生产再展示许可未确认 |
| 用量单位 | `GET https://proj.kitchen/api/units` | **匿名已调用**；本轮 HTTP 200、23 条 | 单位名称；[接口](https://proj.kitchen/api/units) |
| 淘宝买家订单 | TOP 网关 `https://eco.taobao.com/router/rest`，`method=taobao.trades.bought.get` | **需凭证，权限未验证**；网关可达，未传 `app_key` 返回错误 28；须 TOP 应用、签名、买家授权，文档称近三个月交易 | 可能取得本人交易及子订单；[正确的官方买家 API 文档](https://open.taobao.com/api.htm?docId=45&docType=2)。新应用能否申请、买菜订单是否覆盖均未验证 |
| 附近门店 | 高德 `GET https://restapi.amap.com/v5/place/around`，传 `key/location` 及搜索条件；文本搜索 `GET https://restapi.amap.com/v5/place/text`，传 `key/keywords` 或类别 | **需 Web 服务 key**，本轮无 key，未取业务数据；[地点搜索 2.0 文档](https://lbs.amap.com/api/webservice/guide/api-advanced/newpoisearch) | 可找地点与距离，不提供该店商品或库存 |
| 盒马商品/门店 | 社区 [hema-snack-mcp](https://github.com/TomEageer/hema-snack-mcp)：`hema_product(item_id)`、`hema_stores(city,district)`、`hema_categories(shop_id)`、`hema_shop_info(shop_id)`、`hema_nearby_poi(geo)` | **仅社区声明**；基于微信小程序逆向，非官方接口 | 已知商品 ID 的资料和门店信息；作者明确称匿名取不到某店商品列表、库存、加购或下单 |

淘宝文档给出 TOP 请求参数和买家授权要求。**网关可达 ≠ 本项目有权调用**；尚无获批应用、用户授权样本、订单返回样本。高德须先[申请 key](https://lbs.amap.com/api/webservice/create-project-and-key)。厨房计划 API 可取数据，但[项目代码 MIT](https://github.com/chenranj/proj.kitchen/blob/main/LICENSE)不自动证明全部菜谱文字和图片可商用；接口未给逐条来源，也未见稳定性承诺。

## 其他数据路径

- [HowToCook](https://github.com/Anduin2017/HowToCook) 是可下载的中文 Markdown 菜谱库，根[许可证](https://github.com/Anduin2017/HowToCook/blob/master/LICENSE)为 Unlicense；它不是托管菜谱 API。使用前核对被引用的外部素材和图片来源。
- 盒马、叮咚、淘宝、京东、拼多多的官方隐私政策写有消费者“个人信息下载”入口，但下载文件是否含逐项买菜订单尚未测样本；该入口是用户手动导出，不是第三方 API。具体路径见[完整报告](domestic-recipe-grocery-integration-research.md)。
- 已查公开资料未提供盒马、叮咚、美团闪购/小象、京东到家、朴朴、永辉生活、多多买菜可直接申请的**消费者本人订单 API**；淘宝买家 API 是例外。来源入口：[叮咚](https://open.ddxq.mobi/docs)、[美团闪购](https://developer-lingshou.waimai.meituan.com/docCenter/2)、[京东](https://jos.jd.com/apilist)、[永辉](https://open.yonghui.cn/static/page1.html)、[拼多多](https://open.pinduoduo.com/application/document/api?id=pdd.order.list.get)。商家订单接口不能代替本人已购数据。
- 已查公开资料没有获准使用的购物车**整单导出或批量导入 API**。平台层商品分享、淘宝政策提及的购物车分享链接，都还需客户端验证实际内容与外部打开效果。
- 未发现上述买菜平台官方发布的消费者清单 MCP/skill。社区工具只是接口封装，代码许可不授予零售平台数据访问权。

## 首版判断

菜谱可先核查 HowToCook 文本许可并建立本地索引；厨房计划 API 适合原型试读，生产使用前补逐条来源。已购食材先从用户主动导入的截图/清单提取并允许修正；淘宝订单 API 单独验证权限与覆盖。附近门店可先用 POI 查询，**没有门店级商品和库存数据时不能显示“有货”或“可买齐”**。

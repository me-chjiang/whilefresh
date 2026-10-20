# 趁鲜 视觉稿

两版 HTML 视觉稿（Android 手机视口 412×915 CSS px，截图为 2 倍像素），供选定方向后再整理 `Theme.qml`。示例数据均为虚构；菜谱名称仅作示意，不代表 HowToCook 的实际收录。

| 版本 | 目录 | 一句话 |
| --- | --- | --- |
| A「冰箱贴」 | `a-fridge-magnet/` | 明亮活泼；冰箱隔层 + 彩色磁贴；清冰箱是一枚橙色磁贴 |
| B「厨房手账」 | `b-kitchen-journal/` | 温暖手作；纸张、胶带、手账标签；清冰箱是一枚番茄红印章贴纸 |

每个目录包含 `fridge.html`、`clear-fridge.html`、`import-confirm.html`、`style.css` 和对应的三张 PNG 截图（`fridge.png`、`clear-fridge.png`、`import-confirm.png`）。入库确认页用 iframe 嵌入同目录的 `fridge.html` 作为被遮罩的背景。

## 统一刻度（`shared/base.css`）

两版共用一套 CSS 变量，样式表里不再写散落的圆角和阴影数值。

| 变量 | 值 | 用于 |
| --- | --- | --- |
| `--r-card` | 8px | 分区、面板、卡片、入库弹层上缘 |
| `--r-row` | 6px | 物资行、列表项、菜谱卡片、选料项、按钮、虚线框 |
| `--r-tag` | 4px | 标签、标记、数量 / 位置输入框、角标、胶带 |
| `--sh-1` | `0 1px 3px` 暖褐 14% | 行、卡片、选料项 |
| `--sh-2` | `0 4px 10px` 暖褐 20% | 仅清冰箱按钮 |

圆形只留给清冰箱按钮和品类色点；不使用全圆胶囊。左滑操作区放进行容器内，由容器的圆角裁切外侧，内侧接缝为直线。

## A「冰箱贴」设计要点

- **配色**：纸色 `#FFF8EC`、墨色 `#33291F`、辅助灰褐 `#85766A`；主操作橙 `#FF7A2F`，选中 / 先用青绿 `#14968F`，角标黄 `#FFC93C`。隔层内壁用淡色：冷藏 `#E4F3F1`、冷冻 `#E1ECF8`、常温 `#FBEFD2`。品类色 `#5BAE4A / #E4452F / #3E9BD8 / #F2B826 / #C99A5B` 只出现在 16px 色点上。
- **字号与圆角**：标题 25px / 800，分区标题 16px，物资名 17px / 700，数量 15px，辅助 12px，标记 12px；圆角按上面的统一刻度。行高 50px，行间距 4px；点击区靠伪元素外扩到约 48dp。
- **阴影与质感**：只用 `--sh-1`；分区底部一条 5px 的浅蓝灰纯色“隔板”；底色叠 `feTurbulence` 噪点（9% 透明度）；色点是平涂圆点，不加高光。
- **清冰箱按钮**：80px 橙色圆形磁贴，5px 纸色描边“抠”出底栏，`--sh-2` 阴影，右上黄色矩形角标（临期数量）；全页唯一的大面积饱和色块。进入清冰箱页时外圈多一层浅橙光环。
- **其他**：先用标记是青绿胶囊；新鲜度为淡色胶囊（新鲜绿、临期橙、超出估计期限红）；左滑露出青绿“用掉”和灰褐“家里没有”两块方形操作。

## B「厨房手账」设计要点

- **配色**：纸色 `#F6EBD5`（叠 20px 点阵 + 噪点）、卡片纸 `#FFFBF0`、墨色 `#3B2D26`；印章番茄红 `#D5432B`，确认绿 `#3E7F69`，先用芥黄 `#EDBE4C`。胶带三色：鼠尾草绿 `#A8D0BF`、淡蓝 `#A9C9E2`、沙黄 `#F2CE74`。品类色 `#5E9F4B / #D8452F / #3F94CF / #EFB423 / #BF935A` 只用于带白边的圆形贴纸色点。
- **字号与圆角**：标题用衬线（`Noto Serif SC` / `Songti SC`）26px / 900，标题下有一条荧光笔式的沙黄底；物资名 17px 无衬线 / 700；纸片也按统一刻度（最小 4px），不再用直角。行高 54px，行间用 1.5px 虚线。
- **阴影与质感**：只用 `--sh-1`；分区标题是斜贴 -1.5° 的和纸胶带（斜纹，4px 圆角）；新鲜度标记是描边印章，“超出估计期限”为双线框；先用是芥黄矩形标签；入库面板顶部贴一条胶带。
- **清冰箱按钮**：84px 番茄红圆形贴纸，白色“模切”边，内圈虚线、整体旋转 -8°，衬线字体“清冰箱”，`--sh-2` 阴影，右上芥黄矩形角标；像盖在底栏上的一枚印章。
- **其他**：清冰箱页的选料区是淡黄便签，选中项为带硬投影的描边标签、红色圆形勾选；菜谱行是带番茄红上边线的索引卡片；确认入库按钮内缩一圈虚线，像缝线。

## 本地查看

直接用浏览器打开任一 `fridge.html` 等文件即可，无需服务器，不依赖在线字体或 CDN。浏览器窗口建议设为 412×915，或用开发者工具的设备模拟。

重新截图（macOS，Chrome 无头模式）：

```sh
"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" --headless=new --disable-gpu \
  --hide-scrollbars --force-device-scale-factor=2 --window-size=412,915 \
  --virtual-time-budget=2000 --screenshot=<输出.png> file://<html 的绝对路径>
```

## 图标

- `icons/` 内为用到的 Tabler Icons（outline 线条风格，MIT）SVG，许可文件为 `icons/LICENSE-tabler`。图标统一 24px 画布、2px 描边，尺寸在 CSS 中缩放。
- `shared/build-icons.py` 把 `icons/*.svg` 打包成 `shared/icons.js`，页面里用 `<i class="ic" data-ic="名称"></i>` 引用并内联为 SVG（颜色随 `currentColor`）。新增图标：放入 `icons/`，运行 `python3 -I design/shared/build-icons.py`。
- 未使用任何 emoji。星级用 Tabler `star` 加 `fill: currentColor` 表示实心。

## 转 QML 时的实现提示

便宜（Rectangle / Image / 基础属性即可）：

- 三档圆角对应 `Theme.qml` 里的 `radiusCard / radiusRow / radiusTag`；纯色圆角矩形、描边、虚线框（用预渲染的 9-patch 图或 `Shape` + `ShapePath` 的 `strokeStyle: DashLine`）；色点、胶囊标记、选中标签。
- 纸张底纹：做一张 160×160 的无缝噪点 PNG（几 KB），用 `Image` 平铺（`fillMode: Image.Tile`）。不要在运行时用滤镜生成。
- 图标：Tabler SVG 转成 `Image { source: ... }`，或预处理成单色 PNG 后用 `MultiEffect` 着色；列表里数量很多时优先预渲染。
- 色点高光：色点上叠一个小的半透明椭圆 `Rectangle`。
- 两版共有的左滑操作：`SwipeDelegate` 或自定义拖动，开销与普通列表相同。

昂贵（谨慎使用，必要时用预渲染图替代）：

- 柔和阴影（现在只剩 `--sh-1` 一档给行和卡片，仍是主要开销）：每行一个 `MultiEffect`（`shadowEnabled`）在长列表里会明显拖慢滚动。建议：只给分区卡片、底栏、清冰箱按钮、弹层用实时阴影；物资行用预渲染的 9-patch 阴影图或 1px 偏移的深色边代替。
- 半透明大面积叠层（弹层遮罩加模糊）：只做纯色遮罩，不做背景模糊。
- B 版胶带的斜纹：用小块平铺图片（`Image` 平铺后旋转）。B 版只旋转胶带和印章按钮，不旋转正文，避免文字的亚像素抗锯齿问题。
- B 版的衬线字体：Android 设备不保证带 Noto Serif CJK，需要随 App 打包子集字体（体积以 MB 计），或把衬线仅用于少量标题改用图片。A 版只用系统无衬线，无此成本。
- 字体：`Noto Sans SC` 在 Android 上通常可用；桌面开发版回退到 PingFang SC。

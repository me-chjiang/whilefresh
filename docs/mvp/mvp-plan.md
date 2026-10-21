# 趁鲜 MVP 工程规划

> loop-dev-cycle Phase 1 产出（即 skill 的 `PLAN.md`），2026-10-20。依据 [MVP 需求确认稿](mvp-requirements.md) 与 [PRD v5](../prd-v5.md)。状态：已执行（Phase 1 于 2026-10-20 通过；T0–T11 于 2026-10-21 合并；真机验收未做）。

## 1. 架构概述

```
┌──────────────────────── Android APK（Qt 6.8.2）────────────────────────┐
│  QML 界面（qml/）                                                       │
│  冰箱 · 菜谱 · 清冰箱 · 待买 · 我的 · 入库流程 · Theme                 │
│        │  属性绑定 / 信号                                               │
│  视图模型（src/ui/，C++ QObject，暴露给 QML）                           │
│        │                                                                │
│  ┌─────┴──────────── 核心领域（src/core/，只依赖 QtCore）────────────┐  │
│  │ 食材与日期模型 · 新鲜度计算（可注入时钟） · 保质期来源链          │  │
│  │ 常备规则 · 本地记忆规则 · 菜谱匹配 · 标准食材词表                  │  │
│  └─────┬────────────────────────────┬─────────────────────────────────┘  │
│  数据层（src/data/，QtSql+SQLite）   识别层（src/recognition/）           │
│  物资、导入记录、规则、待买          RecognitionService 接口              │
│  内置只读数据：菜谱/词表/参考表      ├ DeepSeekRecognizer（HTTPS）        │
│                                      ├ ReplayRecognizer（录制回放，测试） │
│                                      └ DemoRecognizer（演示模式）         │
│  平台层（src/platform/ + android/ Java）                                  │
│  接收系统分享 · 调用相机 · 读取 content:// · 分享 / 剪贴板                │
└──────────────────────────────────────────────────────────────────────────┘
          ▲ 构建时生成内置数据                         │ HTTPS
┌─────────┴────────── tools/（Python，uv）──────┐      ▼
│ HowToCook → recipes.json                      │   DeepSeek API
│ 用料抽取 → vocabulary.json（标准名+别名）     │
│ 参考数据 → shelf_life.json                    │
│ 合成截图/小票/文字 + 标准答案                 │
│ 真实识别回归（手动运行，计费）                │
└───────────────────────────────────────────────┘
```

**入库数据流**：选图 / 分享 / 拍照 / 粘贴 → 平台层把图片复制到 App 私有目录 → 识别层按 1500px 高、200px 重叠切片，连同词表与提示词发给 DeepSeek → 解析 JSON，合并切片与多图并去重 → 保质期来源链补全日期和存放位置 → 用户在确认页纠错 → 写入数据库。

**新鲜度**：不存状态，每次按"当前时钟"计算。App 启动、回到前台和跨日时刷新视图。

### 目录规划

| 路径 | 内容 |
| --- | --- |
| `app/` | Qt CMake 工程：`src/{core,data,recognition,platform,ui}`、`qml/`、`android/`、`assets/`、`tests/` |
| `tools/` | Python 离线工具（uv 管理）与 pytest |
| `design/` | HTML 视觉稿与截图 |
| `test-data/` | 合成样本、标准答案、DeepSeek 录制回放 |
| `scripts/` | 构建、安装、取密钥等脚本 |

Phase 2 开始前用 build-project-fhs skill 把目录规则写成 `FHS.md`。

## 2. 技术选型决策（ADR）

### ADR-1 界面：Qt Quick/QML + C++ 视图模型
- **决定**：界面用 QML，业务逻辑放 C++ 核心，通过视图模型连接。
- **理由**：用户指定 Qt。QML 适合触控、动效和拟物风格；C++ 核心可在 macOS 上快速单元测试，日后移植其他平台时直接复用。
- **替代**：Qt Widgets，触控和移动端观感差，不选。

### ADR-2 本地存储：SQLite（QtSql）
- **决定**：用户数据存 SQLite，带版本迁移；菜谱、词表、参考表作为只读 JSON 资源内置，启动时载入内存。
- **理由**：Android 套件已带 SQLite 驱动；按日期筛选、关联导入记录用 SQL 更直接。只读数据约数百条，放内存足够。
- **替代**：全部用 JSON 文件，并发写入和迁移不便。

### ADR-3 Android 上的 HTTPS（**需用户决定**）
- **问题**：本机 Qt 6.8.2 Android 套件的 TLS 后端依赖 OpenSSL，但没有附带 OpenSSL 库，App 在手机上无法发起 HTTPS 请求。
- **推荐**：打包 KDAB 维护的 [android_openssl](https://github.com/KDAB/android_openssl) 预编译库。这是 Qt 官方文档推荐的做法，接入方式是 CMake 中引入一行 `android_openssl.cmake`，桌面版和 Android 共用 `QNetworkAccessManager`。
- **替代**：通过 JNI 调用 Android 自带的 `HttpsURLConnection`。不引入新依赖，但需要维护两套网络代码（桌面一套、Android 一套）。
- 按全局规则，新增第三方依赖需你确认。

### ADR-4 识别：App 直连 DeepSeek，切片 + 固定词表
- **决定**：`deepseek-flash`，图片切片发送，`response_format: json_object`；提示词附标准食材词表；超时 60 秒，可重试。（后续决定改为空闲 150 秒、整体 240 秒，超时不重试。）识别层是接口，实现有 DeepSeek、录制回放、演示三种。
- **理由**：实测切片转写更准确；词表解决标准名不一致；回放让日常测试不计费。
- **替代**：整张发送，便宜约一半但有错字；自建后端，用户已决定不部署。

### ADR-5 密钥注入
- **决定**：`scripts/build-android.sh` 从钥匙串 `landing-project-deepseek` 读取密钥，经 CMake 生成到构建目录的头文件中，不写入源码目录。
- **说明**：APK 中的密钥一定能被提取，任何混淆都只能增加难度；真正的保护是平台额度上限和试用后作废。

### ADR-6 标准食材词表为匹配核心
- **决定**：从 HowToCook 372 份菜谱（373 个 Markdown 文件，排除 1 份模板）抽取用料，离线用 DeepSeek 文本模型归一出标准名、别名、品类和是否常备，人工抽查后固定为 `vocabulary.json`。菜谱匹配、识别提示词、参考表、本地记忆都以标准名为键。
- **理由**：识别结果和菜谱只有通过同一套名称才能对上。
- **替代**：运行时模糊匹配，结果不稳定，也不可解释。

### ADR-7 保质期来源链与新鲜度规则
- **决定**：识别结果 > 本地记忆 > 参考表 > AI 判断；每项记录来源。起算日：生产日期 > 购买日期 > 入库日期。新鲜度初版规则：剩余天数 ≤ 0 为"超出估计期限"；剩余天数 ≤ max(1, 总天数 × 25%) 为"临期"；其余为"新鲜"。阈值集中在一处，试用后再调。
- **参考表数据源**：[USDA FoodKeeper](https://catalog.data.gov/dataset/fsis-foodkeeper-data)（CC0 公有领域，可随 APK 再分发）。2026-10-20 经 Lexmount Web Fetch API 取得 v128 数据：661 种食品，按常温 / 冷藏 / 冷冻给出最短 / 最长天数及单位，并区分"从购买日起"（`DOP_*`）与开封后。取"从购买日起"的**最短值**作参考，偏保守。（后续决定：最短值 ≤ 2 天且有上限值时改取上限值。）建立"标准食材名 → FoodKeeper 条目 ID"映射表（离线 AI 辅助，人工抽查），保留条目 ID 与来源说明；App 中标注"参考：USDA FoodKeeper"。豆制品、腊肉、速冻饺子等中国常见食材若无对应条目，不硬套，回落到 AI 判断并缓存。
- **替代**：中文官方来源未找到可再分发的表；转载类表格没有许可说明，不采用。

### ADR-8 Android 原生能力：Java 桥接
- **决定**：自定义 Activity 继承 `QtActivity`，处理 `ACTION_SEND` / `ACTION_SEND_MULTIPLE`（冷启动与 `onNewIntent`）；相机用系统 `ACTION_IMAGE_CAPTURE` + `FileProvider`；`content://` 内容读出后复制到私有目录，再通过 JNI 通知 C++。分享出去和剪贴板用 Qt 自带能力或 `Intent.ACTION_SEND`。
- **理由**：不需要 Qt Multimedia（本机未安装）；系统相机体验更稳定。

### ADR-9 开发与测试平台
- **决定**：同一工程同时构建 macOS 桌面版和 Android 版。桌面版用于日常开发、单元测试和界面调试；Android 版用于验收。平台层在桌面上用文件对话框替代分享和相机。
- **理由**：桌面构建快，测试不依赖模拟器。

### ADR-10 视觉实现
- **决定**：先在 `design/` 用 HTML 做两版视觉稿，用 Playwright 按手机尺寸截图，由你选定。再把配色、字号、圆角、阴影整理成 `Theme.qml`。图标用 [Tabler Icons](https://github.com/tabler/tabler-icons)（MIT 许可）的线条 SVG；拟物质感用轻纸纹理和 `MultiEffect` 阴影。
- **替代**：Lucide（ISC 许可），风格相近，作为备选。

## 3. 任务拆解

每个任务一个分支；完成后经主会话审查、Sonnet 代码审查和测试，摘要交你确认后合并。复杂度：低 / 中 / 高。

| # | 分支 | 内容 | 依赖 | 复杂度 | 执行者 |
| --- | --- | --- | --- | --- | --- |
| T0 | `feat/app-skeleton` | Qt 工程骨架；macOS 与 Android（x86_64 模拟器、arm64）都能构建运行；HTTPS 冒烟测试；构建脚本与密钥注入；README 写运行与验证命令 | ADR-3 决定 | 高 | Opus 子代理 |
| T1 | `feat/recipe-data` | Python：HowToCook → `recipes.json`（原用料、步骤、来源链接、难度），区分原料与工具；pytest | — | 中 | Sonnet |
| T2 | `feat/vocabulary` | 用料抽取与归一，产出 `vocabulary.json`（标准名、别名、品类、默认常备）；抽查报告 | T1 | 中 | Sonnet，Haiku 抽查初稿 |
| T3 | `feat/shelf-life-table` | 存档 FoodKeeper 数据快照及来源说明；标准名 → FoodKeeper 映射；生成 `shelf_life.json`（标准名 × 存放方式 × 天数 × 来源 ID）；pytest | T2 | 中 | Sonnet，Haiku 抽查映射 |
| T4 | `feat/visual-drafts` | 两版 HTML 视觉稿（冰箱、清冰箱、入库确认），出手机尺寸截图 | — | 中 | Sonnet；**你选定** |
| T5 | `feat/core-domain` | C++ 核心与数据层：模型、SQLite 迁移、新鲜度、来源链、本地记忆规则、常备规则、菜谱匹配、待买、导入记录与重复导入检测（图片哈希 + 商品集合）、按导入删除；Qt Test | T0，T2/T3 的数据格式 | 高 | Sonnet |
| T6 | `feat/recognition` | 切片、提示词、DeepSeek 客户端、解析、跨切片/多图去重、回放与演示实现；粘贴文字解析；Qt Test（回放） | T0、T2 | 高 | Sonnet |
| T7 | `feat/synthetic-data` | 合成购物车/订单/小票/粘贴文字（4 类平台风格，无 logo）+ 标准答案；小票拍照变体；真实识别回归脚本 | T2 | 中 | Sonnet，Haiku 生成商品文本 |
| T8 | `feat/ui-pages` | Theme 与全部页面、入库流程、各种空状态与错误状态 | T4 选定、T5 | 高 | Sonnet |
| T9 | `feat/android-integration` | 接收分享、相机、`content://`、分享出去与剪贴板；冷/热启动；读取图片拍摄 / 修改日期，作为"今天生产"等相对日期的基准 | T0 | 高 | Opus 子代理 |
| T10 | `feat/demo-mode` | 演示模式：内置合成样本和对应识别结果，断网可走通 | T6、T7、T8 | 低 | Sonnet |
| T11 | `release/trial-build` | 签名 APK（签名密钥存钥匙串，不进仓库）；模拟器与真机端到端验收；LGPL 告知；交付说明 | 全部 | 中 | 主会话 |

## 4. 迭代策略

每个里程碑结束时向你汇报，并请你确认是否进入下一个。

| 里程碑 | 任务 | 交付 | 并行 |
| --- | --- | --- | --- |
| M1 基础与数据 | T0、T1、T4 → T2 → T3、T7 | 可运行的空壳 App（含 HTTPS）；菜谱、词表、参考表数据；视觉稿供你选择 | 首轮 T0、T1、T4 三个并行 |
| M2 核心逻辑 | T5、T6 | 核心与识别通过单元测试；用合成样本和 `private/bill/` 真实样本跑一次识别回归（计费，小批量） | 2 个并行 |
| M3 界面与平台 | T8、T9、T10 | 模拟器上走通完整路径，含演示模式 | 3 个并行 |
| M4 试用包 | T11 | 可发给试用者的 APK 和验收报告 | 主会话 |

同时最多 3 个子代理并行；同一时刻并行的任务不修改相同文件。

## 5. 风险登记

| # | 风险 | 影响 | 缓解 |
| --- | --- | --- | --- |
| R1 | Android 套件缺 OpenSSL，HTTPS 不可用 | 识别无法工作 | ADR-3；T0 首先在模拟器上做 HTTPS 冒烟测试 |
| R2 | Qt 6.8 是否正式支持 macOS 12 Intel 作为 Android 构建宿主，文档未确认 | 构建失败 | T0 最先验证；失败时评估升级系统或换构建机 |
| R3 | Intel Mac 上 x86_64 模拟器的硬件加速是否可用 | 模拟器很慢或无法启动 | T0 验证；不行则以真机为主 |
| R4 | DeepSeek 视觉为实验性，模型可能变更 | 识别失效 | 识别层可替换；回放测试锁定解析逻辑；备选 Qwen-VL |
| R5 | 词表随提示词发送，增加每次的 token 消耗 | 费用上升 | 词表按品类精简；观察缓存命中；M2 回归时记录用量 |
| R6 | FoodKeeper 是美国口径、英文名，中国常见食材覆盖不全；官网对本机网络返回 403 | 部分食材没有参考值；数据更新不便 | 存档快照并记录版本；中文映射人工抽查；未覆盖的回落到 AI 判断并缓存；更新时经 Lexmount API 获取 |
| R7 | 只有美团小象一个平台的真实样例 | 其他平台识别效果未知 | 合成样本先覆盖版式；请你继续补充 `private/bill/` |
| R8 | 密钥随 APK 下发可被提取 | 额度被盗用 | 平台设额度上限，只发给熟人，试用后作废 |
| R9 | HowToCook 用料写法不统一（含可选项、工具） | 匹配不准 | T1 区分原料与工具、标记可选；T2 抽查 |
| R10 | QML 拟物效果的实现与性能成本 | 进度和流畅度受影响 | 视觉稿阶段控制效果数量；低端机关闭阴影 |
| R11 | Qt LGPL 分发义务 | 合规 | 动态链接；T11 在"我的"中加入许可告知并核对义务（非法律意见） |

**技术债（已知、接受）**：识别和菜谱都只有单一来源；试用阶段没有后端，无法统计或远程关闭密钥；新鲜度阈值靠经验，需试用数据校准。

## 6. 验证方式

- **C++**：Qt Test，在 macOS 桌面上运行，覆盖新鲜度、来源链、规则、匹配、解析（用录制回放）、数据库迁移。
- **Python**：pytest，覆盖菜谱解析、词表、参考表、合成数据。
- **识别回归**：手动运行，计费，小批量。对比合成样本的标准答案，并记录真实样本的修正项数、耗时和 token 用量。
- **Android**：模拟器与真机上用 adb 安装、启动、截图，按需求稿的验收标准逐条走查。
- **安全**：每次提交前扫描仓库中是否有密钥。

具体命令在 T0 完成后写入 README。

## 7. 需要你确认的事项

1. 本规划是否通过（loop Gate）。
2. ADR-3：是否同意引入 KDAB `android_openssl` 预编译库。
3. ADR-10：图标用 Tabler Icons（MIT）是否可以。
4. 保质期参考表以 USDA FoodKeeper 为底（ADR-7），中国常见食材缺口由 AI 判断补充，是否可以。

以上均已确认，见 cycle-state 决策日志。

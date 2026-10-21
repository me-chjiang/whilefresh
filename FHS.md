# File Hierarchy Standard (FHS)

本项目目录结构与文件放置规则的权威说明。新增顶层目录、新增一类文件，或某类文件改放别处时，同步更新本文件。依据：[MVP 工程规划](docs/mvp/mvp-plan.md) 第 1 节。

## Layout

```
.
├── AGENTS.md / CLAUDE.md      # 协作规则（CLAUDE.md 只引入 AGENTS.md）
├── FHS.md                     # 本文件
├── README.md                  # 简介、材料索引、运行与验证命令
├── LICENSE                    # BSD-3-Clause
├── .agents/skills/            # 项目级 skill 实体；.claude/skills/ 为同名符号链接
├── docs/                      # 产品与研究文档（PRD、调研）
│   ├── archive/               # 历史版本（PRD v3、v4，调研速查）
│   ├── images/                # README 截图（仅演示模式数据）
│   ├── development.md         # 开发文档：环境、构建与验证命令、数据工具、调试方法
│   └── mvp/                   # MVP 循环文档：需求、规划、cycle-state、交付报告
│       ├── notes/             # 复杂模块的实现说明（如 android-integration.md）
│       └── todo.md            # 未完成事项
├── app/                       # Qt 6 CMake 工程（Android APK + macOS 桌面开发版）
│   ├── CMakeLists.txt
│   ├── src/
│   │   ├── core/              # 领域逻辑，只依赖 QtCore：食材、日期、新鲜度、来源链、规则、匹配
│   │   ├── data/              # SQLite 存储、迁移、内置只读数据加载
│   │   ├── recognition/       # 识别接口与实现（DeepSeek、回放、演示）、切片、解析、去重
│   │   ├── platform/          # 平台适配的 C++ 侧（分享、相机、文件、剪贴板）、导入载荷与日期来源
│   │   │   ├── android/       # JNI 胶水（只在 Android 构建中编译）
│   │   │   └── desktop/       # 桌面替代实现（文件对话框、剪贴板）
│   │   └── ui/                # 应用服务（ImportService / KitchenService，不依赖 QML）及暴露给 QML 的视图模型
│   ├── qml/                   # QML 页面与组件；Theme 单例
│   ├── android/               # AndroidManifest、Java 桥接（src/io/github/mechjiang/whilefresh/）、res（xml/ 为 FileProvider 路径）
│   ├── tools/                 # 仅桌面可用的 C++ 开发工具（如 recognize-cli 真实识别与录制），不进 ctest、不进 APK
│   ├── assets/
│   │   ├── data/              # 内置只读数据（由 tools/ 生成并提交）：菜谱、词表、参考表
│   │   ├── icons/             # Tabler 线条 SVG（附许可文件）
│   │   ├── fonts/             # Noto Serif SC Black 子集与 OFL 许可
│   │   ├── textures/          # 离线生成的可平铺纸纹与胶带纹理
│   │   └── demo/              # 演示模式的合成截图与对应识别结果（<输入类型>.json，录制格式，嵌入 qrc）
│   └── tests/                 # Qt Test 单元 / 集成测试，按被测层分子目录（core/、platform/ …）
├── tools/                     # Python 离线工具（uv 管理，pytest）
│   ├── pyproject.toml
│   ├── whilefresh_tools/      # 包源码：菜谱转换、词表、参考表、合成数据、识别回归
│   │   ├── synthetic/         # 确定性商品场景、四种购物界面 / 小票 / 文本生成、Chrome 渲染、录制评估
│   │   └── vocabulary/        # 词表：源数据（vocabulary-source.json、ingredient-map.json，人工维护）与构建 / 校验 / AI 初稿代码
│   ├── sources/               # 需要存档的第三方原始数据快照及来源说明（如 FoodKeeper）
│   └── tests/                 # pytest；fixtures/ 放少量第三方原文样例及合成手工录制夹具
├── design/                    # HTML 视觉稿与手机尺寸截图
├── test-data/                 # 合成样本（PNG / JPEG / 文本）、标准答案 JSON、合成样本的 DeepSeek 录制回放
│   ├── synthetic/             # 同名源输入 + 标准答案；screenshots/ 含可重复生成的 HTML/PNG，receipts/ 含 HTML/JPEG，texts/ 含 TXT
│   ├── recordings/            # 识别录制（请求摘要 + 原始响应），仅限合成数据；synthetic/ 中按样本 ID 同名存储
│   └── vocabulary/            # 测试用合成词表（正式词表见 app/assets/data/）
├── scripts/                   # 构建、安装、取密钥、模拟器启动等 shell 脚本
├── build/                     # 不进 Git：构建产物，按目标分子目录（desktop、android-x86_64、android-arm64-v8a）
└── private/                   # 不进 Git：个人资料、真实测试样例（bill/）、真实样例的识别录制（recordings/）
```

## Placement rules

- **业务规则**（新鲜度、来源优先级、匹配、常备规则）放 `app/src/core/`，不得依赖 QtGui / QtQuick / 网络，以便桌面上直接单测。
- **持久化与 SQL** 只在 `app/src/data/`；迁移集中在 `database.cpp` 的递增列表中，结构变更只追加迁移，不改旧项。`Store` 管理用户数据及导入图片清理，`BuiltinData` 缓存内置只读资源。
- **识别服务实现** 放 `app/src/recognition/`，都实现同一个接口（`RecognitionService`）；换服务商只新增实现，不改调用方。提示词文本集中在 `prompt-templates.h`，改动含义时同步改 `kPromptVersion`。
- **桌面开发工具**（需要真实密钥或只在桌面有意义的命令行程序）放 `app/tools/`，仅在非 Android 构建中编译，不注册 ctest。
- **应用服务与视图模型** 放 `app/src/ui/`；`*-service.*` 不依赖 QML，组合领域、持久化、识别与平台载荷；QML 只做展示和交互。
- **分层测试** 放 `app/tests/{core,data,ui}/`；`ui/` 测普通 C++ 应用服务，用合成录制回放，不需要 QML 界面或真实密钥。
- **可复用 QML 组件** 放 `app/qml/components/`，页面放 `app/qml/pages/`，主题常量只在 `app/qml/Theme.qml` 定义；页面只通过 `app/src/ui/app-model.*` 暴露的 `App` 单例取数和调用动作。
- **离线界面资源工具** 放 `tools/whilefresh_tools/assets/`，由 uv 的 `assets` 依赖组运行；生成字体子集到 `app/assets/fonts/`，纹理到 `app/assets/textures/`，许可与资源同行。QML 组件依 Qt 大写类型名使用 PascalCase。
- **内置数据** 只能由 `tools/` 生成到 `app/assets/data/`，不手改；生成物与生成脚本同分支提交。
- **词表源数据**：标准食材词表和菜谱原料映射的人工维护源数据放 `tools/whilefresh_tools/vocabulary/`，`app/assets/data/vocabulary.json` 与 `recipes.json` 的 `canonical` 由它生成。AI 只用于一次性生成初稿，草稿放缓存目录，审阅后才进入源数据；初稿的原始响应不入库。
- **第三方原始数据**：难以重新获取或需固定版本的放 `tools/sources/<来源>/`，附 `SOURCE.md` 写明网址、版本、获取日期、许可；可按固定 commit 重新下载的（如 HowToCook）不入库，下载到被忽略的缓存目录。
- **第三方构建依赖**（如 KDAB android_openssl）不入库：由 `scripts/fetch-deps.sh` 按脚本中记录的固定 commit 下载到 `~/.cache/whilefresh/<名称>/`，CMake 通过变量或环境变量引用。
- **构建时生成的文件**（如含密钥的 `generated/api-config.h`）只写到 `build/<目标>/`；源码目录只放模板（`*.h.in`）。
- **测试样本**：合成输入及其同名标准答案放 `test-data/synthetic/<screenshots|receipts|texts>/`，小票照片采用 JPEG、截图 PNG，HTML 为可重复生成的排版源；录制回放放 `test-data/recordings/synthetic/`（原始 `recognize-cli` format-1 响应 + 补充的 `elapsed_ms`）。生成/录制/评估工具和固定场景放 `tools/whilefresh_tools/synthetic/`，单测的手工录制夹具放 `tools/tests/fixtures/`。真实样例及其识别录制只放 `private/`（录制放 `private/recordings/`），任何内容都不得复制进仓库。
- **视觉稿** 放 `design/<版本或主题>/`，截图与 HTML 同目录。
- **一次性探针或实验脚本** 放 `/tmp`，不进仓库；确需保留的转成 `tools/` 或 `scripts/` 中的正式工具。
- **文档**：产品文档放 `docs/`，MVP 循环文档放 `docs/mvp/`；复杂模块的 `IMPLEMENTATION_NOTE` 写成 `docs/mvp/notes/<模块>.md`。

## Stability notes

- **命名**：项目约定新文件使用 kebab-case，以下语言约束例外：
  - QML 组件文件必须首字母大写（QML 类型名规则），如 `FridgePage.qml`；
  - Java 文件名必须与类名一致（PascalCase）；
  - Python 模块用 snake_case（kebab-case 无法 import）；
  - C++ 源文件用 kebab-case，如 `freshness-rules.cpp`，类名仍用 PascalCase。
- `app/assets/data/` 中的文件名会被 C++ 加载代码和 qrc 引用，改名需同时改两处。
- `private/` 整体被 `.gitignore` 排除；`private/pending/` 禁止 AI 工具访问（见 AGENTS.md）。
- 密钥不得出现在仓库任何位置；构建脚本从 macOS 钥匙串读取，生成文件写到构建目录。
- 历史版本在 `docs/archive/`，历史文件名保留不改。
- `.claude/skills/` 下是指向 `.agents/skills/` 的符号链接，不在其中放实体文件。

## Change log

- 2026-10-21 — 文档整理：开发说明移到 docs/development.md，历史版本移到 docs/archive/。
- 2026-10-21 — T7：增加 `tools/whilefresh_tools/synthetic/`、`test-data/synthetic/` 与 `test-data/recordings/synthetic/`，规定可重复生成的 HTML/PNG/JPEG/TXT、标准答案和合成识别录制的摆放位置。
- 2026-10-21 — T5：细化应用服务与视图模型的分工，记录 SQLite 迁移位置与 core/data/ui 分层测试。
- 2026-10-21 — 注明 `tools/tests/fixtures/` 存放测试用第三方原文样例。
- 2026-10-21 — T0 工程骨架：新增被忽略的 `build/` 构建目录；规定第三方构建依赖下载到 `~/.cache/whilefresh/`、构建生成文件只写入构建目录。
- 2026-10-21 — 新增 `tools/whilefresh_tools/vocabulary/`（词表源数据与工具）和 `docs/mvp/notes/`（模块复核记录）；放置规则补充词表源数据。
- 2026-10-21 — 增加 `app/tools/`（桌面开发工具）、`test-data/recordings/`、`test-data/vocabulary/`、`private/recordings/`，并约定识别提示词与录制格式的放置（T6）。
- 2026-10-21 — T9 Android 桥接：细化 `app/src/platform/`（`android/`、`desktop/` 与根目录的分工）、`app/android/` 的 Java 包路径与 `res/xml/`、`app/tests/` 按层分子目录；新增 `docs/mvp/notes/` 存放实现说明。
- 2026-10-20 — 初版：依据 MVP 工程规划建立 `app/`、`tools/`、`design/`、`test-data/`、`scripts/` 的布局与放置规则。

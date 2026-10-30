# Project Standards (STD)

本文件是项目文件树、放置规则与编码规范的权威说明。本文件由 `FHS.md` 于 2026-10-30 改名为 `STD.md`；使用全局 build-project-fhs skill 时，更新本文件 `STD.md`，不再创建 `FHS.md`。新增顶层目录、新增一类文件，或某类文件改放别处时，同步更新本文件。依据：[MVP 工程规划](docs/done/round-1/mvp-plan.md) 第 1 节。

## Layout

```
.
├── AGENTS.md / CLAUDE.md      # AI 协作规则：边界、构建测试、开发循环、review 标准（CLAUDE.md 只引入 AGENTS.md）
├── STD.md                     # 本文件
├── README.md                  # 面向人的简介：功能、技术栈、构建运行测试
├── LICENSE                    # BSD-3-Clause
├── .agents/skills/            # 项目级 skill 实体；.claude/skills/ 为同名符号链接
├── docs/                      # 产品与过程文档，按完成状态分两类
│   ├── done/                  # 已完成：描述已合并代码或已结束的工作
│   │   ├── development.md     # 开发参考：环境、构建与验证命令、数据工具、调试方法
│   │   ├── review-log.md      # 各分支的审查记录
│   │   ├── notes/             # 复杂模块的实现说明与复核报告
│   │   ├── round-1/           # 第一轮需求、规划、试用版说明
│   │   └── archive/           # 被取代的 PRD（v3–v5）与接入调研
│   ├── todo/                  # 未完成：当前 PRD（v6）、第二轮计划、cycle-state、待办
│   └── images/                # README 截图（仅演示模式数据）
├── app/                       # Qt 6 CMake 工程（Android APK + macOS 桌面开发版）
│   ├── CMakeLists.txt
│   ├── src/
│   │   ├── core/              # 领域逻辑，只依赖 QtCore：食材、日期、新鲜度、来源链、规则、匹配
│   │   ├── data/              # SQLite 存储、迁移、内置只读数据加载
│   │   ├── ai/                # DeepSeek 对话传输（ChatClient）、录制回放、system prompt 组装、密钥来源（T13）；之后的各 AI 服务（归类、方案、平替、偏好整理）
│   │   ├── recognition/       # 识别接口与实现（DeepSeek、回放、演示）、切片、解析、去重
│   │   ├── recipes/           # 第二轮（T17）：菜谱源接口、下厨房 YAML 解析、Lexmount 云浏览器实时来源（含 WebSocket/CDP）、录制样例回放、子模块适配
│   │   ├── platform/          # 平台适配的 C++ 侧（分享、相机、文件、剪贴板）、导入载荷与日期来源
│   │   │   ├── android/       # JNI 胶水（只在 Android 构建中编译）
│   │   │   └── desktop/       # 桌面替代实现（文件对话框、剪贴板）
│   │   └── ui/                # 应用服务（ImportService / KitchenService，不依赖 QML）及暴露给 QML 的视图模型
│   ├── qml/                   # QML 页面与组件；Theme 单例
│   ├── android/               # AndroidManifest、Java 桥接（src/io/github/mechjiang/whilefresh/）、res（xml/ 为 FileProvider 路径）
│   ├── tools/                 # 仅桌面可用的 C++ 开发工具（如 recognize-cli 真实识别与录制、recipe-cli 真实菜谱读取与录制），不进 ctest、不进 APK
│   ├── assets/
│   │   ├── data/              # 内置只读数据（由 tools/ 生成并提交）：菜谱、词表、参考表
│   │   ├── icons/             # Tabler 线条 SVG（附许可文件）
│   │   ├── fonts/             # Noto Serif SC Black 子集与 OFL 许可
│   │   ├── textures/          # 离线生成的可平铺纸纹与胶带纹理
│   │   └── demo/              # 演示模式的合成截图与对应识别结果（<输入类型>.json，录制格式，嵌入 qrc）
│   └── tests/                 # Qt Test 单元 / 集成测试，按被测层分子目录（core/、platform/ …）
├── tools/                     # Python 离线工具（uv 管理，pytest）
│   ├── pyproject.toml
│   ├── whilefresh_tools/      # 包源码：词表、参考表、合成数据、识别回归
│   │   ├── synthetic/         # 确定性商品场景、四种购物界面 / 小票 / 文本生成、Chrome 渲染、录制评估
│   │   └── vocabulary/        # 词表：源数据（vocabulary-source.json，人工维护）与构建 / 校验 / AI 初稿代码
│   ├── sources/               # 需要存档的第三方原始数据快照及来源说明（如 FoodKeeper）
│   └── tests/                 # pytest；fixtures/ 放少量第三方原文样例及合成手工录制夹具
├── design/                    # HTML 视觉稿与手机尺寸截图
├── test-data/                 # 合成样本（PNG / JPEG / 文本）、标准答案 JSON、合成样本的 DeepSeek 录制回放
│   ├── synthetic/             # 同名源输入 + 标准答案；screenshots/ 含可重复生成的 HTML/PNG，receipts/ 含 HTML/JPEG，texts/ 含 TXT
│   ├── recordings/            # 识别录制（请求摘要 + 原始响应），仅限合成数据；synthetic/ 中按样本 ID 同名存储；classifier/ 为归类服务的 ai 录制（格式 2）
│   ├── vocabulary/            # 测试用合成词表（正式词表见 app/assets/data/）与词表归类用例 resolve-cases.json
│   └── recipes/xiachufang/    # 第二轮（T17）：菜谱源子模块返回的 YAML 样例，用于测试与回放
├── scripts/                   # 构建、安装、取密钥、模拟器启动等 shell 脚本
├── build/                     # 不进 Git：构建产物，按目标分子目录（desktop、android-x86_64、android-arm64-v8a）
└── private/                   # 不进 Git：个人资料、真实测试样例（bill/）、真实样例的识别录制（recordings/）
```

## Placement rules

- **业务规则**（新鲜度、来源优先级、匹配、常备规则）放 `app/src/core/`，不得依赖 QtGui / QtQuick / 网络，以便桌面上直接单测。
- **持久化与 SQL** 只在 `app/src/data/`；迁移集中在 `database.cpp` 的递增列表中，结构变更只追加迁移，不改旧项。`Store` 管理用户数据及导入图片清理，`BuiltinData` 缓存内置只读资源。
- **词表**：上下位与名称归类逻辑只在 core（`IngredientCatalog::resolveFinest`、`VocabularyResolver`）；识别层不再有自己的词表类型。用户确认的名称映射通过 `core::NameMappingStore` 接口读写，SQLite 实现在 `app/src/data/name-mappings.*`。
- **AI 服务**（第二轮）放 `app/src/ai/`：只链接 core 和 Qt::Network（CMake 检查）；网络传输只在 `ChatClient`，各服务只通过它调用 DeepSeek、不自己发 HTTP，测试用 `RecordedChatClient` 回放。识别层复用它，不再自带传输。system prompt 用 `SystemPromptBuilder` 组装（偏好摘要在最后），密钥只从 `CredentialProvider` 取。
- **菜谱源**（第二轮）放 `app/src/recipes/`：只依赖 core、Qt::Network 和 yaml-cpp（CMake 检查）；实时来源全部异步，不得阻塞 UI 线程；外部子模块 `recipe-sources-xiachufang-api` 不复制进仓库、不修改，只通过 CMake 选项引用。
- **识别服务实现** 放 `app/src/recognition/`，都实现同一个接口（`RecognitionService`）；换服务商只新增实现，不改调用方。提示词文本集中在 `prompt-templates.h`，改动含义时同步改 `kPromptVersion`。
- **桌面开发工具**（需要真实密钥或只在桌面有意义的命令行程序）放 `app/tools/`，仅在非 Android 构建中编译，不注册 ctest。
- **应用服务与视图模型** 放 `app/src/ui/`；`*-service.*` 不依赖 QML，组合领域、持久化、识别与平台载荷；QML 只做展示和交互。
- **分层测试** 放 `app/tests/{core,ai,data,recognition,ui}/`；本地假 HTTP 服务器只有 `app/tests/ai/fake-http-server.h` 一份；`ui/` 测普通 C++ 应用服务，用合成录制回放，不需要 QML 界面或真实密钥。
- **可复用 QML 组件** 放 `app/qml/components/`，页面放 `app/qml/pages/`，主题常量只在 `app/qml/Theme.qml` 定义；页面只通过 `app/src/ui/app-model.*` 暴露的 `App` 单例取数和调用动作。
- **离线界面资源工具** 放 `tools/whilefresh_tools/assets/`，由 uv 的 `assets` 依赖组运行；生成字体子集到 `app/assets/fonts/`，纹理到 `app/assets/textures/`，许可与资源同行。QML 组件依 Qt 大写类型名使用 PascalCase。
- **内置数据** 只能由 `tools/` 生成到 `app/assets/data/`，不手改；生成物与生成脚本同分支提交。
- **词表源数据**：标准食材词表的人工维护源数据放 `tools/whilefresh_tools/vocabulary/`，`app/assets/data/vocabulary.json` 由它生成。AI 只用于一次性生成初稿，草稿放缓存目录，审阅后才进入源数据；初稿的原始响应不入库。
- **第三方原始数据**：难以重新获取或需固定版本的放 `tools/sources/<来源>/`，附 `SOURCE.md` 写明网址、版本、获取日期、许可；可按固定 commit 重新下载的不入库，下载到被忽略的缓存目录。
- **第三方构建依赖**（如 KDAB android_openssl）不入库：由 `scripts/fetch-deps.sh` 按脚本中记录的固定 commit 下载到 `~/.cache/whilefresh/<名称>/`，CMake 通过变量或环境变量引用。
- **构建时生成的文件**（如含密钥的 `generated/api-config.h`）只写到 `build/<目标>/`；源码目录只放模板（`*.h.in`）。
- **测试样本**：合成输入及其同名标准答案放 `test-data/synthetic/<screenshots|receipts|texts>/`，小票照片采用 JPEG、截图 PNG，HTML 为可重复生成的排版源；录制回放放 `test-data/recordings/synthetic/`（原始 `recognize-cli` format-1 响应 + 补充的 `elapsed_ms`）。生成/录制/评估工具和固定场景放 `tools/whilefresh_tools/synthetic/`，单测的手工录制夹具放 `tools/tests/fixtures/`。真实样例及其识别录制只放 `private/`（录制放 `private/recordings/`），任何内容都不得复制进仓库。
- **视觉稿** 放 `design/<版本或主题>/`，截图与 HTML 同目录。
- **一次性探针或实验脚本** 放 `/tmp`，不进仓库；确需保留的转成 `tools/` 或 `scripts/` 中的正式工具。
- **文档**：文档按完成状态放置：当前目标、计划、进度和待办放 `docs/todo/`；一份文档描述的工作全部合并后移到 `docs/done/`；被取代的 PRD 版本进 `docs/done/archive/`，历史文件名不改；复杂模块的实现说明在合并时写成 `docs/done/notes/<模块>.md`。

## 编码规范

规则从现有代码归纳，新代码与之保持一致。

### 命名

- 项目约定新文件使用 kebab-case，以下语言约束例外：
  - QML 组件文件必须首字母大写（QML 类型名规则），如 `FridgePage.qml`；
  - Java 文件名必须与类名一致（PascalCase）；
  - Python 模块用 snake_case（kebab-case 无法 import）；
  - C++ 源文件用 kebab-case，如 `freshness-rules.cpp`，类名仍用 PascalCase。

### C++

- C++17，4 空格缩进，不用 Tab。参照 `app/src/core/quantity.h`、`app/src/core/quantity.cpp`。
- 函数体的 `{` 单独一行；`if` / `for` / `while` 的 `{` 与语句同行。
- 头文件用 `#pragma once`。
- 代码放在 `namespace whilefresh::<层>`（如 `whilefresh::core`、`whilefresh::ai`）；文件内部实现放匿名命名空间。
- `#include` 从 `src/` 起写，如 `"core/quantity.h"`，不用相对路径。顺序为：本文件对应的头文件、项目头文件、Qt 头文件、标准库头文件，每组内按字母序（见 `app/src/ui/app-model.cpp` 开头）。
- 字符串字面量用 `QStringLiteral(...)` 包裹。
- 注释用英文，只解释“为什么”或给出输入→输出示例（如 `app/src/core/quantity.h` 中的 `"500g" -> 500 g`）；示例中可用中文食材名。

### QML

- 只做展示和交互：数据和动作只经 `app/src/ui/app-model.*` 暴露的 `App` 单例获取或调用（如 `app/qml/pages/ProfilePage.qml`）。
- 主题常量只在 `app/qml/Theme.qml` 定义，页面与组件通过它取值。
- 可复用组件放 `app/qml/components/`，页面放 `app/qml/pages/`，文件名 PascalCase。
- 界面文案只写标签和数值，不加解释性副标题；只显示例外情况（如“缺 酸奶”）。
- 时间文案统一为“剩余x天”“过期x天”，由 `app/src/ui/app-model.cpp` 生成，QML 不自行拼接。

### Python

- 由 uv 管理依赖；运行方式为 `uv run --project tools python -m whilefresh_tools.<包>`（见 `docs/done/development.md`）。
- 遵循 PEP 8；仓库未配置 lint 工具（`tools/pyproject.toml` 中无 ruff / flake8 设置）。
- 模块与包名用 snake_case，如 `whilefresh_tools/vocabulary/schema.py`。
- pytest 测试放 `tools/tests/`（`tools/pyproject.toml` 的 `testpaths`）。

### Shell

- bash 脚本以 `set -euo pipefail` 开头，随后 `source "$(dirname "$0")/env.sh"`（见 `scripts/build-desktop.sh`；`scripts/check-keys.sh` 因需汇总失败只用 `set -uo pipefail`）。
- 必须能在 macOS 自带的 bash 3.2 上运行：不用关联数组；可能为空的数组展开要加保护（见 `scripts/run-emulator.sh` 中的 `"${extra_args[@]+"${extra_args[@]}"}"`）。
- 不使用 `set -x`，不打印密钥；`scripts/env.sh` 只报告密钥是否找到。

### 文档

- 中文，Markdown；文件名 kebab-case（如 `prd-v6.md`、`round-2-plan.md`）。
- 放置按上文「文档」规则。

## Stability notes

- `app/assets/data/` 中的文件名会被 C++ 加载代码和 qrc 引用，改名需同时改两处。
- `private/` 整体被 `.gitignore` 排除；`private/pending/` 禁止 AI 工具访问（见 AGENTS.md）。
- 密钥不得出现在仓库任何位置；构建脚本从 macOS 钥匙串读取，生成文件写到构建目录。
- 历史版本在 `docs/done/archive/`，历史文件名保留不改。
- `.claude/skills/` 下是指向 `.agents/skills/` 的符号链接，不在其中放实体文件。

## Change log

- 2026-10-30 — 文档重组：FHS.md 改名为 STD.md 并增加编码规范；docs/ 按完成状态拆为 done/ 与 todo/（原 docs/mvp/ 与 docs/archive/ 并入）。
- 2026-10-30 — T16：新增 `platform/key-vault.*`（`android/`、`desktop/` 各一个实现）、`app/android/.../KeyVault.java`、`ai/preference-summarizer.*`、`core/preference.h`、`ui/settings-model.*`、`ui/user-keys.*`、`qml/pages/{Services,Preferences}Page.qml`、`qml/components/KeyField.qml`、`test-data/recordings/preferences/`。文件名避开 `.gitignore` 的 `*secret*` / `*credential*`。
- 2026-10-30 — T17 第二轮：`app/src/recipes/` 增加 LexmountRecipeSource、CdpConnection、WebSocketCodec、契约 YAML 生成；`app/tools/recipe-cli`；`app/src/ui/web-recipe-service.*` 与 `WebRecipeDetailPage.qml`；`recipes` 层改为依赖 core、Qt::Network、yaml-cpp；样例目录换成真实详情。
- 2026-10-30 — 测试期内置密钥：新增 `scripts/check-keys.sh`（提交前密钥扫描）；`scripts/env.sh` 同时读取 Lexmount 配置并随 DeepSeek 密钥注入。
- 2026-10-30 — T17：新增 `app/src/recipes/`（RecipeSource、下厨房 YAML 解析、RecordedRecipeSource、可选的 ProcessRecipeSource）与 `app/tests/recipes/`；创建 `test-data/recipes/xiachufang/`（样例 YAML 与 SOURCE.md）；`scripts/fetch-deps.sh` 增加 yaml-cpp（`~/.cache/whilefresh/yaml-cpp/`）。
- 2026-10-30 — T14：删除 `recognition/vocabulary.*`，识别统一使用 `core::IngredientCatalog`；新增 `core/vocabulary-resolver.*`、`core/name-mapping.h`、`ai/ingredient-classifier.*`、`data/name-mappings.*`，`test-data/vocabulary/resolve-cases.json`，预留 `test-data/recordings/classifier/`。
- 2026-10-30 — T13：新增 `app/src/ai/`（ChatClient、录制回放、SystemPromptBuilder、CredentialProvider）与 `app/tests/ai/`；密钥注入从 recognition 移到 ai，`recognition` 改为依赖 `ai`。
- 2026-10-29 — 第二轮规划（T12）：PRD v5 归档，新增 `docs/prd-v6.md`、`docs/mvp/round-2-plan.md`；预留 `app/src/ai/`、`app/src/recipes/`、`test-data/recipes/xiachufang/`，由 T13、T17 创建。HowToCook 相关目录将在 T18b 移除。
- 2026-10-30 — T18b：删除 `app/assets/data/recipes.json`、`app/src/core/recipe.*`、`app/tests/core/recipe-matcher-test.cpp`、`RecipeDetailPage.qml`、`RecipeCard.qml`、`tools/whilefresh_tools/recipes/`、`vocabulary/{ingredient-map.json,mapping.py,report.py}`、`tools/tests/fixtures/dishes/` 与两张旧截图；菜谱页只剩下厨房搜索。
- 2026-10-21 — 文档整理：开发说明移到 docs/development.md，历史版本移到 docs/archive/。
- 2026-10-21 — T7：增加 `tools/whilefresh_tools/synthetic/`、`test-data/synthetic/` 与 `test-data/recordings/synthetic/`，规定可重复生成的 HTML/PNG/JPEG/TXT、标准答案和合成识别录制的摆放位置。
- 2026-10-21 — T5：细化应用服务与视图模型的分工，记录 SQLite 迁移位置与 core/data/ui 分层测试。
- 2026-10-21 — 注明 `tools/tests/fixtures/` 存放测试用第三方原文样例。
- 2026-10-21 — T0 工程骨架：新增被忽略的 `build/` 构建目录；规定第三方构建依赖下载到 `~/.cache/whilefresh/`、构建生成文件只写入构建目录。
- 2026-10-21 — 新增 `tools/whilefresh_tools/vocabulary/`（词表源数据与工具）和 `docs/mvp/notes/`（模块复核记录）；放置规则补充词表源数据。
- 2026-10-21 — 增加 `app/tools/`（桌面开发工具）、`test-data/recordings/`、`test-data/vocabulary/`、`private/recordings/`，并约定识别提示词与录制格式的放置（T6）。
- 2026-10-21 — T9 Android 桥接：细化 `app/src/platform/`（`android/`、`desktop/` 与根目录的分工）、`app/android/` 的 Java 包路径与 `res/xml/`、`app/tests/` 按层分子目录；新增 `docs/mvp/notes/` 存放实现说明。
- 2026-10-20 — 初版：依据 MVP 工程规划建立 `app/`、`tools/`、`design/`、`test-data/`、`scripts/` 的布局与放置规则。

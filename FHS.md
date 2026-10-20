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
│   └── mvp/                   # MVP 循环文档：需求、规划、cycle-state、交付报告
│       └── notes/             # 复杂模块的实现说明（如 android-integration.md）
├── app/                       # Qt 6 CMake 工程（Android APK + macOS 桌面开发版）
│   ├── CMakeLists.txt
│   ├── src/
│   │   ├── core/              # 领域逻辑，只依赖 QtCore：食材、日期、新鲜度、来源链、规则、匹配
│   │   ├── data/              # SQLite 存储、迁移、内置只读数据加载
│   │   ├── recognition/       # 识别接口与实现（DeepSeek、回放、演示）、切片、解析、去重
│   │   ├── platform/          # 平台适配的 C++ 侧（分享、相机、文件、剪贴板）、导入载荷与日期来源
│   │   │   ├── android/       # JNI 胶水（只在 Android 构建中编译）
│   │   │   └── desktop/       # 桌面替代实现（文件对话框、剪贴板）
│   │   └── ui/                # 暴露给 QML 的视图模型
│   ├── qml/                   # QML 页面与组件；Theme 单例
│   ├── android/               # AndroidManifest、Java 桥接（src/io/github/mechjiang/whilefresh/）、res（xml/ 为 FileProvider 路径）
│   ├── assets/
│   │   ├── data/              # 内置只读数据（由 tools/ 生成并提交）：菜谱、词表、参考表
│   │   ├── icons/             # Tabler 线条 SVG（附许可文件）
│   │   ├── textures/          # 纸纹等拟物质感素材
│   │   └── demo/              # 演示模式的合成截图与对应识别结果
│   └── tests/                 # Qt Test 单元 / 集成测试，按被测层分子目录（core/、platform/ …）
├── tools/                     # Python 离线工具（uv 管理，pytest）
│   ├── pyproject.toml
│   ├── whilefresh_tools/      # 包源码：菜谱转换、词表、参考表、合成数据、识别回归
│   ├── sources/               # 需要存档的第三方原始数据快照及来源说明（如 FoodKeeper）
│   └── tests/                 # pytest；fixtures/ 放少量第三方原文样例（须许可允许）
├── design/                    # HTML 视觉稿与手机尺寸截图
├── test-data/                 # 合成样本（PNG / 文本）、标准答案 JSON、合成样本的 DeepSeek 录制回放
├── scripts/                   # 构建、安装、取密钥、模拟器启动等 shell 脚本
├── build/                     # 不进 Git：构建产物，按目标分子目录（desktop、android-x86_64、android-arm64-v8a）
└── private/                   # 不进 Git：个人资料、真实测试样例（bill/）、真实样例的识别录制
```

## Placement rules

- **业务规则**（新鲜度、来源优先级、匹配、常备规则）放 `app/src/core/`，不得依赖 QtGui / QtQuick / 网络，以便桌面上直接单测。
- **持久化与 SQL** 只在 `app/src/data/`；数据库结构变更必须新增迁移，不改旧迁移。
- **识别服务实现** 放 `app/src/recognition/`，都实现同一个接口；换服务商只新增实现，不改调用方。
- **Android 专有代码**：Java 放 `app/android/src/`，C++ 侧 JNI 胶水放 `app/src/platform/android/`；桌面替代实现放 `app/src/platform/desktop/`。平台无关、可在桌面单测的平台逻辑（导入载荷、日期来源、Java 桥接消息解析）放 `app/src/platform/` 根目录。
- **视图模型** 放 `app/src/ui/`；QML 不写业务逻辑，只做展示和交互。
- **可复用 QML 组件** 放 `app/qml/components/`，页面放 `app/qml/pages/`，主题常量只在 `app/qml/Theme.qml` 定义。
- **内置数据** 只能由 `tools/` 生成到 `app/assets/data/`，不手改；生成物与生成脚本同分支提交。
- **第三方原始数据**：难以重新获取或需固定版本的放 `tools/sources/<来源>/`，附 `SOURCE.md` 写明网址、版本、获取日期、许可；可按固定 commit 重新下载的（如 HowToCook）不入库，下载到被忽略的缓存目录。
- **第三方构建依赖**（如 KDAB android_openssl）不入库：由 `scripts/fetch-deps.sh` 按脚本中记录的固定 commit 下载到 `~/.cache/whilefresh/<名称>/`，CMake 通过变量或环境变量引用。
- **构建时生成的文件**（如含密钥的 `generated/api-config.h`）只写到 `build/<目标>/`；源码目录只放模板（`*.h.in`）。
- **测试样本**：合成样本及其标准答案、录制回放放 `test-data/`；真实样例及其识别录制只放 `private/`，任何内容都不得复制进仓库。
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
- `docs/prd_v3.md` 为历史文件名（非 kebab-case），保留不改。
- `.claude/skills/` 下是指向 `.agents/skills/` 的符号链接，不在其中放实体文件。

## Change log

- 2026-10-21 — 注明 `tools/tests/fixtures/` 存放测试用第三方原文样例。
- 2026-10-20 — 初版：依据 MVP 工程规划建立 `app/`、`tools/`、`design/`、`test-data/`、`scripts/` 的布局与放置规则。
- 2026-10-21 — T0 工程骨架：新增被忽略的 `build/` 构建目录；规定第三方构建依赖下载到 `~/.cache/whilefresh/`、构建生成文件只写入构建目录。
- 2026-10-21 — T9 Android 桥接：细化 `app/src/platform/`（`android/`、`desktop/` 与根目录的分工）、`app/android/` 的 Java 包路径与 `res/xml/`、`app/tests/` 按层分子目录；新增 `docs/mvp/notes/` 存放实现说明。

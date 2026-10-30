# 项目协作约定

本文件是本项目 AI 编码与协作规则的唯一来源。Codex、Claude Code 及其他 AI 工具均应遵守本文件；工具专属文件只负责引入本文件，不重复维护规则。`CLAUDE.md` 为 Claude Code 的导入入口，`README.md` 面向人类读者。

## 项目概述

- 项目类型：AI 产品探索与 Landing Project。
- 当前阶段：围绕帮助家庭消耗已购食材的单人手机客户端开展需求验证、产品定义与原型设计；未来可扩展多人协作，已完成 Android MVP 试用版 0.1.0（Qt 6.8.2），正在按 PRD v6 进行第二轮开发。
- 目标：产出可供团队评估和继续实现的产品方案、调研与原型。
- 当前产品定义见 `docs/todo/prd-v6.md`，历史版本在 `docs/done/archive/`；进度见 `docs/todo/cycle-state.md`，第二轮计划见 `docs/todo/round-2-plan.md`。

## 先读什么

1. `STD.md`：文件树、放置规则与编码规范。
2. `docs/todo/cycle-state.md`：当前阶段、进度与决策。
3. `docs/todo/round-2-plan.md` 中当前任务所在的行。当前任务 = cycle-state「第二轮进度」中第一个未勾选的任务。
4. 模块细节见 `docs/done/development.md` 与 `docs/done/notes/`。

## 目录与规范

- 文件树、放置规则、编码规范见 `STD.md`；新增、移动、删除文件前先读，结构变化时同步更新。
- 新文件使用 kebab-case 命名。按内容类型归档，避免把资料长期堆在根目录；不要为了目录整齐而移动已有文件，除非任务需要并已确认。
- `private/`：个人、敏感资料及已归档材料。AI 工具不得浏览、列举、搜索或读取 `private/pending/` 中的内容；只有用户明确要求处理归档材料时例外。

## Skill 索引

Skill 实际文件保存在 `.agents/skills/`，`.claude/skills/` 通过同名符号链接复用。`CLAUDE.md` 通过 `@AGENTS.md` 引入本索引，无需重复维护。

- [loop-dev-cycle](.agents/skills/loop-dev-cycle/SKILL.md)：软件工程循环。
- [lexmount-fetch](.agents/skills/lexmount-fetch/SKILL.md)：使用 Lexmount 抓取公开网页正文并输出 Markdown；脚本和运行条件见 skill 目录。仅作后备：网页检索和抓取默认使用工具内置的搜索与抓取，内置方式失败时再用 Lexmount。本机不安装该 skill 的 Node 依赖，需要时用 Python 直接调用 Lexmount Web Fetch API（`POST {base-url}/v1/extract`，请求头 `X-API-Key`、`X-Project-Id`，凭据读自 `~/.config/lexmount/config.yaml`，不得打印）。

## AI 协作

- 需求存在会影响结果的关键歧义时，先查阅现有材料；仍无法判断再由opus审阅
- 面向内部团队，使用清楚、中性的中文；优先采用 Markdown。

## 安全与隐私

- 不将密钥、`.env`、个人数据或未公开敏感资料纳入 Git。
- `private/` 中资料默认按敏感资料处理。确需共享时先确认授权，并逐项审查内容。
- 不在对话、日志或公开文档中暴露客户信息、个人数据或未发布敏感内容。
- 真实 DeepSeek / Lexmount 调用会计费，需用户同意，次数记入 `docs/todo/cycle-state.md` 的偏差日志。
- 识别与菜谱录制只允许合成数据或公开页面；真实样例及其录制只放 `private/`。

## 构建与测试

- 应用技术栈：Qt 6.8.2（Qt Quick/QML + C++17）、CMake + Ninja，工程在 `app/`；同一工程构建 macOS 桌面开发版和 Android APK（x86_64、arm64-v8a）。命令见 `docs/done/development.md`。
- 必须使用 `~/Qt/6.8.2/<套件>/bin/qt-cmake`（脚本已处理）；PATH 中的 `qmake`/`qtpaths` 来自 Anaconda，不要使用。
- 构建一律通过 `scripts/` 中的脚本，产物在被忽略的 `build/<目标>/`；第三方依赖由 `scripts/fetch-deps.sh` 下载到 `~/.cache/whilefresh/` 并固定 commit，不放进仓库。
- 分层依赖：`core` 只链接 Qt::Core；`data`、`platform` 依赖 `core`；`ai` 只链接 `core` 与 Qt::Network，`recognition` 依赖 `ai`；`recipes` 只依赖 `core`、Qt::Network 与 yaml-cpp；`ui` 依赖以上各层；QML 只做展示。`core`、`ai`、`recipes` 的约束在 CMake 中有检查。
- 测试：Qt Test + CTest，仅桌面构建。改动 C++ 后至少运行 `scripts/build-desktop.sh` 与 `ctest --test-dir build/desktop --output-on-failure`。
- 密钥：只由构建脚本从钥匙串读取并生成到构建目录的 `generated/api-config.h`；脚本不得打印密钥或开启 `set -x`；测试阶段 DeepSeek 密钥（钥匙串）与 Lexmount 凭据（`~/.config/lexmount/config.yaml`）一同注入。需要密钥或更换密钥时，一律通过 `scripts/` 下的构建脚本重新构建，不要在构建目录里直接运行 `ninja`（自动重新配置会得到无密钥构建）。提交前运行 `scripts/check-keys.sh`（`sk-` 模式与真实 Lexmount 值的扫描）确认无密钥。
- 新增代码或工具链时，先在 `docs/done/development.md` 中记录用途、运行方式和验证命令，再补充本节。
- 执行验证前遵循用户当次要求。改动 C++ 后的桌面构建与 ctest 是必做项；“未要求不擅自运行”只针对会产生大量文件或访问外部服务的操作，如真实 DeepSeek / Lexmount 调用、录制、Android 构建与安装。
- 构建和测试输出写到文件，只读末尾或失败行。

常用命令：

```sh
scripts/build-desktop.sh                          # 桌面开发版
ctest --test-dir build/desktop --output-on-failure
uv run --project tools pytest                     # Python 工具测试
scripts/build-android.sh x86_64                   # 模拟器 APK（arm64-v8a 同理）
scripts/install-android.sh x86_64                 # 安装并启动
scripts/check-keys.sh                             # 提交前检查无密钥
```

## 开发循环与停止条件

- 流程：
  1. 每个任务一个分支（如 `feat/meal-plan`）。
  2. 实现 → 桌面构建与 ctest（`scripts/build-desktop.sh`、`ctest --test-dir build/desktop --output-on-failure`）。
  3. 主会话审 diff。
  4. 审查子代理复审。
  5. 修复。
  6. 把摘要交用户。
  7. **用户确认后才以 `--no-ff` 合并到 main**；未经要求不推送。
- 分工：开发由主会话分派子代理完成；过程记入 `docs/todo/cycle-state.md` 的决策日志，审查结果记入 `docs/done/review-log.md`。
- 迭代上限：审查/修复最多 2 轮，只有 blocker / major 才进入下一轮；minor 记入 review-log 后放行。同一问题的测试修复单轮最多 5 次。loop-dev-cycle skill 中的迭代规则与本文件冲突时，以本文件为准。
- 正常结束：测试全部通过且审查无 blocker / major。
- 必须停下汇报：达到迭代上限；需求存在影响结果的歧义且查材料无法判断；需要计划外的计费调用；需要超出任务范围或破坏性的操作；用户说"暂停""停止"。
- 子代理：实现者与审查者默认用中档模型（Claude Code 下为 Sonnet，effort medium）；自第二轮 T16 起执行。

## 合并后更新

用户确认合并后，同一次提交内完成：

1. `docs/done/review-log.md`：记录该分支的审查结果与处理状态。
2. `docs/todo/cycle-state.md`：在「第二轮进度」勾选任务、写明合并提交；决策日志加一行。
3. `docs/todo/todo.md`：删除已完成的条目，新发现的问题补进来。
4. `docs/todo/prd-v6.md`：更新受影响条目的“状态”列。
5. `docs/done/development.md`（新命令、工具）与 `docs/done/notes/<模块>.md`（复杂模块的实现说明）。
6. `STD.md`：目录或放置规则变化时更新文件树与 Change log。
7. 一份 `docs/todo/` 文档描述的工作全部合并后，移到 `docs/done/`。

## Review 标准

审查者按以下检查项逐条核对：

- 范围与任务一致，无顺手改动。
- 分层依赖与 `STD.md` 放置规则符合要求。
- 关键用例有测试且通过，用例见 `docs/todo/round-2-plan.md` 第 5 节。
- 无密钥与个人数据（`scripts/check-keys.sh`）。
- 无 AI 署名。
- 文档同步：`docs/done/development.md`、`docs/done/notes/`、`STD.md` 的变更记录、`docs/todo/cycle-state.md`。

严重度：

- **blocker**：错误结果、数据丢失、密钥泄露、构建失败。
- **major**：违反规则或缺关键测试。
- **minor**：风格、命名、可读性。

处理状态：**已修**（合并前修复）、**延后**（记入后续任务）、**接受**（评估后不改）。结果逐分支记入 `docs/done/review-log.md`。

## Git 约定

- 默认主分支为 `main`，远端为 https://github.com/me-chjiang/whilefresh 。main 只接收审阅通过的合并。
- MVP 开发中每个功能使用独立分支（如 `feat/recipe-data`）；经审查和测试后，把摘要交用户，**用户确认后才合并到 `main`**。流程见上文"开发循环与停止条件"。
- 提交信息遵循 Conventional Commits。
- 提交使用本机 Git 用户配置，不添加 `Co-Authored-By` 等额外署名。
- 未经明确要求，不创建提交、不推送远端、不改写提交历史。

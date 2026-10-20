# 项目协作约定

本文件是本项目 AI 编码与协作规则的唯一来源。Codex、Claude Code 及其他 AI 工具均应遵守本文件；工具专属文件只负责引入本文件，不重复维护规则。

## 项目概述

- 项目类型：AI 产品探索与 Landing Project。
- 当前阶段：围绕帮助家庭消耗已购食材的单人手机客户端开展需求验证、产品定义与原型设计；未来可扩展多人协作，尚未确定实际技术栈或生产代码入口。
- 目标：产出可供团队评估和继续实现的产品方案、调研与原型。
- 考核要求和当前产品定义见 `docs/`，以 `prd-v5.md` 为当前 PRD；MVP 进度见 `docs/mvp/cycle-state.md`。

## 目录结构

- `docs/`：项目要求和当前产品定义。
- `.agents/skills/`：Codex 项目级 skills 的实际文件；`.claude/skills/` 通过符号链接供 Claude Code 使用。
- `private/`：个人、敏感资料及已归档材料。AI 工具不得浏览、列举、搜索或读取 `private/pending/` 中的内容；只有用户明确要求处理归档材料时例外。
- `AGENTS.md`：跨工具共用的项目规则。
- `CLAUDE.md`：Claude Code 的规则导入入口。
- `README.md`：项目简介与材料索引。
- `FHS.md`：目录布局与文件放置规则。新增、移动、删除文件前先读；结构变化时同步更新。

新文件使用 kebab-case 命名。按内容类型归档，避免把资料长期堆在根目录；不要为了目录整齐而移动已有文件，除非任务需要并已确认。

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

## Git 约定

- 默认主分支为 `main`，远端为 https://github.com/me-chjiang/whilefresh 。
- MVP 开发中每个功能使用独立分支（如 `feat/recipe-data`）；经审查和测试后，把摘要交用户，**用户确认后才合并到 `main`**。流程见 `docs/mvp/cycle-state.md`。
- 提交信息遵循 Conventional Commits。
- 提交使用本机 Git 用户配置，不添加 `Co-Authored-By` 等额外署名。
- 未经明确要求，不创建提交、不推送远端、不改写提交历史。

## 开发与验证

- 当前仓库以产品和研究文档为主，尚未建立应用技术栈、构建命令或测试约定。
- 引入代码或工具链时，先在 README 中记录用途、运行方式和验证命令，再按实际技术栈补充本节。
- 执行验证前遵循用户当次要求；如未要求测试，不擅自运行会产生大量文件或访问外部服务的操作。

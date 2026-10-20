# 趁鲜（WhileFresh）

AI 产品探索与 Landing Project 材料库。产品名「趁鲜」：帮助家庭趁食材新鲜时用掉已购食材的单人手机客户端，未来可扩展多人协作。仓库目前包含需求验证、产品定义和 MVP 需求，尚未包含应用实现。

## 项目材料

- [考核要求](docs/requirements.md)：Landing Project 任务要求与团队交付预期。
- [趁鲜 PRD v5](docs/prd-v5.md)：当前产品定义，含冰箱视图、保质期与临期、MVP 交付形态。
- [厨房食材消耗助手 PRD v4](docs/prd-v4.md)：上一版 PRD，作历史参考。
- [国内菜谱与买菜平台接入调研报告](docs/domestic-recipe-grocery-integration-research.md)：菜谱数据来源、买菜平台订单与清单能力、附近补货可行性及验证计划。
- [菜谱与买菜接入能力速查](docs/domestic-integration-brief.md)：独立精简核查，列出可调用 API、凭证要求及实际可用状态。
- [MVP 需求确认稿](docs/mvp/mvp-requirements.md)：Android MVP 的功能范围、对 PRD v4 的修订与验收标准。
- [MVP 循环状态](docs/mvp/cycle-state.md)：loop-dev-cycle 当前阶段、决策日志、分工与待解决问题；新会话先读此文件。

个人或敏感资料放在 `private/`，不应提交到 Git。项目协作规则统一维护在 [AGENTS.md](AGENTS.md)；Claude Code 通过根目录的 `CLAUDE.md` 导入同一份规则。

## 项目级 Skills

Codex 从 `.agents/skills/` 加载项目级 skill。Claude Code 的 `.claude/skills/` 中使用同名符号链接指向这些目录，避免维护两份内容。

- [loop-dev-cycle](.agents/skills/loop-dev-cycle/SKILL.md)：软件工程循环。
- [lexmount-fetch](.agents/skills/lexmount-fetch/SKILL.md)：使用 Lexmount 抓取网页；脚本位于同目录的 `fetch.mjs`，运行条件见 skill 文件。

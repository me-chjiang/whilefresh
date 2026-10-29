# 趁鲜第二轮工程计划

> 2026-10-29。依据 [PRD v6](../prd-v6.md)。第一轮的计划与需求见 [mvp-plan.md](mvp-plan.md)、[mvp-requirements.md](mvp-requirements.md)。本轮先把功能做对，界面能用即可。进度记在 [cycle-state.md](cycle-state.md)。

## 1. 架构变化

在原有分层（core / data / recognition / platform / ui，QML 只通过 `App` 访问）上新增两层：

```
ui ──► ai ──► core          ai：DeepSeek 对话传输与各 AI 服务
 │      ▲                  recipes：下厨房菜谱源接口与 YAML 解析
 ├──► recognition ─┘
 ├──► recipes ──► core
 ├──► data ──► core
 └──► platform ──► core
```

- `ai` 只依赖 core 和 Qt::Network；`recognition` 改为依赖 `ai`。CMake 中的分层检查同步更新。
- `recipes` 只依赖 core；YAML 解析使用 yaml-cpp，版本与菜谱源子模块一致，由 `scripts/fetch-deps.sh` 下载。

## 2. 架构决策

| 编号 | 决策 | 原因 |
| --- | --- | --- |
| R-ADR-1 | 从 `DeepSeekRecognizer` 抽出 `ChatClient`（超时、5xx 重试、JSON 模式、截断时返回部分结果），识别和所有新 AI 服务共用 | 新增的归类、方案、平替、偏好整理都要调 DeepSeek，传输逻辑只写一份 |
| R-ADR-2 | 在传输层录制和回放（`RecordedChatClient`） | 所有 AI 服务的测试和离线回放都能复用，不必每个服务各写一套假实现 |
| R-ADR-3 | `CredentialProvider` 接口：内置密钥（演示模式）和用户密钥（Android Keystore 加密）两种实现 | 演示和正式使用只差密钥来源，调用方不感知 |
| R-ADR-4 | 菜谱源只定义接口 `RecipeSource`（search / detail → `WebRecipe`）；适配层包装外部子模块，默认不编译；开发和测试用录制的 YAML | 子模块由外部实现，本项目不能修改它 |
| R-ADR-5 | 去掉 `recognition::Vocabulary`，统一使用 `core::IngredientCatalog`，新增 `resolveFinest` | 两份词表结构重复；上下位逻辑应只在一处 |
| R-ADR-6 | 方案是否用上全部勾选食材，由 core 中的 `PlanCoverage` 校验，不信任模型自报 | 第一轮的推荐不保证覆盖（勾选虾仁和蓝莓，结果却是蒸蛋） |
| R-ADR-7 | 移除 HowToCook，不做本地回退 | 用户决定；下厨房成为唯一菜谱来源 |
| R-ADR-8 | 删除导入改为软删除：只删在库食材，`Used` / `Discarded` 保留 `import_id` | 已经吃掉的不应因删除记录而消失；每件食材都能追溯到哪次导入 |
| R-ADR-9 | 导入和下厨的续传共用 core 中的 `ResumableJob` 状态机，状态落库 | Android 切到购物 App 时常杀掉进程；批次识别失败不应整单重来 |
| R-ADR-10 | 通知由 Android 端定时调度，接收器直接读 App 写出的快照 JSON，不启动 Qt | 定时任务时 Qt 进程可能不在；快照足够算出临期数量 |
| R-ADR-11 | `AppModel` 拆成子模型（importer / cleanup / settings / attention），仍挂在 `App` 下 | 文件已近 700 行，新功能还会继续加 |

## 3. 数据库迁移 v5（只追加）

- `items`：`kind`（food / staple_seasoning / staple_food）、`amount`、`unit`、`snoozed_until`；状态新增 `Discarded`。
- `imports`：`deleted_at`、`duplicate_of`、`status`（confirmed / skipped_duplicate）。
- 新表：`name_mappings`、`preferences`、`import_jobs`、`import_job_batches`、`cleanup_sessions`。
- `settings.preference_summary` 存偏好摘要。

## 4. 任务

每个任务一个分支；主会话审 diff，Sonnet 复审，测试通过后交用户确认，再以 `--no-ff` 合并。

| 任务 | 分支 | 内容 | 依赖 |
| --- | --- | --- | --- |
| T12 | `docs/prd-v6` | PRD v6、本计划、cycle-state、FHS | — |
| T13 | `feat/ai-client` | `ai` 层：ChatClient、RecordedChatClient、SystemPromptBuilder、CredentialProvider；DeepSeekRecognizer 改用 ChatClient | T12 |
| T14 | `feat/vocabulary-hierarchy` | 词表统一、`resolveFinest`、分层提示词 v3、`VocabularyResolver` 与 `name_mappings`；Haiku 起草补充词条和合成词表用例，主会话审核 | T13 |
| T15 | `feat/item-model` | 迁移 v5、软删除导入、重复记录、调料入库、用量解析、冰箱排序；导入续传与 `ResumableJob` | T13，可与 T14 并行 |
| T16 | `feat/services-settings` | 密钥存储与服务页、演示模式内置密钥、偏好记忆与整理 | T14、T15 |
| T17 | `feat/recipe-source` | `recipes` 层、YAML 解析、录制样例（用子模块的 Python CLI 生成） | T16 |
| T18 | `feat/meal-plan` | MealPlanner、PlanCoverage、换菜、RecipeAdapter、`recipe-stock`、扣减；下厨会话续传与幂等提交 | T16，core 部分可与 T17 并行 |
| T18b | `refactor/remove-howtocook` | 删除内置菜谱、匹配器、菜谱页和相关工具；词表工具与菜谱解耦 | T18 |
| T19 | `feat/home-shell` | 新导航、计划页、我的各页、每日一次弹窗、拆分 AppModel；界面只求能用 | T18b |
| T20 | `feat/reminders` | 通知调度、快照、权限 | T19 |
| T21 | `spike/a11y-bubble` | 无障碍气泡可行性验证与报告 | T20 |

## 5. 验证

- 每个分支：`scripts/build-desktop.sh` 与 `ctest --test-dir build/desktop --output-on-failure`。
- 关键用例：
  - 词表：梅花肉、"黑猪梅花肉切片" → 梅花肉；只能命中上位词；完全未知。
  - 删除导入：在库的删除，`Used` / `Discarded` 保留，手动添加的不受影响。
  - 重复导入：跳过后仍有记录。
  - 方案：勾选虾仁和蓝莓时，没用上蓝莓的方案被丢弃。
  - 续传：第 2 批失败后重启，只重发第 2 批及以后；同一下厨会话重复提交只扣减一次。
  - 冰箱排序：过期在前，剩余天数升序，未知在后。
- 删除导入和疑似重复两项，在 T15 合并前把测试步骤交给用户亲自走一遍。
- 每次提交前：`rg -uu -e 'sk-[0-9a-f]{32}' --glob '!build/**' .`，并确认 Lexmount 凭据不在仓库、日志和 YAML 中。

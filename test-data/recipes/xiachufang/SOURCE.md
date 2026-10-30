# 下厨房菜谱源样例

菜谱源子模块 `recipe-sources-xiachufang-api`（`../landing-project/recipe-sources-xiachufang-api`，Python，`schema_version` 1.0）输出的 YAML，用于 `app/tests/recipes/` 的解析与回放测试，以及离线开发。子模块的 Git 仓库在录制时尚无提交，版本以 `pyproject.toml` 的 0.1.0 和 `uv.lock` 为准。

## 文件

| 文件 | 内容 | 来源 |
|---|---|---|
| `search-tomato-egg.yaml` | 搜索“番茄炒蛋”第 1 页，15 条摘要 | 2026-10-30 实录，`--provider direct` |
| `search-shrimp.yaml` | 搜索“虾仁”第 1 页，14 条摘要 | 同上 |
| `error-detail-blocked.yaml` | 详情 107722218，直连被拦截（`unrecognized detail page`），status error | 同上，真实错误输出 |
| `error-invalid-id.yaml` | 详情 id `abc`，参数错误，status error | 同上 |
| `detail-synthetic-full.yaml` | 详情 900000001，完整（步骤、食材、小贴士） | 合成，见下 |
| `detail-synthetic-partial.yaml` | 详情 900000002，缺步骤和小贴士，status partial | 合成，见下 |

## 生成方式

搜索与错误样例：在子模块目录用它自己的 `.venv`（`uv`）运行 CLI，不修改子模块。

```sh
uv run python -m xiachufang_source --provider direct --keyword 番茄炒蛋 > search-tomato-egg.yaml
uv run python -m xiachufang_source --provider direct --keyword 虾仁 > search-shrimp.yaml
uv run python -m xiachufang_source --provider direct --operation detail --id 107722218 > error-detail-blocked.yaml
uv run python -m xiachufang_source --provider direct --operation detail --id abc > error-invalid-id.yaml
```

真实详情页录制失败：直连详情被站点拦截；`--provider lexmount`（凭据读自 `~/.config/lexmount/config.yaml`，经参数传入、不打印）搜索和详情都返回来源错误（`source recipe must have a name` / `response requires one Content recipe`），子模块当前对 Lexmount 路线并不可用。因此详情样例是合成的：用子模块自己的 `RecipeEngine` 和 `yaml.safe_dump`，输入换成脚本内的固定 `Content` 数据（不联网），保证字段形状与真实输出一致；id 取 900000001/2，`source_url` 不指向真实菜谱，正文是示例文字。拿到真实详情后应替换这两个文件并同步改测试断言。

YAML 的 `query` 不含凭据（子模块在回显前去掉了 `api_key`、`project_id`），提交前用下面的命令确认：

```sh
rg -i 'api.?key|project.?id|credential|token' test-data/recipes/xiachufang
```

只应匹配 `query.lexmount_base_url: null` 这类空字段。

## 使用条款

下厨房内容仅限试用、非商业；保留来源链接与署名（`source_url`、`source.name`），不转载、不打包菜谱正文进应用。样例仅作解析测试，不随应用分发。子模块代码为 Apache-2.0，不授予菜谱正文或图片的转载权。

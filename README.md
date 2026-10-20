# 趁鲜（WhileFresh）

AI 产品探索与 Landing Project 材料库。产品名「趁鲜」：帮助家庭趁食材新鲜时用掉已购食材的单人手机客户端，未来可扩展多人协作。仓库包含需求验证、产品定义、MVP 需求，以及 `app/` 下开发中的 Qt 应用（目前为工程骨架）。

## 项目材料

- [考核要求](docs/requirements.md)：Landing Project 任务要求与团队交付预期。
- [趁鲜 PRD v5](docs/prd-v5.md)：当前产品定义，含冰箱视图、保质期与临期、MVP 交付形态。
- [厨房食材消耗助手 PRD v4](docs/prd-v4.md)：上一版 PRD，作历史参考。
- [国内菜谱与买菜平台接入调研报告](docs/domestic-recipe-grocery-integration-research.md)：菜谱数据来源、买菜平台订单与清单能力、附近补货可行性及验证计划。
- [菜谱与买菜接入能力速查](docs/domestic-integration-brief.md)：独立精简核查，列出可调用 API、凭证要求及实际可用状态。
- [MVP 需求确认稿](docs/mvp/mvp-requirements.md)：Android MVP 的功能范围、对 PRD v4 的修订与验收标准。
- [MVP 循环状态](docs/mvp/cycle-state.md)：loop-dev-cycle 当前阶段、决策日志、分工与待解决问题；新会话先读此文件。
- [MVP 审查记录](docs/mvp/review-log.md)：每个功能分支合并前的审查结论、问题清单与处理状态。
- [视觉稿](design/README.md)：两版 HTML 视觉稿与截图；已选定 B「厨房手账」，含设计参数表。

个人或敏感资料放在 `private/`，不应提交到 Git。项目协作规则统一维护在 [AGENTS.md](AGENTS.md)；Claude Code 通过根目录的 `CLAUDE.md` 导入同一份规则。

## 项目级 Skills

Codex 从 `.agents/skills/` 加载项目级 skill。Claude Code 的 `.claude/skills/` 中使用同名符号链接指向这些目录，避免维护两份内容。

- [loop-dev-cycle](.agents/skills/loop-dev-cycle/SKILL.md)：软件工程循环。
- [lexmount-fetch](.agents/skills/lexmount-fetch/SKILL.md)：使用 Lexmount 抓取网页；脚本位于同目录的 `fetch.mjs`，运行条件见 skill 文件。

## 开发环境与命令

`app/` 是 Qt 6 CMake 工程，同一套源码构建 macOS 桌面开发版（日常开发与单元测试）和 Android APK（验收）。目录规则见 [FHS.md](FHS.md)。

### 前置条件

- macOS（已验证 12.7 Intel x86_64），cmake、ninja、git。
- Qt 6.8.2 安装在 `~/Qt/6.8.2/`，含 `macos`、`android_x86_64`、`android_arm64_v8a` 套件。脚本显式调用其中的 `qt-cmake`，不依赖 PATH 中的 `qmake`/`qtpaths`。
- Android SDK `~/Library/Android/sdk`：platform android-34、build-tools 34.0.0、NDK 26.1.10909125、platform-tools、emulator；AVD `fm_api34`（x86_64，API 34）。
- JDK 17。首次 Android 构建时 Gradle 会下载依赖到 `~/.gradle`。
- 路径可用环境变量覆盖：`WHILEFRESH_QT_ROOT`、`ANDROID_SDK_ROOT`、`ANDROID_NDK_ROOT`、`JAVA_HOME`、`WHILEFRESH_CACHE_DIR`、`WHILEFRESH_AVD`（见 `scripts/env.sh`）。

### 命令

| 用途 | 命令 |
| --- | --- |
| 下载第三方依赖（KDAB android_openssl，固定 commit，存到 `~/.cache/whilefresh/`） | `scripts/fetch-deps.sh` |
| 构建桌面版（Debug；`--with-key` 注入密钥，`--release` 构建 Release） | `scripts/build-desktop.sh` |
| 运行桌面版 | `build/desktop/whilefresh` |
| 单元测试 | `ctest --test-dir build/desktop --output-on-failure` |
| 构建 Android APK（debug 签名；`--no-key` 不注入密钥） | `scripts/build-android.sh x86_64` 或 `scripts/build-android.sh arm64-v8a` |
| 启动模拟器并等待开机（`--headless` 无窗口） | `scripts/run-emulator.sh --headless` |
| 安装并启动 APK（ABI 默认取设备主 ABI；多设备时设 `ANDROID_SERIAL`） | `scripts/install-android.sh` |
| 关闭模拟器 | `~/Library/Android/sdk/platform-tools/adb emu kill` |

APK 输出为 `build/android-<abi>/whilefresh-<abi>-debug.apk`。编辑器诊断用的 `compile_commands.json` 生成在各构建目录下（如 `build/desktop/compile_commands.json`）。

### 密钥注入

`scripts/build-android.sh` 从 macOS 钥匙串读取服务名为 `landing-project-deepseek` 的密钥，通过环境变量交给 CMake，生成到构建目录的 `generated/api-config.h`，源码目录和 `CMakeCache.txt` 中都不出现。钥匙串中没有该条目时照常构建，App 显示"识别服务：未配置"。

- 桌面版默认不注入密钥，需要时加 `--with-key`。注意 `scripts/build-desktop.sh --release --with-key` 会把密钥编进 Release 二进制，不要分发这样的产物。
- 密钥只在 CMake 配置阶段读取。需要密钥或更换密钥时，一律重新运行 `scripts/` 下的构建脚本，不要在构建目录里直接运行 `ninja`：ninja 自动重新配置时拿不到脚本导出的环境变量，会生成无密钥构建，CMake 会给出"本次为无密钥构建"的警告。
- 修改了 CMakeLists.txt 后同样用构建脚本重新构建。

添加密钥：

```sh
security add-generic-password -s landing-project-deepseek -a "$USER" -w   # 按提示输入，不要写在命令行里
```

### 验证 HTTPS

Debug 构建默认不主动连接 DeepSeek（T8a 仅模拟界面）。仅在单独验证 HTTPS 时，显式设置 `WHILEFRESH_TLS_SMOKE=1` 才对 `https://api.deepseek.com/models` 发一次不带密钥的 GET；预期状态 401，日志前缀 `WhileFresh tls-smoke:`。Android 上需在可注入该进程环境变量的测试环境中单独执行，不作为界面截图流程的一部分。

```sh
# 桌面版：仅手动 HTTPS 探针时运行
WHILEFRESH_TLS_SMOKE=1 build/desktop/whilefresh
```

### 离线数据工具（Python）

`tools/` 是用 uv 管理的 Python 工具包，用于生成随 App 内置的只读数据（输出到 `app/assets/data/`，生成物需提交）。

```bash
# 生成菜谱数据：下载固定版本的 HowToCook 到 ~/.cache/whilefresh/howtocook/，输出 app/assets/data/recipes.json
uv run --project tools python -m whilefresh_tools.recipes build

# 标准食材词表：由人工维护的源数据生成 app/assets/data/vocabulary.json（不调用 AI）
uv run --project tools python -m whilefresh_tools.vocabulary build

# 校验词表、原料映射和 recipes.json（名称 / 别名唯一、parent 无环、每个菜谱原料都有映射等）
uv run --project tools python -m whilefresh_tools.vocabulary validate

# 复核报告：规模统计与固定种子的映射抽样（Markdown，输出到标准输出）
uv run --project tools python -m whilefresh_tools.vocabulary report

# 一次性：调用 DeepSeek 生成词表和原料映射的草稿（需钥匙串中有 landing-project-deepseek）
uv run --project tools python -m whilefresh_tools.vocabulary draft

# 保质期参考表：由 foodkeeper-map.json + FoodKeeper 快照生成 app/assets/data/shelf-life.json（不联网）
uv run --project tools python -m whilefresh_tools.shelf_life build

# 校验映射与参考表（键在词表中、天数合理、FoodKeeper ID 存在、冷冻不短于冷藏、生成物未陈旧）
uv run --project tools python -m whilefresh_tools.shelf_life validate

# 参考表复核报告：覆盖率、无参考值的高频食材、固定种子的抽样对照（Markdown，输出到标准输出）
uv run --project tools python -m whilefresh_tools.shelf_life report

# 一次性：调用 DeepSeek 生成"标准名 → FoodKeeper 条目"的草稿（需钥匙串中有 landing-project-deepseek）
uv run --project tools python -m whilefresh_tools.shelf_life draft

# 运行工具测试
uv run --project tools pytest
```

- 同一版本的输入，两次生成的 `recipes.json` 应逐字节一致；改动解析规则后需重新生成并一起提交。
- `recipes.json` 每个原料项带 `canonical`（需求组列表：组内是备选标准名、任一满足即可，组间全部需要；工具和忽略项为空列表，匹配规则见复核报告）。它由 `tools/whilefresh_tools/vocabulary/ingredient-map.json` 决定；菜谱原料写法不在映射里时，`recipes build` 会报错并列出缺失项。改映射或词表后依次运行 `recipes build`、`vocabulary build`、`vocabulary validate`，并一起提交生成物。
- 词表源数据是 `vocabulary-source.json`（人工维护，每项一行）和 `ingredient-map.json`，都在 `tools/whilefresh_tools/vocabulary/`。AI 只用于 `draft` 生成初稿：响应缓存在 `~/.cache/whilefresh/vocabulary/`（可用 `WHILEFRESH_CACHE` 改位置），草稿输出到缓存目录下的 `draft/`，由人工审阅后手工复制到源数据位置；之后的重建、校验和测试都不联网、不调用 AI。密钥只在进程内读取，不会被打印或缓存。
- 词表字段约定：`name`（标准名）、`category`（品类代码）、`aliases[]`（全表唯一）为识别层读取的三个固定字段；`parent` 表示"持有子项即可满足父项"；`pantry_default` 默认算家里常备；`bulk_default` 默认存量多。复核记录见 [词表复核报告](docs/mvp/notes/vocabulary-review.md)。
- 保质期参考表的数据源是 USDA FoodKeeper v128 快照（`tools/sources/foodkeeper/`，附 `SOURCE.md`，CC0）。映射源数据是 `tools/whilefresh_tools/shelf_life/foodkeeper-map.json`（标准名 → FoodKeeper 条目 ID，每项一行，含依据说明，人工维护）；`draft` 的草稿和响应缓存在 `~/.cache/whilefresh/shelf-life/`，由人工审阅后复制到源数据位置。取值规则与换算（周 × 7、月 × 30、年 × 365）见 `SOURCE.md`。没有对应条目的食材不列入参考表，由上层回退到 AI 判断。复核记录见 [保质期参考表复核报告](docs/mvp/notes/shelf-life-review.md)。
- 下载失败时可设置 `WHILEFRESH_CACHE` 指定缓存目录，或把同版本源码压缩包手动放到 `<缓存目录>/snapshot.tar.gz`。

### 识别层与录制工具

识别层在 `app/src/recognition/`：`RecognitionService` 接口，`DeepSeekRecognizer`（真实服务）、`ReplayRecognizer`（回放录制）、`DemoRecognizer`（演示预置）三种实现。`ctest` 默认全部离线（假服务器 + 合成录制），不计费。

`recognize-cli` 是仅桌面可用的命令行程序，**调用真实 DeepSeek（计费）**，需要带密钥构建，用完建议重新用无密钥方式构建：

```sh
scripts/build-desktop.sh --with-key
build/desktop/tools/recognize-cli --vocab app/assets/data/vocabulary.json \
    [--kind screenshot|receipt] [--date YYYY-MM-DD] [--no-thinking] [--record <文件>] <图片>...
build/desktop/tools/recognize-cli --vocab <词表> --text <订单文字文件>
```

只在终端打印结果摘要（耗时、token、商品清单；回复被截断时有警告）。默认保持 deepseek-flash 的思考开启（已验证的识别效果基于此），`--no-thinking` 用于对比。`--record` 保存录制（请求摘要 + 原始响应，文件权限 0600）：仓库内除 `private/` 外的路径会被拒绝；合成样本的录制先写到仓库外（如 `/tmp`），确认不含真实数据后再复制到 `test-data/recordings/`；真实样例（`private/bill/`）的录制只能放 `private/recordings/`，不进仓库。在线识别是手动验证，不属于 ctest。

### Android 导入调试

Debug 构建的"我的"页有调试区：选图、拍小票、分享测试文字、复制测试文字，并显示最近一次收到的导入摘要。每次导入在日志中打一行，前缀固定为 `WhileFresh import:`（只含类型、来源、数量和日期来源，不含文字内容）。设计与已知限制见 [docs/mvp/notes/android-integration.md](docs/mvp/notes/android-integration.md)。

```sh
adb=~/Library/Android/sdk/platform-tools/adb
act=io.github.mechjiang.whilefresh/.WhileFreshActivity

# 放一张自己生成的图片到相册并让 MediaStore 收录，查出其 _id
$adb push synthetic.png /sdcard/Pictures/synthetic.png
$adb shell am broadcast -a android.intent.action.MEDIA_SCANNER_SCAN_FILE -d file:///sdcard/Pictures/synthetic.png
$adb shell content query --uri content://media/external/images/media --projection _id:_display_name:datetaken:date_added

# 分享图片（先 am force-stop 包名即测冷启动）。-d 传同一 URI，--grant-read-uri-permission 才会授权
uri=content://media/external/images/media/<_id>
$adb shell am start -a android.intent.action.SEND -t image/png --grant-read-uri-permission -d $uri --eu android.intent.extra.STREAM $uri -n $act

# 分享文字
$adb shell am start -a android.intent.action.SEND -t text/plain --es android.intent.extra.TEXT "'订单：西红柿 500g'" -n $act

$adb logcat -d | grep 'WhileFresh import:'
# 期望：WhileFresh import: kind=images origin=share count=1 capturedAtSource=media-added
```

`am` 不能传 `Uri` 列表；`ACTION_SEND_MULTIPLE` 用系统"文件"App 多选后分享到"趁鲜"来测。

### 界面资源与模拟预览（T8a）

QML 界面位于 `app/qml/`：`Theme.qml` 管颜色/字体/尺寸；`components/` 放可复用控件；`pages/` 放页面；`mock/MockData.qml` 是唯一演示数据源。**界面目前只接模拟数据**，各页持续标示「演示 · 界面预览」；T8b 再替换为视图模型与真实操作。无需 API 密钥，不向 DeepSeek 发送内容。桌面验证：`scripts/build-desktop.sh`，再运行 `ctest --test-dir build/desktop --output-on-failure`；Android 用 `scripts/build-android.sh x86_64`、`scripts/run-emulator.sh --headless`、`scripts/install-android.sh x86_64`。

字体子集来自 [Noto Serif CJK v2.003（SC Black）](https://github.com/notofonts/noto-cjk/raw/Serif2.003/Serif/SubsetOTF/SC/NotoSerifSC-Black.otf)，SIL OFL 1.1，源文件 SHA-256 `91dde57622845beeb663f300813999febdb7fcd5e451bde199d7c954a9740cb4`；许可见 `app/assets/fonts/OFL.txt`。用 `cd tools && uv run --group assets python -m whilefresh_tools.assets font` 重新下载、校验并生成字体子集；`cd tools && uv run --group assets python -m whilefresh_tools.assets texture` 重建纸纹；工具依赖仅装在项目本地 uv 环境。Tabler 图标（MIT）来自 `design/icons/`，应用内许可见 `app/assets/icons/LICENSE-tabler`。

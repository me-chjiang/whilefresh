# 开发文档

本文说明趁鲜（WhileFresh）的开发环境、构建命令、数据工具与调试方法。产品概览见 [README.md](../README.md)，目录规则见 [FHS.md](../FHS.md)。

## 开发环境与命令

`app/` 是 Qt 6 CMake 工程，同一套源码构建 macOS 桌面开发版（日常开发与单元测试）和 Android APK（验收）。目录规则见 [FHS.md](../FHS.md)。

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
| 构建签名 Release APK（签名密钥库与口令见 [mvp/trial-release.md](mvp/trial-release.md)） | `scripts/build-android.sh arm64-v8a --release` |
| 启动模拟器并等待开机（`--headless` 无窗口） | `scripts/run-emulator.sh --headless` |
| 安装并启动 APK（ABI 默认取设备主 ABI；多设备时设 `ANDROID_SERIAL`） | `scripts/install-android.sh` |
| 关闭模拟器 | `~/Library/Android/sdk/platform-tools/adb emu kill` |

`run-emulator.sh` 使用冷启动和 SwiftShader，并关闭模拟器 Vulkan，以避开本机快照锁与 ColorBuffer 初始化失败。受限执行环境还须允许模拟器写入 `~/Library/Caches/TemporaryItems/avd/running/`；若默认 `fm_api34` 的数据目录仍被占用，可设置 `ANDROID_AVD_HOME` 和 `WHILEFRESH_AVD` 指向独立 AVD。

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

Debug 构建默认不主动连接 DeepSeek。仅在单独验证 HTTPS 时，显式设置 `WHILEFRESH_TLS_SMOKE=1` 才对 `https://api.deepseek.com/models` 发一次不带密钥的 GET；预期状态 401，日志前缀 `WhileFresh tls-smoke:`。Android 上需在可注入该进程环境变量的测试环境中单独执行，不作为界面截图流程的一部分。

```sh
# 桌面版：仅手动 HTTPS 探针时运行
WHILEFRESH_TLS_SMOKE=1 build/desktop/whilefresh
```

### 离线数据工具（Python）

独立实验 `recipe-sources-xiachufang-api/` 移植 ruter/xiachufang-api 的公开网页解析接口，使用现代 Python 依赖并保留菜谱 YAML 契约。在该目录执行 `uv sync --group dev`、`uv run pytest`；真实站点验证使用 `uv run pytest -m live --live`，可能受站点限流影响。CLI 和 HTTP 运行方式见该仓库 README。

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
- 词表字段约定：`name`（标准名）、`category`（品类代码）、`aliases[]`（全表唯一）为识别层读取的三个固定字段；`parent` 表示"持有子项即可满足父项"；`pantry_default` 默认算家里常备；`bulk_default` 默认存量多。复核记录见 [词表复核报告](mvp/notes/vocabulary-review.md)。
- 保质期参考表的数据源是 USDA FoodKeeper v128 快照（`tools/sources/foodkeeper/`，附 `SOURCE.md`，CC0）。映射源数据是 `tools/whilefresh_tools/shelf_life/foodkeeper-map.json`（标准名 → FoodKeeper 条目 ID，每项一行，含依据说明，人工维护）；`draft` 的草稿和响应缓存在 `~/.cache/whilefresh/shelf-life/`，由人工审阅后复制到源数据位置。取值规则与换算（周 × 7、月 × 30、年 × 365）见 `SOURCE.md`。没有对应条目的食材不列入参考表，由上层回退到 AI 判断。复核记录见 [保质期参考表复核报告](mvp/notes/shelf-life-review.md)。
- 下载失败时可设置 `WHILEFRESH_CACHE` 指定缓存目录，或把同版本源码压缩包手动放到 `<缓存目录>/snapshot.tar.gz`。

#### 合成测试数据与识别回归

`tools/whilefresh_tools/synthetic/` 离线生成 23 个虚构样本（13 张买菜截图、6 张小票照片、4 段订单文字）及逐样本 JSON 标准答案。截图仿四种常见 App 信息布局，不使用真实平台商标或 logo；小票含旋转、透视、模糊、阴影/不均匀光照、褶皱、部分遮挡等变体。数据均为虚构，不包含真实收货人、手机号和地址。生成命令只读取正式词表，不调用 AI；需要本机 Google Chrome 与 tools 项目中的 Pillow（`uv` 安装），不安装全局依赖。

```sh
# 截图和小票由 Chrome 无头渲染 HTML，JPEG 照片由 Pillow 后处理；文字直接生成
uv run --project tools python -m whilefresh_tools.synthetic generate
# 仅刷新 HTML/TXT/标准答案（不启动 Chrome），或只生成一个样本
uv run --project tools python -m whilefresh_tools.synthetic generate --no-render
uv run --project tools python -m whilefresh_tools.synthetic generate --id cart-fresh-long-01

# 离线评估已有合成样本录制，不产生网络请求
uv run --project tools python -m whilefresh_tools.synthetic evaluate --out docs/mvp/notes/recognition-baseline.md
uv run --project tools pytest
```

产物位于 `test-data/synthetic/{screenshots,receipts,texts}/`（同名图片或 TXT、HTML 排版源与 JSON 标准答案），合成录制位于 `test-data/recordings/synthetic/`。固定场景/随机种子保证重复生成 HTML 与 JSON 逐字节相同；PNG 在不同 Chrome 版本下可能有细微差别，JPEG 另受 Pillow 版本影响。小票遮挡处不可辨认的两行在标准答案中标记 `optional: true`，评估时不计入指标分母。`quantity_text` 指可见规格，`count` 指购买件数；订单时间按页面原文保留，不换算相对日期。

**计费，仅在用户批准后运行**，先带密钥构建并逐样本调用 CLI 一次；驱动器写 `/tmp/whilefresh-t7-synthetic/`，调用前留下 `.attempted` 防止重复计费，默认最多启动 30 个样本（失败也计入）。一个样本可能因图片切片或服务端重试产生多次 HTTP 请求；录制中的响应数只统计成功保存的结果。需人工检查该目录的录制内容确为合成数据后再收集：

```sh
scripts/build-desktop.sh --with-key
uv run --project tools python -m whilefresh_tools.synthetic.regress run
uv run --project tools python -m whilefresh_tools.synthetic.regress status
# 检查 /tmp/whilefresh-t7-synthetic/*.json 仅包含虚构样本的返回；不要复制私人真实样例
uv run --project tools python -m whilefresh_tools.synthetic.regress collect
uv run --project tools python -m whilefresh_tools.synthetic evaluate --out docs/mvp/notes/recognition-baseline.md
scripts/build-desktop.sh  # 用完密钥后恢复为不带密钥的桌面构建
```

采集器把 `recognize-cli --record` 原样写入 `/tmp`，通过校验后仅在仓库内副本补一个 `elapsed_ms`（取自同次 CLI 摘要）；原生 format-1 录制本身没有耗时字段，回放器忽略这个补充字段。报告统计标准名+勾选匹配召回/精确、类别、赠品、日期、件数、规格、token 与耗时；首版不设阈值。**不要用该合成基线代替 `private/` 真实样例的验收**。

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

Debug 构建的"我的"页有调试区：选图、拍小票、分享测试文字、复制测试文字，并显示最近一次收到的导入摘要。每次导入在日志中打一行，前缀固定为 `WhileFresh import:`（只含类型、来源、数量和日期来源，不含文字内容）。设计与已知限制见 [Android 集成说明](mvp/notes/android-integration.md)。

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

### 核心、数据与应用服务

- `app/src/core/` 只依赖 QtCore：分开的购买/入库/生产/包装到期日期、拍摄日基准的日期解析、带来源的保质期、注入时钟的新鲜度、常备/存量建议、需求组匹配与待买文本。
- `app/src/data/`：SQLite `Database` + `Store`（`PRAGMA user_version` 版本迁移），以及加载一次的 `BuiltinData`。正式词表、菜谱、FoodKeeper 表嵌入 `:/whilefresh/data`；找不到参考表按无参考值处理。
- `app/src/ui/{import,kitchen}-service.*` 是普通 C++ 应用服务，不依赖 QML；可替换识别器，用户确认草稿才入库，常备建议/本地记忆需要单独确认。接线、日期语义、迁移和文件删除细节见 [核心领域实现说明](mvp/notes/core-domain.md)。
- **FoodKeeper 不等于从生产日起的寿命**：参考表只在购买日期已知且用户确认适用条件后计算到期日；否则保留参考天数、显示待确认，不改用较长的 AI 值掩盖未知状态。`basis` 只保留作展示，不据其字符串推断逻辑。

使用已有命令验证，不调用真实 DeepSeek：

```sh
scripts/build-desktop.sh
ctest --test-dir build/desktop --output-on-failure
# 只跑核心、数据与应用服务相关的九个测试套件（其余套件仍应在提交前全部运行）
ctest --test-dir build/desktop --output-on-failure -R 'date-text|shelf-life|freshness|recipe-matcher|household-rules|database-test|store-test|builtin-data|application-service'
scripts/build-android.sh x86_64 --no-key
```

`builtin-data-test` 用实际 372 道菜、544 项词表、290 项参考数据验证匹配和保质期；`application-service-test` 用 Replay/Demo 识别器验证导入→草稿→编辑→确认→入库、取消与图片删除、重复导入、事务回滚、建议确认及清冰箱/待买。测试数据库与图片只写临时目录。

### MVP 体验打磨验证

若模拟器已有试用签名安装包，使用 `scripts/build-android.sh x86_64 --no-key --sign-debug` 构建同签名 Debug 包，再用安装脚本覆盖；签名口令仍只由构建脚本读取钥匙串。避免为解决签名冲突卸载并丢失数据。

清冰箱将选择、轻量覆盖预览与确认后的结果分开；候选匹配和覆盖解释由 C++ 提供，QML 不计算业务相关性。标签页按需创建并保留独立导航栈，模式切换清空页面状态。图片暂不纳入本轮。

验证复用上面的桌面构建 / CTest、`uv run --project tools pytest` 和 Android 构建脚本。Qt 6.8.2 的 `qmllint`、`qmlprofiler` 用于检查 QML 与分析创建/绑定耗时；Android 用 `adb shell input` 操作，`adb exec-out screencap -p` 取图、`adb logcat` 检查错误，Debug 构建的导航日志 `WhileFresh navigation: ... first-frame-ms=N` 记录动作到下一次换帧，区分首次与缓存切页；它不是完整可交互耗时，不能冒充 Release 真机结果。日志和截图放构建目录或 `/tmp`，不提交个人数据。外部 skill 仅静态检查公开索引，不执行第三方指令。

### 界面资源与模拟预览

QML 界面位于 `app/qml/`：`Theme.qml` 管颜色/字体/尺寸；`components/` 放可复用控件；`pages/` 放页面；页面通过 `App`（`app/src/ui/app-model.*`，QML 单例）读取真实服务。「我的」中的演示模式使用独立的 `demo.db`、预设识别结果和示例食材，界面顶部持续显示「演示模式」，可断网体验；关闭后回到真实数据。真实识别需构建脚本注入 DeepSeek 密钥，未配置时启动会提示。桌面验证：`scripts/build-desktop.sh`，再运行 `ctest --test-dir build/desktop --output-on-failure`；Android 用 `scripts/build-android.sh x86_64`、`scripts/run-emulator.sh --headless`、`scripts/install-android.sh x86_64`。

字体子集来自 [Noto Serif CJK v2.003（SC Black）](https://github.com/notofonts/noto-cjk/raw/Serif2.003/Serif/SubsetOTF/SC/NotoSerifSC-Black.otf)，SIL OFL 1.1，源文件 SHA-256 `91dde57622845beeb663f300813999febdb7fcd5e451bde199d7c954a9740cb4`；许可见 `app/assets/fonts/OFL.txt`。用 `cd tools && uv run --group assets python -m whilefresh_tools.assets font` 重新下载、校验并生成字体子集；`cd tools && uv run --group assets python -m whilefresh_tools.assets texture` 重建纸纹；工具依赖仅装在项目本地 uv 环境。Tabler 图标（MIT）来自 `design/icons/`，应用内许可见 `app/assets/icons/LICENSE-tabler`。

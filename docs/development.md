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
- 词表字段约定：`name`（标准名）、`category`（品类代码）、`aliases[]`（全表唯一）、`parent` 由 App 的 `core::IngredientCatalog` 读取（识别和匹配共用这一份）；`parent` 表示"持有子项即可满足父项"；`pantry_default` 默认算家里常备；`bulk_default` 默认存量多。复核记录见 [词表复核报告](mvp/notes/vocabulary-review.md)。
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

**计费，仅在用户批准后运行**，先带密钥构建并逐样本调用 CLI 一次；驱动器写 `/tmp/whilefresh-t7-synthetic/`，调用前留下 `.attempted` 防止重复计费，达到 30 个底层 HTTP 响应就停。需人工检查该目录的录制内容确为合成数据后再收集：

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

### AI 层

`app/src/ai/`（`whilefresh-ai`，只链接 core 与 Qt::Network，CMake 中有检查）是识别和之后所有 AI 服务共用的 DeepSeek 传输层：

- `ChatClient`：一次 chat-completions 调用的异步接口（请求 id、可取消）。请求为 `ChatRequest`（system/user 消息，user 可含 image_url、JSON 模式、max_tokens、temperature、thinking）；大请求可在工作线程先 `encodeChatRequest` 再 `sendEncoded`。结果为正文、finish_reason、token 用量、耗时、截断标记和原始响应；失败为带类型的 `ChatError`（`ErrorKind` 与 HTTP/网络错误分类也在这里）。
- `DeepSeekChatClient`：每次尝试空闲超时 150 s，整次调用（含重试）总时限 240 s，单个请求可用 `ChatRequest::idleTimeoutMs` / `deadlineMs` 覆盖；只有 HTTP 5xx 和服务器断开连接（RemoteHostClosed）重试一次，协议错误、超时和 4xx 不重试；`finish_reason=length` 时作为成功返回部分正文并标记 `truncated`。日志只记大小、数量、耗时、token、状态码。密钥在发送时向 `CredentialProvider` 取，没有密钥直接报 `NotConfigured`，不发请求。
- `CredentialProvider`（`api-keys.h`；文件名避开 `.gitignore` 的 `*credential*`）：`key(Service)`，Service 为 DeepSeek / Lexmount。现有 `BuiltinCredentialProvider`（构建时注入的密钥）和 `StaticCredentialProvider`（测试、工具）；`AppModel` 持有提供者和应用级 `DeepSeekChatClient`，运行时用 `AppModel::setCredentialProvider()` 更换（同时通知 QML 的 `recognitionConfigured`）。
- `RecordedChatClient` / `RecordingChatClient`：传输层回放与录制。录制格式 2 只存请求摘要和请求体 SHA-256，不存提示词和图片；也能读 `recognize-cli` 的格式 1（可带 `requestDigests`）。带摘要的录制只回答相同的请求，提示词或输入变了会失败，需要重新录制。
- `parseJsonContent(ChatResult)`：把 JSON 模式的回答正文（可带 ```json 围栏）解析成对象，失败时返回 BadResponse，供纯文本 JSON 服务使用。
- `SystemPromptBuilder`：基础文本 + 按顺序追加的段落，偏好摘要永远放最后；空段落省略。
- `IngredientClassifier`（T14）：把本地对不上词表的商品名一次批量发给 DeepSeek 归类。纯文本 JSON 模式、关闭思考、空闲 30 s / 总时限 60 s，一次最多 80 个名称；system 中附分层词表（`IngredientCatalog::hierarchyText()`），每个回答都用词表校验，词表外的名称丢弃并计数，`none` 保持未归类。只依赖 core，词表由调用方传入。

测试在 `app/tests/ai/`，全部离线：`ingredient-classifier-test` 覆盖请求形状、回答校验、取消、失败，并回放 `test-data/recordings/classifier/synthetic-names.json`（没有该录制时跳过，见下文"重新录制归类"）；`deepseek-chat-client-test`（本地假 HTTP 服务器 `fake-http-server.h`，与识别测试共用）覆盖重试、超时、截断、无密钥、日志；`chat-types-test` 覆盖请求编码与 `parseJsonContent`；`chat-recording-test` 覆盖回放顺序、摘要校验、取消不占用录制、录制→写入→读取→回放，以及读取 `test-data/recordings/` 与演示预置的格式 1 录制；`system-prompt-builder-test` 覆盖段落顺序。

```sh
ctest --test-dir build/desktop --output-on-failure -R 'chat|system-prompt|deepseek-recognizer'
```

### 词表与名称归类

- 正式词表 `app/assets/data/vocabulary.json`（581 项，含 `parent` 上下位）由 `IngredientCatalog` 读取，识别请求、匹配和归类共用。识别提示词 v3 用 `hierarchyText()` 按品类分组列出词表，如"猪肉 > 五花肉、前腿肉、里脊"，要求取最细词条、`raw_name` 保留原文；约 3.9k 字符（v2 为 3.5k）。
- `IngredientCatalog::resolveFinest(标题)`：精确名称 / 别名优先；否则按空格、标点和数字切词，取词内包含的名称或别名。单字名称只在词尾时算数（藕粉 ≠ 莲藕）；同时命中父项和子项时丢掉父项；词内取结束位置最靠后的（中文商品名中心词在后：五香牛肉干 → 牛肉干），再取较长的；跨词取最长的（速冻水饺 猪肉大葱 → 速冻水饺）。
- `VocabularyResolver`（core）：用户确认过的映射 → 识别给出的标准名（标题里有它的子项，或它是上位词而标题里有同品类更细的词条时改用后者）→ `resolveFinest(原文)` → 未归类。未归类的名称（不含非食品）由 `ImportService` 收集后交 `IngredientClassifier` 一次归类；结果在核对页标"AI 归类"。用户确认入库时，保留的 AI 归类和用户改过的名称写入 `NameMappingStore`，下次同名直接命中。
- `NameMappingStore`：core 中的接口，测试用 `MemoryNameMappingStore`；App 使用 `data::SqliteNameMappingStore`（`name_mappings` 表，由 T15 的迁移 v5 创建；表不存在时查不到、写入失败只记警告）。
- 归类用例 `test-data/vocabulary/resolve-cases.json`（55 条，人工复核）由 `catalog-test` 逐条对正式词表运行；改词表或规则后必须全部通过，不能通过的要在用例里写明原因，不得悄悄放宽。

```sh
ctest --test-dir build/desktop --output-on-failure -R 'catalog|vocabulary-resolver|ingredient-classifier|name-mappings|application-service'
```

重新录制归类（**调用真实 DeepSeek，计费**；只用合成名称，录制先写到仓库外）：

```sh
scripts/build-desktop.sh --with-key
printf '%s\n' 冰鲜鸡全腿 '牛霖 500g' '鲜毛肚 200g' '花胶 100g' '罗汉果 1个' '黑松露酱 80g' '藕粉 500g' '洗衣液 2kg' '电热水壶 1.5L' > /tmp/names.txt
build/desktop/tools/recognize-cli --vocab app/assets/data/vocabulary.json --classify /tmp/names.txt --record /tmp/synthetic-names.json
# 检查录制只含请求摘要与回答、不含密钥后复制到 test-data/recordings/classifier/synthetic-names.json
scripts/build-desktop.sh  # 恢复无密钥构建
```

录制带请求摘要，归类提示词、词表或名称列表变化后回放会失败，需要重新录制（名称列表须与 `ingredient-classifier-test` 中的 `kRecordedNames` 一致）。

### 识别层与录制工具

识别层在 `app/src/recognition/`：`RecognitionService` 接口，`DeepSeekRecognizer`（真实服务）、`ReplayRecognizer`（回放录制）、`DemoRecognizer`（演示预置）三种实现。`DeepSeekRecognizer` 负责图片处理、切片、分批、构造请求和解析结果，经注入的 `ai::ChatClient` 发送；其测试用 `RecordedChatClient` 代替网络，并验证合成样本的格式 1 录制经传输层回放与 `ReplayRecognizer` 的结果完全一致。`ReplayRecognizer` / `DemoRecognizer` 仍在识别层直接回放录制：演示和回放时手上的图片不是录制时的图片，重新切片分批后请求数和批次范围对不上。`ctest` 默认全部离线（假服务器 + 合成录制），不计费。

`recognize-cli` 是仅桌面可用的命令行程序，**调用真实 DeepSeek（计费）**，需要带密钥构建，用完建议重新用无密钥方式构建：

```sh
scripts/build-desktop.sh --with-key
build/desktop/tools/recognize-cli --vocab app/assets/data/vocabulary.json \
    [--kind screenshot|receipt] [--date YYYY-MM-DD] [--no-thinking] [--record <文件>] <图片>...
build/desktop/tools/recognize-cli --vocab <词表> --text <订单文字文件>
# 归类服务：每行一个商品名，一次请求（--record / --replay 用 ai 录制格式 2）
build/desktop/tools/recognize-cli --vocab <词表> --classify <名称文件>
# 离线回放：同样的输入 + 录制文件，走同一条请求路径，不需要密钥、不联网
build/desktop/tools/recognize-cli --vocab app/assets/data/vocabulary.json \
    --replay test-data/recordings/synthetic/cart-fresh-01.json test-data/synthetic/screenshots/cart-fresh-01.png
```

只在终端打印结果摘要（耗时、token、商品清单；回复被截断时有警告）。默认保持 deepseek-flash 的思考开启（已验证的识别效果基于此），`--no-thinking` 用于对比。`--record` 保存录制（格式 1：请求摘要、每个请求体的 SHA-256 + 原始响应，文件权限 0600）：仓库内除 `private/` 外的路径会被拒绝；合成样本的录制先写到仓库外（如 `/tmp`），确认不含真实数据后再复制到 `test-data/recordings/`；真实样例（`private/bill/`）的录制只能放 `private/recordings/`，不进仓库。在线识别是手动验证，不属于 ctest。

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
- 第二轮（T15）：迁移 v5（食材种类 / 用量 / 忽略到期、导入软删除与跳过记录、导入续传表）；`core/quantity.*` 用量解析、`core/fridge-groups.*` 种类判定与冰箱分组排序、`core/resumable-job.*` 续传状态机。删除导入、清空历史、跳过重复、调料入库与续传的设计见 [食材模型与导入续传说明](mvp/notes/item-model.md)。
- **FoodKeeper 不等于从生产日起的寿命**：参考表只在购买日期已知且用户确认适用条件后计算到期日；否则保留参考天数、显示待确认，不改用较长的 AI 值掩盖未知状态。`basis` 只保留作展示，不据其字符串推断逻辑。

使用已有命令验证，不调用真实 DeepSeek：

```sh
scripts/build-desktop.sh
ctest --test-dir build/desktop --output-on-failure
# 只跑核心、数据与应用服务相关的测试套件（其余套件仍应在提交前全部运行）
ctest --test-dir build/desktop --output-on-failure -R 'date-text|shelf-life|freshness|recipe-matcher|household-rules|quantity|fridge-groups|resumable-job|database-test|store-test|builtin-data|application-service'
scripts/build-android.sh x86_64 --no-key
```

`builtin-data-test` 用实际 372 道菜、581 项词表、290 项参考数据验证匹配和保质期；`application-service-test` 用 Replay/Demo 识别器验证导入→草稿→编辑→确认→入库、取消与图片删除、重复导入与跳过记录、软删除与清空历史、调料/常备食材入库、扔掉与忽略、事务回滚、建议确认及清冰箱/待买；导入续传用 `DeepSeekRecognizer` + `ai::RecordedChatClient` 验证第 2 批失败后换新服务只重发第 2 批及以后。测试数据库与图片只写临时目录。

### 界面资源与模拟预览

QML 界面位于 `app/qml/`：`Theme.qml` 管颜色/字体/尺寸；`components/` 放可复用控件；`pages/` 放页面；页面通过 `App`（`app/src/ui/app-model.*`，QML 单例）读取真实服务。「我的」中的演示模式使用独立的 `demo.db`、预设识别结果和示例食材，界面顶部持续显示「演示模式」，可断网体验；关闭后回到真实数据。真实识别需构建脚本注入 DeepSeek 密钥，未配置时启动会提示。桌面验证：`scripts/build-desktop.sh`，再运行 `ctest --test-dir build/desktop --output-on-failure`；Android 用 `scripts/build-android.sh x86_64`、`scripts/run-emulator.sh --headless`、`scripts/install-android.sh x86_64`。

字体子集来自 [Noto Serif CJK v2.003（SC Black）](https://github.com/notofonts/noto-cjk/raw/Serif2.003/Serif/SubsetOTF/SC/NotoSerifSC-Black.otf)，SIL OFL 1.1，源文件 SHA-256 `91dde57622845beeb663f300813999febdb7fcd5e451bde199d7c954a9740cb4`；许可见 `app/assets/fonts/OFL.txt`。用 `cd tools && uv run --group assets python -m whilefresh_tools.assets font` 重新下载、校验并生成字体子集；`cd tools && uv run --group assets python -m whilefresh_tools.assets texture` 重建纸纹；工具依赖仅装在项目本地 uv 环境。Tabler 图标（MIT）来自 `design/icons/`，应用内许可见 `app/assets/icons/LICENSE-tabler`。

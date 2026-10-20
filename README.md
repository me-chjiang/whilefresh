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

Debug 构建启动时会对 `https://api.deepseek.com/models` 发一次不带密钥的 GET（不计费），收到任何 HTTP 状态码（预期 401）即说明 TLS 正常。日志前缀固定为 `WhileFresh tls-smoke:`。

```sh
# 桌面版：日志直接输出到终端
build/desktop/whilefresh

# Android：安装启动后查看 logcat
~/Library/Android/sdk/platform-tools/adb logcat -d | grep 'WhileFresh tls-smoke:'
# 期望看到：WhileFresh tls-smoke: ok http-status=401
```

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

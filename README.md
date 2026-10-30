# 趁鲜 WhileFresh

一款帮家里**趁新鲜用掉已买食材**的 Android 应用。把买菜 App 的截图、订单文字或购物小票交给它，它会整理出冰箱里有什么、哪些该先用，并能用临期食材去下厨房找菜谱。

> **状态**：MVP 试用版 0.1.0 已构建，arm64 真机尚未实测；第二轮开发中，见 [docs/todo/cycle-state.md](docs/todo/cycle-state.md)。

<p>
  <img src="docs/images/fridge.jpg" width="240" alt="冰箱：按冷藏、冷冻、常温分区，显示新鲜度与保质期来源">
</p>

<sub>截图来自演示模式，数据为示例。</sub>

## 功能

- **入库**：相册截图（可多张）、系统分享、拍购物小票、粘贴订单文字或手动添加。AI 识别商品、规格和截图上可见的生产日期，分出食材、调料和无关内容（推荐、凑单、配送费等），用户核对后才入冰箱。
- **名称识别**：先按标准词表取最细的词条（如"黑猪梅花肉切片"归为梅花肉）；词表外的名称由 DeepSeek 归类，用户确认后记住。
- **冰箱**：按冷藏 / 冷冻 / 常温分区，另有常备调料、常备食材两组。每件食材显示新鲜 / 临期 / 超出估计期限，以及剩余或过期天数，过期的排在前面，随日期自动变化。
- **保质期有依据**：取值优先级为 包装标注 > 你确认过的规则 > USDA FoodKeeper 参考表 > AI 估计，每项都标出来源；非包装标注的一律标"估计"，不给食品安全结论。
- **调料**：识别出的调料作为常备调料入库，可记录用量。
- **导入历史**：可删除单次导入，只删其中仍在冰箱里的食材，已用掉或扔掉的保留；跳过的重复导入也留一条记录；导入中断后可续传，不必整单重来。
- **清冰箱做饭**：底栏中间的按钮。默认选中临期和标记先用的食材，用所选食材的名字去下厨房搜菜谱。
- **菜谱**：经 Lexmount 云浏览器读取下厨房的搜索结果与详情，保留原页链接并标明来源。下厨房偶尔会弹出滑动验证，此时无法显示详情。应用不再内置菜谱。
- **待买**：可手动添加，或采纳常备规则的建议；按品类整理，可复制或分享。
- **常备规则**：盐、油、酱油、葱姜蒜等默认算家里有，可增删；系统的任何建议都需你确认。
- **服务与偏好**：在"服务"页填写 DeepSeek / Lexmount 密钥，只存在本机，页面只显示是否已配置。"偏好"页记录家里的饮食偏好，可由 DeepSeek 整理成摘要。
- **演示模式**：内置示例数据与预设识别结果，识别不联网，不影响真实数据。
- **隐私**：数据只存在手机本机，不需要账号。识别前提示内容会发送到 DeepSeek；搜索菜谱时，食材名会经 Lexmount 云浏览器访问下厨房。

## 技术栈

| 层 | 选型 |
| --- | --- |
| 客户端 | Qt 6.8.2（Qt Quick / QML + C++17），同一工程构建 Android APK 与 macOS 桌面开发版 |
| 本地存储 | SQLite（QtSql），带版本迁移 |
| AI 服务 | DeepSeek：识别、名称归类、偏好整理；App 直连，经 ai 层统一传输，无后端 |
| 识别 | DeepSeek 多模态 API（`deepseek-flash`），长截图切片后识别 |
| 菜谱 | 下厨房，经 Lexmount 云浏览器读取；yaml-cpp 解析契约 YAML |
| 内置数据 | 标准食材词表、USDA FoodKeeper 保质期参考，由 Python 离线工具生成 |
| 网络 | KDAB android_openssl 提供 HTTPS |

## 快速开始

需要 macOS、Qt 6.8.2（含 Android 套件）、Android SDK / NDK 26、JDK 17、CMake、Ninja、uv。完整说明见 [docs/done/development.md](docs/done/development.md)。

```sh
scripts/fetch-deps.sh                      # 下载并固定 Android OpenSSL
scripts/build-desktop.sh                   # 桌面开发版
build/desktop/whilefresh                   # 运行
ctest --test-dir build/desktop             # C++ 测试
uv run --project tools pytest              # Python 工具测试
scripts/check-keys.sh                      # 提交前检查无密钥

scripts/run-emulator.sh                    # 启动 Android 模拟器
scripts/build-android.sh x86_64            # 模拟器用 APK
scripts/install-android.sh x86_64          # 安装并启动
scripts/build-android.sh arm64-v8a --release   # 真机用签名 APK
```

构建脚本从 macOS 钥匙串读取 DeepSeek 密钥（条目 `landing-project-deepseek`），并从 `~/.config/lexmount/config.yaml` 读取 Lexmount 凭据，生成到构建目录的 `generated/api-config.h`，不进仓库。桌面版需加 `--with-key` 才注入，Android 默认注入。没有密钥也能构建：对应服务显示"未配置"，演示模式照常可用。

## 目录

```
app/       Qt 应用：src/{core,data,ai,recognition,recipes,platform,ui}、qml/、android/、assets/、tests/
tools/     Python 离线工具：词表、保质期参考表、合成测试数据
scripts/   构建、安装、模拟器与密钥检查脚本
test-data/ 合成样本、标准答案与识别录制
design/    HTML 视觉稿与设计参数
docs/      已完成（done/）与未完成（todo/）的产品与过程文档
```

各目录的放置规则见 [STD.md](STD.md)。

## 许可

本项目代码采用 [BSD 3-Clause](LICENSE)。第三方内容：

- Qt 6.8.2 — LGPL v3（动态链接）
- OpenSSL 3（KDAB android_openssl）— Apache 2.0
- 保质期参考 [USDA FoodKeeper](https://catalog.data.gov/dataset/fsis-foodkeeper-data) — CC0
- YAML 解析 [yaml-cpp](https://github.com/jbeder/yaml-cpp) — MIT
- 图标 [Tabler Icons](https://github.com/tabler/tabler-icons) — MIT
- 标题字体 Noto Serif SC — SIL OFL 1.1

文件与编码规范见 [STD.md](STD.md)，AI 协作规则见 [AGENTS.md](AGENTS.md)，产品与过程文档见 [docs/todo/](docs/todo/) 和 [docs/done/](docs/done/)，视觉稿见 [design/README.md](design/README.md)。

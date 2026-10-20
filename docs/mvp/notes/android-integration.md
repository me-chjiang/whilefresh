# Android 原生能力桥接（T9）

对应需求第 3 节（入库）、第 4 节（截图日期作相对日期基准）、第 6 节（待买分享）；决策见 ADR-8、ADR-9。

## 接口

C++ 侧唯一入口是 `whilefresh::platform::PlatformServices`（`app/src/platform/platform-services.h`），由 `PlatformServices::create()` 按平台返回实现，`main.cpp` 以 `platform` 属性传给 QML。

| 成员 | 说明 |
| --- | --- |
| `pickImages()` | 系统选图器，可多选。API 33+ 用 `ACTION_PICK_IMAGES`，更低版本用 `ACTION_GET_CONTENT` + `EXTRA_ALLOW_MULTIPLE`。不申请存储权限 |
| `captureReceipt()` | 系统相机 `ACTION_IMAGE_CAPTURE`，经 FileProvider 写入 `files/captures/` |
| `shareText(text)` | `ACTION_SEND` 选择器（排除本应用自身） |
| `copyText(text)` | `QClipboard`，两个平台共用 |
| `importReceived(ImportPayload)` | 导入成功 |
| `importCancelled(Origin)` | 用户取消选图或拍照；不是错误 |
| `importFailed(Origin, message)` | 导入失败；`message` 为面向用户的中文短句，不含文件名或文本内容 |

每次主动请求（选图、拍照）恰好产生上述三个信号之一；部分图片失败时先发 `importReceived`（成功的部分），再发 `importFailed`。来自其他 App 的分享不经请求，直接以 `origin = Share` 到达。

载荷（`app/src/platform/import-payload.h`，`Q_GADGET`，QML 可读属性）：

- `ImportPayload`：`kind`（`Images` / `Text`）、`origin`（`Gallery` / `Share` / `Camera`）、`images`、`imageCount`、`text`、`summary`（调试摘要）。
- `ImportedImage`：`localPath`（App 私有目录 `files/imports/<uuid>.<ext>`，此后归 App 所有）、`mimeType`、`capturedAt`（`QDateTime`，可能无效）、`capturedAtSource`、`capturedAtSourceName`。

T6 可直接把 `localPath` 交给识别；T8 在 `app/src/ui/` 中包一层视图模型即可。

## 生命周期与 intent 缓存

- Activity：`io.github.mechjiang.whilefresh.WhileFreshActivity` 继承 `QtActivity`，`launchMode="singleTask"`。Qt 6.8 的 `QtActivityBase.onCreate` 在进程中 Qt 已启动时会重启整个应用，所以不能让分享叠出第二个实例；`singleTop` 只在实例位于同一任务栈顶时复用，从其他 App 分享仍会新建实例。实测：其他 App 在前台时分享，进程号不变，只有一个 Activity 实例。
- 冷启动：`onCreate` 处理 `getIntent()`；热启动：`onNewIntent`。`savedInstanceState != null`（重建）或带 `FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY`（从最近任务重开）时不再处理，避免重复导入。
- Java 侧所有结果都序列化为 JSON 消息（格式见 `app/src/platform/bridge-event.cpp`），经 `WhileFreshBridge.post()` 投递。C++ 的 `AndroidPlatformServices` 构造时注册 native 方法并调用 `setNativeReady(true)`，在此之前的消息排队，就绪时按顺序补发；析构时先 `setNativeReady(false)`。native 回调在 Java 线程上执行，只把消息以 `Qt::QueuedConnection` 转到主线程，因此冷启动的分享会在 QML 加载完成、事件循环开始后才发出信号。
- 复制 `content://` 在单线程 `ExecutorService` 上进行，保证结果顺序与请求顺序一致，且不阻塞 UI 线程。
- 拍照途中进程被回收：`capturePath` 存入 `onSaveInstanceState`，重建后 `onActivityResult` 仍能找到照片。实测（相机打开时 `am kill`）进程重启后照片正常导入。

## 文件

- 复制：`ContentResolver.openInputStream` → `files/imports/<uuid>.part`，64 KB 缓冲，超过 50 MB 中止并删除半成品。复制后用 `BitmapFactory` 只解码尺寸，不是图片的拒绝。
- 命名：C++ `finalizeStagedImage` 按 MIME（其次按显示名后缀）推断扩展名，把 `.part` 改名为 `<uuid>.<ext>`；MIME 为空或 `image/*` 时按扩展名补全。显示名只用于取后缀，从不参与路径。
- 相机：照片先写到 `files/captures/<uuid>.jpg`（FileProvider 只暴露这个目录），返回后立即移到 `files/imports/`；每次拍照前清空 `captures/` 中的残留。
- 安全：只接受 `content://`；拒绝 `file://` 和本应用自身 authority 的 URI，防止恶意分享让 App 读出自己的私有文件再上传。

## 图片日期

Java 只收集原始线索，C++ `resolveCapturedAt`（`app/src/platform/captured-at.cpp`）按以下顺序取第一个可信值：

1. `camera`：App 自己拉起相机时，以收到拍照结果的时间为准；
2. `exif`：`DateTimeOriginal`（有 `OffsetTimeOriginal` 时带时区，否则按本地时间）；
3. `media-taken`：MediaStore `DATE_TAKEN`；
4. `media-added`：MediaStore `DATE_ADDED`（截图通常落在这一级）；
5. `file-modified`：MediaStore `DATE_MODIFIED`，或 DocumentsProvider 的 `last_modified`；
6. `none`：都没有，界面需请用户确认"今天"指哪天。

早于 2000 年（相机时钟未设置）或晚于当前时间一天以上的值视为不可信，跳过。`DATE_ADDED` 排在 `DATE_MODIFIED` 之前：截图被裁剪、编辑后修改时间会变，入库时间更接近截图当天。

## 验证方法

见 README「Android 导入调试」。合成测试图由 `/tmp` 中的一次性脚本生成，不进仓库。

## 已知限制

- 系统选图器（Photo Picker）的 URI 只提供 `DATE_TAKEN`；实测没有 EXIF 的 PNG 也返回了 `DATE_TAKEN`（选图器自行回落到文件时间），因此来源显示为 `media-taken`，实际精度与 `media-added` 相当。
- 从"文件"App（DocumentsUI）分享时 URI 属于 ExternalStorageProvider，只有 `last_modified`，来源为 `file-modified`。无存储权限，不能换成 MediaStore URI 查询。
- `adb shell am start` 无法构造 `Uri` 列表，`ACTION_SEND_MULTIPLE` 需通过真实 App（如"文件"）分享来测；单图分享需同时用 `-d` 传同一 URI，`--grant-read-uri-permission` 才会授权。
- 分享同时带图片和文字时只取图片（`text/*` 类型除外）。
- `files/imports/` 中的文件不会自动清理，由后续识别 / 入库流程在用完后删除。
- `allowBackup="false"` 只关闭云备份；Android 12+ 的设备间迁移（D2D）不受影响，如需排除另配 `dataExtractionRules`。
- 每张图上限 50 MB，不做压缩或缩放；识别前是否需要缩图由 T6 决定。
- 桌面替代实现读不到 EXIF，选图时使用文件修改时间；"拍照"退化为选一个文件，日期按当前时间（与 Android 相机一致）。

# BW16BLETouch

让 Ai-Thinker BW16（RTL8720DN / AmebaD）作为单指 BLE HID 触摸屏，向 Android 发送绝对坐标触控。草图可以按屏幕比例或截图像素执行点击、双击、长按、滑动和拖拽。库使用板卡包内置的 AmebaBLE，坐标原点在左上角。

## 在自己的草图中点击屏幕中心

```cpp
#include <BW16BLETouch.h>

BW16BLETouch touch;

void setup() {
    Serial.begin(115200);
    touch.begin("BW16 Touch", "My Device");
}

void loop() {
    // 在串口监视器发送 t，触发一次中心点击。
    if (Serial.available() && Serial.read() == 't') {
        if (touch.tap(16384, 16384)) {
            Serial.println("Tap submitted");
        } else {
            Serial.println("Check the Bluetooth connection");
        }
    }
    delay(1);
}
```

1. 在 Arduino IDE 安装 Realtek **AmebaD 3.1.7** 板卡包，并选择 **Ai-Thinker BW16 (RTL8720DN)**。首次配置可参考 [Realtek BW16 安装指南](https://ameba-doc-arduino-sdk.readthedocs-hosted.com/en/latest/ameba_d/bw16-typeb/Getting_Started/Getting%20Started%20with%20BW16.html)。
2. 从 [v0.2.0 发布页](https://github.com/xiangwen-ctrl/BW16BLETouch/releases/tag/v0.2.0) 下载 `BW16BLETouch-0.2.0.zip`，选择「项目 → 加载库 → 添加 .ZIP 库」。手动安装时，将仓库目录放到草图簿的 `libraries/BW16BLETouch` 下，确保目录内直接包含 `library.properties`。
3. 新建草图，粘贴以上代码，选择 BW16 的 USB 串口并上传。按 RST 后，在 Android 系统蓝牙设置中配对 **BW16 Touch**。
4. 打开绘图或触点测试页面，将 Arduino 串口监视器设为 **115200**，发送字母 `t`。屏幕中心应收到一次点击。自己的按钮或定时器可以直接调用同一个 `touch.tap()`。

### 用截图像素执行手势

例如当前方向的完整截图是 1080 × 2400，在 `setup()` 中设置尺寸：

```cpp
touch.setScreenResolution(1080, 2400);
```

在自己的事件处理函数中任选一种调用。以下坐标均按该截图计算：

```cpp
using Units = BW16BLETouch::Units;
touch.tapPixel(540, 1200);                                  // 点击中心
touch.longPress(270, 840, 800, Units::Pixels);              // 按住 800 ms
touch.swipe(180, 1600, 900, 1600, 600, 30, Units::Pixels);  // 向右滑动
```

## API 参考

库使用 AmebaD 的全局 BLE 外设和 HID 服务。所有实例共享一套状态，在同一个任务中顺序调用，勿与另一个直接管理 `BLE` 的外设库并用。手势会阻塞调用方直到提交抬起帧或检测到断连。

| 调用 | 参数与行为 |
| --- | --- |
| `begin(deviceName, manufacturer)` | 两项可省略，默认分别为 `BW16 Touch` 和 `BW16BLETouch`。名称限 1–18 个 UTF-8 字节以容纳广播，制造商限 1–20 字节。启动中重复调用或参数无效返回 false。 |
| `isConnected()` | 返回 BLE 链路状态。首次配对还需等待 Android 完成 HID 发现和通知订阅。 |
| `end()` | 尝试抬起有效触点、断开连接并关闭 BLE 外设。可再次 begin。当前运行期内保留尺寸和旋转设置。 |
| `setScreenResolution(w, h)`、`setRotation(degrees)` | 配置坐标映射。角度仅接受 0、90、180、270；使用 uint16_t 容纳 270。按下期间拒绝改变映射。 |
| `press(x, y, units)` | 开始一个触点。已有触点时返回 false。 |
| `moveTo(x, y, units)` | 将当前按下的触点移动到绝对位置。 |
| `release()` | 在最后报告的位置抬起。需要已有触点。 |
| `tap(x, y, holdMs, units)` | 按下后等待并抬起。holdMs 默认 60。 |
| `doubleTap(x, y, intervalMs, units)` | 两次各按住 60 ms 的点击。intervalMs 从首次抬起算到第二次按下，默认 120。 |
| `longPress(x, y, holdMs, units)` | 默认按住 800 ms。长按阈值由 Android 和应用决定。 |
| `swipe(x1, y1, x2, y2, durationMs, steps, units)` | 按下、按时间线性插值移动到终点、抬起。默认 400 ms、20 步。两个端点均在按下前校验，要求 1 ≤ steps ≤ durationMs。 |

所有带 `units` 的方法默认使用 `Units::Hid`，显式传入 `Units::Pixels` 可使用像素坐标。另有 `pressPixel(x,y)`、`moveToPixel(x,y)`、`tapPixel(x,y,holdMs=60)` 简写。


手势的时长、间隔和步数参数为 uint16_t。建议先按每 15–30 ms 一步调试滑动。成功返回 true 表示调用已向本地 BLE 栈提交所需报告。AmebaBLE 的 `inputReport()` 返回 void，无法提供主机接收确认。断连、坐标越界、未配置像素尺寸或触点状态不匹配时返回 false。分步拖拽中若移动参数有误，仍可在最后有效位置调用 `release()`。在应用主循环轮询 `isConnected()` 可及时清除已断开链路的本地触点状态。

## 选择坐标系

**逻辑坐标**的两个轴独立取整数 `0..32767`，适合跨分辨率操作。`(0, 0)` 位于左上角，X 向右，Y 向下。横纵各取 16384 约为屏幕中心。

**像素坐标**取自当前方向的完整截图。先调用 `setScreenResolution(W, H)`，合法范围为 `0 ≤ x < W`、`0 ≤ y < H`。每个轴使用整数公式 `floor((2 * pixel + 1) * 16384 / extent)` 将点击定位到像素区间的中心。宽高支持 1 到 32768。初始像素尺寸未设置，复位后需重新设置。建议用原始截图读取坐标，裁剪图需要补回裁剪偏移，缩放图需要先换算到原图尺寸。

**旋转校准**。先用默认 `0` 测试；若 Android 已代为处理旋转，直接使用当前截图宽高即可。如果方向不符，再设置当前截图相对原生 HID 方向的顺时针角度。每次旋转后把宽高更新为新截图的实际宽高。下表中的 `u, v` 是按当前截图换算出的逻辑坐标，`M = 32767`。

| 设置 | 发给 HID 的 X、Y |
| --- | --- |
| 0 | u, v |
| 90 | v, M − u |
| 180 | M − u, M − v |
| 270 | M − v, u |

例如原生竖屏为 1080 × 2400，画面顺时针转成 2400 × 1080，且需要固件补偿，则依次调用 `setScreenResolution(2400, 1080)` 和 `setRotation(90)`。建议在目标 Android 上用中心、宽 20% 高 40%、宽 80% 高 30% 三点检验方向和范围。单测中心无法识别旋转。

## 通用示例

在 Arduino IDE 的「文件 → 示例 → BW16BLETouch」中打开以下草图。

### [BasicTouch](examples/BasicTouch/BasicTouch.ino)

在配对前打开测试页面。每次检测到新连接，示例等待 2 秒，然后演示中心点击、左上长按、底部从左向右滑动；同一连接只演示一次。

### [SerialCommand](examples/SerialCommand/SerialCommand.ino)

串口监视器设为 115200 波特率，行尾选择「换行」。每行一条命令，串口响应 `OK`、`ERR` 或连接状态。`pixel` 使用当前截图坐标，`tap`、`longpress`、`swipe` 使用 HID 逻辑坐标。以下各行可以单独发送。

```text
status
size 1080 2400
rotation 0
pixel 540 1200
tap 16384 16384
longpress 8192 8192 800
swipe 4096 24576 28672 24576 600 30
```

### [WiFiHttpControl](examples/WiFiHttpControl/WiFiHttpControl.ino)

在草图顶部填写局域网 SSID 与密码，上传并将 Android 配对到 BW16。电脑浏览器打开串口打印的 IP。选择 Tap、Long press 或 Swipe；Coordinates 可选 HID 或 Screen pixels。像素模式填写当前方向的完整截图宽高。滑动还需填写终点、时长及步数。BW16 可以加入 2.4 GHz 或 5 GHz Wi-Fi，触控通过 BLE 发送。

`GET /` 返回控制页。以下接口均使用 `POST`，参数是十进制整数，无请求体。路径不加前缀时使用 HID 逻辑坐标（0..32767）。加上 `/px/W/H` 前缀后，以本次请求的屏幕宽高和像素坐标执行，左上角为原点。W、H 各为 1..32768，X < W，Y < H。每个像素请求自带宽高，客户端可独立使用。

| 操作 | HID 路径 | 像素路径 |
|---|---|---|
| 单击，按住 60 ms | `/tap/X/Y` | `/px/W/H/tap/X/Y` |
| 长按 | `/longpress/X/Y/MS` | `/px/W/H/longpress/X/Y/MS` |
| 滑动 | `/swipe/X1/Y1/X2/Y2/MS/STEPS` | `/px/W/H/swipe/X1/Y1/X2/Y2/MS/STEPS` |

MS 为 1..65535 毫秒，滑动要求 1 ≤ STEPS ≤ MS。手势完成提交后返回 HTTP 200；参数错误返回 400，未知路由返回 404，触控不可用返回 409。滑动与长按占用一次请求的执行时间，客户端超时应长于手势时长。此示例使用无需登录的明文 HTTP，适合受信任的局域网。

将下面的 IP 换成串口显示的地址，示例使用 1080 × 2400 的完整截图。

```sh
curl -X POST http://192.168.1.50/px/1080/2400/tap/540/1200
curl -X POST http://192.168.1.50/px/1080/2400/longpress/270/840/800
curl -X POST http://192.168.1.50/px/1080/2400/swipe/180/1600/900/1600/600/30
```

## 设备配对与排查

- **能连接但没有触控**。在系统蓝牙设置中打开该设备的「输入设备」开关（若系统提供），等待首次配对完成。在开发者选项打开「指针位置」或「显示点按操作」，用绘图页面复测。如仅出现光标，检查 Android 对外部触摸屏的识别和当前活动显示屏。不同厂商的映射可能有差异。
- **换固件后仍沿用旧设备类型**。先在 Android 蓝牙详情中「取消配对 / 忽略设备」，复位 BW16，重新扫描并配对。必要时关闭再开启蓝牙后重试。如果依旧读到旧服务，可在该系统允许时清除系统蓝牙服务的存储后重配；此操作可能同时移除其他配对记录。
- **落点成比例偏移、横竖颠倒**。用当前方向的完整截图重设尺寸，恢复 rotation 0 后按三个测试点检查，再决定是否需要旋转补偿。应用的局部坐标需要加上它在全屏中的偏移。
- **上传失败或看不到串口回应**。确认选择 USB 串口，关闭占用端口的终端；按住 BURN，短按并松开 RST，再松开 BURN 进入下载模式。烧录后复位，以 115200 波特率和换行符发送命令。

## 实测兼容性

| 开发板与板卡包 | Android 主机 | 实测功能 |
| --- | --- | --- |
| Ai-Thinker BW16-Kit · AmebaD 3.1.7 | 小米 Pad 6 Pro，竖屏 1800 × 2880 | 串口像素单击、长按、滑动；5 GHz 局域网网页与独立 HTTP 接口各执行三种手势 |

在这六次网络手势测试中，平板浏览器回传的起点和终点换算到屏幕像素后，单轴偏差最大为 0.5 像素。其他设备可按上方三点法校准。三个随库示例均已使用 BW16 板型编译。

## HID 报告

使用单指 Digitizers / Touch Screen Report Map，Report ID 为 1。输入帧是 7 字节，依次为 Tip、Contact ID、X 低字节、X 高字节、Y 低字节、Y 高字节、Contact Count。按下和移动用 Tip=1，抬起用 Tip=0。抬起帧仍包含一条记录，因此 Contact Count=1。Report ID 由 GATT Report Reference 携带。保留一字节的保留输出报告及一个输出特征，避免 AmebaD 3.1.7 在初始化零个输出报告时调用 malloc(0)。

当前描述符一次只能描述一个触点。`pinch` 需要能携带两个独立 Contact ID 及各自位置、状态的报告，并需在 Android 上重新配对、验证双指的按下、移动与抬起。本版提供单指 API。

## 许可

[MIT License](LICENSE)。

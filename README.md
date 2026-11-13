# BW16BLETouch

English · [简体中文](README_zh.md)

Use an Ai-Thinker BW16 (RTL8720DN / AmebaD) as a single-finger Bluetooth LE HID touchscreen for Android. Send taps, double taps, holds, and drags from an Arduino sketch using either absolute HID coordinates or pixels from a full-screen screenshot.

## Tap the screen from your sketch

```cpp
#include <BW16BLETouch.h>

BW16BLETouch touch;

void setup() {
    Serial.begin(115200);
    touch.begin("BW16 Touch", "My Device");
}

void loop() {
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

1. Install the Realtek **AmebaD 3.1.7** board package and select **Ai-Thinker BW16 (RTL8720DN)**. The package provides AmebaBLE and WiFi. See the [Realtek setup guide](https://ameba-doc-arduino-sdk.readthedocs-hosted.com/en/latest/ameba_d/bw16-typeb/Getting_Started/Getting%20Started%20with%20BW16.html).
2. Download `BW16BLETouch-0.3.0.zip` from the [release page](https://github.com/xiangwen-ctrl/BW16BLETouch/releases/tag/v0.3.0). In Arduino IDE choose **Sketch → Include Library → Add .ZIP Library**. For manual installation, put the repository under the sketchbook's `libraries/BW16BLETouch` directory with `library.properties` directly inside.
3. Upload the sketch and reset the board. Pair **BW16 Touch** in Android's Bluetooth settings. Open a drawing or touch-test app.
4. Set Serial Monitor to **115200** and send `t` to tap the center. Call the same method from a button or timer in your own sketch.

### Work in screenshot pixels

For a 1080 × 2400 screenshot, call `touch.setScreenResolution(1080, 2400)` during setup. In an event handler, choose the gesture you need:

```cpp
touch.tapPixel(540, 1200);
touch.doubleTapPixel(540, 1200);                  // 120 ms between taps
touch.longPressPixel(270, 840, 800);               // hold for 800 ms
touch.swipePixel(180, 1600, 900, 1600, 600, 30);   // swipe right in 30 steps
```

## API reference

All instances share the board's BLE peripheral, HID service and contact state. Make calls sequentially from one task. The gesture methods block the caller until the final report has been submitted or a disconnect has been detected.

| Method | Behavior |
| --- | --- |
| `begin(name, manufacturer)` | Starts the peripheral. Defaults to `"BW16 Touch"` and `"BW16BLETouch"`. Name and manufacturer accept 1–18 and 1–20 UTF-8 bytes respectively. Invalid arguments or a second call while running return false. |
| `isConnected()` | Reports BLE link state. Allow time after pairing for Android to discover HID and subscribe. Polling also clears local contact state when a disconnect is observed. |
| `end()` | Releases an active contact if possible, disconnects and shuts down BLE. Resolution and rotation persist in RAM. |
| `setScreenResolution(w,h)` | Sets the current screenshot width and height, each from 1 to 32768. Required for pixel methods after a board reset. |
| `setRotation(degrees)` | Accepts 0 (default), 90, 180 or 270. Both mapping setters reject changes during an active press. |
| `press(x,y)`, `moveTo(x,y)`, `release()` | Starts a contact, moves it, then lifts at its last reported position. A move or release requires an active contact. |
| `tap(x,y,holdMs=60)` | Press, hold for the specified time, then release. |
| `doubleTap(x,y,intervalMs=120)` | Two 60 ms taps. The interval runs from the first release to the second press. |
| `longPress(x,y,holdMs=800)` | Holds for the requested duration. Android and the target app determine the long-press threshold. |
| `swipe(x1,y1,x2,y2,durationMs=400,steps=20)` | Validates both endpoints, presses, moves in linear steps and releases. Requires 1 ≤ steps ≤ durationMs. Start with 15–30 ms per step. |

Coordinates default to `BW16BLETouch::Units::Hid`. Each method taking coordinates also accepts a final `Units::Pixels` argument (use `using Units = BW16BLETouch::Units;`). The equivalent pixel shortcuts are `pressPixel`, `moveToPixel`, `tapPixel`, `doubleTapPixel`, `longPressPixel` and `swipePixel`. They use the same timing defaults; `release()` works with either coordinate system.

Times and step counts are unsigned 16-bit integers. A successful gesture call means the reports were submitted to the local BLE stack. AmebaBLE provides no Android delivery acknowledgement. Invalid coordinates, missing pixel dimensions, incompatible contact state and detected disconnects return false. After an invalid move, call `release()` to lift at the last valid position.

## Coordinates and rotation

HID coordinates run from 0 to 32767 independently on each axis. The origin is the top left, with X increasing to the right and Y down. Pixel coordinates are zero-based. Use the dimensions of the full screenshot in its current orientation, including any system bars. Add the offset of a cropped image or rescale a resized image before sending its coordinates.

The library maps a pixel to the center of its HID interval using integer division: `((2 * pixel + 1) * 16384) / extent`. Start with rotation 0. If Android already accounts for the display orientation, only update the screenshot dimensions. When compensation is needed, specify the screenshot's clockwise rotation relative to the native HID frame.

| Degrees | HID output for screenshot coordinates (u, v), with M = 32767 |
| --- | --- |
| 0 | u, v |
| 90 | v, M − u |
| 180 | M − u, M − v |
| 270 | M − v, u |

Check the center and two asymmetric points, for example (20%, 40%) and (80%, 30%). The center alone looks correct even with swapped or reversed axes.

## Examples

Open **File → Examples → BW16BLETouch** in Arduino IDE.

- [BasicTouch](examples/BasicTouch/BasicTouch.ino) waits two seconds after detecting a connection, then demonstrates a tap, a long press and a swipe once per connection.
- [SerialCommand](examples/SerialCommand/SerialCommand.ino) accepts one command per line at 115200 baud. Try `status`, `size 1080 2400`, then `pixel 540 1200`. Other commands are `rotation DEG`, `tap X Y`, `longpress X Y MS` and `swipe X1 Y1 X2 Y2 MS STEPS`. Only `pixel` takes pixel coordinates.
- [WiFiHttpControl](examples/WiFiHttpControl/WiFiHttpControl.ino) connects to a 2.4 or 5 GHz network using the SSID and password you enter in the sketch. Open the IP printed on Serial Monitor, select a gesture and choose HID or screen pixels. The board sends the gesture to the Android host over BLE.

### Local HTTP interface

`GET /` serves the form. Send a `POST` with an empty body to any path below. All path arguments are unsigned decimal integers. Replace the example IP with the board's IP.

| Gesture | HID path | Pixel path |
| --- | --- | --- |
| Tap | `/tap/X/Y` | `/px/W/H/tap/X/Y` |
| Long press | `/longpress/X/Y/MS` | `/px/W/H/longpress/X/Y/MS` |
| Swipe | `/swipe/X1/Y1/X2/Y2/MS/STEPS` | `/px/W/H/swipe/X1/Y1/X2/Y2/MS/STEPS` |

```sh
curl -X POST http://192.168.1.50/px/1080/2400/tap/540/1200
curl -X POST http://192.168.1.50/px/1080/2400/longpress/270/840/800
curl -X POST http://192.168.1.50/px/1080/2400/swipe/180/1600/900/1600/600/30
```

Each pixel request supplies its own W and H. MS is 1–65535. The client timeout should exceed the gesture duration. HTTP 200 indicates submission, 400 invalid parameters, 404 an unknown route, and 409 an unavailable touch device. The example uses unauthenticated HTTP on a trusted LAN.

## Troubleshooting

- **Connected but no touches.** Enable the input-device option if Android offers it, allow HID discovery to complete, and use Android's **Pointer location** or a drawing app to check events on the active display.
- **Old HID profile after reflashing.** Forget the board in Bluetooth settings, reset it and pair again. If needed, toggle Bluetooth. Clearing system Bluetooth storage on devices that allow it can also remove other pairings.
- **Offset or rotated touches.** Recheck the full screenshot dimensions, restore rotation 0, and test the three points above.
- **Upload or serial trouble.** Close terminals holding the USB port. For manual download mode, hold BURN, press and release RST, then release BURN. Reset after uploading. Select 115200 baud and a newline for the serial example.

## Tested compatibility

| Board and core | Android host | Hardware checks |
| --- | --- | --- |
| Ai-Thinker BW16-Kit, AmebaD 3.1.7 | Xiaomi Pad 6 Pro, 1800 × 2880 portrait | Pixel taps, holds and swipes over serial; all three gestures from a browser and direct HTTP requests on a 5 GHz LAN. |

Across the six network gesture tests the largest per-axis endpoint difference reported by the tablet was 0.5 screen pixels. Calibrate each new display independently.

**Manual reconnection.** The paired tablet registered a tap at (450.49, 1400.45). On the same boot, the sketch called `end()`, confirmed the link was down, and called `begin()` again. After manually reconnecting the existing pairing, the tablet registered a second tap at (900.49, 1400.45) using the original pixel dimensions.

**Automatic reconnection.** In a separate test on the same board and tablet, the sketch ran 10 consecutive `end()` → `begin()` cycles without a board reset or manual Bluetooth interaction. Each `begin()` succeeded and the board reported advertising. The tablet reconnected in **0 of 10** observation windows (60 seconds after each `begin()` returned), so the sketch sent no test taps during those rounds. Only the first cycle started with an active connection. The remaining nine started with the link already down.

## HID report

The single-finger Digitizers / Touch Screen descriptor uses report ID 1. Each input report has seven bytes: tip switch, contact ID, little-endian X, little-endian Y, and contact count. A lift clears the tip switch and still describes one contact record. GATT carries the report ID separately. One reserved output byte avoids an SDK 3.1.7 zero-length allocation during setup. Two-finger pinch would require a different descriptor.

## License

[MIT](LICENSE).

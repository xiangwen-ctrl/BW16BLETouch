# Gesture acceptance page

Open `index.html` in an Android browser, or serve this directory from a computer on the same LAN:

```sh
python -m http.server 8766 --directory tools/gesture-lab
```

Open `http://YOUR_COMPUTER_IP:8766/` on the Android device. Keep the page in the foreground at 100% zoom. Choose one of the three tasks.

| Task | Acceptance condition |
| --- | --- |
| Long-press reorder | Hold A for at least 650 ms, move through B and drop inside C. The resulting order must be B, C, A. |
| Polyline | One finger must follow the corridor through gates 1, 2, 3, 4, with an intermediate movement sample on each leg, and release inside gate 4. |
| Hold, reverse and stop | Hold gate 1 for at least 650 ms, reach gate 2, stay inside its radius for at least 400 ms, then reverse through gate 3 and release inside gate 4. |

The three navigation buttons read 长按排序, 折线路径 and 长按折返与停靠. 重置本项 resets the current task. 导出测试记录 downloads all recorded attempts as JSON. A green 通过 indicates acceptance; a red 失败 includes the failure reason.

Expand 固件目标坐标, tap the test surface once to measure the offset between browser content and the full screen, then reset the task. The displayed coordinates estimate screen pixels as `screenX * devicePixelRatio`. Configure the library using the full screenshot dimensions. Recalibrate after rotating, zooming, changing toolbars or scrolling. Match the coordinates to your own display.

Use `longPressDragPixel()` to move from the center of A to C. Use `swipePathPixel()` with four points for the path tasks. Set the first hold to 850 ms for the combined task and the hold at gate 2 to 600 ms; moving each segment over 900 ms in 45 steps leaves some timing margin. The thresholds are CSS pixels: initial movement allowance 12, gate radius 16, corridor radius 22. Measured gate dwell includes motion within its radius. The browser can coalesce movement notifications, so event count can differ from the number of reports sent by the board.

This page's sorting and route evaluation belong to the acceptance tool. The library and Arduino examples accept general coordinates and timing parameters.

## Browser regression

Install Python, Chrome, and the Python `playwright` package, then run:

```sh
python tools/gesture-lab/verify.py
```

The script loads the sibling HTML and uses Chrome CDP touch events. It checks 35 scenarios in three viewport configurations, including hold and dwell boundaries, early release, shortcuts, second contacts, cancellation, resize/reset during a contact, retry recovery and exported verdicts. It writes `automation-results.json` with the results and browser version. Explicit input timestamps are based on the page's navigation time origin, and the boundary tests check the timestamps actually received by the page.

Browser simulation checks the acceptance page. Run the board against the page to verify BLE delivery and Android input handling on a specific host.

At an exact timing boundary, Chrome can deliver a timestamp fractionally below the requested millisecond. The runner verifies the delivered time is within 0.2 ms of the request and determines acceptance from that delivered time. Exported events retain the raw `timeStamp` as well as relative milliseconds.

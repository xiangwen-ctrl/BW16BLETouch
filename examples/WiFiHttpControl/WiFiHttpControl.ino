#include <BW16BLETouch.h>
#include <WiFi.h>
extern "C" {
#include <ard_socket.h>
}
#include <stdlib.h>
#include <string.h>

char ssid[] = "YOUR_SSID";
char password[] = "YOUR_PASSWORD";
int listener = -1;
BW16BLETouch touch;

const char page[] =
    "<!doctype html>\n"
    "<html lang=\"en\"><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
    "<title>BW16 Touch</title>\n"
    "<style>\n"
    "*{box-sizing:border-box}body{font:16px system-ui,sans-serif;color:#192f36;background:#f5f7f6;margin:0}\n"
    "main{max-width:34rem;margin:6vh auto;padding:1.5rem}h1{margin:0 0 1.4rem}form{padding:1.3rem;background:white;border:1px solid #cbd4d4;border-radius:8px}\n"
    ".row{display:grid;grid-template-columns:1fr 1fr;gap:1rem;margin-bottom:1rem}\n"
    "label{display:grid;gap:.35rem}input,select,button{font:inherit;padding:.55rem;border:1px solid #aebdbc;border-radius:4px;width:100%}\n"
    "button{background:#105b57;color:white;border:0;cursor:pointer}button:disabled{opacity:.6}output{display:block;margin-top:.7rem}[hidden]{display:none}\n"
    "</style><main><h1>BW16 Touch</h1><form id=\"control\">\n"
    "<div class=\"row\"><label>Gesture<select id=\"gesture\"><option value=\"tap\">Tap</option><option value=\"longpress\">Long press</option><option value=\"swipe\">Swipe</option></select></label>\n"
    "<label>Coordinates<select id=\"units\"><option value=\"hid\">HID (0-32767)</option><option value=\"px\">Screen pixels</option></select></label></div>\n"
    "<div class=\"row\" id=\"size\"><label>Screen width<input id=\"w\" type=\"number\" min=\"1\" max=\"32768\" required placeholder=\"1080\"></label>\n"
    "<label>Screen height<input id=\"h\" type=\"number\" min=\"1\" max=\"32768\" required placeholder=\"2400\"></label></div>\n"
    "<div class=\"row\"><label>X<input id=\"x\" type=\"number\" min=\"0\" max=\"32767\" required value=\"16384\"></label>\n"
    "<label>Y<input id=\"y\" type=\"number\" min=\"0\" max=\"32767\" required value=\"16384\"></label></div>\n"
    "<div class=\"row\" id=\"end\"><label>End X<input id=\"ex\" type=\"number\" min=\"0\" max=\"32767\" required></label>\n"
    "<label>End Y<input id=\"ey\" type=\"number\" min=\"0\" max=\"32767\" required></label></div>\n"
    "<div class=\"row\" id=\"timing\"><label>Duration (ms)<input id=\"ms\" type=\"number\" min=\"1\" max=\"65535\" required value=\"800\"></label>\n"
    "<label id=\"stepsLabel\">Steps<input id=\"steps\" type=\"number\" min=\"1\" required value=\"30\"></label></div>\n"
    "<button type=\"submit\">Tap</button><output aria-live=\"polite\"></output></form></main>\n"
    "<script>\n"
    "const $=id=>document.getElementById(id), form=$('control'),button=form.querySelector('button'),output=form.querySelector('output');\n"
    "function visible(id,on){const group=$(id);group.hidden=!on;group.querySelectorAll('input').forEach(i=>i.disabled=!on)}\n"
    "function update(){\n"
    " const g=$('gesture').value,px=$('units').value==='px';\n"
    " visible('size',px);visible('end',g==='swipe');visible('timing',g!=='tap');visible('stepsLabel',g==='swipe');\n"
    " for(const id of ['x','ex']) $(id).max=px&&$('w').value ? Number($('w').value)-1 : 32767;\n"
    " for(const id of ['y','ey']) $(id).max=px&&$('h').value ? Number($('h').value)-1 : 32767;\n"
    " $('steps').max=$('ms').value||65535;button.textContent=$('gesture').selectedOptions[0].text;\n"
    "}\n"
    "$('units').onchange=()=>{for(const id of ['x','y','ex','ey']) $(id).value='';update()};\n"
    "form.oninput=update;update();\n"
    "form.onsubmit=async e=>{e.preventDefault();if(button.disabled)return;\n"
    " const g=$('gesture').value,parts=$('units').value==='px'?['px',$('w').value,$('h').value]:[];\n"
    " parts.push(g,$('x').value,$('y').value);\n"
    " if(g==='swipe')parts.push($('ex').value,$('ey').value);\n"
    " if(g!=='tap')parts.push($('ms').value);\n"
    " if(g==='swipe')parts.push($('steps').value);\n"
    " button.disabled=true;output.textContent='Sending...';\n"
    " try{const r=await fetch('/'+parts.join('/'),{method:'POST'});output.textContent=r.status+' '+await r.text()}\n"
    " catch(err){output.textContent=err.message}finally{button.disabled=false}};\n"
    "</script></html>\n";

void respond(int client, const char* status, const char* type, const char* body) {
    char header[192];
    int n = snprintf(header, sizeof(header),
                     "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %u\r\n"
                     "Connection: close\r\nCache-Control: no-store\r\n\r\n",
                     status, type, unsigned(strlen(body)));
    if (n > 0 && size_t(n) < sizeof(header) &&
        send_data(client, (const uint8_t*)header, n, 0) == n) {
        send_data(client, (const uint8_t*)body, strlen(body), 0);
    }
}

// Parse one unsigned decimal path component, leaving the cursor at '/' or NUL.
bool takeNumber(const char*& path, uint16_t& value) {
    if (*path != '/') return false;
    ++path;
    if (*path < '0' || *path > '9') return false;
    uint32_t number = 0;
    do {
        number = number * 10 + (*path++ - '0');
        if (number > 65535) return false;
    } while (*path >= '0' && *path <= '9');
    value = uint16_t(number);
    return *path == '/' || *path == 0;
}

void gestureRequest(int client, const char* path) {
    BW16BLETouch::Units units = BW16BLETouch::Units::Hid;
    uint16_t width = 32768, height = 32768;
    if (!strncmp(path, "/px/", 4)) {
        units = BW16BLETouch::Units::Pixels;
        path += 3;
        if (!takeNumber(path, width) || !takeNumber(path, height) ||
            !width || !height || width > 32768 || height > 32768) {
            respond(client, "400 Bad Request", "text/plain", "Invalid screen dimensions");
            return;
        }
    }
    unsigned count;
    if (!strncmp(path, "/tap/", 5)) { count = 2; path += 4; }
    else if (!strncmp(path, "/longpress/", 11)) { count = 3; path += 10; }
    else if (!strncmp(path, "/swipe/", 7)) { count = 6; path += 6; }
    else {
        respond(client, "404 Not Found", "text/plain", "Unknown gesture");
        return;
    }
    uint16_t a[6] = {};
    for (unsigned i = 0; i < count; ++i) {
        if (!takeNumber(path, a[i])) {
            respond(client, "400 Bad Request", "text/plain", "Expected decimal arguments");
            return;
        }
    }
    if (*path || a[0] >= width || a[1] >= height ||
        (count == 3 && !a[2]) ||
        (count == 6 && (a[2] >= width || a[3] >= height || !a[5] || a[5] > a[4]))) {
        respond(client, "400 Bad Request", "text/plain", "Invalid coordinates, duration or steps");
        return;
    }
    bool ok = units == BW16BLETouch::Units::Hid || touch.setScreenResolution(width, height);
    const char* result = "Tap submitted";
    if (count == 2) {
        ok = ok && touch.tap(a[0], a[1], 60, units);
    } else if (count == 3) {
        ok = ok && touch.longPress(a[0], a[1], a[2], units);
        result = "Long press submitted";
    } else {
        ok = ok && touch.swipe(a[0], a[1], a[2], a[3], a[4], a[5], units);
        result = "Swipe submitted";
    }
    respond(client, ok ? "200 OK" : "409 Conflict", "text/plain", ok ? result : "Touch unavailable");
}

void serve(int client) {
    // Read the full header block, retaining only the request line.
    char headers[128];
    size_t used = 0, total = 0;
    bool firstLine = true;
    unsigned match = 0;
    const char terminator[] = "\r\n\r\n";
    set_sock_recv_timeout(client, 500);
    unsigned long start = millis();
    while (total < 4096 && millis() - start < 3000) {
        uint8_t chunk[256];
        int n = recv_data(client, chunk, sizeof(chunk), 0);
        if (n <= 0) break;
        total += n;
        for (int i = 0; i < n && match < 4; ++i) {
            char c = char(chunk[i]);
            if (firstLine) {
                if (c == '\r') {
                    headers[used] = 0;
                    firstLine = false;
                } else if (c < ' ' || used == sizeof(headers) - 1) {
                    respond(client, "400 Bad Request", "text/plain", "Invalid request line");
                    return;
                } else {
                    headers[used++] = c;
                }
            }
            if (c == terminator[match]) ++match;
            else match = c == '\r' ? 1 : 0;
        }
        if (match < 4) continue;
        if (used < 9) break;
        char* version = headers + used - 9;
        if (strcmp(version, " HTTP/1.1") && strcmp(version, " HTTP/1.0")) break;
        *version = 0;
        if (!strcmp(headers, "GET /")) {
            respond(client, "200 OK", "text/html; charset=utf-8", page);
        } else if (!strncmp(headers, "POST ", 5)) {
            gestureRequest(client, headers + 5);
        } else {
            respond(client, "404 Not Found", "text/plain", "Use GET / or POST a gesture path");
        }
        return;
    }
    respond(client, "400 Bad Request", "text/plain", "Incomplete or oversized request");
}

void setup() {
    Serial.begin(115200);
    while (WiFi.begin(ssid, password) != WL_CONNECTED) {
        Serial.println("Check Wi-Fi SSID and password");
        delay(3000);
    }
    touch.begin();
    WiFi.disablePowerSave();
    listener = start_server(80, TCP_MODE);
    if (listener < 0 || sock_listen(listener, 3) < 0) {
        Serial.println("Could not start HTTP listener");
        listener = -1;
        return;
    }
    Serial.print("Open http://");
    Serial.println(WiFi.localIP());
}

void loop() {
    if (listener < 0) { delay(1000); return; }
    int client = get_available(listener);
    if (client >= 0) {
        serve(client);
        close_socket(client);
    }
    delay(5);
}

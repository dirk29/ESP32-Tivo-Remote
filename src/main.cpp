#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDConsumerControl.h"

// ---------- Wi-Fi Credentials ----------
#define WIFI_SSID      "MY_WIFI_SSID"
#define WIFI_PASSWORD  "MY_WIFI_PASSWORD"

// ---------- TFT Pins (Waveshare ESP32-S3 GEEK) ----------
#define TFT_BL    7   // Backlight
#define TFT_DC    8   // Data/Command
#define TFT_RST   9   // Reset
#define TFT_CS    10  // Chip Select
#define TFT_MOSI  11  // SPI MOSI
#define TFT_SCLK  12  // SPI SCLK
#define ST77XX_DARKGREY 0x7BEF

SPIClass *hspi = new SPIClass(HSPI);
Adafruit_ST7789 tft = Adafruit_ST7789(hspi, TFT_CS, TFT_DC, TFT_RST);

// ---------- Globals ----------
WebServer server(80);
USBHIDKeyboard Keyboard;
USBHIDConsumerControl ConsumerControl;
String lastKey = "None";
bool uiNeedsUpdate = true;

// ---------- UI Drawing (Narrowed for 240x135 Screen) ----------
void drawUI() {
  tft.fillScreen(ST77XX_BLACK);

  // 1. Bigger IP Address Header
  tft.setTextSize(2);
  tft.setCursor(6, 6);
  if (WiFi.status() == WL_CONNECTED) {
    tft.setTextColor(ST77XX_CYAN);
    tft.println(WiFi.localIP());
  } else {
    tft.setTextColor(ST77XX_RED);
    tft.println("Connecting...");
  }

  // 2. Sub-header
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(6, 26);
  tft.print("TiVo Remote Status");

  // 3. Conditional Drawing: App Card vs D-pad
  if (lastKey.startsWith("app_")) {
    String appName = "";
    uint16_t brandBg = ST77XX_BLACK;
    uint16_t brandBorder = ST77XX_WHITE;
    uint16_t textColor = ST77XX_WHITE;

    if (lastKey == "app_youtube") {
      appName = "YouTube";
      brandBg = ST77XX_RED;
      brandBorder = ST77XX_WHITE;
    } else if (lastKey == "app_espn") {
      appName = "ESPN";
      brandBg = 0x8000; // Deep red/maroon
      brandBorder = ST77XX_RED;
    } else if (lastKey == "app_paramount") {
      appName = "Paramount+";
      brandBg = 0x011F; // Deep royal blue
      brandBorder = ST77XX_CYAN;
    } else if (lastKey == "app_hbo") {
      appName = "HBO Max";
      brandBg = 0x4810; // Purple
      brandBorder = 0xF81F; // Magenta
    } else if (lastKey == "app_prime") {
      appName = "Prime Video";
      brandBg = 0x03EF; // Prime Cyan/Blue
      brandBorder = ST77XX_WHITE;
    } else if (lastKey == "app_netflix") {
      appName = "Netflix";
      brandBg = 0x9800; // Dark red
      brandBorder = ST77XX_RED;
    }

    // Outer card
    tft.fillRoundRect(20, 42, 200, 75, 10, brandBg);
    tft.drawRoundRect(20, 42, 200, 75, 10, brandBorder);

    // Text "LAUNCHING..."
    tft.setTextSize(1);
    tft.setTextColor(textColor == ST77XX_BLACK ? ST77XX_BLACK : ST77XX_WHITE);
    tft.setCursor(85, 52);
    tft.print("LAUNCHING...");

    // App Name in center
    tft.setTextSize(2);
    tft.setTextColor(textColor);
    int textX = 120 - (appName.length() * 12) / 2;
    tft.setCursor(textX, 74);
    tft.print(appName);
  } else {
    // Compact D-pad (Narrowed and shifted up to fit vertically)
    // Button size: 50x25. Center X = 120.
    tft.fillRoundRect(95, 45, 50, 25, 5, lastKey=="up" ? ST77XX_YELLOW : ST77XX_DARKGREY);
    tft.setCursor(115, 53); tft.setTextColor(ST77XX_WHITE); tft.print("^");

    tft.fillRoundRect(95, 105, 50, 25, 5, lastKey=="down" ? ST77XX_YELLOW : ST77XX_DARKGREY);
    tft.setCursor(115, 113); tft.print("v");

    tft.fillRoundRect(40, 75, 50, 25, 5, lastKey=="left" ? ST77XX_YELLOW : ST77XX_DARKGREY);
    tft.setCursor(60, 83);  tft.print("<");

    tft.fillRoundRect(150, 75, 50, 25, 5, lastKey=="right" ? ST77XX_YELLOW : ST77XX_DARKGREY);
    tft.setCursor(170, 83); tft.print(">");

    tft.fillRoundRect(95, 75, 50, 25, 5, lastKey=="ok" ? ST77XX_BLUE : ST77XX_DARKGREY);
    tft.setCursor(110, 83); tft.print("OK");
  }

  // 4. Status Bar
  tft.setTextSize(1);
  tft.setCursor(6, 125);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("Last Key: ");
  tft.setTextColor(ST77XX_YELLOW);

  // Format lastKey for user-friendly display
  String displayKey = lastKey;
  if (lastKey == "app_youtube") displayKey = "YouTube";
  else if (lastKey == "app_espn") displayKey = "ESPN";
  else if (lastKey == "app_netflix") displayKey = "Netflix";
  else if (lastKey == "app_prime") displayKey = "Prime Video";
  else if (lastKey == "app_paramount") displayKey = "Paramount+";
  else if (lastKey == "app_hbo") displayKey = "HBO Max";
  else if (lastKey == "playpause") displayKey = "Play/Pause";
  else if (lastKey == "forward") displayKey = "Forward";
  else if (lastKey == "rewind") displayKey = "Rewind";

  tft.print(displayKey);
}

void sendKey(const String &key) {
  if (key == "") return;
  lastKey = key;
  uiNeedsUpdate = true;

  if      (key == "up")    Keyboard.write(KEY_UP_ARROW);
  else if (key == "down")  Keyboard.write(KEY_DOWN_ARROW);
  else if (key == "left")  Keyboard.write(KEY_LEFT_ARROW);
  else if (key == "right") Keyboard.write(KEY_RIGHT_ARROW);
  else if (key == "ok")    Keyboard.write(KEY_RETURN);
  else if (key == "back") { ConsumerControl.press(HID_USAGE_CONSUMER_AC_BACK); delay(50); ConsumerControl.release(); }
  else if (key == "home") { ConsumerControl.press(HID_USAGE_CONSUMER_AC_HOME); delay(50); ConsumerControl.release(); }
  else if (key == "guide"){ ConsumerControl.press(0x008D); delay(50); ConsumerControl.release(); }
  else if (key == "chup") { ConsumerControl.press(0x009C); delay(50); ConsumerControl.release(); }
  else if (key == "chdown"){ ConsumerControl.press(0x009D); delay(50); ConsumerControl.release(); }
  else if (key == "apps") {
    ConsumerControl.press(HID_USAGE_CONSUMER_AC_HOME);
    delay(800);
    ConsumerControl.release();
  }
  else if (key == "rewind")        { ConsumerControl.press(0x00B4); delay(50); ConsumerControl.release(); }
  else if (key == "playpause")     { ConsumerControl.press(0x00CD); delay(50); ConsumerControl.release(); }
  else if (key == "forward")       { ConsumerControl.press(0x00B3); delay(50); ConsumerControl.release(); }
  else if (key == "app_youtube")   Keyboard.write(KEY_F1);
  else if (key == "app_espn")      Keyboard.write(KEY_F2);
  else if (key == "app_paramount") Keyboard.write(KEY_F3);
  else if (key == "app_hbo")       Keyboard.write(KEY_F4);
  else if (key == "app_prime")     Keyboard.write(KEY_F5);
  else if (key == "app_netflix")   Keyboard.write(KEY_F6);
}

// ---------- Original Web Look & Feel with Icons ----------
const char* REMOTE_HTML = R"HTML(
<!doctype html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
<title>ESP32 TiVo Remote</title>
<style>
  :root {
    --bg: #0b0d10;
    --card: rgba(23, 27, 34, 0.7);
    --border: rgba(255, 255, 255, 0.08);
    --btn-bg: rgba(35, 41, 53, 0.8);
    --btn-active: rgba(55, 65, 83, 1);
    --text: #e2e8f0;
    --accent: #00d2ff;
  }
  * { box-sizing: border-box; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
  body {
    margin: 0;
    background: radial-gradient(circle at center, #161a22 0%, #080a0d 100%);
    color: var(--text);
    display: flex;
    min-height: 100vh;
    align-items: center;
    justify-content: center;
    padding: 12px;
    touch-action: none;
  }
  .wrap {
    width: min(400px, 94vw);
    background: var(--card);
    backdrop-filter: blur(25px);
    -webkit-backdrop-filter: blur(25px);
    border: 1px solid var(--border);
    border-radius: 36px;
    padding: 24px 20px;
    box-shadow: 0 30px 60px rgba(0,0,0,0.6), inset 0 1px 0 rgba(255,255,255,0.05);
  }
  .header {
    text-align: center;
    margin-bottom: 16px;
  }
  .header h1 {
    font-size: 22px;
    font-weight: 700;
    margin: 0;
    letter-spacing: 0.5px;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    gap: 8px;
    color: #ffffff;
  }
  .header .dot {
    display: inline-block;
    width: 8px;
    height: 8px;
    background: #48bb78;
    border-radius: 50%;
    box-shadow: 0 0 8px #48bb78;
  }
  .pad {
    position: relative;
    background: radial-gradient(circle at 50% 50%, rgba(35, 41, 53, 0.4) 0%, rgba(15, 18, 24, 0.85) 80%);
    border: 1px solid rgba(255, 255, 255, 0.05);
    border-radius: 50%;
    aspect-ratio: 1/1;
    width: 100%;
    box-shadow: inset 0 0 30px rgba(0,0,0,0.6), 0 15px 35px rgba(0,0,0,0.4);
    margin-bottom: 20px;
  }
  .ring {
    position: absolute;
    inset: 4%;
    border-radius: 50%;
    border: 1px dashed rgba(255, 255, 255, 0.04);
    pointer-events: none;
  }
  .btn {
    position: absolute;
    display: flex;
    align-items: center;
    justify-content: center;
    border-radius: 50%;
    background: var(--btn-bg);
    color: #fff;
    cursor: pointer;
    user-select: none;
    transition: all 0.12s cubic-bezier(0.4, 0, 0.2, 1);
    border: 1px solid rgba(255, 255, 255, 0.03);
    box-shadow: 0 6px 12px rgba(0,0,0,0.3);
  }
  .btn:hover {
    background: rgba(48, 56, 74, 0.9);
    border-color: rgba(255, 255, 255, 0.08);
  }
  .btn.pressed {
    background: var(--accent);
    color: #000;
    transform: scale(0.92);
    box-shadow: 0 0 15px var(--accent);
  }
  .ok {
    position: absolute;
    inset: 31%;
    background: var(--btn-bg);
    color: #fff;
    border-radius: 50%;
    display: flex;
    align-items: center;
    justify-content: center;
    font-weight: 700;
    font-size: 18px;
    cursor: pointer;
    border: 1px solid rgba(255, 255, 255, 0.05);
    box-shadow: 0 8px 20px rgba(0,0,0,0.4), inset 0 1px 0 rgba(255,255,255,0.05);
    transition: all 0.12s ease;
  }
  .ok:hover {
    background: rgba(48, 56, 74, 0.9);
    border-color: rgba(255, 255, 255, 0.08);
  }
  .ok.pressed {
    background: var(--accent);
    color: #000;
    transform: scale(0.92);
    box-shadow: 0 0 20px var(--accent);
  }
  .up { top: 6%; left: 35%; right: 35%; height: 18%; border-radius: 24px 24px 12px 12px; font-size: 26px; }
  .down { bottom: 6%; left: 35%; right: 35%; height: 18%; border-radius: 12px 12px 24px 24px; font-size: 26px; }
  .left { left: 6%; top: 35%; bottom: 35%; width: 18%; border-radius: 24px 12px 12px 24px; font-size: 26px; }
  .right { right: 6%; top: 35%; bottom: 35%; width: 18%; border-radius: 12px 24px 24px 12px; font-size: 26px; }

  .row3 { display: flex; justify-content: space-between; gap: 10px; margin-top: 10px; }
  .small {
    flex: 1;
    height: 52px;
    border-radius: 14px;
    background: var(--btn-bg);
    border: 1px solid rgba(255, 255, 255, 0.04);
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    font-weight: 600;
    cursor: pointer;
    font-size: 11px;
    gap: 2px;
    transition: all 0.12s ease;
  }
  .small:hover {
    background: rgba(48, 56, 74, 0.9);
    border-color: rgba(255, 255, 255, 0.08);
  }
  .small.pressed {
    background: var(--btn-active);
    transform: scale(0.94);
  }
  .small span { font-size: 16px; }
  .small.media {
    height: 34px;
    border-radius: 10px;
    font-size: 10px;
    flex-direction: row;
    gap: 4px;
  }
  .small.media span { font-size: 10px; }

  .app-grid {
    display: grid;
    grid-template-columns: repeat(5, 1fr);
    gap: 6px;
  }
  .app-btn {
    height: 48px;
    border-radius: 12px;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    cursor: pointer;
    font-weight: 700;
    font-size: 8px;
    gap: 2px;
    transition: all 0.2s cubic-bezier(0.4, 0, 0.2, 1);
    border: 1px solid rgba(255, 255, 255, 0.03);
    background: rgba(15, 18, 24, 0.6);
  }
  .app-btn.pressed {
    transform: scale(0.92);
  }

  .app-yt { color: #ff0000; }
  .app-yt:hover { background: rgba(255, 0, 0, 0.12); border-color: rgba(255, 0, 0, 0.35); box-shadow: 0 0 15px rgba(255, 0, 0, 0.2); }
  .app-yt.pressed { background: #ff0000; color: #fff; box-shadow: 0 0 20px rgba(255, 0, 0, 0.4); }

  .app-espn { color: #ff3c00; }
  .app-espn:hover { background: rgba(255, 60, 0, 0.12); border-color: rgba(255, 60, 0, 0.35); box-shadow: 0 0 15px rgba(255, 60, 0, 0.2); }
  .app-espn.pressed { background: #ff3c00; color: #fff; box-shadow: 0 0 20px rgba(255, 60, 0, 0.4); }

  .app-para { color: #3182ce; }
  .app-para:hover { background: rgba(49, 130, 206, 0.12); border-color: rgba(49, 130, 206, 0.35); box-shadow: 0 0 15px rgba(49, 130, 206, 0.2); }
  .app-para.pressed { background: #3182ce; color: #fff; box-shadow: 0 0 20px rgba(49, 130, 206, 0.4); }

  .app-hbo { color: #9f7aea; }
  .app-hbo:hover { background: rgba(159, 122, 234, 0.12); border-color: rgba(159, 122, 234, 0.35); box-shadow: 0 0 15px rgba(159, 122, 234, 0.2); }
  .app-hbo.pressed { background: #9f7aea; color: #fff; box-shadow: 0 0 20px rgba(159, 122, 234, 0.4); }

  .app-prime { color: #00a8e1; }
  .app-prime:hover { background: rgba(0, 168, 225, 0.12); border-color: rgba(0, 168, 225, 0.35); box-shadow: 0 0 15px rgba(0, 168, 225, 0.2); }
  .app-prime.pressed { background: #00a8e1; color: #fff; box-shadow: 0 0 20px rgba(0, 168, 225, 0.4); }

  .app-btn span { font-size: 15px; font-weight: bold; }

  .ip { opacity: .25; text-align: center; margin-top: 14px; font-size: 10px; }
</style></head>
<body>
<div class="wrap">
  <div class="header">
    <h1>TiVo Remote <span class="dot"></span></h1>
  </div>

  <div class="pad">
    <div class="ring"></div>
    <div class="btn up" data-k="up">↑</div><div class="btn down" data-k="down">↓</div>
    <div class="btn left" data-k="left">←</div><div class="btn right" data-k="right">→</div>
    <div class="ok" data-k="ok">OK</div>
  </div>

  <div class="row3" style="margin-top: 12px;">
    <div class="small" data-k="back"><span>↩</span>Back</div>
    <div class="small" data-k="home"><span>⌂</span>TiVo</div>
    <div class="small" data-k="apps"><span>▦</span>Apps</div>
  </div>
  <div class="row3" style="margin-top: 10px;">
    <div class="small media" data-k="chdown"><span>−</span> CH</div>
    <div class="small media" data-k="guide"><span>≡</span> Guide</div>
    <div class="small media" data-k="chup"><span>+</span> CH</div>
  </div>
  <div class="row3" style="margin-top: 10px; margin-bottom: 18px;">
    <div class="small media" data-k="rewind"><span style="font-size: 9px;">◀◀</span></div>
    <div class="small media" data-k="playpause"><span style="font-size: 9px;">▶‖</span></div>
    <div class="small media" data-k="forward"><span style="font-size: 9px;">▶▶</span></div>
  </div>

  <div class="app-grid">
    <div class="app-btn app-yt" data-k="app_youtube"><span>▶</span>YouTube</div>
    <div class="app-btn app-espn" data-k="app_espn"><span>★</span>ESPN</div>
    <div class="app-btn app-para" data-k="app_paramount"><span>P+</span>Paramount</div>
    <div class="app-btn app-hbo" data-k="app_hbo"><span>H</span>HBO</div>
    <div class="app-btn app-prime" data-k="app_prime"><span>a</span>Prime</div>
  </div>

  <div class="ip" id="ip"></div>
</div>
<script>
  function flash(el){ if(!el)return; el.classList.add('pressed'); setTimeout(()=>el.classList.remove('pressed'),140); }
  async function send(k, el){
    flash(el);
    try { await fetch('/k?key='+k, {method:'POST'}); } catch(e){}
  }
  document.querySelectorAll('[data-k]').forEach(el=>{
    el.addEventListener('pointerdown', (e)=>{ e.preventDefault(); send(el.dataset.k, el); });
  });
  fetch('/ip').then(r=>r.text()).then(t=>document.getElementById('ip').textContent='Device IP: '+t);
</script></body></html>
)HTML";

void setup() {
  // SPI & LCD
  hspi->begin(TFT_SCLK, -1, TFT_MOSI, -1);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.init(135, 240); // Standard for ESP32-S3 GEEK
  tft.setRotation(3); // Landscape
  tft.invertDisplay(true); // Fixes inverted colors on IPS

  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 10);
  tft.println("Connecting WiFi...");

  // Optimized Wi-Fi sequence to prevent HID interference
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 30) {
    delay(500);
    tft.print(".");
    retry++;
  }

  // Update UI after connection
  drawUI();

  // Initialize HID after Wi-Fi
  USB.begin();
  Keyboard.begin();
  ConsumerControl.begin();

  server.on("/", [](){ server.send(200, "text/html", REMOTE_HTML); });
  server.on("/k", [](){ if(server.hasArg("key")) sendKey(server.arg("key")); server.send(200); });
  server.on("/ip", [](){ server.send(200, "text/plain", WiFi.localIP().toString()); });
  server.begin();

  uiNeedsUpdate = true;
}

void loop() {
  server.handleClient();
  if (uiNeedsUpdate) { drawUI(); uiNeedsUpdate = false; }
}

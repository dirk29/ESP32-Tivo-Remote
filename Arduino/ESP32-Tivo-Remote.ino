#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDConsumerControl.h"

// ---------- Wi-Fi Credentials ----------
#define WIFI_SSID      "DNET"
#define WIFI_PASSWORD  "tinyfinch802"

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

  // 3. Compact D-pad (Narrowed and shifted up to fit vertically)
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

  // 4. Status Bar
  tft.setTextSize(1);
  tft.setCursor(6, 125);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("Last Key: ");
  tft.setTextColor(ST77XX_YELLOW);
  tft.print(lastKey);
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
}

// ---------- Original Web Look & Feel with Icons ----------
const char* REMOTE_HTML = R"HTML(
<!doctype html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
<title>ESP32 TiVo Remote</title>
<style>
  :root { --bg:#111; --card:#1b1b1b; --muted:#2a2a2a; --accent:#e6e6e6; --active:#3a3a3a; }
  * { box-sizing:border-box; font-family:system-ui, sans-serif; }
  body { margin:0; background:var(--bg); color:#eee; display:flex; min-height:100vh; align-items:center; justify-content:center; touch-action:none; }
  .wrap { width:min(440px,92vw); padding:16px; }
  .pad { position:relative; background:var(--card); border-radius:50%; aspect-ratio:1/1; width:100%; box-shadow:0 10px 30px rgba(0,0,0,.4); margin-bottom:20px; }
  .ring { position:absolute; inset:6%; border-radius:50%; background:radial-gradient(circle at 50% 50%, #2a2a2a 0, #1a1a1a 65%); }
  .btn { position:absolute; display:flex; align-items:center; justify-content:center; border-radius:14px; background:#2a2a2a; color:#fff; cursor:pointer; user-select:none; transition:transform .05s ease; }
  .btn.pressed { background:var(--active); transform:scale(.95); }
  .ok { position:absolute; inset:30%; background:#ddd; color:#111; border-radius:50%; display:flex; align-items:center; justify-content:center; font-weight:700; cursor:pointer; }
  .ok.pressed { background:#bbb; }
  .up { top:6%; left:35%; right:35%; height:16%; font-size:28px; }
  .down { bottom:6%; left:35%; right:35%; height:16%; font-size:28px; }
  .left { left:6%; top:35%; bottom:35%; width:16%; font-size:28px; }
  .right { right:6%;  top:35%; bottom:35%; width:16%; font-size:28px; }
  .row3 { display:flex; justify-content:center; gap:12px; margin-top:14px; }
  .small { flex:1; height:64px; border-radius:18px; background:#2a2a2a; display:flex; align-items:center; justify-content:center; font-weight:600; cursor:pointer; font-size:14px; }
  .small.pressed { background:var(--active); }
  /* Icons as requested */
  .home::before { content:"⌂"; font-size:26px; margin-right:8px; }
  .back::before { content:"↩"; font-size:22px; margin-right:8px; }
  .apps::before { content:"▦"; font-size:24px; margin-right:8px; }
  .guide::before { content:"≡"; font-size:24px; margin-right:8px; }
  .ip { opacity:.4; text-align:center; margin-top:20px; font-size:12px; }
</style></head>
<body>
<div class="wrap">
  <div class="pad">
    <div class="ring"></div>
    <div class="btn up" data-k="up">↑</div><div class="btn down" data-k="down">↓</div>
    <div class="btn left" data-k="left">←</div><div class="btn right" data-k="right">→</div>
    <div class="ok" data-k="ok">OK</div>
  </div>
  <div class="row3">
    <div class="small back" data-k="back">Back</div>
    <div class="small home" data-k="home">TiVo</div>
    <div class="small apps" data-k="apps">Apps</div>
  </div>
  <div class="row3">
    <div class="small" data-k="chdown">CH −</div>
    <div class="small guide" data-k="guide">Guide</div>
    <div class="small" data-k="chup">CH +</div>
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
  fetch('/ip').then(r=>r.text()).then(t=>document.getElementById('ip').textContent='Device: '+t);
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



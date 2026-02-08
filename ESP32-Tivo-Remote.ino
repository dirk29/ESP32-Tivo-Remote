#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDConsumerControl.h"

// ---------- Wi-Fi ----------
#define WIFI_SSID      "MY_WIFI_SSID"
#define WIFI_PASSWORD  "MY_WIFI_PASSWORD"

// ---------- TFT (Waveshare ESP32-S3 GEEK 1.14” ST7789) ----------
#define TFT_CS    37
#define TFT_RST   33
#define TFT_DC    38
#define TFT_MOSI  35  // Maps to HSPI MOSI
#define TFT_SCLK  36  // Maps to HSPI SCLK
#define TFT_BL    34
#define ST77XX_DARKGREY 0x7BEF

SPIClass *hspi = new SPIClass(HSPI);
Adafruit_ST7789 tft = Adafruit_ST7789(hspi, TFT_CS, TFT_DC, TFT_RST);

// ---------- Web + HID ----------
WebServer server(80);
USBHIDKeyboard Keyboard;
USBHIDConsumerControl ConsumerControl;
String lastKey = "";

// ---------- UI drawing on the TFT ----------
void drawUI() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setCursor(6, 6);
  if (WiFi.status() == WL_CONNECTED) {
    tft.setTextColor(ST77XX_GREEN);
    tft.print("WiFi OK ");
    tft.setTextColor(ST77XX_CYAN);
    tft.println(WiFi.localIP());
  } else {
    tft.setTextColor(ST77XX_RED);
    tft.println("WiFi Connecting...");
  }

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(6, 26);
  tft.print("Connected to TiVo");

  // D-pad visuals
  tft.fillRoundRect(90, 40, 60, 40, 8, lastKey=="up" ? ST77XX_YELLOW : ST77XX_DARKGREY);
  tft.setCursor(112, 50); tft.print("^");

  // Down button (tight to bottom of 135px screen)
  tft.fillRoundRect(90, 120, 60, 40, 8, lastKey=="down" ? ST77XX_YELLOW : ST77XX_DARKGREY);
  tft.setCursor(108, 130); tft.print("v");

  tft.fillRoundRect(40, 80, 60, 40, 8, lastKey=="left" ? ST77XX_YELLOW : ST77XX_DARKGREY);
  tft.setCursor(60, 90);  tft.print("<");

  tft.fillRoundRect(140, 80, 60, 40, 8, lastKey=="right" ? ST77XX_YELLOW : ST77XX_DARKGREY);
  tft.setCursor(160, 90); tft.print(">");

  tft.fillRoundRect(90, 80, 60, 40, 8, lastKey=="ok" ? ST77XX_BLUE : ST77XX_DARKGREY);
  tft.setCursor(110, 90); tft.print("OK");

  tft.fillRoundRect(10, 5, 40, 25, 5, lastKey=="back" ? ST77XX_RED : ST77XX_DARKGREY);
  tft.setTextSize(1); tft.setCursor(20, 12); tft.print("<-");

  tft.fillRoundRect(200, 5, 40, 25, 5, lastKey=="home" ? ST77XX_ORANGE : ST77XX_DARKGREY);
  tft.setCursor(210, 12); tft.print("Home");

  tft.setTextSize(2);
  tft.setCursor(6, 118);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("Last: ");
  tft.setTextColor(ST77XX_YELLOW);
  tft.print(lastKey);
}

// ---------- Send a key over USB HID ----------
void sendKey(const String &key) {
  lastKey = key;

  if      (key == "up")    Keyboard.write(KEY_UP_ARROW);
  else if (key == "down")  Keyboard.write(KEY_DOWN_ARROW);
  else if (key == "left")  Keyboard.write(KEY_LEFT_ARROW);
  else if (key == "right") Keyboard.write(KEY_RIGHT_ARROW);
  else if (key == "ok")    Keyboard.write(KEY_RETURN);

  else if (key == "back") {
    ConsumerControl.press(HID_USAGE_CONSUMER_AC_BACK);
    ConsumerControl.release();
  }
  else if (key == "home") {
    ConsumerControl.press(HID_USAGE_CONSUMER_AC_HOME);
    ConsumerControl.release();
  }

  // Program Guide (Consumer page 0x0C, Usage ID 0x008D)
  else if (key == "guide") {
    ConsumerControl.press(0x008D); // Media Select Program Guide
    ConsumerControl.release();
  }

  // Apps / Smart Hub (Consumer page 0x0C, Usage ID 0x0182)
  else if (key == "apps") {
    // Try long-press HOME (many Android TV / TiVo UIs bind Apps to this)
    ConsumerControl.press(HID_USAGE_CONSUMER_AC_HOME);
    delay(800);                 // tweak: 600–1200ms
    ConsumerControl.release();
  }

  // Channel Up/Down (Consumer page 0x0C, Usage IDs 0x009C/0x009D)
  else if (key == "chup") {
    ConsumerControl.press(0x009C); // Channel Increment
    ConsumerControl.release();
  }
  else if (key == "chdown") {
    ConsumerControl.press(0x009D); // Channel Decrement
    ConsumerControl.release();
  }

  drawUI();
}

// ---------- Web page (UTF-8, highlight, keyboard shortcuts) ----------
const char* REMOTE_HTML = R"HTML(
<!doctype html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32 TV Remote</title>

<style>
  :root {
    --bg:#111;
    --card:#1b1b1b;
    --muted:#2a2a2a;
    --accent:#e6e6e6;
    --active:#3a3a3a;
  }

  * {
    box-sizing:border-box;
    font-family:system-ui, Segoe UI, Roboto, Arial;
  }

  body {
    margin:0;
    background:var(--bg);
    color:#eee;
    display:flex;
    min-height:100vh;
    align-items:center;
    justify-content:center;
  }

  .wrap {
    width:min(440px,92vw);
    padding:16px;
  }

  /* ---------- D-pad ---------- */
  .pad {
    position:relative;
    background:var(--card);
    border-radius:50%;
    aspect-ratio:1/1;
    indicatesize:100%;
    width:100%;
    box-shadow:0 10px 30px rgba(0,0,0,.4);
  }

  .ring {
    position:absolute;
    inset:6%;
    border-radius:50%;
    background:radial-gradient(circle at 50% 50%, #2a2a2a 0, #1a1a1a 65%);
  }

  .btn {
    position:absolute;
    display:flex;
    align-items:center;
    justify-content:center;
    border-radius:14px;
    background:#2a2a2a;
    color:#fff;
    cursor:pointer;
    user-select:none;
    transition:transform .05s ease, background .15s ease;
  }

  .btn.pressed {
    background:var(--active);
    transform:scale(.97);
  }

  .ok {
    position:absolute;
    inset:30%;
    background:#ddd;
    color:#111;
    border-radius:50%;
    display:flex;
    align-items:center;
    justify-content:center;
    font-weight:700;
    cursor:pointer;
  }

  .ok.pressed { background:#bbb; }

  .arrow { font-size:28px; }

  .up    { top:6%;    left:35%; right:35%; height:16%; }
  .down  { bottom:6%; left:35%; right:35%; height:16%; }
  .left  { left:6%;   top:35%; bottom:35%; width:16%; }
  .right { right:6%;  top:35%; bottom:35%; width:16%; }

  /* ---------- Buttons ---------- */
  .small {
    width:96px;
    height:64px;
    border-radius:18px;
    background:#2a2a2a;
    display:flex;
    align-items:center;
    justify-content:center;
    font-weight:600;
    cursor:pointer;
  }

  .small.pressed { background:var(--active); }

  /* ---------- Rows ---------- */
  .row3 {
    display:flex;
    justify-content:center;
    gap:18px;
    margin-top:14px;
  }

  .row3tight {
    display:flex;
    justify-content:center;
    gap:18px;
    margin-top:14px;
  }

  /* Column cells for CH row */
  .colcell {
    width:96px;                 /* SAME width as Back / Apps */
    display:flex;
    align-items:center;
  }

  .colcell.left  { justify-content:flex-start; }
  .colcell.mid   { justify-content:center; }
  .colcell.right { justify-content:flex-end; }

  /* Channel buttons (unchanged size) */
  .small.chbtn {
    width:72px;
    height:52px;
    border-radius:16px;
    font-weight:700;
  }

  /* Guide matches CH row height */
  .small.guide {
    width:96px;
    height:52px;
    border-radius:16px;
    font-weight:700;
  }

  /* ---------- Icons ---------- */
  .home::before  { content:"⌂"; font-size:28px; margin-right:8px; }
  .back::before  { content:"↩"; font-size:24px; margin-right:8px; }
  .apps::before  { content:"▦"; font-size:26px; margin-right:8px; }
  .guide::before { content:"≡"; font-size:26px; margin-right:8px; }

  .apps { opacity:.9; }

  .ip {
    opacity:.8;
    text-align:center;
    margin:10px 0 2px 0;
    font-size:14px;
  }
</style>
</head>

<body>
<div class="wrap">

  <!-- D-pad -->
  <div class="pad" id="pad">
    <div class="ring"></div>
    <div class="btn up arrow"    data-k="up">↑</div>
    <div class="btn down arrow"  data-k="down">↓</div>
    <div class="btn left arrow"  data-k="left">←</div>
    <div class="btn right arrow" data-k="right">→</div>
    <div class="ok" data-k="ok">OK</div>
  </div>

  <!-- Row 1 -->
  <div class="row3">
    <div class="small back" data-k="back">Back</div>
    <div class="small home" data-k="home">Tivo</div>
    <div class="small apps" data-k="apps">Apps</div>
  </div>

  <!-- Row 2 -->
  <div class="row3tight">
    <div class="colcell left">
      <div class="small chbtn chdown" data-k="chdown">CH−</div>
    </div>

    <div class="colcell mid">
      <div class="small guide" data-k="guide">Guide</div>
    </div>

    <div class="colcell right">
      <div class="small chbtn chup" data-k="chup">CH+</div>
    </div>
  </div>

  <div class="ip" id="ip"></div>
</div>

<script>
function flash(el){
  if(!el) return;
  el.classList.add('pressed');
  setTimeout(()=>el.classList.remove('pressed'),140);
}

async function sendKey(k, el){
  try { await fetch('/k?key='+encodeURIComponent(k), {method:'POST'}); }
  catch(e){}
  flash(el);
}

document.querySelectorAll('[data-k]').forEach(el=>{
  el.addEventListener('click', ()=>sendKey(el.dataset.k, el));
  el.addEventListener('touchstart', ()=>flash(el), {passive:true});
});

document.addEventListener('keydown', e=>{
  let k=null;
  switch(e.key){
    case 'ArrowUp':    k='up'; break;
    case 'ArrowDown':  k='down'; break;
    case 'ArrowLeft':  k='left'; break;
    case 'ArrowRight': k='right'; break;
    case 'Enter':      k='ok'; break;
    case 'Backspace':
    case 'Escape':     k='back'; break;
    case 'h': case 'H':k='home'; break;
    case 'a': case 'A':k='apps'; break;
    case 'g': case 'G':k='guide'; break;
    case 'PageUp':     k='chup'; break;
    case 'PageDown':   k='chdown'; break;
  }
  if(k){
    e.preventDefault();
    const el=document.querySelector('[data-k="'+k+'"]');
    sendKey(k, el);
  }
});

fetch('/ip')
  .then(r=>r.text())
  .then(t=>document.getElementById('ip').textContent='Device: '+t);
</script>
</body>
</html>
)HTML";



// ---------- Web handlers ----------
void handleRoot() {
  server.sendHeader("Cache-Control","no-store");
  server.send(200, "text/html; charset=utf-8", REMOTE_HTML);
}
void handleKey() {
  if (!server.hasArg("key")) { server.send(400, "text/plain", "missing key"); return; }
  String k = server.arg("key"); k.toLowerCase();
  sendKey(k);
  server.send(200, "text/plain", "OK");
}
void handleIP() {
  server.send(200, "text/plain", WiFi.localIP().toString());
}

// ---------- Setup ----------
void setup() {
  hspi->begin(TFT_SCLK, -1, TFT_MOSI, -1);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.init(135, 240);
  tft.setRotation(3);

  tft.fillScreen(ST77XX_RED); delay(400);
  tft.fillScreen(ST77XX_GREEN); delay(400);
  tft.fillScreen(ST77XX_BLUE); delay(400);

  drawUI();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis()-start < 15000) {
    delay(300);
    drawUI();
  }

  drawUI();

  USB.begin();
  Keyboard.begin();
  ConsumerControl.begin();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/k", HTTP_POST, handleKey);
  server.on("/ip", HTTP_GET, handleIP);

  server.on("/up",    [](){ sendKey("up");    server.send(200,"text/plain","OK"); });
  server.on("/down",  [](){ sendKey("down");  server.send(200,"text/plain","OK"); });
  server.on("/left",  [](){ sendKey("left");  server.send(200,"text/plain","OK"); });
  server.on("/right", [](){ sendKey("right"); server.send(200,"text/plain","OK"); });
  server.on("/ok",    [](){ sendKey("ok");    server.send(200,"text/plain","OK"); });
  server.on("/back",  [](){ sendKey("back");  server.send(200,"text/plain","OK"); });
  server.on("/home",  [](){ sendKey("home");  server.send(200,"text/plain","OK"); });
  server.on("/guide", [](){ sendKey("guide"); server.send(200,"text/plain","OK"); });
  server.on("/apps",  [](){ sendKey("apps");  server.send(200,"text/plain","OK"); });
  server.on("/chup",  [](){ sendKey("chup");  server.send(200,"text/plain","OK"); });
  server.on("/chdown",[](){ sendKey("chdown");server.send(200,"text/plain","OK"); });

  server.begin();
}

// ---------- Loop ----------
void loop() {
  server.handleClient();
}



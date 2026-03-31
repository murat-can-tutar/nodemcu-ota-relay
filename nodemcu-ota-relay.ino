#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <Servo.h>
#include <Updater.h>

// =========================
// WIFI
// =========================
const char* ssid = "RKS";
const char* password = "44AFT748";

// =========================
// WEB SERVER
// =========================
ESP8266WebServer server(80);

// =========================
// PINLER - NodeMCU ESP8266
// LOW aktif röle kartına göre
// =========================
#define RELAY_LOCK_OPEN   D1   // GPIO5
#define RELAY_LOCK_CLOSE  D2   // GPIO4
#define RELAY_START       D5   // GPIO14
#define RELAY_ALARM       D6   // GPIO12
#define RELAY_LED         D7   // GPIO13
#define RELAY_FOG         D0   // GPIO16
#define RELAY_HEADLIGHT   D3   // GPIO0
#define RELAY_SEAT        D4   // GPIO2

// Servo pinleri
// Not: TX/RX kullanıldığı için Serial Monitor kullanma
#define SERVO_RIGHT_PIN   3    // RX / GPIO3
#define SERVO_LEFT_PIN    1    // TX / GPIO1

// Bağımsız servo tetik girişi
// NodeMCU A0 girişine sadece 0-3.3V ver
#define SERVO_TRIGGER_PIN A0
const int SERVO_TRIGGER_THRESHOLD = 600; // ~1.9V üstü = aktif

// =========================
// EEPROM
// =========================
#define EEPROM_SIZE 64
#define EEPROM_MAGIC 0x4D

struct MirrorConfig {
  uint8_t magic;
  uint8_t rightOpen;
  uint8_t leftOpen;
  uint8_t rightClosed;
  uint8_t leftClosed;
};

MirrorConfig mirrorCfg;

// =========================
// SERVO
// =========================
Servo servoRight;
Servo servoLeft;

int currentRightPos = 0;
int currentLeftPos = 0;
bool mirrorsOpen = false;

// =========================
// DURUMLAR
// =========================
bool motorRunning = false;

bool ledState = false;
bool fogState = false;
bool headlightState = false;

// koltuk pulse, toggle değil
bool otaUploadSuccess = false;

// =========================
// LOG
// =========================
String logLines[10];
uint8_t logCount = 0;

void addLog(const String &msg) {
  String line = msg;
  if (logCount < 10) {
    logLines[logCount++] = line;
  } else {
    for (uint8_t i = 0; i < 9; i++) {
      logLines[i] = logLines[i + 1];
    }
    logLines[9] = line;
  }
}

String getLogsHtml() {
  String out = "";
  for (uint8_t i = 0; i < logCount; i++) {
    out += logLines[i];
    if (i < logCount - 1) out += "<br>";
  }
  return out;
}

// =========================
// RÖLE YARDIMCI
// LOW aktif
// =========================
void relayOff(uint8_t pin) {
  digitalWrite(pin, HIGH);
}

void relayOn(uint8_t pin) {
  digitalWrite(pin, LOW);
}

void pulseRelay(uint8_t pin, uint8_t times = 1, uint16_t onMs = 250, uint16_t offMs = 250) {
  for (uint8_t i = 0; i < times; i++) {
    relayOn(pin);
    delay(onMs);
    relayOff(pin);
    if (i < times - 1) delay(offMs);
    yield();
  }
}

void setToggleRelay(uint8_t pin, bool stateOn) {
  digitalWrite(pin, stateOn ? LOW : HIGH);
}

// =========================
// EEPROM
// =========================
void loadMirrorConfig() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, mirrorCfg);

  if (mirrorCfg.magic != EEPROM_MAGIC ||
      mirrorCfg.rightOpen > 180 ||
      mirrorCfg.leftOpen > 180 ||
      mirrorCfg.rightClosed > 180 ||
      mirrorCfg.leftClosed > 180) {
    mirrorCfg.magic = EEPROM_MAGIC;
    mirrorCfg.rightOpen = 90;
    mirrorCfg.leftOpen = 90;
    mirrorCfg.rightClosed = 0;
    mirrorCfg.leftClosed = 0;
    EEPROM.put(0, mirrorCfg);
    EEPROM.commit();
  }
}

void saveMirrorConfig() {
  mirrorCfg.magic = EEPROM_MAGIC;
  EEPROM.put(0, mirrorCfg);
  EEPROM.commit();
}

// =========================
// SERVO
// =========================
void writeMirrorsNow(int r, int l) {
  currentRightPos = constrain(r, 0, 180);
  currentLeftPos = constrain(l, 0, 180);

  servoRight.write(currentRightPos);
  servoLeft.write(currentLeftPos);
}

void openMirrors() {
  writeMirrorsNow(mirrorCfg.rightOpen, mirrorCfg.leftOpen);
  mirrorsOpen = true;
  addLog("Aynalar acildi");
}

void closeMirrors() {
  writeMirrorsNow(mirrorCfg.rightClosed, mirrorCfg.leftClosed);
  mirrorsOpen = false;
  addLog("Aynalar kapandi");
}

// =========================
// HTML
// =========================
String page = R"rawliteral(
<!DOCTYPE html>
<html lang="tr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>RKS Motor Panel</title>
<style>
  body{
    font-family: Arial, sans-serif;
    background:#111;
    color:#eee;
    margin:0;
    padding:16px;
    text-align:center;
  }
  .wrap{
    max-width:700px;
    margin:0 auto;
  }
  .card{
    background:#1b1b1b;
    border:1px solid #333;
    border-radius:14px;
    padding:14px;
    margin-bottom:14px;
  }
  h2,h3{
    margin:8px 0 14px 0;
  }
  .row{
    display:flex;
    gap:10px;
    flex-wrap:wrap;
    justify-content:center;
    margin-bottom:10px;
  }
  button{
    min-width:130px;
    padding:12px 14px;
    border:none;
    border-radius:10px;
    color:white;
    font-size:15px;
    cursor:pointer;
  }
  button:disabled{
    opacity:0.45;
    cursor:not-allowed;
  }
  .red{background:#c62828;}
  .green{background:#2e7d32;}
  .blue{background:#1565c0;}
  .orange{background:#ef6c00;}
  .gray{background:#555;}
  .slider-wrap{
    text-align:left;
    margin:10px 0;
  }
  .slider-wrap label{
    display:block;
    margin-bottom:6px;
  }
  input[type=range]{
    width:100%;
  }
  .small{
    font-size:13px;
    color:#bbb;
  }
  #statusText{
    font-weight:bold;
  }
  #logBox{
    min-height:170px;
    max-height:170px;
    overflow:auto;
    text-align:left;
    background:#000;
    color:#00ff66;
    border:1px solid #333;
    border-radius:10px;
    padding:10px;
    font-family:monospace;
    font-size:13px;
  }
  input[type=file]{
    width:100%;
    color:#ddd;
    margin-bottom:10px;
  }
</style>
</head>
<body>
<div class="wrap">

  <div class="card">
    <h2>RKS Motor Kontrol Paneli</h2>
    <div>Durum: <span id="statusText">Baglaniyor...</span></div>
    <div class="small">Motor durumu ve pin islemleri alttaki log ekraninda gorunur.</div>
  </div>

  <div class="card">
    <h3>Motor</h3>
    <div class="row">
      <button id="motorBtn" class="red" onclick="sendCmd('motor')">Motoru Calistir</button>
    </div>
  </div>

  <div class="card">
    <h3>Kumanda</h3>
    <div class="row">
      <button id="lockOpenBtn" class="gray ctrl" onclick="sendCmd('lock_open')">Kilit Ac</button>
      <button id="lockCloseBtn" class="gray ctrl" onclick="sendCmd('lock_close')">Kilit Kapat</button>
      <button id="alarmBtn" class="orange ctrl" onclick="sendCmd('alarm')">Alarm</button>
    </div>
  </div>

  <div class="card">
    <h3>Aydinlatma</h3>
    <div class="row">
      <button id="farBtn" class="red ctrl" onclick="sendCmd('far')">Far</button>
      <button id="fogBtn" class="red ctrl" onclick="sendCmd('fog')">Sis</button>
      <button id="ledBtn" class="red ctrl" onclick="sendCmd('led')">LED</button>
      <button id="seatBtn" class="orange ctrl" onclick="sendCmd('seat')">Koltuk</button>
    </div>
  </div>

  <div class="card">
    <h3>Ayna Ayarlari</h3>

    <div class="slider-wrap">
      <label>Sag Acik Aci: <span id="roVal">90</span></label>
      <input id="ro" type="range" min="0" max="180" value="90" oninput="syncLabels()">
    </div>

    <div class="slider-wrap">
      <label>Sol Acik Aci: <span id="loVal">90</span></label>
      <input id="lo" type="range" min="0" max="180" value="90" oninput="syncLabels()">
    </div>

    <div class="slider-wrap">
      <label>Sag Kapali Aci: <span id="rcVal">0</span></label>
      <input id="rc" type="range" min="0" max="180" value="0" oninput="syncLabels()">
    </div>

    <div class="slider-wrap">
      <label>Sol Kapali Aci: <span id="lcVal">0</span></label>
      <input id="lc" type="range" min="0" max="180" value="0" oninput="syncLabels()">
    </div>

    <div class="row">
      <button id="saveMirrorBtn" class="blue ctrl" onclick="saveMirrors()">Ayna Ayarlarini Kaydet</button>
    </div>

    <div class="small">
      A0 tetik aktif oldugunda aynalar acik acilara gider, pasif oldugunda kapali acilara doner.
    </div>
  </div>

  <div class="card">
    <h3>OTA Guncelleme</h3>
    <form method="POST" action="/update" enctype="multipart/form-data">
      <input type="file" name="update">
      <button class="green" type="submit">BIN Dosyasi Yukle</button>
    </form>
  </div>

  <div class="card">
    <h3>Bilgi Ekrani (Son 10 Kayit)</h3>
    <div id="logBox"></div>
  </div>
</div>

<script>
let uiLoaded = false;

function syncLabels(){
  document.getElementById('roVal').textContent = document.getElementById('ro').value;
  document.getElementById('loVal').textContent = document.getElementById('lo').value;
  document.getElementById('rcVal').textContent = document.getElementById('rc').value;
  document.getElementById('lcVal').textContent = document.getElementById('lc').value;
}

function setControlsEnabled(enabled){
  document.querySelectorAll('.ctrl').forEach(el => el.disabled = !enabled);
}

function sendCmd(cmd){
  fetch('/cmd?c=' + encodeURIComponent(cmd), {cache:'no-store'})
    .then(() => setTimeout(updateStatus, 150))
    .catch(() => {
      document.getElementById('statusText').textContent = 'Baglanti Yok';
      setControlsEnabled(false);
    });
}

function saveMirrors(){
  const ro = document.getElementById('ro').value;
  const lo = document.getElementById('lo').value;
  const rc = document.getElementById('rc').value;
  const lc = document.getElementById('lc').value;

  fetch('/servo/save?ro=' + ro + '&lo=' + lo + '&rc=' + rc + '&lc=' + lc, {cache:'no-store'})
    .then(() => setTimeout(updateStatus, 150))
    .catch(() => {
      document.getElementById('statusText').textContent = 'Baglanti Yok';
      setControlsEnabled(false);
    });
}

function applyButtonStates(d){
  const motorBtn = document.getElementById('motorBtn');
  const farBtn = document.getElementById('farBtn');
  const fogBtn = document.getElementById('fogBtn');
  const ledBtn = document.getElementById('ledBtn');

  motorBtn.textContent = d.motor ? 'Motoru Kapat' : 'Motoru Calistir';
  motorBtn.className = d.motor ? 'green' : 'red';

  farBtn.className = d.far ? 'blue ctrl' : 'red ctrl';
  fogBtn.className = d.fog ? 'blue ctrl' : 'red ctrl';
  ledBtn.className = d.led ? 'green ctrl' : 'red ctrl';

  document.getElementById('lockOpenBtn').disabled = d.motor;
  document.getElementById('lockCloseBtn').disabled = d.motor;
  document.getElementById('alarmBtn').disabled = d.motor;
}

function updateStatus(){
  fetch('/status', {cache:'no-store'})
    .then(r => r.json())
    .then(d => {
      document.getElementById('statusText').textContent = d.status;
      setControlsEnabled(true);

      if(!uiLoaded){
        document.getElementById('ro').value = d.ro;
        document.getElementById('lo').value = d.lo;
        document.getElementById('rc').value = d.rc;
        document.getElementById('lc').value = d.lc;
        syncLabels();
        uiLoaded = true;
      }

      applyButtonStates(d);

      const logBox = document.getElementById('logBox');
      logBox.innerHTML = d.logs || '';
      logBox.scrollTop = logBox.scrollHeight;
    })
    .catch(() => {
      document.getElementById('statusText').textContent = 'Baglanti Yok';
      setControlsEnabled(false);
    });
}

syncLabels();
updateStatus();
setInterval(updateStatus, 1000);
</script>

</body>
</html>
)rawliteral";

// =========================
// STATUS JSON
// =========================
String buildStatusJson() {
  String json = "{";
  json += "\"status\":\"Baglandi\",";
  json += "\"motor\":" + String(motorRunning ? "true" : "false") + ",";
  json += "\"far\":" + String(headlightState ? "true" : "false") + ",";
  json += "\"fog\":" + String(fogState ? "true" : "false") + ",";
  json += "\"led\":" + String(ledState ? "true" : "false") + ",";
  json += "\"mirrorsOpen\":" + String(mirrorsOpen ? "true" : "false") + ",";
  json += "\"ro\":" + String(mirrorCfg.rightOpen) + ",";
  json += "\"lo\":" + String(mirrorCfg.leftOpen) + ",";
  json += "\"rc\":" + String(mirrorCfg.rightClosed) + ",";
  json += "\"lc\":" + String(mirrorCfg.leftClosed) + ",";
  json += "\"logs\":\"" + getLogsHtml() + "\"";
  json += "}";
  return json;
}

// =========================
// ROUTE HANDLERLAR
// =========================
void handleRoot() {
  server.send(200, "text/html; charset=utf-8", page);
}

void handleStatus() {
  server.send(200, "application/json", buildStatusJson());
}

void handleCommand() {
  String c = server.arg("c");

  // MOTOR TEK TUS MANTIGI
  if (c == "motor") {
    if (!motorRunning) {
      addLog("Motor start -> Kilit Ac [GPIO5]");
      pulseRelay(RELAY_LOCK_OPEN, 1, 250, 250);
      delay(350);

      addLog("Motor start -> Start 2 pulse [GPIO14]");
      pulseRelay(RELAY_START, 2, 250, 300);

      motorRunning = true;
      addLog("Motor calisiyor");
    } else {
      addLog("Motor stop -> Kilit Ac [GPIO5]");
      pulseRelay(RELAY_LOCK_OPEN, 1, 250, 250);

      motorRunning = false;
      addLog("Motor durduruldu");
    }

    server.send(200, "text/plain", "OK");
    return;
  }

  // MOTOR CALISIRKEN KILIT/ALARM KAPALI
  if (motorRunning) {
    if (c == "lock_open" || c == "lock_close" || c == "alarm") {
      addLog("Komut engellendi: motor acik");
      server.send(200, "text/plain", "LOCKED_WHILE_RUNNING");
      return;
    }
  }

  if (c == "lock_open") {
    pulseRelay(RELAY_LOCK_OPEN, 1, 250, 250);
    addLog("Kilit ac [GPIO5]");
  }
  else if (c == "lock_close") {
    pulseRelay(RELAY_LOCK_CLOSE, 1, 250, 250);
    addLog("Kilit kapat [GPIO4]");
  }
  else if (c == "alarm") {
    pulseRelay(RELAY_ALARM, 1, 300, 250);
    addLog("Alarm [GPIO12]");
  }
  else if (c == "far") {
    headlightState = !headlightState;
    setToggleRelay(RELAY_HEADLIGHT, headlightState);
    addLog(headlightState ? "Far acildi [GPIO0]" : "Far kapandi [GPIO0]");
  }
  else if (c == "fog") {
    fogState = !fogState;
    setToggleRelay(RELAY_FOG, fogState);
    addLog(fogState ? "Sis acildi [GPIO16]" : "Sis kapandi [GPIO16]");
  }
  else if (c == "led") {
    ledState = !ledState;
    setToggleRelay(RELAY_LED, ledState);
    addLog(ledState ? "LED acildi [GPIO13]" : "LED kapandi [GPIO13]");
  }
  else if (c == "seat") {
    addLog("Koltuk pulse [GPIO2]");
    pulseRelay(RELAY_SEAT, 1, 1200, 250);
  }

  server.send(200, "text/plain", "OK");
}

void handleServoSave() {
  if (!server.hasArg("ro") || !server.hasArg("lo") || !server.hasArg("rc") || !server.hasArg("lc")) {
    server.send(400, "text/plain", "MISSING_ARGS");
    return;
  }

  mirrorCfg.rightOpen   = constrain(server.arg("ro").toInt(), 0, 180);
  mirrorCfg.leftOpen    = constrain(server.arg("lo").toInt(), 0, 180);
  mirrorCfg.rightClosed = constrain(server.arg("rc").toInt(), 0, 180);
  mirrorCfg.leftClosed  = constrain(server.arg("lc").toInt(), 0, 180);

  saveMirrorConfig();

  addLog("Ayna ayarlari kaydedildi");

  // O anki tetik durumuna gore servolari hemen yeni pozisyona getir
  int triggerValue = analogRead(SERVO_TRIGGER_PIN);
  bool triggerActive = (triggerValue >= SERVO_TRIGGER_THRESHOLD);

  if (triggerActive) {
    writeMirrorsNow(mirrorCfg.rightOpen, mirrorCfg.leftOpen);
    mirrorsOpen = true;
    addLog("Kayit sonrasi acik pozisyon uygulandi");
  } else {
    writeMirrorsNow(mirrorCfg.rightClosed, mirrorCfg.leftClosed);
    mirrorsOpen = false;
    addLog("Kayit sonrasi kapali pozisyon uygulandi");
  }

  server.send(200, "text/plain", "OK");
}

// =========================
// OTA UPLOAD
// =========================
void handleUpdateUpload() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    otaUploadSuccess = false;
    WiFiUDP::stopAll();
    addLog("Web OTA basladi");

    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if (!Update.begin(maxSketchSpace)) {
      Update.printError(Serial);
    }
  }
  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  }
  else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      otaUploadSuccess = true;
      addLog("Web OTA tamamlandi");
    } else {
      Update.printError(Serial);
      otaUploadSuccess = false;
      addLog("Web OTA hata");
    }
  }
}

void handleUpdateFinish() {
  server.sendHeader("Connection", "close");
  if (otaUploadSuccess) {
    server.send(200, "text/plain", "GUNCELLEME_BASARILI");
    delay(500);
    ESP.restart();
  } else {
    server.send(500, "text/plain", "GUNCELLEME_HATA");
  }
}

// =========================
// SETUP
// =========================
void setup() {
  // Serial yok: TX/RX servo icin kullaniliyor
  loadMirrorConfig();

  pinMode(RELAY_LOCK_OPEN, OUTPUT);
  pinMode(RELAY_LOCK_CLOSE, OUTPUT);
  pinMode(RELAY_START, OUTPUT);
  pinMode(RELAY_ALARM, OUTPUT);
  pinMode(RELAY_LED, OUTPUT);
  pinMode(RELAY_FOG, OUTPUT);
  pinMode(RELAY_HEADLIGHT, OUTPUT);
  pinMode(RELAY_SEAT, OUTPUT);

  // Hepsi kapali
  relayOff(RELAY_LOCK_OPEN);
  relayOff(RELAY_LOCK_CLOSE);
  relayOff(RELAY_START);
  relayOff(RELAY_ALARM);
  relayOff(RELAY_LED);
  relayOff(RELAY_FOG);
  relayOff(RELAY_HEADLIGHT);
  relayOff(RELAY_SEAT);

  // Servo attach
  servoRight.attach(SERVO_RIGHT_PIN);
  servoLeft.attach(SERVO_LEFT_PIN);

  // Baslangicta kapali pozisyona al
  writeMirrorsNow(mirrorCfg.rightClosed, mirrorCfg.leftClosed);
  mirrorsOpen = false;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(350);
    yield();
  }

  ArduinoOTA.setHostname("RKS-Motor");
  ArduinoOTA.onStart([]() {
    addLog("Arduino OTA basladi");
  });
  ArduinoOTA.onEnd([]() {
    addLog("Arduino OTA tamamlandi");
  });
  ArduinoOTA.begin();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/cmd", HTTP_GET, handleCommand);
  server.on("/servo/save", HTTP_GET, handleServoSave);
  server.on("/update", HTTP_POST, handleUpdateFinish, handleUpdateUpload);

  server.begin();

  addLog("WiFi baglandi");
  addLog("IP: " + WiFi.localIP().toString());
  addLog("Panel hazir");
}

// =========================
// LOOP
// =========================
void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  static bool lastTriggerActive = false;
  static bool firstRead = true;

  int triggerValue = analogRead(SERVO_TRIGGER_PIN);
  bool triggerActive = (triggerValue >= SERVO_TRIGGER_THRESHOLD);

  if (firstRead) {
    lastTriggerActive = triggerActive;
    firstRead = false;

    if (triggerActive) {
      openMirrors();
    } else {
      closeMirrors();
    }
  }

  if (triggerActive != lastTriggerActive) {
    lastTriggerActive = triggerActive;

    if (triggerActive) {
      addLog("Servo tetik aktif (A0)");
      openMirrors();
    } else {
      addLog("Servo tetik pasif (A0)");
      closeMirrors();
    }
  }
} 

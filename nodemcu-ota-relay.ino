#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <Servo.h>
#include <Updater.h>

// WIFI
const char* ssid = "RKS";
const char* password = "44AFT748";

ESP8266WebServer server(80);

// RÖLELER (LOW AKTİF)
#define RELAY_START D1
#define RELAY_HORN D2
#define RELAY_LOCK_OPEN D5
#define RELAY_LOCK_CLOSE D6
#define RELAY_LED D7
#define RELAY_FOG D8
#define RELAY_HEADLIGHT D0
#define RELAY_SEAT D3

// SERVO
#define SERVO_RIGHT 3   // RX
#define SERVO_LEFT 1    // TX
#define SERVO_TRIGGER D4

Servo servoRight;
Servo servoLeft;

// DURUMLAR
bool motorRunning=false;
bool ledState=false;
bool fogState=false;
bool headlightState=false;
bool seatState=false;

// SERVO
int rightAngle=90;
int leftAngle=90;

// LOG SİSTEMİ
String logs[10];
int logIndex=0;

void addLog(String msg){
  logs[logIndex]=msg;
  logIndex=(logIndex+1)%10;
}

String getLogs(){
  String out="";
  for(int i=0;i<10;i++){
    int idx=(logIndex+i)%10;
    if(logs[idx]!="") out+=logs[idx]+"<br>";
  }
  return out;
}

// PULSE
void pulseRelay(int pin,int times=1){
  for(int i=0;i<times;i++){
    digitalWrite(pin,LOW);
    delay(250);
    digitalWrite(pin,HIGH);
    delay(250);
  }
}

// EEPROM
void saveAngles(){
  EEPROM.write(0,rightAngle);
  EEPROM.write(1,leftAngle);
  EEPROM.commit();
}

// HTML
String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body {text-align:center;font-family:Arial;}
button {padding:10px;margin:5px;border-radius:10px;}
</style>
</head>
<body>

<h2>Motor Panel</h2>

<p>Durum: <span id="status">Baglaniyor...</span></p>

<button id="motorBtn" onclick="send('motor')">Motor</button><br><br>

<button onclick="send('lock_open')">Kilit Aç</button>
<button onclick="send('lock_close')">Kilit Kapat</button>
<button onclick="send('alarm')">Alarm</button><br><br>

<button onclick="send('far')">Far</button>
<button onclick="send('fog')">Sis</button>
<button onclick="send('led')">LED</button>
<button onclick="send('seat')">Koltuk</button><br><br>

<input type="range" min="0" max="180" id="r" onchange="servo()"><br>
<input type="range" min="0" max="180" id="l" onchange="servo()"><br><br>

<form method='POST' action='/update' enctype='multipart/form-data'>
<input type='file' name='update'>
<input type='submit'>
</form>

<h3>Log Paneli</h3>
<div id="log" style="height:200px;overflow:auto;border:1px solid black;"></div>

<script>
function send(c){ fetch('/cmd?c='+c); }

function servo(){
 let r=document.getElementById("r").value;
 let l=document.getElementById("l").value;
 fetch('/servo?r='+r+'&l='+l);
}

function update(){
 fetch('/status')
 .then(r=>r.json())
 .then(d=>{
  document.getElementById("status").innerHTML=d.status;

  let m=document.getElementById("motorBtn");
  if(d.motor){
    m.innerHTML="Motoru Kapat";
  }else{
    m.innerHTML="Motoru Çalıştır";
  }

  document.getElementById("log").innerHTML=d.logs;
 });
}
setInterval(update,1000);
</script>

</body>
</html>
)rawliteral";

// WEB
void handleRoot(){ server.send(200,"text/html",page); }

void handleCmd(){
  String c=server.arg("c");

  if(c=="motor"){
    if(!motorRunning){
      pulseRelay(RELAY_LOCK_OPEN);
      delay(400);
      pulseRelay(RELAY_START,2);
      motorRunning=true;
      addLog("Motor Baslatildi");
    }else{
      pulseRelay(RELAY_LOCK_OPEN);
      motorRunning=false;
      addLog("Motor Kapatildi");
    }
  }

  if(!motorRunning){
    if(c=="lock_open"){ pulseRelay(RELAY_LOCK_OPEN); addLog("Kilit Acildi"); }
    if(c=="lock_close"){ pulseRelay(RELAY_LOCK_CLOSE); addLog("Kilit Kapandi"); }
    if(c=="alarm"){ pulseRelay(RELAY_HORN); addLog("Alarm Caldi"); }
  }

  if(c=="far"){
    headlightState=!headlightState;
    digitalWrite(RELAY_HEADLIGHT, headlightState?LOW:HIGH);
    addLog(headlightState?"Far Acildi":"Far Kapandi");
  }

  if(c=="fog"){
    fogState=!fogState;
    digitalWrite(RELAY_FOG, fogState?LOW:HIGH);
    addLog(fogState?"Sis Acildi":"Sis Kapandi");
  }

  if(c=="led"){
    ledState=!ledState;
    digitalWrite(RELAY_LED, ledState?LOW:HIGH);
    addLog(ledState?"LED Acildi":"LED Kapandi");
  }

  if(c=="seat"){
    seatState=!seatState;
    digitalWrite(RELAY_SEAT, seatState?LOW:HIGH);
    addLog(seatState?"Koltuk Acildi":"Koltuk Kapandi");
  }

  server.send(200,"text/plain","OK");
}

void handleServo(){
  rightAngle=server.arg("r").toInt();
  leftAngle=server.arg("l").toInt();

  servoRight.write(rightAngle);
  servoLeft.write(leftAngle);

  saveAngles();
  addLog("Servo Ayarlandi");

  server.send(200,"text/plain","OK");
}

void handleStatus(){
  String json="{";
  json+="\"status\":\"Baglandi\",";
  json+="\"motor\":"+String(motorRunning?"true":"false")+",";
  json+="\"logs\":\""+getLogs()+"\"";
  json+="}";
  server.send(200,"application/json",json);
}

void handleUpdate(){
  HTTPUpload& upload = server.upload();
  if(upload.status==UPLOAD_FILE_START) Update.begin();
  else if(upload.status==UPLOAD_FILE_WRITE) Update.write(upload.buf,upload.currentSize);
  else if(upload.status==UPLOAD_FILE_END) Update.end(true);
}

// SETUP
void setup(){
  EEPROM.begin(10);

  pinMode(RELAY_START,OUTPUT);
  pinMode(RELAY_HORN,OUTPUT);
  pinMode(RELAY_LOCK_OPEN,OUTPUT);
  pinMode(RELAY_LOCK_CLOSE,OUTPUT);
  pinMode(RELAY_LED,OUTPUT);
  pinMode(RELAY_FOG,OUTPUT);
  pinMode(RELAY_HEADLIGHT,OUTPUT);
  pinMode(RELAY_SEAT,OUTPUT);

  // HEPSİ KAPALI
  digitalWrite(RELAY_START,HIGH);
  digitalWrite(RELAY_HORN,HIGH);
  digitalWrite(RELAY_LOCK_OPEN,HIGH);
  digitalWrite(RELAY_LOCK_CLOSE,HIGH);
  digitalWrite(RELAY_LED,HIGH);
  digitalWrite(RELAY_FOG,HIGH);
  digitalWrite(RELAY_HEADLIGHT,HIGH);
  digitalWrite(RELAY_SEAT,HIGH);

  pinMode(SERVO_TRIGGER,INPUT);

  servoRight.attach(SERVO_RIGHT);
  servoLeft.attach(SERVO_LEFT);

  WiFi.begin(ssid,password);
  while(WiFi.status()!=WL_CONNECTED) delay(500);

  ArduinoOTA.begin();

  server.on("/",handleRoot);
  server.on("/cmd",handleCmd);
  server.on("/servo",handleServo);
  server.on("/status",handleStatus);
  server.on("/update",HTTP_POST,[](){server.send(200);},handleUpdate);

  server.begin();
}

// LOOP
void loop(){
  ArduinoOTA.handle();
  server.handleClient();

  bool trig=digitalRead(SERVO_TRIGGER);

  if(trig){
    servoRight.write(rightAngle);
    servoLeft.write(leftAngle);
  }else{
    servoRight.write(0);
    servoLeft.write(0);
  }
}    Serial.println("Sabit IP Hatası!");
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nBaglandi!");

  // --- OTA AYARI ---
  // Tarayıcıdan 192.168.4.50/update adresine girerek güncelleme yapabilirsin
  httpUpdater.setup(&server); 

  server.on("/ac", handleAC);
  server.on("/kapat", handleKapat);
  server.on("/", []() {
    server.send(200, "text/html", "<h3>NodeMCU Isitici Sistemi</h3><p>OTA icin: <a href='/update'>/update</a></p>");
  });

  server.begin();
}

void loop() {
  server.handleClient();
}

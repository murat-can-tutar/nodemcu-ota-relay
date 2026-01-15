#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

const char* ap_ssid = "NodeMCU_Role";
const char* ap_pass = "12345678";

#define RELAY_PIN D1

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

bool relayState = false;

String webpage() {
  String page = "<!DOCTYPE html><html><head>";
  page += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  page += "<title>NodeMCU Röle</title>";
  page += "<style>";
  page += "body{font-family:Arial;text-align:center;background:#111;color:#fff;}";
  page += "button{padding:20px;font-size:20px;border:none;border-radius:10px;margin:10px;}";
  page += ".on{background:#2ecc71;color:#000;}";
  page += ".off{background:#e74c3c;color:#000;}";
  page += "</style></head><body>";
  page += "<h2>NodeMCU Röle Kontrol</h2>";

  if (relayState) {
    page += "<p>Durum: <b>AÇIK</b></p>";
    page += "<a href='/off'><button class='off'>KAPAT</button></a>";
  } else {
    page += "<p>Durum: <b>KAPALI</b></p>";
    page += "<a href='/on'><button class='on'>AÇ</button></a>";
  }

  page += "<br><br><a href='/update'>OTA Güncelleme</a>";
  page += "</body></html>";
  return page;
}

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_pass);

  server.on("/", []() {
    server.send(200, "text/html", webpage());
  });

  server.on("/on", []() {
    relayState = true;
    digitalWrite(RELAY_PIN, LOW);
    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/off", []() {
    relayState = false;
    digitalWrite(RELAY_PIN, HIGH);
    server.sendHeader("Location", "/");
    server.send(303);
  });

  httpUpdater.setup(&server, "/update");

  server.begin();
}

void loop() {
  server.handleClient();
}

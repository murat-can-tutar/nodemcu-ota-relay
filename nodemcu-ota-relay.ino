#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

#define RELAY_PIN D1

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

bool relayState = false;

void handleRoot() {
  String page = "<html><body style='text-align:center;font-family:Arial;'>";
  page += "<h2>Röle Kontrol</h2>";
  page += "<p>Durum: <b>";
  page += relayState ? "ACIK" : "KAPALI";
  page += "</b></p>";
  page += "<form action='/toggle' method='POST'>";
  page += "<button style='width:200px;height:60px;font-size:20px;'>AC / KAPA</button>";
  page += "</form>";
  page += "<br><a href='/update'>OTA Guncelle</a>";
  page += "</body></html>";
  server.send(200, "text/html", page);
}

void handleToggle() {
  relayState = !relayState;
  digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);
  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  WiFi.mode(WIFI_AP);
  WiFi.softAP("NodeMCU-OTA", "12345678");

  server.on("/", handleRoot);
  server.on("/toggle", HTTP_POST, handleToggle);

  httpUpdater.setup(&server);
  server.begin();
}

void loop() {
  server.handleClient();
}

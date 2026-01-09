#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

const char* ssid = "NodeMCU-OTA";
const char* password = "12345678";

ESP8266WebServer server(80);

bool relayState = false;

void handleRoot() {
  String page =
    "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<style>body{font-family:Arial;text-align:center;}button{font-size:24px;padding:20px;}</style>"
    "</head><body>"
    "<h2>Röle Kontrol</h2>"
    "<p>Durum: " + String(relayState ? "ACIK" : "KAPALI") + "</p>"
    "<a href='/toggle'><button>AC / KAPA</button></a>"
    "</body></html>";
  server.send(200, "text/html", page);
}

void handleToggle() {
  relayState = !relayState;
  digitalWrite(D1, relayState ? LOW : HIGH);
  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  pinMode(D1, OUTPUT);
  digitalWrite(D1, HIGH);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  ArduinoOTA.setHostname("nodemcu-ota");
  ArduinoOTA.begin();

  server.on("/", handleRoot);
  server.on("/toggle", handleToggle);
  server.begin();
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
}

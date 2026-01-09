#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

const char* ssid = "NodeMCU_OTA";
const char* password = "12345678";

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer ota;

bool relayState = false;

String page() {
  String s = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  s += "<style>button{width:150px;height:60px;font-size:20px;}</style></head><body>";
  s += "<h2>NodeMCU Röle Kontrol</h2>";
  s += "<p>Durum: ";
  s += relayState ? "ACIK" : "KAPALI";
  s += "</p>";
  s += "<a href='/toggle'><button>AC / KAPA</button></a>";
  s += "<br><br><a href='/update'>OTA</a>";
  s += "</body></html>";
  return s;
}

void setup() {
  pinMode(D1, OUTPUT);
  digitalWrite(D1, LOW);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  server.on("/", []() {
    server.send(200, "text/html", page());
  });

  server.on("/toggle", []() {
    relayState = !relayState;
    digitalWrite(D1, relayState ? HIGH : LOW);
    server.sendHeader("Location", "/");
    server.send(303);
  });

  ota.setup(&server);

  server.begin();
}

void loop() {
  server.handleClient();
}

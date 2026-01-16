#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

void setup() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("NodeMCU-OTA", "12345678");

  httpUpdater.setup(&server);
  server.begin();
}

void loop() {
  server.handleClient();
} 

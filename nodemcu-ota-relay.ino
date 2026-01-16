#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#define RELAY_PIN D1

ESP8266WebServer server(80);

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

WiFi.mode(WIFI_STA);
IPAddress ip(192,168,4,2);
IPAddress gateway(192,168,4,1);
IPAddress subnet(255,255,255,0);
WiFi.config(ip, gateway, subnet);
WiFi.begin("MCT LED", "192513356");

  server.on("/relay", HTTP_GET, []() {
    if (server.hasArg("state")) {
      String s = server.arg("state");
      digitalWrite(RELAY_PIN, s == "1" ? HIGH : LOW);
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "ERR");
    }
  });

  server.on("/status", HTTP_GET, []() {
    server.send(200, "text/plain", "OK");
  });

  server.begin();
}

void loop() {
  server.handleClient();
}

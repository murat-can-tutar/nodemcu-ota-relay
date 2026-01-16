#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h> // OTA için gerekli kütüphane

// --- AYARLAR ---
const char* ssid = "MCT LED";      
const char* password = "192513356";   

// Sabit IP (ESP32'deki nodeMcuIP ile aynı olmalı)
IPAddress local_IP(192, 168, 4, 50);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

#define RELAY_PIN D1 

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater; // OTA Sunucusu

void handleAC() {
  digitalWrite(RELAY_PIN, HIGH); 
  server.send(200, "text/plain", "Relay ON");
  Serial.println("Komut: ROLE ACIK");
}

void handleKapat() {
  digitalWrite(RELAY_PIN, LOW); 
  server.send(200, "text/plain", "Relay OFF");
  Serial.println("Komut: ROLE KAPALI");
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); 

  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("Sabit IP Hatası!");
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

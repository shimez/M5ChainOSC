#include "wifi_manager.h"
#include "globals.h"
#include "display.h"
#include "web_ui.h"

void startAPMode() {
  isAPMode = true;
  IPAddress apIP(192, 168, 4, 1);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  delay(500);
  dnsServer.start(DNS_PORT, "*", apIP);

  server.on("/", handleAPRoot);
  server.on("/save_wifi", handleSaveWiFi);
  server.onNotFound([]() { handleAPRoot(); });
  server.begin();

  showMessage("AP Mode", "192.168.4.1");
}

void connectOrStartAP() {
  if (wifi_ssid.length()) {
    showMessage("WiFi", "Connecting");
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
    unsigned long t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < WIFI_TIMEOUT_MS) delay(300);
  }

  if (WiFi.status() == WL_CONNECTED) {
    isAPMode = false;
    ipStr = WiFi.localIP().toString();
    if (MDNS.begin("atoms3r-osc"))
      hostStr = "atoms3r-osc.local";
    else
      hostStr = "(mDNS fail)";

    registerWebRoutes();
    server.begin();
    showMessage("WiFi OK", ipStr.c_str());
    delay(800);
  } else {
    startAPMode();
  }
}

void handleWifiLoop() {
  if (isAPMode) dnsServer.processNextRequest();
}

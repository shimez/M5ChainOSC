#include "wifi_manager.h"
#include "globals.h"
#include "display.h"
#include "web_ui.h"

namespace {
constexpr unsigned long WIFI_RECONNECT_INTERVAL_MS = 5000;
bool stationConnected = false;
bool mdnsRunning = false;
unsigned long lastReconnectAttemptMs = 0;

void startMdns() {
  if (mdnsRunning) MDNS.end();
  mdnsRunning = MDNS.begin("atoms3r-osc");
  hostStr = mdnsRunning ? "atoms3r-osc.local" : "(mDNS fail)";
}
}  // namespace

void startAPMode() {
  isAPMode = true;
  IPAddress apIP(192, 168, 4, 1);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  delay(500);
  dnsServer.start(DNS_PORT, "*", apIP);

  const char* trackedHeaders[] = {"Accept-Language"};
  server.collectHeaders(trackedHeaders, 1);
  server.on("/", HTTP_GET, handleAPRoot);
  server.on("/set_language", HTTP_POST, handleSetLanguage);
  server.on("/save_wifi", HTTP_POST, handleSaveWiFi);
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
    stationConnected = true;
    ipStr = WiFi.localIP().toString();
    startMdns();

    registerWebRoutes();
    server.begin();
    showWifiConnected(ipStr);
    delay(800);
  } else {
    startAPMode();
  }
}

void handleWifiLoop() {
  if (isAPMode) {
    dnsServer.processNextRequest();
    return;
  }

  const bool connected = WiFi.status() == WL_CONNECTED;
  const unsigned long now = millis();

  if (connected) {
    if (!stationConnected) {
      stationConnected = true;
      lastReconnectAttemptMs = 0;
      ipStr = WiFi.localIP().toString();
      startMdns();
      drawMainScreen();
    }
    return;
  }

  if (stationConnected) {
    stationConnected = false;
    if (mdnsRunning) {
      MDNS.end();
      mdnsRunning = false;
    }
    showMessage("WiFi", "Reconnecting");
    WiFi.reconnect();
    lastReconnectAttemptMs = now;
    return;
  }

  if (lastReconnectAttemptMs == 0 ||
      now - lastReconnectAttemptMs >= WIFI_RECONNECT_INTERVAL_MS) {
    WiFi.reconnect();
    lastReconnectAttemptMs = now;
  }
}

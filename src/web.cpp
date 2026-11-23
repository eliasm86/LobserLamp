#include "web.h"
#include <WebServer.h>
#include <WiFi.h>
#include "lamp.h"
#include "settings.h"
#include "netmgr.h"
#include "ha_mqtt.h"
#include "printer.h"
#include "web_page.h"

namespace {

WebServer server(80);
uint32_t restartAtMs = 0;  // non-zero: restart at this time (after saving setup)

void sendJson(int code, JsonDocument& d) {
  String out;
  serializeJson(d, out);
  server.send(code, "application/json", out);
}

void sendError(int code, const String& msg) {
  JsonDocument d;
  d["error"] = msg;
  sendJson(code, d);
}

bool parseBody(JsonDocument& d) {
  if (deserializeJson(d, server.arg("plain")) == DeserializationError::Ok) return true;
  sendError(400, "invalid json");
  return false;
}

// While the hotspot is up, requests for other hosts (phones checking for internet
// access) are sent to the setup page, which makes phones show it automatically.
bool captiveRedirect() {
  if (!netmgr::apActive()) return false;
  String host = server.hostHeader();
  String apIp = WiFi.softAPIP().toString();
  if (host == apIp || host == WiFi.localIP().toString() || host.startsWith(settings::get().hostname)) return false;
  server.sendHeader("Location", "http://" + apIp + "/setup", true);
  server.send(302, "text/plain", "");
  return true;
}

// ?look=RUNNING (printer status mode): read/change that printer state's look instead of
// the lamp's own settings. Same JSON format either way.
void sendState() {
  JsonDocument d;
  JsonObject o = d.to<JsonObject>();
  if (server.hasArg("look")) {
    if (!printer::writeLookJson(server.arg("look").c_str(), o)) {
      sendError(400, "Unknown printer state");
      return;
    }
    o["look"] = server.arg("look");
  } else {
    lamp::writeJson(o);
  }
  o["mqtt"] = !ha_mqtt::enabled() ? "disabled" : (ha_mqtt::connected() ? "connected" : "disconnected");
  if (printer::active()) printer::writeStatusJson(o["printer"].to<JsonObject>());
  sendJson(200, d);
}

void sendPrinter() {
  JsonDocument d;
  printer::writeJson(d.to<JsonObject>());
  sendJson(200, d);
}

// {"test":"STATE"}: preview that state's look for 15 s
void handlePrinterPost() {
  JsonDocument d;
  if (!parseBody(d)) return;
  String err;
  if (!printer::applyJson(d.as<JsonVariantConst>(), err)) {
    sendError(400, err);
    return;
  }
  sendPrinter();
}

void handleStatePost() {
  JsonDocument d;
  if (!parseBody(d)) return;
  if (server.hasArg("look")) {
    String err;
    if (!printer::applyLookJson(server.arg("look").c_str(), d.as<JsonVariantConst>(), err)) {
      sendError(400, err);
      return;
    }
  } else {
    lamp::applyJson(d.as<JsonVariantConst>());
  }
  sendState();
}

void sendSetup() {
  JsonDocument d;
  JsonObject o = d.to<JsonObject>();
  settings::writeJson(o);
  netmgr::writeStatus(o["status"].to<JsonObject>());
  sendJson(200, d);
}

void handleSetupPost() {
  JsonDocument d;
  if (!parseBody(d)) return;
  String err;
  if (!settings::applyJson(d.as<JsonVariantConst>(), err)) {
    sendError(400, err);
    return;
  }
  lamp::flush();
  printer::flush();
  restartAtMs = millis() + 1500;  // let the response reach the browser first
  JsonDocument r;
  r["ok"] = true;
  sendJson(200, r);
}

// Flash one LED 3 times: {"led": index}
void handleIdentify() {
  JsonDocument d;
  if (!parseBody(d)) return;
  lamp::identify(d["led"] | -1);
  server.send(204);
}

// Async WiFi scan: the first call starts it, later calls return the result
void handleScan() {
  JsonDocument d;
  int n = netmgr::scan();
  if (n < 0) {
    d["scanning"] = true;
  } else {
    JsonArray nets = d["networks"].to<JsonArray>();
    for (int i = 0; i < n; i++) {
      String ssid = WiFi.SSID(i);
      if (ssid.isEmpty()) continue;
      bool dup = false;  // same network from several access points: keep the strongest
      for (JsonObject e : nets)
        if (ssid == e["ssid"].as<const char*>()) { dup = true; if (WiFi.RSSI(i) > e["rssi"].as<int>()) e["rssi"] = WiFi.RSSI(i); }
      if (dup) continue;
      JsonObject e = nets.add<JsonObject>();
      e["ssid"] = ssid;
      e["rssi"] = WiFi.RSSI(i);
      e["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }
    WiFi.scanDelete();
  }
  sendJson(200, d);
}

}  // namespace

namespace web {

void begin() {
  server.on("/", HTTP_GET, [] {
    if (captiveRedirect()) return;
    if (settings::get().wifiSsid.isEmpty()) {  // not set up yet
      server.sendHeader("Location", "/setup", true);
      server.send(302, "text/plain", "");
      return;
    }
    server.send_P(200, "text/html", INDEX_HTML);
  });
  server.on("/setup", HTTP_GET, [] { server.send_P(200, "text/html", SETUP_HTML); });
  server.on("/app.css", HTTP_GET, [] { server.send_P(200, "text/css", APP_CSS); });
  server.on("/api/state", HTTP_GET, sendState);
  server.on("/api/state", HTTP_POST, handleStatePost);
  server.on("/api/setup", HTTP_GET, sendSetup);
  server.on("/api/setup", HTTP_POST, handleSetupPost);
  server.on("/api/scan", HTTP_GET, handleScan);
  server.on("/api/identify", HTTP_POST, handleIdentify);
  server.on("/api/printer", HTTP_GET, sendPrinter);
  server.on("/api/printer", HTTP_POST, handlePrinterPost);
  server.onNotFound([] {
    if (!captiveRedirect()) server.send(404, "text/plain", "Not found");
  });
  server.begin();
}

void loop() {
  server.handleClient();
  if (restartAtMs && (int32_t)(millis() - restartAtMs) >= 0) {
    Serial.println("Restarting to apply setup");
    ESP.restart();
  }
}

}  // namespace web

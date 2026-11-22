#include "serial_setup.h"
#include <WiFi.h>
#include <vector>
#include "settings.h"
#include "netmgr.h"
#include "lamp.h"
#include "printer.h"
#include "ha_mqtt.h"

namespace {

const char* const FW_NAME = "LobserLamp";
const char* const FW_VERSION = "1.0.0";

// ---- Improv Serial protocol ----
// Packet: "IMPROV" | version 1 | type | length | data... | checksum (sum of all previous bytes)
enum : uint8_t { TYPE_STATE = 0x01, TYPE_ERROR = 0x02, TYPE_RPC = 0x03, TYPE_RPC_RESULT = 0x04 };
enum : uint8_t { STATE_READY = 0x02, STATE_PROVISIONING = 0x03, STATE_PROVISIONED = 0x04 };
enum : uint8_t { ERR_NONE = 0x00, ERR_INVALID_RPC = 0x01, ERR_UNKNOWN_RPC = 0x02, ERR_CANT_CONNECT = 0x03 };
enum : uint8_t { CMD_WIFI = 0x01, CMD_STATE = 0x02, CMD_INFO = 0x03, CMD_SCAN = 0x04 };

const uint32_t IMPROV_TIMEOUT_MS = 20000;  // time allowed to join a network sent over Improv

uint8_t buf[300];  // incoming bytes: an Improv packet or a text line
size_t len = 0;

bool trying = false;  // testing credentials received over Improv
uint32_t tryStartMs = 0;
String trySsid, tryPass;
bool scanPending = false;
uint32_t restartAtMs = 0;

void sendPacket(uint8_t type, const uint8_t* data, uint8_t n) {
  const uint8_t hdr[9] = {'I', 'M', 'P', 'R', 'O', 'V', 1, type, n};
  uint8_t sum = 0;
  for (uint8_t b : hdr) sum += b;
  for (uint8_t i = 0; i < n; i++) sum += data[i];
  Serial.write(hdr, sizeof(hdr));
  Serial.write(data, n);
  Serial.write(sum);
  Serial.write('\n');
}

void sendState(uint8_t s) { sendPacket(TYPE_STATE, &s, 1); }
void sendError(uint8_t e) { sendPacket(TYPE_ERROR, &e, 1); }

// RPC result: command | data length | length-prefixed strings
void sendResult(uint8_t cmd, std::initializer_list<String> strings) {
  uint8_t d[240];
  size_t n = 2;
  for (const String& s : strings) {
    if (n + 1 >= sizeof(d)) break;
    size_t l = min(s.length(), sizeof(d) - n - 1);
    d[n++] = l;
    memcpy(d + n, s.c_str(), l);
    n += l;
  }
  d[0] = cmd;
  d[1] = n - 2;
  sendPacket(TYPE_RPC_RESULT, d, n);
}

String deviceUrl() { return "http://" + WiFi.localIP().toString() + "/"; }

String bytesToString(const uint8_t* p, uint8_t l) {
  String s;
  s.reserve(l);
  for (uint8_t i = 0; i < l; i++) s += (char)p[i];
  return s;
}

void handleRpc(const uint8_t* d, uint8_t n) {
  if (n < 2 || d[1] != n - 2) { sendError(ERR_INVALID_RPC); return; }
  const uint8_t cmd = d[0];
  const uint8_t* p = d + 2;
  const uint8_t pl = d[1];

  switch (cmd) {
    case CMD_WIFI: {
      if (pl < 2 || 1 + p[0] + 1 > pl || 1 + p[0] + 1 + p[1 + p[0]] > pl) { sendError(ERR_INVALID_RPC); return; }
      uint8_t sl = p[0];
      trySsid = bytesToString(p + 1, sl);
      tryPass = bytesToString(p + 2 + sl, p[1 + sl]);
      sendError(ERR_NONE);
      sendState(STATE_PROVISIONING);
      trying = true;
      tryStartMs = millis();
      netmgr::tryWifi(trySsid, tryPass);
      break;
    }
    case CMD_STATE:
      if (WiFi.status() == WL_CONNECTED) {
        sendState(STATE_PROVISIONED);
        sendResult(CMD_STATE, {deviceUrl()});
      } else {
        sendState(STATE_READY);
      }
      break;
    case CMD_INFO:
      sendResult(CMD_INFO, {FW_NAME, FW_VERSION, ESP.getChipModel(), settings::get().deviceName});
      break;
    case CMD_SCAN:
      scanPending = true;
      if (WiFi.scanComplete() == WIFI_SCAN_FAILED) WiFi.scanNetworks(true);
      break;
    default:
      sendError(ERR_UNKNOWN_RPC);
  }
}

void improvLoop() {
  if (trying) {
    if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == trySsid) {
      trying = false;
      JsonDocument d;
      d["wifi_ssid"] = trySsid;
      d["wifi_pass"] = tryPass;
      String err;
      if (!settings::applyJson(d.as<JsonVariantConst>(), err)) Serial.printf("WiFi not saved: %s\n", err.c_str());
      netmgr::endTry(true);
      sendState(STATE_PROVISIONED);
      sendResult(CMD_WIFI, {deviceUrl()});
    } else if (millis() - tryStartMs > IMPROV_TIMEOUT_MS) {
      trying = false;
      netmgr::endTry(false);
      sendError(ERR_CANT_CONNECT);
      sendState(STATE_READY);
    }
  }

  if (scanPending) {
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_FAILED) WiFi.scanNetworks(true);  // results were taken by the web page, scan again
    if (n < 0) return;
    for (int i = 0; i < n; i++) {
      String ssid = WiFi.SSID(i);
      if (ssid.isEmpty()) continue;
      bool dup = false;  // same network from several access points: list it once (strongest comes first)
      for (int j = 0; j < i && !dup; j++) dup = WiFi.SSID(j) == ssid;
      if (dup) continue;
      sendResult(CMD_SCAN, {ssid, String(WiFi.RSSI(i)), WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "NO" : "YES"});
    }
    sendResult(CMD_SCAN, {});  // empty result = end of list
    WiFi.scanDelete();
    scanPending = false;
  }
}

// ---- Text commands ----

// Split on spaces, "double quotes" keep spaces together
std::vector<String> splitArgs(const String& line) {
  std::vector<String> out;
  String cur;
  bool quoted = false, has = false;
  for (unsigned i = 0; i < line.length(); i++) {
    char c = line[i];
    if (c == '"') { quoted = !quoted; has = true; }
    else if (c == ' ' && !quoted) { if (has) out.push_back(cur); cur = ""; has = false; }
    else { cur += c; has = true; }
  }
  if (has) out.push_back(cur);
  return out;
}

void printStatus() {
  const settings::Settings& s = settings::get();
  if (WiFi.status() == WL_CONNECTED)
    Serial.printf("WiFi:    connected to %s, IP %s, http://%s.local/\n", WiFi.SSID().c_str(),
                  WiFi.localIP().toString().c_str(), s.hostname.c_str());
  else if (s.wifiSsid.isEmpty())
    Serial.println("WiFi:    not set up");
  else
    Serial.printf("WiFi:    not connected (network %s)\n", s.wifiSsid.c_str());
  if (netmgr::apActive())
    Serial.printf("Hotspot: on, setup at http://%s/setup\n", WiFi.softAPIP().toString().c_str());
  Serial.printf("MQTT:    %s\n", !ha_mqtt::enabled() ? "disabled" : ha_mqtt::connected() ? "connected" : "disconnected");
  Serial.printf("Firmware %s %s\n", FW_NAME, FW_VERSION);
}

void printHelp() {
  Serial.println("Commands:");
  Serial.println("  wifi <ssid> [password]   save WiFi and restart (use \"quotes\" for names with spaces)");
  Serial.println("  status                   show WiFi, hotspot and MQTT status");
  Serial.println("  restart                  restart the lamp");
}

void handleLine(String line) {
  line.trim();
  if (line.isEmpty()) return;
  std::vector<String> a = splitArgs(line);
  String cmd = a[0];
  cmd.toLowerCase();

  if (cmd == "wifi") {
    if (a.size() < 2 || a.size() > 3) { Serial.println("Usage: wifi <ssid> [password]"); return; }
    JsonDocument d;
    d["wifi_ssid"] = a[1];
    d["wifi_pass"] = a.size() == 3 ? a[2] : String();
    String err;
    if (!settings::applyJson(d.as<JsonVariantConst>(), err)) { Serial.printf("Error: %s\n", err.c_str()); return; }
    Serial.printf("WiFi saved (%s). Restarting...\n", a[1].c_str());
    lamp::flush();
    printer::flush();
    restartAtMs = millis() + 500;
  } else if (cmd == "status") {
    printStatus();
  } else if (cmd == "restart" || cmd == "reboot") {
    Serial.println("Restarting...");
    lamp::flush();
    printer::flush();
    restartAtMs = millis() + 200;
  } else if (cmd == "help" || cmd == "?") {
    printHelp();
  } else {
    Serial.printf("Unknown command \"%s\", type help\n", cmd.c_str());
  }
}

// Sort incoming bytes into Improv packets and text lines
void onByte(uint8_t c) {
  static const char MAGIC[] = "IMPROV";
  if (len >= sizeof(buf)) len = 0;  // overlong garbage, start over
  buf[len++] = c;

  bool improv = true;
  for (size_t i = 0; i < len && i < 6; i++)
    if (buf[i] != (uint8_t)MAGIC[i]) { improv = false; break; }

  if (improv) {
    if (len < 9) return;
    size_t total = 9 + buf[8] + 1;
    if (len < total) return;
    uint8_t sum = 0;
    for (size_t i = 0; i < total - 1; i++) sum += buf[i];
    if (sum != buf[total - 1]) sendError(ERR_INVALID_RPC);
    else if (buf[7] == TYPE_RPC) handleRpc(buf + 9, buf[8]);
    len = 0;
    return;
  }

  if (c == '\n' || c == '\r') {
    buf[len - 1] = 0;
    handleLine(String((const char*)buf));
    len = 0;
  }
}

}  // namespace

namespace serial_setup {

void begin() { Serial.println("Serial setup: type help for commands"); }

void loop() {
  while (Serial.available()) onByte(Serial.read());
  improvLoop();
  if (restartAtMs && (int32_t)(millis() - restartAtMs) >= 0) ESP.restart();
}

}  // namespace serial_setup

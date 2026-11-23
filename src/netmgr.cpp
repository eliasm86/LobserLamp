#include "netmgr.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include "settings.h"

namespace {

const uint32_t CONNECT_TIMEOUT_MS = 20000;  // start the hotspot after this long without WiFi
const uint32_t RETRY_MS = 60000;            // retry the saved network this often while the hotspot is up
const uint32_t AP_LINGER_MS = 60000;        // keep the hotspot this long after WiFi connects

DNSServer dns;
String apSsid;
bool ap = false;
bool wasConnected = false;
bool mdnsStarted = false;
bool trying = false;  // testing credentials from Improv
uint32_t lostSinceMs = 0;
uint32_t connectedSinceMs = 0;
uint32_t lastRetryMs = 0;

bool hasWifi() { return !settings::get().wifiSsid.isEmpty(); }

// The C3 Super Mini's antenna sits right next to the crystal and regulator; at full
// TX power the signal distorts and many routers refuse it. Lower power connects far
// more reliably. Must be called after every WiFi.mode() change.
void tuneRadio() {
#if CONFIG_IDF_TARGET_ESP32C3
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
#endif
}

void connectSta() {
  const settings::Settings& s = settings::get();
  WiFi.begin(s.wifiSsid.c_str(), s.wifiPass.c_str());
  Serial.printf("WiFi: connecting to %s\n", s.wifiSsid.c_str());
}

void startAp() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apSsid.c_str());  // open network, so phones can join without a password
  tuneRadio();
  // STA reconnect attempts make the radio hop channels and drop hotspot clients,
  // so automatic reconnect is off while the hotspot is up and we retry manually.
  WiFi.setAutoReconnect(false);
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", WiFi.softAPIP());  // answer every name with our IP -> captive portal
  ap = true;
  lastRetryMs = millis();
  Serial.printf("WiFi: hotspot \"%s\" started, setup at http://%s/setup\n", apSsid.c_str(),
                WiFi.softAPIP().toString().c_str());
}

void stopAp() {
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.setAutoReconnect(true);
  ap = false;
  Serial.println("WiFi: hotspot stopped");
}

}  // namespace

namespace netmgr {

void begin() {
  const settings::Settings& s = settings::get();
  WiFi.mode(WIFI_STA);
  tuneRadio();
  WiFi.setHostname(s.hostname.c_str());
  WiFi.setAutoReconnect(true);

  uint8_t mac[6];
  WiFi.macAddress(mac);
  char suffix[5];
  snprintf(suffix, sizeof(suffix), "%02X%02X", mac[4], mac[5]);
  apSsid = String("LobserLamp-") + suffix;

  lostSinceMs = millis();
  if (hasWifi()) connectSta();
  else startAp();
}

void loop() {
  if (ap) dns.processNextRequest();

  uint32_t now = millis();
  bool connected = WiFi.status() == WL_CONNECTED;

  if (connected && !wasConnected) {
    const String& host = settings::get().hostname;
    Serial.printf("WiFi: connected, IP %s  ->  http://%s/  or  http://%s.local/\n",
                  WiFi.localIP().toString().c_str(), WiFi.localIP().toString().c_str(), host.c_str());
    connectedSinceMs = now;
    if (!mdnsStarted) {
      MDNS.begin(host.c_str());
      MDNS.addService("http", "tcp", 80);
      mdnsStarted = true;
    }
  } else if (!connected && wasConnected) {
    Serial.println("WiFi: disconnected, reconnecting...");
    lostSinceMs = now;
  }
  wasConnected = connected;

  if (connected) {
    if (ap && now - connectedSinceMs > AP_LINGER_MS) stopAp();
    return;
  }
  if (trying) return;
  if (!ap && now - lostSinceMs > CONNECT_TIMEOUT_MS) startAp();
  // Retry the saved network, but only while nobody is using the hotspot
  if (ap && hasWifi() && WiFi.softAPgetStationNum() == 0 && now - lastRetryMs > RETRY_MS) {
    lastRetryMs = now;
    connectSta();
  }
}

bool apActive() { return ap; }

int scan() {
  static bool scanning = false;
  static uint32_t startMs = 0;
  const uint32_t SCAN_WAIT_MS = 15000;

  int n = WiFi.scanComplete();
  if (n >= 0) { scanning = false; return n; }
  // The core reports a scan as failed after 6 s, but with the hotspot running a scan
  // can take longer. Its result still arrives, so keep waiting: starting over would
  // throw it away (and the next scan could time out the same way).
  if (scanning && millis() - startMs < SCAN_WAIT_MS) return WIFI_SCAN_RUNNING;
  if (n == WIFI_SCAN_RUNNING) return n;
  if (WiFi.scanNetworks(true) == WIFI_SCAN_RUNNING) {
    scanning = true;
    startMs = millis();
  }
  return WIFI_SCAN_RUNNING;
}

void tryWifi(const String& ssid, const String& pass) {
  trying = true;
  WiFi.disconnect();
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.printf("WiFi: trying %s\n", ssid.c_str());
}

void endTry(bool success) {
  trying = false;
  if (success) return;
  WiFi.disconnect();
  lostSinceMs = millis();
  if (hasWifi()) connectSta();
}

void writeStatus(JsonObject o) {
  bool connected = WiFi.status() == WL_CONNECTED;
  o["connected"] = connected;
  o["ssid"] = settings::get().wifiSsid;
  if (connected) o["ip"] = WiFi.localIP().toString();
  o["ap"] = ap;
  o["ap_ssid"] = apSsid;
  if (ap) o["ap_ip"] = WiFi.softAPIP().toString();
}

}  // namespace netmgr

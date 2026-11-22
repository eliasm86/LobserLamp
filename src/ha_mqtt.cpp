#include "ha_mqtt.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include "settings.h"
#include "lamp.h"

namespace {

// Fixed prefix for MQTT topics and HA ids, so renaming the device or hostname
// doesn't create a new device in Home Assistant
const char* const ID_PREFIX = "lobserlamp";

WiFiClient net;
PubSubClient mqtt(net);
String mqttHost;  // own copy: PubSubClient keeps the pointer

String devId, tSet, tState, tAvail, tSpeedSet, tSpeedState, tTailSet, tTailState;
String tBriSet, tBriState, tModeSet, tModeState;
String tClawSet[2], tClawState[2];
String tHeadSet, tHeadState;
String tPSpeedSet, tPSpeedState, tPMinSet, tPMinState, tPMaxSet, tPMaxState;
String tCSpeedSet, tCSpeedState;
String tCPulseSet, tCPulseState, tCPSpeedSet, tCPSpeedState;
String tCTogSet, tCTogState, tCTSpeedSet, tCTSpeedState, tCTFadeSet, tCTFadeState;
String tHPulseSet, tHPulseState, tHPSpeedSet, tHPSpeedState, tHPFadeSet, tHPFadeState;
String tHSyncSet, tHSyncState, tCPFadeSet, tCPFadeState;
const char* HA_STATUS = "homeassistant/status";

uint32_t lastAttemptMs = 0;
uint32_t lastPublishMs = 0;
uint32_t publishedVer = 0;

void addDevice(JsonDocument& d) {
  JsonObject dev = d["device"].to<JsonObject>();
  dev["identifiers"][0] = devId;
  dev["name"] = settings::get().deviceName;
  dev["manufacturer"] = "DIY";
  dev["model"] = "ESP32 LED strip";
  dev["configuration_url"] = "http://" + WiFi.localIP().toString() + "/";
}

void publishJson(const String& topic, JsonDocument& d) {
  String out;
  serializeJson(d, out);
  mqtt.publish(topic.c_str(), out.c_str(), true);
}

void publishNumberConfig(const char* key, const char* name, const char* icon,
                         const String& cmd, const String& stat, int min, int max,
                         const char* unit = nullptr) {
  JsonDocument d;
  d["name"] = name;
  d["unique_id"] = devId + "_" + key;
  d["command_topic"] = cmd;
  d["state_topic"] = stat;
  d["availability_topic"] = tAvail;
  d["min"] = min;
  d["max"] = max;
  d["step"] = 1;
  d["icon"] = icon;
  d["mode"] = "slider";
  if (unit) d["unit_of_measurement"] = unit;
  addDevice(d);
  publishJson("homeassistant/number/" + devId + "_" + key + "/config", d);
}

// Single LED (claw / top of head) as its own RGB light (JSON schema)
void publishClawConfig(const char* key, const char* name, const String& cmd, const String& stat) {
  JsonDocument d;
  d["name"] = name;
  d["unique_id"] = devId + "_" + key;
  d["schema"] = "json";
  d["command_topic"] = cmd;
  d["state_topic"] = stat;
  d["availability_topic"] = tAvail;
  d["brightness"] = true;
  d["supported_color_modes"][0] = "rgb";
  d["icon"] = "mdi:lightbulb";
  addDevice(d);
  publishJson("homeassistant/light/" + devId + "_" + key + "/config", d);
  // Remove an older on/off switch entity with the same key, if any
  mqtt.publish(("homeassistant/switch/" + devId + "_" + key + "/config").c_str(), "", true);
}

void publishDiscovery() {
  JsonDocument d;
  d["name"] = nullptr;  // use the device name
  d["unique_id"] = devId;
  d["schema"] = "json";
  d["command_topic"] = tSet;
  d["state_topic"] = tState;
  d["availability_topic"] = tAvail;
  d["brightness"] = true;
  d["supported_color_modes"][0] = "rgb";
  d["effect"] = true;
  for (uint8_t i = 0; i < LAMP_MODE_COUNT; i++) d["effect_list"].add(lamp::modeLabel((Mode)i));
  addDevice(d);
  publishJson("homeassistant/light/" + devId + "/config", d);

  publishNumberConfig("speed", "Speed", "mdi:speedometer", tSpeedSet, tSpeedState, 1, 100);
  publishNumberConfig("tail", "Tail", "mdi:blur-linear", tTailSet, tTailState, 0, 100);
  publishNumberConfig("brightness", "Brightness", "mdi:brightness-6", tBriSet, tBriState, 1, 100, "%");

  publishClawConfig("claw1", "Claw light 1", tClawSet[0], tClawState[0]);
  publishClawConfig("claw2", "Claw light 2", tClawSet[1], tClawState[1]);

  publishNumberConfig("pulse_speed", "Pulse speed", "mdi:heart-pulse", tPSpeedSet, tPSpeedState, 1, 100);
  publishNumberConfig("pulse_min", "Pulse min brightness", "mdi:brightness-4", tPMinSet, tPMinState, 0, 100, "%");
  publishNumberConfig("pulse_max", "Pulse max brightness", "mdi:brightness-7", tPMaxSet, tPMaxState, 0, 100, "%");
  publishNumberConfig("candle_speed", "Candle speed", "mdi:candle", tCSpeedSet, tCSpeedState, 1, 100);

  publishClawConfig("head", "Top of head", tHeadSet, tHeadState);  // also removes the old head switch

  {
    JsonDocument s;
    s["name"] = "Claw pulse";
    s["unique_id"] = devId + "_claw_pulse";
    s["command_topic"] = tCPulseSet;
    s["state_topic"] = tCPulseState;
    s["availability_topic"] = tAvail;
    s["icon"] = "mdi:heart-pulse";
    addDevice(s);
    publishJson("homeassistant/switch/" + devId + "_claw_pulse/config", s);
  }
  publishNumberConfig("claw_pulse_speed", "Claw pulse speed", "mdi:speedometer", tCPSpeedSet, tCPSpeedState, 1, 100);
  publishNumberConfig("claw_pulse_fade", "Claw pulse fade", "mdi:transition", tCPFadeSet, tCPFadeState, 0, 100, "%");

  {
    JsonDocument s;
    s["name"] = "Claw toggle";
    s["unique_id"] = devId + "_claw_toggle";
    s["command_topic"] = tCTogSet;
    s["state_topic"] = tCTogState;
    s["availability_topic"] = tAvail;
    s["icon"] = "mdi:swap-horizontal";
    addDevice(s);
    publishJson("homeassistant/switch/" + devId + "_claw_toggle/config", s);
  }
  publishNumberConfig("claw_toggle_speed", "Claw toggle speed", "mdi:speedometer", tCTSpeedSet, tCTSpeedState, 1, 100);
  publishNumberConfig("claw_toggle_fade", "Claw toggle fade", "mdi:transition", tCTFadeSet, tCTFadeState, 0, 100, "%");

  {
    JsonDocument s;
    s["name"] = "Head pulse";
    s["unique_id"] = devId + "_head_pulse";
    s["command_topic"] = tHPulseSet;
    s["state_topic"] = tHPulseState;
    s["availability_topic"] = tAvail;
    s["icon"] = "mdi:heart-pulse";
    addDevice(s);
    publishJson("homeassistant/switch/" + devId + "_head_pulse/config", s);
  }
  publishNumberConfig("head_pulse_speed", "Head pulse speed", "mdi:speedometer", tHPSpeedSet, tHPSpeedState, 1, 100);
  publishNumberConfig("head_pulse_fade", "Head pulse fade", "mdi:transition", tHPFadeSet, tHPFadeState, 0, 100, "%");
  {
    JsonDocument s;
    s["name"] = "Head pulse sync with claws";
    s["unique_id"] = devId + "_head_sync";
    s["command_topic"] = tHSyncSet;
    s["state_topic"] = tHSyncState;
    s["availability_topic"] = tAvail;
    s["icon"] = "mdi:sync";
    addDevice(s);
    publishJson("homeassistant/switch/" + devId + "_head_sync/config", s);
  }

  JsonDocument m;
  m["name"] = "Mode";
  m["unique_id"] = devId + "_mode";
  m["command_topic"] = tModeSet;
  m["state_topic"] = tModeState;
  m["availability_topic"] = tAvail;
  m["icon"] = "mdi:animation-play";
  for (uint8_t i = 0; i < LAMP_MODE_COUNT; i++) m["options"].add(lamp::modeLabel((Mode)i));
  addDevice(m);
  publishJson("homeassistant/select/" + devId + "_mode/config", m);
}

// Brightness entity is in percent (1-100), the lamp uses 1-255 internally
int briToPercent(uint8_t b) { return max(1, (int)lroundf(b / 2.55f)); }
uint8_t percentToBri(float p) { return (uint8_t)constrain(lroundf(p * 2.55f), 1L, 255L); }

void publishState() {
  JsonDocument d;
  lamp::writeJson(d.to<JsonObject>());
  publishJson(tState, d);
  mqtt.publish(tSpeedState.c_str(), String(lamp::state().speed).c_str(), true);
  mqtt.publish(tTailState.c_str(), String(lamp::state().tail).c_str(), true);
  mqtt.publish(tBriState.c_str(), String(briToPercent(lamp::state().brightness)).c_str(), true);
  mqtt.publish(tModeState.c_str(), lamp::modeLabel(lamp::state().mode), true);
  mqtt.publish(tPSpeedState.c_str(), String(lamp::state().pulseSpeed).c_str(), true);
  mqtt.publish(tPMinState.c_str(), String(lamp::state().pulseMin).c_str(), true);
  mqtt.publish(tPMaxState.c_str(), String(lamp::state().pulseMax).c_str(), true);
  mqtt.publish(tCSpeedState.c_str(), String(lamp::state().candleSpeed).c_str(), true);
  mqtt.publish(tCPulseState.c_str(), lamp::state().clawPulse ? "ON" : "OFF", true);
  mqtt.publish(tCPSpeedState.c_str(), String(lamp::state().clawPulseSpeed).c_str(), true);
  mqtt.publish(tCPFadeState.c_str(), String(lamp::state().clawPulseFade).c_str(), true);
  mqtt.publish(tCTogState.c_str(), lamp::state().clawToggle ? "ON" : "OFF", true);
  mqtt.publish(tCTSpeedState.c_str(), String(lamp::state().clawToggleSpeed).c_str(), true);
  mqtt.publish(tCTFadeState.c_str(), String(lamp::state().clawToggleFade).c_str(), true);
  mqtt.publish(tHPulseState.c_str(), lamp::state().headPulse ? "ON" : "OFF", true);
  mqtt.publish(tHPSpeedState.c_str(), String(lamp::state().headPulseSpeed).c_str(), true);
  mqtt.publish(tHPFadeState.c_str(), String(lamp::state().headPulseFade).c_str(), true);
  mqtt.publish(tHSyncState.c_str(), lamp::state().headSync ? "ON" : "OFF", true);
  {
    JsonDocument h;
    lamp::writeHeadJson(h.to<JsonObject>());
    publishJson(tHeadState, h);
  }
  for (uint8_t i = 0; i < 2; i++) {
    JsonDocument c;
    lamp::writeClawJson(i, c.to<JsonObject>());
    publishJson(tClawState[i], c);
  }
  publishedVer = lamp::version();
  lastPublishMs = millis();
}

void onMessage(char* topic, byte* payload, unsigned int len) {
  String t(topic);
  String msg;
  msg.reserve(len);
  for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];

  if (t == tSet) {
    JsonDocument d;
    if (deserializeJson(d, msg) == DeserializationError::Ok) lamp::applyJson(d.as<JsonVariantConst>());
  } else if (t == tSpeedSet || t == tTailSet || t == tPSpeedSet || t == tPMinSet || t == tPMaxSet ||
             t == tCSpeedSet || t == tCPSpeedSet || t == tCTSpeedSet || t == tCTFadeSet ||
             t == tHPSpeedSet || t == tHPFadeSet || t == tCPFadeSet) {
    const char* key = t == tSpeedSet  ? "speed"
                    : t == tTailSet   ? "tail"
                    : t == tPSpeedSet ? "pulse_speed"
                    : t == tPMinSet   ? "pulse_min"
                    : t == tPMaxSet   ? "pulse_max"
                    : t == tCSpeedSet ? "candle_speed"
                    : t == tCPSpeedSet ? "claw_pulse_speed"
                    : t == tCTSpeedSet ? "claw_toggle_speed"
                    : t == tCTFadeSet ? "claw_toggle_fade"
                    : t == tHPSpeedSet ? "head_pulse_speed"
                    : t == tHPFadeSet ? "head_pulse_fade"
                                      : "claw_pulse_fade";
    JsonDocument d;
    d[key] = (int)lroundf(msg.toFloat());
    lamp::applyJson(d.as<JsonVariantConst>());
  } else if (t == tCPulseSet) {
    JsonDocument d;
    d["claw_pulse"] = msg;
    lamp::applyJson(d.as<JsonVariantConst>());
  } else if (t == tCTogSet) {
    JsonDocument d;
    d["claw_toggle"] = msg;
    lamp::applyJson(d.as<JsonVariantConst>());
  } else if (t == tHPulseSet) {
    JsonDocument d;
    d["head_pulse"] = msg;
    lamp::applyJson(d.as<JsonVariantConst>());
  } else if (t == tHSyncSet) {
    JsonDocument d;
    d["head_sync"] = msg;
    lamp::applyJson(d.as<JsonVariantConst>());
  } else if (t == tHeadSet) {
    // JSON light command, or a plain "ON"/"OFF"
    JsonDocument d;
    if (deserializeJson(d, msg) != DeserializationError::Ok) d.set(msg);
    lamp::applyHeadJson(d.as<JsonVariantConst>());
  } else if (t == tBriSet) {
    JsonDocument d;
    d["brightness"] = percentToBri(msg.toFloat());
    lamp::applyJson(d.as<JsonVariantConst>());
  } else if (t == tModeSet) {
    JsonDocument d;
    d["effect"] = msg;
    lamp::applyJson(d.as<JsonVariantConst>());
  } else if (t == tClawSet[0] || t == tClawSet[1]) {
    // JSON light command, or a plain "ON"/"OFF"
    uint8_t i = t == tClawSet[0] ? 0 : 1;
    JsonDocument d;
    if (deserializeJson(d, msg) != DeserializationError::Ok) d.set(msg);
    lamp::applyClawJson(i, d.as<JsonVariantConst>());
  } else if (t == HA_STATUS && msg == "online") {
    publishDiscovery();  // Home Assistant restarted
    publishState();
  }
}

bool connect() {
  const settings::Settings& s = settings::get();
  Serial.printf("MQTT: connecting to %s:%d ... ", mqttHost.c_str(), s.mqttPort);
  const char* user = s.mqttUser.isEmpty() ? nullptr : s.mqttUser.c_str();
  const char* pass = s.mqttPass.isEmpty() ? nullptr : s.mqttPass.c_str();
  if (!mqtt.connect(devId.c_str(), user, pass, tAvail.c_str(), 0, true, "offline")) {
    Serial.printf("failed (rc=%d)\n", mqtt.state());
    return false;
  }
  Serial.println("ok");
  mqtt.publish(tAvail.c_str(), "online", true);
  mqtt.subscribe(tSet.c_str());
  mqtt.subscribe(tSpeedSet.c_str());
  mqtt.subscribe(tTailSet.c_str());
  mqtt.subscribe(tBriSet.c_str());
  mqtt.subscribe(tModeSet.c_str());
  mqtt.subscribe(tClawSet[0].c_str());
  mqtt.subscribe(tClawSet[1].c_str());
  mqtt.subscribe(tHeadSet.c_str());
  mqtt.subscribe(tPSpeedSet.c_str());
  mqtt.subscribe(tPMinSet.c_str());
  mqtt.subscribe(tPMaxSet.c_str());
  mqtt.subscribe(tCSpeedSet.c_str());
  mqtt.subscribe(tCPulseSet.c_str());
  mqtt.subscribe(tCPSpeedSet.c_str());
  mqtt.subscribe(tCTogSet.c_str());
  mqtt.subscribe(tCTSpeedSet.c_str());
  mqtt.subscribe(tCTFadeSet.c_str());
  mqtt.subscribe(tHPulseSet.c_str());
  mqtt.subscribe(tHPSpeedSet.c_str());
  mqtt.subscribe(tHPFadeSet.c_str());
  mqtt.subscribe(tHSyncSet.c_str());
  mqtt.subscribe(tCPFadeSet.c_str());
  mqtt.subscribe(HA_STATUS);
  publishDiscovery();
  publishState();
  return true;
}

}  // namespace

namespace ha_mqtt {

bool enabled() { return !mqttHost.isEmpty(); }
bool connected() { return mqtt.connected(); }

void begin() {
  mqttHost = settings::get().mqttHost;
  if (!enabled()) {
    Serial.println("MQTT: disabled (no server set on the setup page)");
    return;
  }
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char suffix[7];
  snprintf(suffix, sizeof(suffix), "%02x%02x%02x", mac[3], mac[4], mac[5]);
  devId = String(ID_PREFIX) + "_" + suffix;

  String base = String(ID_PREFIX) + "/" + suffix;
  tSet = base + "/set";
  tState = base + "/state";
  tAvail = base + "/availability";
  tSpeedSet = base + "/speed/set";
  tSpeedState = base + "/speed";
  tTailSet = base + "/tail/set";
  tTailState = base + "/tail";
  tBriSet = base + "/brightness/set";
  tBriState = base + "/brightness";
  tModeSet = base + "/mode/set";
  tModeState = base + "/mode";
  for (int i = 0; i < 2; i++) {
    tClawState[i] = base + "/claw" + String(i + 1);
    tClawSet[i] = tClawState[i] + "/set";
  }
  tHeadState = base + "/head";
  tHeadSet = tHeadState + "/set";
  tPSpeedState = base + "/pulse_speed";
  tPSpeedSet = tPSpeedState + "/set";
  tPMinState = base + "/pulse_min";
  tPMinSet = tPMinState + "/set";
  tPMaxState = base + "/pulse_max";
  tPMaxSet = tPMaxState + "/set";
  tCSpeedState = base + "/candle_speed";
  tCSpeedSet = tCSpeedState + "/set";
  tCPulseState = base + "/claw_pulse";
  tCPulseSet = tCPulseState + "/set";
  tCPSpeedState = base + "/claw_pulse_speed";
  tCPSpeedSet = tCPSpeedState + "/set";
  tCTogState = base + "/claw_toggle";
  tCTogSet = tCTogState + "/set";
  tCTSpeedState = base + "/claw_toggle_speed";
  tCTSpeedSet = tCTSpeedState + "/set";
  tCTFadeState = base + "/claw_toggle_fade";
  tCTFadeSet = tCTFadeState + "/set";
  tHPulseState = base + "/head_pulse";
  tHPulseSet = tHPulseState + "/set";
  tHPSpeedState = base + "/head_pulse_speed";
  tHPSpeedSet = tHPSpeedState + "/set";
  tHPFadeState = base + "/head_pulse_fade";
  tHPFadeSet = tHPFadeState + "/set";
  tHSyncState = base + "/head_sync";
  tHSyncSet = tHSyncState + "/set";
  tCPFadeState = base + "/claw_pulse_fade";
  tCPFadeSet = tCPFadeState + "/set";

  mqtt.setServer(mqttHost.c_str(), settings::get().mqttPort);
  mqtt.setBufferSize(1024);
  mqtt.setSocketTimeout(3);
  mqtt.setCallback(onMessage);
}

void loop() {
  if (!enabled() || WiFi.status() != WL_CONNECTED) return;

  if (!mqtt.connected()) {
    if (millis() - lastAttemptMs > 5000 || lastAttemptMs == 0) {
      lastAttemptMs = millis();
      connect();
    }
    return;
  }
  mqtt.loop();

  // Report changes (from web UI or HA), throttled while sliders are dragged
  if (publishedVer != lamp::version() && millis() - lastPublishMs > 250) publishState();
}

}  // namespace ha_mqtt

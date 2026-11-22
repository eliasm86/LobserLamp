#include "printer.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include "settings.h"
#include "lamp.h"

namespace {

// ---- Printer states. OFFLINE = not connected; the rest are Bambu's gcode_state values.
enum PState : uint8_t { P_OFFLINE, P_IDLE, P_PREPARE, P_RUNNING, P_PAUSE, P_FINISH, P_FAILED, P_COUNT };
const char* const STATE_KEYS[P_COUNT] = {"OFFLINE", "IDLE", "PREPARE", "RUNNING", "PAUSE", "FINISH", "FAILED"};
const char* const STATE_LABELS[P_COUNT] = {"Offline (not connected)", "Idle", "Preparing", "Printing",
                                           "Paused", "Finished", "Failed"};

// ---- Looks: a full set of lamp settings per state (same as the lamp page), edited on the
// lamp page. The LED roles come from the lamp itself.
LampState looks[P_COUNT];

// Defaults (used until changed): the lamp's current colors, brightness and speeds, with
//   OFFLINE  body off,      claws off,    head off
//   IDLE     Solid,         claws off,    head on
//   PREPARE  Progress,      claws pulse,  head pulses with the claws
//   RUNNING  Progress,      claws pulse,  head pulses with the claws
//   PAUSE    Progress,      claws on,     head pulse
//   FINISH   Progress (full), claws on,   head on
//   FAILED   body off,      claws toggle, head pulses with the claws
enum DefClaws : uint8_t { C_OFF, C_ON, C_PULSE, C_TOGGLE };
enum DefHead : uint8_t { H_OFF, H_ON, H_PULSE, H_SYNCED };

void setDefault(LampState& s, bool on, Mode mode, DefClaws claws, DefHead head) {
  s = lamp::state();
  s.on = on;
  s.mode = mode;
  for (ClawState& c : s.claw) c.on = claws == C_ON;
  s.clawPulse = claws == C_PULSE;
  s.clawToggle = claws == C_TOGGLE;
  s.head.on = head == H_ON;
  s.headPulse = head == H_PULSE || head == H_SYNCED;
  s.headSync = head == H_SYNCED;
}

void setDefaults() {
  setDefault(looks[P_OFFLINE], false, Mode::Solid, C_OFF, H_OFF);
  setDefault(looks[P_IDLE], true, Mode::Solid, C_OFF, H_ON);
  setDefault(looks[P_PREPARE], true, Mode::Progress, C_PULSE, H_SYNCED);
  setDefault(looks[P_RUNNING], true, Mode::Progress, C_PULSE, H_SYNCED);
  setDefault(looks[P_PAUSE], true, Mode::Progress, C_ON, H_PULSE);
  setDefault(looks[P_FINISH], true, Mode::Progress, C_ON, H_ON);
  setDefault(looks[P_FAILED], false, Mode::Solid, C_TOGGLE, H_SYNCED);
}

bool enabled = false;

// ---- Preview: show one state's look for a while, whatever the printer is doing.
// Started by the Test button and by every change to a look.
const uint32_t TEST_MS = 15000;
int8_t testState = -1;  // -1 = no preview running
uint32_t testStartMs = 0;

uint32_t testElapsed() { return millis() - testStartMs; }

// Index of a state key (case-insensitive), P_COUNT if unknown
uint8_t stateIndex(const char* key) {
  uint8_t i = 0;
  while (key && i < P_COUNT && strcasecmp(key, STATE_KEYS[i]) != 0) i++;
  return key ? i : P_COUNT;
}

// ---- Shared between the printer task (writes) and the main loop (reads)
struct Status {
  bool     connected;
  PState   state;
  uint8_t  percent;
  int16_t  remainingMin;  // -1 = unknown
  int16_t  layer, totalLayers;
};
Status shared = {false, P_OFFLINE, 0, -1, -1, -1};
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

Status snapshot() {
  portENTER_CRITICAL(&mux);
  Status s = shared;
  portEXIT_CRITICAL(&mux);
  return s;
}

void setConnected(bool c) {
  portENTER_CRITICAL(&mux);
  shared.connected = c;
  portEXIT_CRITICAL(&mux);
}

PState stateFromKey(const char* s) {
  for (uint8_t i = 1; i < P_COUNT; i++)  // OFFLINE is never sent by the printer
    if (strcasecmp(s, STATE_KEYS[i]) == 0) return (PState)i;
  return P_IDLE;  // SLICING, INIT and unknown values count as idle
}

// ---- Look storage: one blob with all looks. Saved a few seconds after the last change
// (sliders send many updates). If the blob doesn't match this firmware's layout (other
// size), the defaults are used.
const char* const LOOKS_KEY = "looks";
const uint32_t SAVE_DELAY_MS = 3000;
bool saveDirty = false;
uint32_t lastChangeMs = 0;

void loadLooks() {
  setDefaults();
  Preferences p;
  p.begin("printer", true);
  LampState raw[P_COUNT];
  if (p.getBytesLength(LOOKS_KEY) == sizeof(raw) && p.getBytes(LOOKS_KEY, raw, sizeof(raw)) == sizeof(raw)) {
    for (uint8_t i = 0; i < P_COUNT; i++) {
      LampState& s = raw[i];
      if ((uint8_t)s.mode >= (uint8_t)Mode::COUNT) s.mode = Mode::Solid;
      s.brightness = max<uint8_t>(1, s.brightness);
      looks[i] = s;
    }
  }
  p.end();
}

void saveLooks() {
  Preferences p;
  p.begin("printer", false);
  p.putBytes(LOOKS_KEY, looks, sizeof(looks));
  if (p.isKey("scenes")) p.remove("scenes");    // earlier versions of this firmware
  if (p.isKey("scenes2")) p.remove("scenes2");
  p.end();
  Serial.println("Printer looks saved");
}

// ---- Printer connection (runs in its own task)
const uint16_t BAMBU_PORT = 8883;
const uint32_t RETRY_MS = 15000;
// Full status reports (pushall) can be large, especially with an AMS
const uint16_t BUFFER_SIZE = 32768;
const char* const PUSHALL = "{\"pushing\":{\"sequence_id\":\"0\",\"command\":\"pushall\"}}";

WiFiClientSecure tls;
PubSubClient mqtt(tls);
String tReport, tRequest, host, code;

// Reports are JSON with the interesting parts under "print". X1 printers send the
// full status every time; P1/A1 printers send only what changed, so values that
// aren't in a message are kept from earlier ones.
void onMessage(char*, byte* payload, unsigned int len) {
  static JsonDocument filter;
  if (filter.isNull()) {
    JsonObject f = filter["print"].to<JsonObject>();
    f["gcode_state"] = true;
    f["mc_percent"] = true;
    f["mc_remaining_time"] = true;
    f["layer_num"] = true;
    f["total_layer_num"] = true;
  }
  JsonDocument d;
  if (deserializeJson(d, payload, len, DeserializationOption::Filter(filter)) != DeserializationError::Ok) return;
  JsonVariantConst p = d["print"];
  if (p.isNull()) return;

  bool hasState = p["gcode_state"].is<const char*>();
  PState st = hasState ? stateFromKey(p["gcode_state"]) : P_IDLE;
  portENTER_CRITICAL(&mux);
  if (hasState) shared.state = st;
  if (p["mc_percent"].is<int>()) shared.percent = constrain(p["mc_percent"].as<int>(), 0, 100);
  if (p["mc_remaining_time"].is<int>()) shared.remainingMin = p["mc_remaining_time"].as<int>();
  if (p["layer_num"].is<int>()) shared.layer = p["layer_num"].as<int>();
  if (p["total_layer_num"].is<int>()) shared.totalLayers = p["total_layer_num"].as<int>();
  portEXIT_CRITICAL(&mux);
}

void printerTask(void*) {
  uint32_t lastTryMs = 0;
  bool first = true;
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      setConnected(false);
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    if (!mqtt.connected()) {
      setConnected(false);
      if (first || millis() - lastTryMs >= RETRY_MS) {
        first = false;
        lastTryMs = millis();
        String id = "lobserlamp-" + String((uint32_t)ESP.getEfuseMac(), HEX);
        Serial.printf("Printer: connecting to %s\n", host.c_str());
        if (mqtt.connect(id.c_str(), "bblp", code.c_str())) {
          Serial.println("Printer: connected");
          mqtt.subscribe(tReport.c_str());
          mqtt.publish(tRequest.c_str(), PUSHALL);  // ask for the full status once
          setConnected(true);
        } else {
          Serial.printf("Printer: connection failed (state %d)\n", mqtt.state());
        }
      }
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }
    mqtt.loop();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

}  // namespace

namespace printer {

bool active() { return enabled; }

void begin() {
  loadLooks();  // defaults copy the lamp's settings, so after lamp::begin()
  if (!settings::printerMode()) return;
  const settings::Settings& s = settings::get();
  host = s.bambuHost;  // own copies: PubSubClient keeps the pointers
  code = s.bambuCode;
  tReport = "device/" + s.bambuSerial + "/report";
  tRequest = "device/" + s.bambuSerial + "/request";

  tls.setInsecure();  // the printer uses a self-signed certificate
  tls.setHandshakeTimeout(10);
  mqtt.setServer(host.c_str(), BAMBU_PORT);
  mqtt.setCallback(onMessage);
  mqtt.setKeepAlive(30);
  if (!mqtt.setBufferSize(BUFFER_SIZE)) mqtt.setBufferSize(16384);

  enabled = true;
  xTaskCreatePinnedToCore(printerTask, "printer", 12288, nullptr, 1, nullptr, 0);
  Serial.printf("Printer status mode: printer %s, serial %s\n", host.c_str(), s.bambuSerial.c_str());
}

void loop() {
  if (saveDirty && millis() - lastChangeMs > SAVE_DELAY_MS) flush();
  if (!enabled) return;
  if (testState >= 0 && testElapsed() >= TEST_MS) testState = -1;
  if (testState >= 0) {
    // The progress bar fills up during the preview so it can be seen
    float progress = testState == P_FINISH ? 100.0f : testElapsed() * 100.0f / TEST_MS;
    lamp::setOverride(&looks[testState], progress);
    return;
  }
  Status s = snapshot();
  float progress = s.state == P_FINISH ? 100.0f : s.percent;
  lamp::setOverride(&looks[s.connected ? s.state : P_OFFLINE], progress);
}

void flush() {
  if (!saveDirty) return;
  saveDirty = false;
  saveLooks();
}

void writeStatusJson(JsonObject o) {
  Status s = snapshot();
  o["connected"] = s.connected;
  o["state"] = STATE_KEYS[s.connected ? s.state : P_OFFLINE];
  o["state_label"] = STATE_LABELS[s.connected ? s.state : P_OFFLINE];
  o["percent"] = s.percent;
  if (s.remainingMin >= 0) o["remaining_min"] = s.remainingMin;
  if (s.layer >= 0) o["layer"] = s.layer;
  if (s.totalLayers >= 0) o["total_layers"] = s.totalLayers;
  if (testState >= 0 && testElapsed() < TEST_MS) {
    o["test"] = STATE_KEYS[testState];
    o["test_left"] = (TEST_MS - testElapsed() + 999) / 1000;  // seconds
  }
}

void writeJson(JsonObject o) {
  o["active"] = enabled;
  writeStatusJson(o["status"].to<JsonObject>());
  JsonArray states = o["states"].to<JsonArray>();
  for (uint8_t i = 0; i < P_COUNT; i++) {
    JsonObject e = states.add<JsonObject>();
    e["key"] = STATE_KEYS[i];
    e["label"] = STATE_LABELS[i];
  }
}

bool applyJson(JsonVariantConst j, String& err) {
  // "test": state key = show that state's look for TEST_MS; "" or false = stop
  JsonVariantConst t = j["test"];
  if (t.is<const char*>() && *t.as<const char*>()) {
    uint8_t i = stateIndex(t.as<const char*>());
    if (i == P_COUNT) { err = String("Unknown printer state: ") + t.as<const char*>(); return false; }
    if (!enabled) { err = "Printer status mode isn't running. Save and restart in Setup first."; return false; }
    testState = i;
    testStartMs = millis();
  } else if (t.is<const char*>() || (t.is<bool>() && !t.as<bool>())) {
    testState = -1;
  }
  return true;
}

bool writeLookJson(const char* key, JsonObject o) {
  uint8_t i = stateIndex(key);
  if (i == P_COUNT) return false;
  lamp::writeLookJson(looks[i], o);
  // LED roles are the lamp's own (the page greys out parts without an LED)
  const LampState& st = lamp::state();
  o["head_led"] = st.headLed;
  o["claw1_led"] = st.clawLed[0];
  o["claw2_led"] = st.clawLed[1];
  o["led_count"] = settings::get().ledCount;
  return true;
}

bool applyLookJson(const char* key, JsonVariantConst j, String& err) {
  uint8_t i = stateIndex(key);
  if (i == P_COUNT) { err = String("Unknown printer state: ") + (key ? key : ""); return false; }
  if (!enabled) { err = "Printer status mode isn't running. Save and restart in Setup first."; return false; }
  if (lamp::applyLookJson(looks[i], j)) {
    saveDirty = true;
    lastChangeMs = millis();
  }
  // Show what is being edited
  testState = i;
  testStartMs = millis();
  return true;
}

}  // namespace printer

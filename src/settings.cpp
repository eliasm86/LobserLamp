#include "settings.h"
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>
#include <driver/gpio.h>

namespace {

// ---- Defaults, used until changed on the setup page ----
const char* const DEF_DEVICE_NAME    = "LobserLamp";
const char* const DEF_HOSTNAME       = "lobserlamp";
const uint16_t    DEF_MQTT_PORT      = 1883;
const uint8_t     DEF_LED_PIN        = 13;
const uint8_t     DEF_LED_COUNT      = 31;
const uint8_t     DEF_LED_ORDER      = 0;    // GRB = WS2812B
const uint8_t     DEF_MAX_BRIGHTNESS = 255;

struct Order {
  const char* name;
  uint16_t type;
};
const Order ORDERS[] = {{"GRB", NEO_GRB}, {"RGB", NEO_RGB}, {"BRG", NEO_BRG},
                        {"RBG", NEO_RBG}, {"GBR", NEO_GBR}, {"BGR", NEO_BGR}};
const uint8_t ORDER_COUNT = sizeof(ORDERS) / sizeof(ORDERS[0]);

// Device mode names used in the JSON API, in DeviceMode order
const char* const MODE_KEYS[] = {"lamp", "printer"};
static_assert(sizeof(MODE_KEYS) / sizeof(MODE_KEYS[0]) == (size_t)settings::DeviceMode::COUNT, "MODE_KEYS must match DeviceMode");

settings::Settings cur;

// Output-capable pins that are safe to use (not flash, USB or the serial console)
bool validPin(int p) {
  if (p < 0 || p >= GPIO_PIN_COUNT || !GPIO_IS_VALID_OUTPUT_GPIO(p)) return false;
#if CONFIG_IDF_TARGET_ESP32
  if (p == 1 || p == 3 || (p >= 6 && p <= 11)) return false;
#elif CONFIG_IDF_TARGET_ESP32S3
  if (p == 19 || p == 20 || (p >= 26 && p <= 32) || p == 43 || p == 44) return false;
#elif CONFIG_IDF_TARGET_ESP32C3
  if ((p >= 12 && p <= 21)) return false;
#endif
  return true;
}

// Lowercase letters, digits and hyphens, max 32 characters
String cleanHostname(const char* in) {
  String out;
  for (const char* c = in; *c && out.length() < 32; c++) {
    char ch = tolower(*c);
    if (isalnum(ch) || ch == '-') out += ch;
    else if (ch == ' ' || ch == '_') out += '-';
  }
  while (out.startsWith("-")) out.remove(0, 1);
  while (out.endsWith("-")) out.remove(out.length() - 1);
  return out;
}

// Read a string field if present. Returns false (with err set) if invalid.
bool readStr(JsonVariantConst j, const char* key, size_t maxLen, const char* label, bool trim,
             String& dst, String& err) {
  JsonVariantConst v = j[key];
  if (v.isNull()) return true;
  if (!v.is<const char*>()) { err = String(label) + ": invalid value"; return false; }
  String s = v.as<const char*>();
  if (trim) s.trim();
  if (s.length() > maxLen) { err = String(label) + " is too long (max " + maxLen + " characters)"; return false; }
  dst = s;
  return true;
}

// Read a number field if present. Returns false (with err set) if out of range.
template <typename T>
bool readNum(JsonVariantConst j, const char* key, long lo, long hi, const char* label, T& dst, String& err) {
  JsonVariantConst v = j[key];
  if (v.isNull()) return true;
  if (!v.is<long>() || v.as<long>() < lo || v.as<long>() > hi) {
    err = String(label) + " must be " + lo + "-" + hi;
    return false;
  }
  dst = (T)v.as<long>();
  return true;
}

void save(const settings::Settings& s) {
  Preferences p;
  p.begin("setup", false);
  p.putString("ssid", s.wifiSsid);
  p.putString("wpass", s.wifiPass);
  p.putString("name", s.deviceName);
  p.putString("host", s.hostname);
  p.putString("mhost", s.mqttHost);
  p.putUShort("mport", s.mqttPort);
  p.putString("muser", s.mqttUser);
  p.putString("mpass", s.mqttPass);
  p.putUChar("pin", s.ledPin);
  p.putUChar("count", s.ledCount);
  p.putUChar("order", s.ledOrder);
  p.putUChar("maxbri", s.maxBrightness);
  p.putUChar("mode", (uint8_t)s.mode);
  p.putString("bhost", s.bambuHost);
  p.putString("bser", s.bambuSerial);
  p.putString("bcode", s.bambuCode);
  p.end();
  Serial.println("Setup saved");
}

}  // namespace

namespace settings {

void begin() {
  Preferences p;
  p.begin("setup", true);
  cur.wifiSsid      = p.getString("ssid", "");
  cur.wifiPass      = p.getString("wpass", "");
  cur.deviceName    = p.getString("name", DEF_DEVICE_NAME);
  cur.hostname      = p.getString("host", DEF_HOSTNAME);
  cur.mqttHost      = p.getString("mhost", "");
  cur.mqttPort      = p.getUShort("mport", DEF_MQTT_PORT);
  cur.mqttUser      = p.getString("muser", "");
  cur.mqttPass      = p.getString("mpass", "");
  cur.ledPin        = p.getUChar("pin", DEF_LED_PIN);
  cur.ledCount      = p.getUChar("count", DEF_LED_COUNT);
  cur.ledOrder      = p.getUChar("order", DEF_LED_ORDER);
  cur.maxBrightness = p.getUChar("maxbri", DEF_MAX_BRIGHTNESS);
  cur.mode          = (DeviceMode)p.getUChar("mode", (uint8_t)DeviceMode::Lamp);
  cur.bambuHost     = p.getString("bhost", "");
  cur.bambuSerial   = p.getString("bser", "");
  cur.bambuCode     = p.getString("bcode", "");
  p.end();

  if ((uint8_t)cur.mode >= (uint8_t)DeviceMode::COUNT) cur.mode = DeviceMode::Lamp;

  if (!validPin(cur.ledPin)) cur.ledPin = DEF_LED_PIN;
  if (cur.ledCount < 1 || cur.ledCount > MAX_LEDS) cur.ledCount = DEF_LED_COUNT;
  if (cur.ledOrder >= ORDER_COUNT) cur.ledOrder = DEF_LED_ORDER;
  if (cur.maxBrightness < 1) cur.maxBrightness = DEF_MAX_BRIGHTNESS;
  if (cur.mqttPort == 0) cur.mqttPort = DEF_MQTT_PORT;
  if (cur.deviceName.isEmpty()) cur.deviceName = DEF_DEVICE_NAME;
  if (cleanHostname(cur.hostname.c_str()).isEmpty()) cur.hostname = DEF_HOSTNAME;
}

const Settings& get() { return cur; }
uint16_t neoPixelType() { return ORDERS[cur.ledOrder].type + NEO_KHZ800; }
bool mqttEnabled() { return !cur.mqttHost.isEmpty(); }
bool printerMode() {
  return cur.mode == DeviceMode::Printer && !cur.bambuHost.isEmpty() && !cur.bambuSerial.isEmpty() &&
         !cur.bambuCode.isEmpty();
}

void writeJson(JsonObject o) {
  o["wifi_ssid"] = cur.wifiSsid;
  o["wifi_pass_set"] = !cur.wifiPass.isEmpty();
  o["device_name"] = cur.deviceName;
  o["hostname"] = cur.hostname;
  o["mqtt_host"] = cur.mqttHost;
  o["mqtt_port"] = cur.mqttPort;
  o["mqtt_user"] = cur.mqttUser;
  o["mqtt_pass_set"] = !cur.mqttPass.isEmpty();
  o["led_pin"] = cur.ledPin;
  o["led_count"] = cur.ledCount;
  o["led_order"] = ORDERS[cur.ledOrder].name;
  o["max_brightness"] = cur.maxBrightness;
  o["max_leds"] = MAX_LEDS;
  o["mode"] = MODE_KEYS[(uint8_t)cur.mode];
  o["bambu_host"] = cur.bambuHost;
  o["bambu_serial"] = cur.bambuSerial;
  o["bambu_code_set"] = !cur.bambuCode.isEmpty();
  JsonArray pins = o["pins"].to<JsonArray>();
  for (int p = 0; p < GPIO_PIN_COUNT; p++)
    if (validPin(p)) pins.add(p);
  JsonArray orders = o["orders"].to<JsonArray>();
  for (uint8_t i = 0; i < ORDER_COUNT; i++) orders.add(ORDERS[i].name);
}

bool applyJson(JsonVariantConst j, String& err) {
  Settings s = cur;

  if (!readStr(j, "wifi_ssid", 32, "WiFi name", true, s.wifiSsid, err) ||
      !readStr(j, "wifi_pass", 64, "WiFi password", false, s.wifiPass, err) ||
      !readStr(j, "device_name", 32, "Device name", true, s.deviceName, err) ||
      !readStr(j, "mqtt_host", 64, "MQTT server", true, s.mqttHost, err) ||
      !readStr(j, "mqtt_user", 64, "MQTT user", true, s.mqttUser, err) ||
      !readStr(j, "mqtt_pass", 64, "MQTT password", false, s.mqttPass, err) ||
      !readNum(j, "mqtt_port", 1, 65535, "MQTT port", s.mqttPort, err) ||
      !readNum(j, "led_count", 1, MAX_LEDS, "LED count", s.ledCount, err) ||
      !readNum(j, "max_brightness", 1, 255, "Max brightness", s.maxBrightness, err) ||
      !readNum(j, "led_pin", 0, 255, "LED pin", s.ledPin, err) ||
      !readStr(j, "bambu_host", 64, "Printer IP address", true, s.bambuHost, err) ||
      !readStr(j, "bambu_serial", 32, "Printer serial number", true, s.bambuSerial, err) ||
      !readStr(j, "bambu_code", 32, "Printer access code", true, s.bambuCode, err))
    return false;
  s.bambuSerial.toUpperCase();  // the printer's MQTT topics use the serial in upper case

  if (!j["mode"].isNull()) {
    const char* m = j["mode"] | "";
    uint8_t i = 0;
    while (i < (uint8_t)DeviceMode::COUNT && strcasecmp(m, MODE_KEYS[i]) != 0) i++;
    if (i == (uint8_t)DeviceMode::COUNT) { err = "Unknown mode"; return false; }
    s.mode = (DeviceMode)i;
  }
  if (s.mode == DeviceMode::Printer) {
    if (s.bambuHost.isEmpty()) { err = "Enter the printer's IP address"; return false; }
    if (s.bambuSerial.isEmpty()) { err = "Enter the printer's serial number"; return false; }
    if (s.bambuCode.isEmpty()) { err = "Enter the printer's access code"; return false; }
  }

  if (!s.wifiPass.isEmpty() && s.wifiPass.length() < 8) {
    err = "WiFi password must be at least 8 characters (or empty for an open network)";
    return false;
  }
  if (s.deviceName.isEmpty()) { err = "Device name can't be empty"; return false; }
  if (!validPin(s.ledPin)) { err = "GPIO " + String(s.ledPin) + " can't be used for the LED strip"; return false; }

  if (!j["hostname"].isNull()) {
    s.hostname = cleanHostname(j["hostname"] | "");
    if (s.hostname.isEmpty()) { err = "Hostname must contain letters or digits"; return false; }
  }
  if (!j["led_order"].isNull()) {
    const char* o = j["led_order"] | "";
    uint8_t i = 0;
    while (i < ORDER_COUNT && strcasecmp(o, ORDERS[i].name) != 0) i++;
    if (i == ORDER_COUNT) { err = "Unknown color order"; return false; }
    s.ledOrder = i;
  }

  save(s);
  cur = s;
  return true;
}

}  // namespace settings

#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// Device setup: WiFi, names, Home Assistant MQTT and the LED strip hardware.
// Edited on the web setup page (/setup), stored in flash. Changes apply after a restart.
namespace settings {

constexpr uint8_t MAX_LEDS = 127;  // LED indexes are stored as int8_t

// What drives the lamp. Lamp: normal stand-alone lamp (also when connected to Home
// Assistant). Printer: a Bambu Lab printer's status decides which lights/patterns run.
enum class DeviceMode : uint8_t { Lamp, Printer, COUNT };

struct Settings {
  DeviceMode mode;
  String   bambuHost;            // printer IP address (or hostname)
  String   bambuSerial;          // printer serial number, used in its MQTT topics
  String   bambuCode;            // printer's LAN access code (the MQTT password)
  String   wifiSsid, wifiPass;   // empty SSID = not configured (lamp starts its hotspot)
  String   deviceName;           // shown in Home Assistant
  String   hostname;             // http://<hostname>.local
  String   mqttHost;             // empty = Home Assistant integration disabled
  uint16_t mqttPort;
  String   mqttUser, mqttPass;
  uint8_t  ledPin;               // GPIO connected to the strip's DIN
  uint8_t  ledCount;             // total LEDs on the strip, including claws and top of head
  uint8_t  ledOrder;             // index into the color order list (GRB, RGB, ...)
  uint8_t  maxBrightness;        // hard cap 1-255, protects the power supply
};

void begin();
const Settings& get();
uint16_t neoPixelType();  // for Adafruit_NeoPixel (color order + 800 kHz)
bool mqttEnabled();
bool printerMode();       // printer status mode selected (and the printer connection filled in)

// Passwords and the printer access code are never sent out, only whether one is set
void writeJson(JsonObject out);
// Partial update. Validates everything first; on success saves to flash and returns true.
bool applyJson(JsonVariantConst in, String& error);

}  // namespace settings

#pragma once
#include <ArduinoJson.h>

// WiFi handling. Joins the saved network; if none is saved, or it can't connect
// within a short time, it starts an open setup hotspot ("LobserLamp-xxxx") with a
// captive portal so phones pop up the setup page automatically. While the hotspot
// is up it keeps retrying the saved network, and closes the hotspot once connected.
namespace netmgr {
void begin();
void loop();
bool apActive();
void writeStatus(JsonObject out);

// Test new credentials without saving them (Improv). Hotspot start/retry logic
// is paused until endTry(). On failure the saved network is reconnected.
void tryWifi(const String& ssid, const String& pass);
void endTry(bool success);
}

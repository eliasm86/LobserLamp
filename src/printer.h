#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// Printer status mode: reads a Bambu Lab printer's status over its local MQTT
// (TLS, port 8883, user "bblp", password = the printer's LAN access code) and shows
// it on the lamp. Each printer state (gcode_state, plus "OFFLINE" while the printer
// can't be reached) has its own look: a full set of lamp settings (body, effect, colors,
// brightness, speeds, claws, top of head), edited with the normal lamp page controls.
// The same settings work for all current models (X1, P1, A1, H2 series).
//
// The connection runs in its own FreeRTOS task, so a slow connect never freezes the
// lights. Looks are saved a few seconds after the last change (no restart).
namespace printer {

void begin();   // call after settings::begin() and lamp::begin(); does nothing in lamp mode
void loop();    // applies the current printer state to the lamp, saves changed looks
void flush();   // save changed looks now (before a restart)
bool active();  // printer status mode is running

// Status for the lamp page: {connected, state, percent, remaining_min, layer, total_layers,
// test + test_left (seconds) while a preview runs}
void writeStatusJson(JsonObject out);

// Status + the list of states {key, label}
void writeJson(JsonObject out);

// {"test":"FAILED"} shows that state's look for 15 s whatever the printer is doing (the
// progress bar fills up meanwhile); {"test":""} stops it. Returns false (with err) if invalid.
bool applyJson(JsonVariantConst in, String& err);

// One state's look, in the same JSON format as the lamp's own state (lamp::writeJson /
// applyJson; "Progress" is allowed as effect, LED roles are the lamp's). Changing a look
// also previews it for 15 s. Return false for an unknown state key (or when not running).
bool writeLookJson(const char* key, JsonObject out);
bool applyLookJson(const char* key, JsonVariantConst in, String& err);

}  // namespace printer

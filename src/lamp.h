#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// Progress is only for printer status looks (lit LEDs = % done); the lamp itself uses the first LAMP_MODE_COUNT.
enum class Mode : uint8_t { Solid, Wander, Pulse, Candle, Progress, COUNT };
constexpr uint8_t LAMP_MODE_COUNT = 4;

constexpr uint8_t CLAW_COUNT = 2;

// A single independently controlled LED (claws, top of head)
struct ClawState {
  bool    on;
  uint8_t r, g, b;
  uint8_t brightness;  // 1-255
};

struct LampState {
  bool      on;
  uint8_t   r, g, b;
  uint8_t   brightness;  // 1-255
  Mode      mode;
  uint8_t   speed;       // 1-100
  uint8_t   tail;        // 0-100 (trail length for moving modes)
  ClawState claw[CLAW_COUNT];    // claw lights, independent of the main power
  int8_t    clawLed[CLAW_COUNT]; // claw LED index on the strip, -1 = none
  int8_t    headLed;     // "Top of head" LED index on the strip, -1 = none
  ClawState head;        // head LED light: own color/brightness, not affected by power or effects
  uint8_t   pulseSpeed;  // 1-100
  uint8_t   pulseMin;    // 0-100 % of brightness
  uint8_t   pulseMax;    // 0-100 % of brightness
  uint8_t   candleSpeed; // 1-100
  bool      clawPulse;      // both claws pulse together (each in its own color/brightness);
                            // turning it on switches the individual claws off and vice versa
  uint8_t   clawPulseSpeed; // 1-100, same scale as head pulse (1 = 2 s on / 2 s off, 100 = 0.1 s)
  bool      clawToggle;     // claws take turns: claw 1, claw 2, claw 1, ... (each in its own color/brightness);
                            // excludes claw pulse and the individual claws the same way
  uint8_t   clawToggleSpeed; // 1-100 (1 = each claw stays on 2 s, 100 = 0.1 s)
  uint8_t   clawToggleFade;  // 0-100 %: 0 = hard on/off, 100 = continuous crossfade
  bool      headPulse;       // top of head blinks/breathes in its own color/brightness;
                             // turning it on switches the plain head light off and vice versa
  uint8_t   headPulseSpeed;  // 1-100 (1 = 2 s on / 2 s off, 100 = 0.1 s)
  uint8_t   headPulseFade;   // 0-100 %: 0 = hard blink, 100 = smooth breathing
  bool      headSync;        // head pulse follows the claw pattern while one runs: breathes with
                             // claw pulse, or is on with claw 1 during claw toggle. Its own
                             // speed/fade are used only while no claw pattern is running.
  uint8_t   clawPulseFade;   // 0-100 %: 0 = hard blink, 100 = smooth breathing (same as head pulse)
};

namespace lamp {

// Printer status mode: draw `look` instead of the lamp's own state (nothing is saved).
// The LED roles (head/claw LEDs) always come from the lamp. nullptr = back to the lamp's
// own state. progress (0-100 %) is used by Mode::Progress. The pointer must stay valid.
void setOverride(const LampState* look, float progress);

// Same JSON keys as applyJson/writeJson, for a printer status look instead of the lamp's
// own state. The LED role keys are ignored/left out; "Progress" is allowed as effect.
// applyLookJson returns true if anything changed.
bool applyLookJson(LampState& look, JsonVariantConst json);
void writeLookJson(const LampState& look, JsonObject out);

void begin();  // call after settings::begin()
void loop();
void flush();  // save pending changes now (before a restart)
void identify(int led);  // flash one LED 3 times (to find it when choosing LED roles)

const LampState& state();
uint32_t version();                    // increments on every change

// Apply a (partial) JSON update. Keys: state ("ON"/"OFF"), brightness,
// color {r,g,b}, effect ("Solid"/"Wander"/"Pulse"/"Candle"; "Progress" for printer looks only), speed, tail,
// pulse_speed (1-100), pulse_min/pulse_max (0-100 %), candle_speed (1-100), head_led / claw1_led / claw2_led (-1 = none),
// claw_pulse (switch value), claw_pulse_speed (1-100), claw_pulse_fade (0-100 %), claw_toggle (switch value), claw_toggle_speed (1-100), claw_toggle_fade (0-100 %),
// head_pulse (switch value), head_pulse_speed (1-100), head_pulse_fade (0-100 %), head_sync (switch value),
// head, claw1, claw2. The head and claw keys each take either an object with the same light keys
// ({state, brightness, color}) or a plain switch value (true/false or "ON"/"OFF").
// Same format as Home Assistant's MQTT JSON light schema.
void applyJson(JsonVariantConst json);
void writeJson(JsonObject out);

// Update or report one claw (index 0/1) using the light keys only
void applyClawJson(uint8_t i, JsonVariantConst json);
void writeClawJson(uint8_t i, JsonObject out);

// Update or report the top-of-head LED (light object or plain switch value)
void applyHeadJson(JsonVariantConst json);
void writeHeadJson(JsonObject out);

const char* modeLabel(Mode m);
}

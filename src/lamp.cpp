#include "lamp.h"
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>
#include "settings.h"

namespace {

using settings::MAX_LEDS;

Adafruit_NeoPixel strip;  // length, pin and type come from the setup settings in begin()

// ---- Defaults, used until changed (then the saved values are used) ----
// Main light: red, 80/255 brightness, Solid. Speed/tail 50.
// Claws: warm white, off, the last two LEDs on the strip. Top of head: white, on, no LED chosen.
// Pulse: speed 20, 10-100 %. Candle: speed 40. Claw pulse: off, speed 30, full fade. Claw toggle: off, speed 30, no fade.
// Head pulse: off, speed 30, full fade (smooth breathing), not synced to the claws.
const ClawState DEFAULT_CLAW = {false, 255, 180, 107, 255};
const ClawState DEFAULT_HEAD = {true, 255, 255, 255, 255};
LampState st = {true, 255, 0, 0, 80, Mode::Solid, 50, 50,
                {DEFAULT_CLAW, DEFAULT_CLAW}, {-1, -1}, -1, DEFAULT_HEAD,
                20, 10, 100, 40, false, 30, false, 30, 0, false, 30, 100, false, 100};

uint8_t ledCount() { return settings::get().ledCount; }
uint32_t maxBri() { return settings::get().maxBrightness; }
uint32_t ver = 1;

// Settings are saved to flash a few seconds after the last change (limits flash wear)
const uint32_t SAVE_DELAY_MS = 3000;
bool saveDirty = false;
uint32_t lastChangeMs = 0;

// Animation. Effects draw into "effect positions" 0..fxCount-1, which map to the
// strip LEDs that aren't a claw or the top of head (rebuilt every frame).
const uint32_t FRAME_MS = 16;  // ~60 fps
uint8_t fx[MAX_LEDS];
int fxCount = 0;
float trail[MAX_LEDS];        // per-position intensity 0-255 (float so slow fades aren't cut short by rounding)
float phase = 0;
uint32_t lastFrameMs = 0;
Mode renderedMode = Mode::Solid;

// Candle mode: shared flame level plus a small per-pixel variation
float flame = 1.0f, flameTarget = 1.0f;
float candleLvl[MAX_LEDS], candleTgt[MAX_LEDS];

const char* const LABELS[] = {"Solid", "Wander", "Pulse", "Candle", "Progress"};
static_assert(sizeof(LABELS) / sizeof(LABELS[0]) == (size_t)Mode::COUNT, "LABELS must match Mode");

bool modeFromLabel(const char* s, Mode& out) {
  for (uint8_t i = 0; i < (uint8_t)Mode::COUNT; i++)
    if (strcasecmp(s, LABELS[i]) == 0) { out = (Mode)i; return true; }
  return false;
}

bool validLed(int i) { return i >= 0 && i < ledCount(); }

// Parse an LED index from JSON (-1 or out of range = none)
void readLed(JsonVariantConst v, int8_t& led) {
  if (v.is<int>()) led = validLed(v.as<int>()) ? v.as<int>() : -1;
}

void load() {
  Preferences p;
  p.begin("lamp", true);
  st.on         = p.getBool("on", st.on);
  st.r          = p.getUChar("r", st.r);
  st.g          = p.getUChar("g", st.g);
  st.b          = p.getUChar("b", st.b);
  st.brightness = p.getUChar("bri", st.brightness);
  // Mode is saved by name. Older firmware saved an index into this list instead.
  if (p.isKey("effect")) {
    if (!modeFromLabel(p.getString("effect").c_str(), st.mode) || (uint8_t)st.mode >= LAMP_MODE_COUNT) st.mode = Mode::Solid;
  } else if (p.isKey("mode")) {
    static const char* const OLD[] = {"Solid", "Wander", "Wander" /* Center */, "Pulse", "Candle"};
    uint8_t m = p.getUChar("mode", 0);
    if (m >= sizeof(OLD) / sizeof(OLD[0]) || !modeFromLabel(OLD[m], st.mode)) st.mode = Mode::Solid;
  }
  st.speed      = p.getUChar("speed", st.speed);
  st.tail       = p.getUChar("tail", st.tail);
  st.headLed    = p.getChar("head", st.headLed);
  if (!validLed(st.headLed)) st.headLed = -1;
  st.head.on    = p.getBool("headOn", st.head.on);
  st.head.r     = p.getUChar("hr", st.head.r);
  st.head.g     = p.getUChar("hg", st.head.g);
  st.head.b     = p.getUChar("hb", st.head.b);
  st.head.brightness = max<uint8_t>(1, p.getUChar("hbri", st.head.brightness));
  st.pulseSpeed = constrain(p.getUChar("pspd", st.pulseSpeed), 1, 100);
  st.pulseMin   = min<uint8_t>(100, p.getUChar("pmin", st.pulseMin));
  st.pulseMax   = min<uint8_t>(100, p.getUChar("pmax", st.pulseMax));
  st.candleSpeed = constrain(p.getUChar("cspd", st.candleSpeed), 1, 100);
  st.clawPulse  = p.getBool("cpulse", st.clawPulse);
  st.clawPulseSpeed = constrain(p.getUChar("cpspd", st.clawPulseSpeed), 1, 100);
  st.clawPulseFade = min<uint8_t>(100, p.getUChar("cpfade", st.clawPulseFade));
  st.clawToggle = p.getBool("ctog", st.clawToggle) && !st.clawPulse;
  st.clawToggleSpeed = constrain(p.getUChar("ctspd", st.clawToggleSpeed), 1, 100);
  st.clawToggleFade = min<uint8_t>(100, p.getUChar("ctfade", st.clawToggleFade));
  st.headPulse = p.getBool("hpulse", st.headPulse);
  st.headPulseSpeed = constrain(p.getUChar("hpspd", st.headPulseSpeed), 1, 100);
  st.headPulseFade = min<uint8_t>(100, p.getUChar("hpfade", st.headPulseFade));
  st.headSync = p.getBool("hsync", st.headSync);
  for (uint8_t i = 0; i < CLAW_COUNT; i++) {
    ClawState& c = st.claw[i];
    char key[8];
    snprintf(key, sizeof(key), "claw%d", i + 1);  // on/off, same key as the old GPIO claws
    c.on = p.getBool(key, c.on);
    snprintf(key, sizeof(key), "c%dr", i + 1);   c.r = p.getUChar(key, c.r);
    snprintf(key, sizeof(key), "c%dg", i + 1);   c.g = p.getUChar(key, c.g);
    snprintf(key, sizeof(key), "c%db", i + 1);   c.b = p.getUChar(key, c.b);
    snprintf(key, sizeof(key), "c%dbri", i + 1); c.brightness = max<uint8_t>(1, p.getUChar(key, c.brightness));
    snprintf(key, sizeof(key), "c%dled", i + 1);
    st.clawLed[i] = p.getChar(key, ledCount() - CLAW_COUNT + i);  // default: the last LEDs
    if (!validLed(st.clawLed[i])) st.clawLed[i] = -1;
  }
  p.end();
}

void save() {
  Preferences p;
  p.begin("lamp", false);
  p.putBool("on", st.on);
  p.putUChar("r", st.r);
  p.putUChar("g", st.g);
  p.putUChar("b", st.b);
  p.putUChar("bri", st.brightness);
  p.putString("effect", LABELS[(uint8_t)st.mode]);
  if (p.isKey("mode")) p.remove("mode");  // old index-based key
  p.putUChar("speed", st.speed);
  p.putUChar("tail", st.tail);
  p.putChar("head", st.headLed);
  p.putBool("headOn", st.head.on);
  p.putUChar("hr", st.head.r);
  p.putUChar("hg", st.head.g);
  p.putUChar("hb", st.head.b);
  p.putUChar("hbri", st.head.brightness);
  p.putUChar("pspd", st.pulseSpeed);
  p.putUChar("pmin", st.pulseMin);
  p.putUChar("pmax", st.pulseMax);
  p.putUChar("cspd", st.candleSpeed);
  p.putBool("cpulse", st.clawPulse);
  p.putUChar("cpspd", st.clawPulseSpeed);
  p.putUChar("cpfade", st.clawPulseFade);
  p.putBool("ctog", st.clawToggle);
  p.putUChar("ctspd", st.clawToggleSpeed);
  p.putUChar("ctfade", st.clawToggleFade);
  p.putBool("hpulse", st.headPulse);
  p.putUChar("hpspd", st.headPulseSpeed);
  p.putUChar("hpfade", st.headPulseFade);
  p.putBool("hsync", st.headSync);
  for (uint8_t i = 0; i < CLAW_COUNT; i++) {
    const ClawState& c = st.claw[i];
    char key[8];
    snprintf(key, sizeof(key), "claw%d", i + 1); p.putBool(key, c.on);
    snprintf(key, sizeof(key), "c%dr", i + 1);   p.putUChar(key, c.r);
    snprintf(key, sizeof(key), "c%dg", i + 1);   p.putUChar(key, c.g);
    snprintf(key, sizeof(key), "c%db", i + 1);   p.putUChar(key, c.b);
    snprintf(key, sizeof(key), "c%dbri", i + 1); p.putUChar(key, c.brightness);
    snprintf(key, sizeof(key), "c%dled", i + 1); p.putChar(key, st.clawLed[i]);
  }
  p.end();
  Serial.println("Settings saved");
}

void changed() {
  ver++;
  saveDirty = true;
  lastChangeMs = millis();
}

// Shared light keys: state, brightness, color {r,g,b}
void readLight(JsonVariantConst j, bool& on, uint8_t& bri, uint8_t& r, uint8_t& g, uint8_t& b) {
  if (j["state"].is<const char*>()) on = strcasecmp(j["state"], "ON") == 0;
  if (j["brightness"].is<int>()) bri = constrain(j["brightness"].as<int>(), 1, 255);
  JsonVariantConst c = j["color"];
  if (c.is<JsonObjectConst>()) {
    if (c["r"].is<int>()) r = constrain(c["r"].as<int>(), 0, 255);
    if (c["g"].is<int>()) g = constrain(c["g"].as<int>(), 0, 255);
    if (c["b"].is<int>()) b = constrain(c["b"].as<int>(), 0, 255);
  }
}

void writeLight(JsonObject o, bool on, uint8_t bri, uint8_t r, uint8_t g, uint8_t b) {
  o["state"] = on ? "ON" : "OFF";
  o["brightness"] = bri;
  o["color_mode"] = "rgb";
  JsonObject c = o["color"].to<JsonObject>();
  c["r"] = r;
  c["g"] = g;
  c["b"] = b;
}

// Plain switch value: true/false, 1/0, "ON"/"OFF". Leaves `on` untouched otherwise.
void readSwitch(JsonVariantConst v, bool& on) {
  if (v.is<bool>()) on = v.as<bool>();
  else if (v.is<int>()) on = v.as<int>() != 0;
  else if (v.is<const char*>()) on = strcasecmp(v.as<const char*>(), "ON") == 0;
}

// Claw value: a light object, or a plain switch
void readClaw(JsonVariantConst v, ClawState& c) {
  if (v.is<JsonObjectConst>()) readLight(v, c.on, c.brightness, c.r, c.g, c.b);
  else readSwitch(v, c.on);
}

// Claw pulse, claw toggle and the individual claws exclude each other: turning one on
// switches the others off. If several are turned on at once, pulse wins over toggle,
// and both win over the individual claws.
void enforceClawModes(LampState& s, const LampState& old) {
  if (s.clawPulse && !old.clawPulse) {
    for (ClawState& c : s.claw) c.on = false;
    s.clawToggle = false;
    return;
  }
  if (s.clawToggle && !old.clawToggle) {
    for (ClawState& c : s.claw) c.on = false;
    s.clawPulse = false;
    return;
  }
  for (uint8_t i = 0; i < CLAW_COUNT; i++)
    if (s.claw[i].on && !old.claw[i].on) s.clawPulse = s.clawToggle = false;
}

// Head pulse and the plain head light exclude each other the same way (pulse wins if both)
void enforceHeadModes(LampState& s, const LampState& old) {
  if (s.headPulse && !old.headPulse) s.head.on = false;
  else if (s.head.on && !old.head.on) s.headPulse = false;
}

// Apply the JSON keys to any state (the lamp's own or a printer look). Returns true if
// anything changed. LED roles only for the lamp's own state; Progress only for looks.
bool applyTo(LampState& s, JsonVariantConst j, bool look) {
  LampState old = s;
  readLight(j, s.on, s.brightness, s.r, s.g, s.b);
  if (j["effect"].is<const char*>()) {
    Mode m;
    if (modeFromLabel(j["effect"], m) && (look || (uint8_t)m < LAMP_MODE_COUNT)) s.mode = m;
  }
  if (j["speed"].is<int>()) s.speed = constrain(j["speed"].as<int>(), 1, 100);
  if (j["tail"].is<int>()) s.tail = constrain(j["tail"].as<int>(), 0, 100);
  if (j["pulse_speed"].is<int>()) s.pulseSpeed = constrain(j["pulse_speed"].as<int>(), 1, 100);
  if (j["pulse_min"].is<int>()) s.pulseMin = constrain(j["pulse_min"].as<int>(), 0, 100);
  if (j["pulse_max"].is<int>()) s.pulseMax = constrain(j["pulse_max"].as<int>(), 0, 100);
  if (j["candle_speed"].is<int>()) s.candleSpeed = constrain(j["candle_speed"].as<int>(), 1, 100);
  if (!look) {
    readLed(j["head_led"], s.headLed);
    readLed(j["claw1_led"], s.clawLed[0]);
    readLed(j["claw2_led"], s.clawLed[1]);
  }
  readClaw(j["head"], s.head);
  readClaw(j["claw1"], s.claw[0]);
  readClaw(j["claw2"], s.claw[1]);
  readSwitch(j["claw_pulse"], s.clawPulse);
  if (j["claw_pulse_speed"].is<int>()) s.clawPulseSpeed = constrain(j["claw_pulse_speed"].as<int>(), 1, 100);
  if (j["claw_pulse_fade"].is<int>()) s.clawPulseFade = constrain(j["claw_pulse_fade"].as<int>(), 0, 100);
  readSwitch(j["claw_toggle"], s.clawToggle);
  if (j["claw_toggle_speed"].is<int>()) s.clawToggleSpeed = constrain(j["claw_toggle_speed"].as<int>(), 1, 100);
  if (j["claw_toggle_fade"].is<int>()) s.clawToggleFade = constrain(j["claw_toggle_fade"].as<int>(), 0, 100);
  readSwitch(j["head_pulse"], s.headPulse);
  if (j["head_pulse_speed"].is<int>()) s.headPulseSpeed = constrain(j["head_pulse_speed"].as<int>(), 1, 100);
  if (j["head_pulse_fade"].is<int>()) s.headPulseFade = constrain(j["head_pulse_fade"].as<int>(), 0, 100);
  readSwitch(j["head_sync"], s.headSync);
  enforceClawModes(s, old);
  enforceHeadModes(s, old);
  return memcmp(&old, &s, sizeof(s)) != 0;
}

// Write the JSON keys for any state; LED roles are written by the caller
void writeFrom(const LampState& s, JsonObject o) {
  writeLight(o, s.on, s.brightness, s.r, s.g, s.b);
  o["effect"] = LABELS[(uint8_t)s.mode];
  o["speed"] = s.speed;
  o["tail"] = s.tail;
  o["pulse_speed"] = s.pulseSpeed;
  o["pulse_min"] = s.pulseMin;
  o["pulse_max"] = s.pulseMax;
  o["candle_speed"] = s.candleSpeed;
  o["claw_pulse"] = s.clawPulse ? "ON" : "OFF";
  o["claw_pulse_speed"] = s.clawPulseSpeed;
  o["claw_pulse_fade"] = s.clawPulseFade;
  o["claw_toggle"] = s.clawToggle ? "ON" : "OFF";
  o["claw_toggle_speed"] = s.clawToggleSpeed;
  o["claw_toggle_fade"] = s.clawToggleFade;
  o["head_pulse"] = s.headPulse ? "ON" : "OFF";
  o["head_pulse_speed"] = s.headPulseSpeed;
  o["head_pulse_fade"] = s.headPulseFade;
  o["head_sync"] = s.headSync ? "ON" : "OFF";
  writeLight(o["head"].to<JsonObject>(), s.head.on, s.head.brightness, s.head.r, s.head.g, s.head.b);
  for (uint8_t i = 0; i < CLAW_COUNT; i++) {
    const ClawState& c = s.claw[i];
    writeLight(o[i == 0 ? "claw1" : "claw2"].to<JsonObject>(), c.on, c.brightness, c.r, c.g, c.b);
  }
}

// Draw a dot at a fractional position, spread over two pixels for smooth motion
void plot(float x) {
  int i = (int)floorf(x);
  float f = x - i;
  auto put = [](int idx, float v) {
    if (idx < 0 || idx >= fxCount) return;
    float val = v * 255.0f;
    if (val > trail[idx]) trail[idx] = val;
  };
  put(i, 1.0f - f);
  put(i + 1, f);
}

// Triangle wave 0 -> span -> 0 driven by phase
float pingpong(float span) {
  if (span <= 0) return 0;
  float x = fmodf(phase, 2 * span);
  return x <= span ? x : 2 * span - x;
}

// What is drawn this frame: the lamp state, or the printer look while one is set
// (see render()). The render functions below read only this, never `st`.
LampState fr;
const LampState* ovLook = nullptr;
float ovProgress = 0;

// Collect the LEDs the effects may use (everything except claws and top of head)
void buildFxMap() {
  bool reserved[MAX_LEDS] = {};
  if (validLed(fr.headLed)) reserved[fr.headLed] = true;
  for (uint8_t i = 0; i < CLAW_COUNT; i++)
    if (validLed(fr.clawLed[i])) reserved[fr.clawLed[i]] = true;
  fxCount = 0;
  for (int i = 0; i < ledCount(); i++)
    if (!reserved[i]) fx[fxCount++] = i;
}

void renderMain(float dt) {
  buildFxMap();
  if (!fr.on) {
    for (int i = 0; i < fxCount; i++) strip.setPixelColor(fx[i], 0);
    return;
  }

  if (fr.mode == Mode::Progress) {
    // Progress bar: lit LEDs = % done, in the body color. The LED at the edge is
    // partly lit so the bar grows smoothly instead of jumping one LED at a time.
    uint32_t maxB = fr.brightness * maxBri() / 255;
    float lit = constrain(ovProgress, 0.0f, 100.0f) / 100.0f * fxCount;
    for (int i = 0; i < fxCount; i++) {
      float l = constrain(lit - i, 0.0f, 1.0f);
      uint32_t s = (uint32_t)Adafruit_NeoPixel::gamma8((uint8_t)lroundf(l * 255)) * maxB / 255;
      strip.setPixelColor(fx[i], fr.r * s / 255, fr.g * s / 255, fr.b * s / 255);
    }
    return;
  }

  if (fr.mode != renderedMode) {  // reset animation on mode change
    for (float& t : trail) t = 0;
    phase = 0;
    flame = flameTarget = 1.0f;
    for (int i = 0; i < ledCount(); i++) candleLvl[i] = candleTgt[i] = 1.0f;
    renderedMode = fr.mode;
  }

  uint32_t maxB = fr.brightness * maxBri() / 255;

  if (fr.mode == Mode::Pulse) {
    // Slow breathing: one cycle takes ~23 s at speed 1 down to ~1 s at speed 100.
    // Min/max are linear output levels; the wave is squared so it lingers near the
    // dim end, which looks smoother to the eye than a plain sine.
    float periodSec = 30.0f / (1.0f + fr.pulseSpeed * 0.29f);
    phase += dt / periodSec;
    phase -= floorf(phase);
    float w = 0.5f - 0.5f * cosf(2.0f * PI * phase);
    w *= w;
    float lo = min(fr.pulseMin, fr.pulseMax) / 100.0f;
    float hi = max(fr.pulseMin, fr.pulseMax) / 100.0f;
    uint32_t s = (uint32_t)lroundf((lo + (hi - lo) * w) * maxB);
    uint32_t c = strip.Color(fr.r * s / 255, fr.g * s / 255, fr.b * s / 255);
    for (int i = 0; i < fxCount; i++) strip.setPixelColor(fx[i], c);
    return;
  }

  if (fr.mode == Mode::Candle) {
    // Random targets picked at a speed-dependent rate, eased towards smoothly.
    // The whole flame flickers together, each pixel adds a little of its own.
    float sp = fr.candleSpeed / 100.0f;
    float retarget = (2.0f + sp * 28.0f) * dt;             // chance per frame of a new target
    float ease = 1.0f - expf(-dt * (3.0f + sp * 37.0f));  // how fast levels follow
    auto rnd = []() { return (esp_random() & 0xFFFF) / 65535.0f; };

    if (rnd() < retarget) {
      float r = rnd();
      flameTarget = r < 0.08f ? 0.35f + rnd() * 0.2f   // occasional deep dip (draft)
                              : 0.7f + rnd() * 0.3f;   // normal flicker
    }
    flame += (flameTarget - flame) * ease;

    for (int i = 0; i < fxCount; i++) {
      if (rnd() < retarget) candleTgt[i] = 0.75f + rnd() * 0.25f;
      candleLvl[i] += (candleTgt[i] - candleLvl[i]) * ease;
      float l = flame * candleLvl[i];
      uint32_t s = (uint32_t)lroundf(l * maxB);
      // Dimmer = slightly warmer: green and blue drop faster than red
      float gK = 0.6f + 0.4f * l, bK = l;
      strip.setPixelColor(fx[i], fr.r * s / 255, (uint32_t)(fr.g * gK) * s / 255, (uint32_t)(fr.b * bK) * s / 255);
    }
    return;
  }

  if (fr.mode == Mode::Solid) {
    for (float& t : trail) t = 255;
  } else {
    // Wander: a dot travels from one end to the other and back.
    // Fade existing pixels -> leaves a trail behind the moving dot
    // Tail 0-100: the fade gets slower, and the trail lasts up to 3x longer at 100
    // than the plain curve would give (x1 at 0, x3 at 100).
    float stretch = powf(3.0f, fr.tail / 100.0f);
    float keep = powf(0.5f + fr.tail * 0.0045f, dt * 60.0f / stretch);
    for (int i = 0; i < fxCount; i++) trail[i] *= keep;

    float pixelsPerSec = 1.0f + fr.speed * 0.3f;
    float span = fxCount - 1;
    phase += pixelsPerSec * dt;
    if (span > 0) phase = fmodf(phase, 2 * span);
    plot(pingpong(span));
  }

  for (int i = 0; i < fxCount; i++) {
    uint32_t s = (uint32_t)Adafruit_NeoPixel::gamma8((uint8_t)trail[i]) * maxB / 255;
    strip.setPixelColor(fx[i], fr.r * s / 255, fr.g * s / 255, fr.b * s / 255);
  }
}

// Speed 1-100 -> how long each slot lasts: 2 s down to 0.1 s (even steps on a log scale)
float slotSeconds(uint8_t speed) { return 2.0f * powf(0.05f, (speed - 1) / 99.0f); }

// Level 0-1 of slot i in a cycle of n equal slots, at position t (0..n, in slots).
// Without fade the slot is fully on inside its slot and off outside. With fade 0-1 it
// ramps over a window of `fade` slots centred on each slot boundary, so neighbouring
// slots cross at half level and always add up to full.
float slotLevel(float t, uint8_t i, uint8_t n, float fade) {
  float d = fabsf(fmodf(t - i - 0.5f + 1.5f * n, n) - n / 2.0f);  // distance from slot middle
  if (fade <= 0) return d < 0.5f ? 1.0f : 0.0f;
  float x = constrain((0.5f + fade / 2 - d) / fade, 0.0f, 1.0f);
  return x * x * (3 - 2 * x);  // smoothstep: eases in and out of each ramp
}

// Claw levels (0-1) from the latest frame; renderClaws() runs first so head sync can follow them
float clawLevel[CLAW_COUNT] = {};

// "Top of head" LED, not touched by the effects. Head pulse: on and off take turns
// (slot 0 = on, slot 1 = off), with optional fade between them. With sync on and a claw
// pattern running, it copies claw 1's level instead (same for both claws during claw pulse).
float headPhase = 0;

void renderHead(float dt) {
  float level;
  if (fr.headPulse && fr.headSync && (fr.clawPulse || fr.clawToggle)) {
    headPhase = 0;
    level = clawLevel[0];
  } else if (fr.headPulse) {
    headPhase += dt / (slotSeconds(fr.headPulseSpeed) * 2);
    headPhase -= floorf(headPhase);
    level = slotLevel(headPhase * 2, 0, 2, fr.headPulseFade / 100.0f);
  } else {
    headPhase = 0;  // next pulse starts with the "on" half
    level = fr.head.on ? 1.0f : 0.0f;
  }
  if (!validLed(fr.headLed)) return;
  const ClawState& h = fr.head;
  uint32_t s = (uint32_t)lroundf(level * h.brightness * maxBri() / 255);
  strip.setPixelColor(fr.headLed, h.r * s / 255, h.g * s / 255, h.b * s / 255);
}

// Claw pulse: both claws pulse together, each in its own color, up to its own brightness.
// Same on/off timing and fade as head pulse, so the two match when synced.
// Claw toggle: the claws take turns being on, each in its own color and brightness,
// optionally ramping across each handover (fade).
float clawPhase = 0;

void renderClaws(float dt) {
  float* level = clawLevel;
  if (fr.clawPulse) {
    clawPhase += dt / (slotSeconds(fr.clawPulseSpeed) * 2);  // slot 0 = on, slot 1 = off
    clawPhase -= floorf(clawPhase);
    float l = slotLevel(clawPhase * 2, 0, 2, fr.clawPulseFade / 100.0f);
    for (uint8_t i = 0; i < CLAW_COUNT; i++) level[i] = l;
  } else if (fr.clawToggle) {
    // Each claw owns one slot of the cycle
    clawPhase += dt / (slotSeconds(fr.clawToggleSpeed) * CLAW_COUNT);
    clawPhase -= floorf(clawPhase);
    for (uint8_t i = 0; i < CLAW_COUNT; i++)
      level[i] = slotLevel(clawPhase * CLAW_COUNT, i, CLAW_COUNT, fr.clawToggleFade / 100.0f);
  } else {
    clawPhase = 0;  // next pulse starts with the "on" half, next toggle starts with claw 1
    for (uint8_t i = 0; i < CLAW_COUNT; i++) level[i] = fr.claw[i].on ? 1.0f : 0.0f;
  }
  for (uint8_t i = 0; i < CLAW_COUNT; i++) {
    if (!validLed(fr.clawLed[i])) continue;
    const ClawState& c = fr.claw[i];
    uint32_t s = (uint32_t)lroundf(level[i] * c.brightness * maxBri() / 255);
    strip.setPixelColor(fr.clawLed[i], c.r * s / 255, c.g * s / 255, c.b * s / 255);
  }
}

// Identify: one LED blinks white 3 times on top of everything else
const uint32_t IDENTIFY_PERIOD_MS = 400;  // 200 ms on, 200 ms off
const uint8_t IDENTIFY_FLASHES = 3;
int identifyLed = -1;
uint32_t identifyStartMs = 0;

void renderIdentify() {
  if (identifyLed < 0) return;
  uint32_t t = millis() - identifyStartMs;
  if (t >= IDENTIFY_PERIOD_MS * IDENTIFY_FLASHES || !validLed(identifyLed)) {
    identifyLed = -1;  // done; the normal render restores the LED next frame
    return;
  }
  uint32_t v = (t % IDENTIFY_PERIOD_MS) < IDENTIFY_PERIOD_MS / 2 ? maxBri() : 0;
  strip.setPixelColor(identifyLed, v, v, v);
}

void render(float dt) {
  if (ovLook) {
    // Printer mode: the look for the printer's status, on the lamp's LED roles
    fr = *ovLook;
    fr.headLed = st.headLed;
    memcpy(fr.clawLed, st.clawLed, sizeof(fr.clawLed));
  } else {
    fr = st;
  }
  renderMain(dt);
  renderClaws(dt);  // before the head: head sync uses the claw levels
  renderHead(dt);
  renderIdentify();
  strip.show();
}

}  // namespace

namespace lamp {

void begin() {
  load();
  const settings::Settings& s = settings::get();
  strip.updateType(settings::neoPixelType());
  strip.updateLength(s.ledCount);
  strip.setPin(s.ledPin);
  strip.begin();
  strip.clear();
  strip.show();
  lastFrameMs = millis();
}

void loop() {
  uint32_t now = millis();
  if (now - lastFrameMs >= FRAME_MS) {
    float dt = (now - lastFrameMs) / 1000.0f;
    lastFrameMs = now;
    render(dt);
  }
  if (saveDirty && now - lastChangeMs > SAVE_DELAY_MS) flush();
}

void setOverride(const LampState* look, float progress) {
  ovLook = look;
  ovProgress = progress;
}

bool applyLookJson(LampState& look, JsonVariantConst j) { return applyTo(look, j, true); }
void writeLookJson(const LampState& look, JsonObject o) { writeFrom(look, o); }

void identify(int led) {
  if (!validLed(led)) return;
  identifyLed = led;
  identifyStartMs = millis();
}

void flush() {
  if (!saveDirty) return;
  saveDirty = false;
  save();
}

const LampState& state() { return st; }
uint32_t version() { return ver; }
const char* modeLabel(Mode m) { return LABELS[(uint8_t)m]; }

void applyJson(JsonVariantConst j) {
  if (applyTo(st, j, false)) changed();
}

void writeJson(JsonObject o) {
  writeFrom(st, o);
  o["head_led"] = st.headLed;
  o["claw1_led"] = st.clawLed[0];
  o["claw2_led"] = st.clawLed[1];
  o["led_count"] = ledCount();
}

void applyHeadJson(JsonVariantConst j) {
  LampState old = st;
  readClaw(j, st.head);
  enforceHeadModes(st, old);
  if (memcmp(&old, &st, sizeof(st)) != 0) changed();
}

void writeHeadJson(JsonObject o) {
  const ClawState& h = st.head;
  writeLight(o, h.on, h.brightness, h.r, h.g, h.b);
}

void applyClawJson(uint8_t i, JsonVariantConst j) {
  if (i >= CLAW_COUNT) return;
  LampState old = st;
  readClaw(j, st.claw[i]);
  enforceClawModes(st, old);
  if (memcmp(&old, &st, sizeof(st)) != 0) changed();
}

void writeClawJson(uint8_t i, JsonObject o) {
  if (i >= CLAW_COUNT) return;
  const ClawState& c = st.claw[i];
  writeLight(o, c.on, c.brightness, c.r, c.g, c.b);
}

}  // namespace lamp

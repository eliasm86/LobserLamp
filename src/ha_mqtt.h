#pragma once

// Home Assistant integration via MQTT discovery.
// Shows up as a light (on/off, color, brightness, effect) plus Brightness/Speed/Tail
// number sliders, Pulse speed/min/max and Candle speed sliders, a Mode select, two Claw lights
// and a "Top of head" light.
namespace ha_mqtt {
void begin();
void loop();
bool enabled();
bool connected();
}

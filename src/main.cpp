#include <Arduino.h>
#include "settings.h"
#include "netmgr.h"
#include "lamp.h"
#include "web.h"
#include "ha_mqtt.h"
#include "serial_setup.h"
#include "printer.h"

void setup() {
  Serial.begin(115200);
  Serial.println("\nLobserLamp starting");

  settings::begin();  // WiFi, MQTT and LED strip setup (web page /setup)
  lamp::begin();      // LEDs work even without WiFi
  netmgr::begin();    // joins WiFi, or starts the setup hotspot
  web::begin();       // web UI, also reachable through the hotspot
  ha_mqtt::begin();
  printer::begin();   // printer status mode only: connects to the Bambu Lab printer
  serial_setup::begin();  // WiFi over USB: Improv (browser flashers) or text commands
}

void loop() {
  lamp::loop();
  netmgr::loop();
  web::loop();
  ha_mqtt::loop();
  printer::loop();
  serial_setup::loop();
}

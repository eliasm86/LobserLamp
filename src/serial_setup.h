#pragma once

// WiFi setup over USB serial (115200 baud), two ways:
//  - Improv Serial (https://www.improv-wifi.com/serial/): browser flashers such as
//    ESP Web Tools ask for WiFi right after flashing. Credentials are tested first
//    and only saved if the lamp could connect.
//  - Text commands in a serial monitor:
//      wifi <ssid> [password]   save WiFi and restart (quote names with spaces)
//      status                   show WiFi, hotspot and MQTT status
//      restart                  restart the lamp
//      help
namespace serial_setup {
void begin();
void loop();
}

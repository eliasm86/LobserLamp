#pragma once

// Web interface on port 80:
//   GET /            lamp controls        GET/POST /api/state  lamp state (same format as lamp::applyJson); ?look=STATE = that printer state's look
//   GET /setup       setup page           GET/POST /api/setup  device setup (POST saves and restarts)
//                                         GET /api/scan        WiFi networks (async, poll until ready)
//                                         POST /api/identify   {"led": n} flash one LED 3 times
//                                         GET/POST /api/printer printer status + state list; POST {"test":"STATE"} previews a look
namespace web {
void begin();
void loop();
}

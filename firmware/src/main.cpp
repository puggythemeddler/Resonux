#include "runtime/App.h"
#include <Arduino.h>

void setup() { App::instance().begin(); }

void loop() { vTaskDelay(pdMS_TO_TICKS(1000)); }
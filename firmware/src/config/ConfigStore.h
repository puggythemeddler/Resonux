#pragma once
#include "config/Config.h"
#include <Arduino.h>
#include <Print.h>

namespace ConfigStore {

bool begin();
bool exists();
bool load(Config& cfg);
bool save(const Config& cfg);
String dumpString(const Config& cfg);
void printToSerial(const Config& cfg);

}  // namespace ConfigStore
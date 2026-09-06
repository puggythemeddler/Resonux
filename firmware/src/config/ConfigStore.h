#pragma once
#include "config/Config.h"

namespace ConfigStore {

bool begin();
bool exists();
bool load(Config& cfg);
bool save(const Config& cfg);
void printToSerial(const Config& cfg);

}  // namespace ConfigStore
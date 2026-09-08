#pragma once
#include "display/DisplayDriver.h"
#include "display/DisplayTypes.h"
#include "display/TouchDriver.h"

#ifdef ENABLE_TOUCHUI

// Concrete driver factory. Purely config-driven — no hardware assumptions
// baked in. Returns nullptr when the config asks for no/unknown hardware.
DisplayDriver* createDisplayDriver(const DisplayConfig& cfg);
TouchDriver*   createTouchDriver(const DisplayConfig& cfg);

#endif  // ENABLE_TOUCHUI
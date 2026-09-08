#include "display/DisplayFactory.h"

#ifdef ENABLE_TOUCHUI

#include "display/panels/Ili9488Driver.h"
#include "display/touch/Ft6236Driver.h"

DisplayDriver* createDisplayDriver(const DisplayConfig& cfg) {
  switch (cfg.panel) {
    case DISPLAY_ILI9488:
      return new Ili9488Driver(cfg);
    default:
      return nullptr;
  }
}

TouchDriver* createTouchDriver(const DisplayConfig& cfg) {
  switch (cfg.touch) {
    case TOUCH_FT6236:
      return new Ft6236Driver(cfg);
    default:
      return nullptr;
  }
}

#endif  // ENABLE_TOUCHUI
#pragma once
#include <stdint.h>

// Hardware-independent display & touch declarations. Concrete drivers
// (ILI9488, ILI9341, FT6236, ...) register with DisplayManager; nothing
// here touches GPIO directly.

enum DisplayPanelId : int {
  DISPLAY_NONE = 0,   // no panel attached
  DISPLAY_ILI9488,    // reference: 320x480 RGB
  DISPLAY_ILI9341,    // 240x320
  DISPLAY_ST7789,     // 240x240 / 240x320 variants
  DISPLAY_PANEL_COUNT
};

enum TouchChipId : int {
  TOUCH_NONE = 0,
  TOUCH_FT6236,       // I2C capacitive (most 3.5" SPI modules)
  TOUCH_XPT2046,      // SPI resistive
  TOUCH_CHIP_COUNT
};

enum DisplayOrientation : int {
  ORIENT_PORTRAIT = 0,   // logical reference 320x480
  ORIENT_LANDSCAPE = 1,
};

struct DisplayConfig {
  bool      enabled = false;
  int       panel = DISPLAY_NONE;
  int       touch = TOUCH_NONE;
  int       orientation = ORIENT_PORTRAIT;
  int       spiSck = 12;
  int       spiMosi = 11;
  int       spiMiso = 13;
  int       csPin = 10;
  int       dcPin = 9;
  int       rstPin = 14;
  int       blPin = 21;          // backlight — screen brightness, NOT LED master
  int       touchSda = 8;        // I2C touch (FT6236)
  int       touchScl = 3;
  int       touchIrq = 34;
  int       touchRst = -1;
  int       logicalW = 320;      // responsive reference; scaled to panel
  int       logicalH = 480;
  int       backlightPct = 70;   // screen brightness % (separate from LEDs)
  int       screenTimeoutS = 60; // 0 => always on
  int       uiFps = 30;          // controlled UI render rate
};

struct TouchPoint {
  int  x = -1;
  int  y = -1;
  bool touched = false;
};
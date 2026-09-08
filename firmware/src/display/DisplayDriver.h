#pragma once
#include <stdint.h>

// Abstract display driver. Implementations (ILI9488, ILI9341, ST7789, ...)
// are created and owned by DisplayManager. Coordinates are PHYSICAL panel
// pixels; DisplayManager translates logical UI coordinates before calling.

class DisplayDriver {
public:
  virtual ~DisplayDriver() {}

  virtual bool init() = 0;
  virtual const char* name() const = 0;

  virtual bool isReady() const = 0;
  virtual int  width() const = 0;   // physical, after rotation
  virtual int  height() const = 0;

  virtual void setRotation(uint8_t r) = 0;
  virtual uint8_t rotation() const = 0;

  virtual void drawPixel(int x, int y, uint16_t c) = 0;
  virtual void fillRect(int x, int y, int w, int h, uint16_t c) = 0;
  // Bulk RGB565 (big-endian bytes) write; LVGL flush uses this.
  virtual void writeArea(int x, int y, int w, int h,
                         const uint16_t* pixels) = 0;
  virtual void flush() = 0;

  // Backlight % (0..100) — screen brightness, fully separate from LED master.
  virtual void setBacklightPct(uint8_t pct) = 0;
  virtual void setPowered(bool on) = 0;
};
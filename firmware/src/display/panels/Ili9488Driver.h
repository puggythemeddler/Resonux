#pragma once
#include "display/DisplayDriver.h"
#include "display/DisplayTypes.h"

#ifdef ENABLE_TOUCHUI

#include <SPI.h>

// ILI9488 320x480 SPI panel (the reference 3.5" module). Pins and dimensions
// come from DisplayConfig. 16bpp RGB565, big-endian byte order on the wire.

class Ili9488Driver : public DisplayDriver {
public:
  explicit Ili9488Driver(const DisplayConfig& cfg) : _cfg(cfg) {}

  bool init() override;
  const char* name() const override { return "ILI9488"; }

  bool isReady() const override { return _ready; }
  int  width() const override { return _w; }
  int  height() const override { return _h; }

  void setRotation(uint8_t r) override;
  uint8_t rotation() const override { return _rot; }

  void drawPixel(int x, int y, uint16_t c) override;
  void fillRect(int x, int y, int w, int h, uint16_t c) override;
  void writeArea(int x, int y, int w, int h,
                 const uint16_t* pixels) override;
  void flush() override;

  void setBacklightPct(uint8_t pct) override;
  void setPowered(bool on) override;

private:
  void writeCmd(uint8_t c);
  void writeData(uint8_t d);
  void setWindow(int x, int y, int w, int h);

  const DisplayConfig& _cfg;
  SPIClass* _spi = nullptr;
  bool  _ready = false;
  uint8_t _rot = 0;
  int   _w = 320;
  int   _h = 480;
};

#endif  // ENABLE_TOUCHUI
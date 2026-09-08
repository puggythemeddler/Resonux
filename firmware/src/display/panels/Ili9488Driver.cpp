#include "display/panels/Ili9488Driver.h"
#include <Arduino.h>

#ifdef ENABLE_TOUCHUI

static const int kDefaultSpiFreq = 40000000;  // 40 MHz

// Command bytes (ILI9488).
#define ILI_CMD_SWRESET 0x01
#define ILI_CMD_SLPOUT  0x11
#define ILI_CMD_PIXEL_FORMAT 0x3A
#define ILI_CMD_MADCTR  0x36
#define ILI_CMD_COLMOD  0x3A
#define ILI_CMD_DISPON  0x29
#define ILI_CMD_CASET   0x2A
#define ILI_CMD_PASET   0x2B
#define ILI_CMD_RAMWR   0x2C

// --- low-level ---------------------------------------------------------------
void Ili9488Driver::writeCmd(uint8_t c) {
  digitalWrite(_cfg.dcPin, LOW);
  _spi->transfer(c);
  digitalWrite(_cfg.dcPin, HIGH);
}

void Ili9488Driver::writeData(uint8_t d) {
  _spi->transfer(d);
}

bool Ili9488Driver::init() {
  if (_cfg.csPin >= 0) {
    pinMode(_cfg.csPin, OUTPUT);
    digitalWrite(_cfg.csPin, HIGH);
  }
  if (_cfg.dcPin >= 0) pinMode(_cfg.dcPin, OUTPUT);
  if (_cfg.rstPin >= 0) pinMode(_cfg.rstPin, OUTPUT);
  if (_cfg.blPin >= 0) {
    pinMode(_cfg.blPin, OUTPUT);
    digitalWrite(_cfg.blPin, LOW);
  }

  _spi = &SPI;
  _spi->begin(_cfg.spiSck, _cfg.spiMiso, _cfg.spiMosi, _cfg.csPin);

  if (_cfg.rstPin >= 0) {
    digitalWrite(_cfg.rstPin, HIGH);
    delay(10);
    digitalWrite(_cfg.rstPin, LOW);
    delay(30);
    digitalWrite(_cfg.rstPin, HIGH);
    delay(120);
  }

  _spi->beginTransaction(SPISettings(kDefaultSpiFreq, MSBFIRST, SPI_MODE0));

  writeCmd(0xE0);  // PGAMCTRL
  static const uint8_t pgam[15] = {0x00,0x03,0x09,0x08,0x16,0x0A,0x3F,0x78,
                                   0x4C,0x0C,0x08,0x16,0x1F,0x1F,0x1F};
  for (int i = 0; i < 15; ++i) writeData(pgam[i]);
  writeCmd(0xE1);  // NGAMCTRL
  static const uint8_t ngam[15] = {0x00,0x16,0x19,0x03,0x0F,0x05,0x32,0x45,
                                   0x46,0x04,0x0E,0x0D,0x35,0x37,0x0F};
  for (int i = 0; i < 15; ++i) writeData(ngam[i]);

  writeCmd(ILI_CMD_MADCTR);
  writeData(0x48);
  writeCmd(ILI_CMD_PIXEL_FORMAT);
  writeData(0x55);  // 16bpp RGB565
  writeCmd(0x36);
  writeData(0x28);  // portrait, RGB order
  writeCmd(0x3A);
  writeData(0x55);
  writeCmd(0xB0);
  writeData(0x00);
  writeCmd(0xB1);
  writeData(0xA0);
  writeCmd(0xB6);
  writeData(0x02);
  writeData(0x02);
  writeCmd(0xC0);  // PWRCTRL1
  writeData(0x19);
  writeData(0x1A);
  writeCmd(0xC1);  // PWRCTRL2
  writeData(0x45);
  writeCmd(0xC2);  // VCOMCTRL
  writeData(0x33);
  writeCmd(0xC5);  // VCOM value
  writeData(0x00);
  writeData(0x28);
  writeCmd(0x36);
  writeData(0x48);

  writeCmd(ILI_CMD_SLPOUT);
  delay(120);
  writeCmd(ILI_CMD_DISPON);
  delay(10);
  _spi->endTransaction();

  _ready = true;
  setRotation(_rot);
  fillRect(0, 0, _w, _h, 0x0000);
  return true;
}

void Ili9488Driver::setRotation(uint8_t r) {
  _rot = r & 1;
  _spi->beginTransaction(SPISettings(kDefaultSpiFreq, MSBFIRST, SPI_MODE0));
  writeCmd(ILI_CMD_MADCTR);
  if (_rot == 0) {
    writeData(0x48);  // portrait
    _w = 320; _h = 480;
  } else {
    writeData(0x28);  // landscape (swap rows/columns)
    _w = 480; _h = 320;
  }
  _spi->endTransaction();
}

void Ili9488Driver::setWindow(int x, int y, int w, int h) {
  _spi->beginTransaction(SPISettings(kDefaultSpiFreq, MSBFIRST, SPI_MODE0));
  digitalWrite(_cfg.csPin, LOW);
  writeCmd(ILI_CMD_CASET);
  writeData(x >> 8); writeData(x & 0xFF);
  writeData((x + w - 1) >> 8); writeData((x + w - 1) & 0xFF);
  writeCmd(ILI_CMD_PASET);
  writeData(y >> 8); writeData(y & 0xFF);
  writeData((y + h - 1) >> 8); writeData((y + h - 1) & 0xFF);
  writeCmd(ILI_CMD_RAMWR);
}

void Ili9488Driver::drawPixel(int x, int y, uint16_t c) {
  if (!_ready || x < 0 || y < 0 || x >= _w || y >= _h) return;
  setWindow(x, y, 1, 1);
  writeData(c >> 8);
  writeData(c & 0xFF);
  digitalWrite(_cfg.csPin, HIGH);
  _spi->endTransaction();
}

void Ili9488Driver::fillRect(int x, int y, int w, int h, uint16_t c) {
  if (!_ready || w <= 0 || h <= 0) return;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > _w) w = _w - x;
  if (y + h > _h) h = _h - y;
  if (w <= 0 || h <= 0) return;
  setWindow(x, y, w, h);
  uint32_t n = (uint32_t)w * h;
  uint8_t hi = c >> 8;
  uint8_t lo = c & 0xFF;
  for (uint32_t i = 0; i < n; ++i) {
    writeData(hi);
    writeData(lo);
  }
  digitalWrite(_cfg.csPin, HIGH);
  _spi->endTransaction();
}

void Ili9488Driver::writeArea(int x, int y, int w, int h,
                              const uint16_t* pixels) {
  if (!_ready || w <= 0 || h <= 0 || !pixels) return;
  setWindow(x, y, w, h);
  uint32_t n = (uint32_t)w * h;
  for (uint32_t i = 0; i < n; ++i) {
    writeData(pixels[i] >> 8);
    writeData(pixels[i] & 0xFF);
  }
  digitalWrite(_cfg.csPin, HIGH);
  _spi->endTransaction();
}

void Ili9488Driver::flush() {}

void Ili9488Driver::setBacklightPct(uint8_t pct) {
  if (_cfg.blPin < 0) return;
  if (pct > 100) pct = 100;
  analogWrite(_cfg.blPin, (uint16_t)pct * 255 / 100);
}

void Ili9488Driver::setPowered(bool on) {
  if (_cfg.blPin < 0) return;
  analogWrite(_cfg.blPin, 0);
}

#endif  // ENABLE_TOUCHUI
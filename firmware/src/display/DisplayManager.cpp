#include "display/DisplayManager.h"
#include <Arduino.h>

bool TouchDriver::poll(TouchPoint& out) {
  return readTouch(out, false);
}

DisplayManager& DisplayManager::instance() {
  static DisplayManager mgr;
  return mgr;
}

void DisplayManager::begin(const DisplayConfig& cfg, DisplayDriver* display,
                           TouchDriver* touch) {
  _cfg = cfg;
  _display = display;
  _touch = touch;

  if (!_cfg.enabled || !_display) {
    _enabled = false;
    return;
  }
  _enabled = true;
  _panelW = _display->width();
  _panelH = _display->height();
  _active = _display->isReady();

  if (!_cfg.logicalW || !_cfg.logicalH) { _cfg.logicalW = 320; _cfg.logicalH = 480; }
  if (_cfg.backlightPct > 100) _cfg.backlightPct = 100;
  _backlightPct = (uint8_t)_cfg.backlightPct;

  _display->setRotation((uint8_t)(_cfg.orientation == ORIENT_LANDSCAPE ? 1 : 0));
  _display->setPowered(true);

  recomputeTransform();
  _lastActivityMs = millis();
  _lastRenderMs = 0;
  _awake = true;
}

void DisplayManager::recomputeTransform() {
  int lw = _cfg.logicalW;
  int lh = _cfg.logicalH;
  _swapAxes = (_cfg.orientation == ORIENT_LANDSCAPE);
  int effW = _swapAxes ? lh : lw;
  int effH = _swapAxes ? lw : lh;

  if (!_display || _panelW <= 0 || _panelH <= 0) return;
  float sx = (float)_panelW / effW;
  float sy = (float)_panelH / effH;
  _scale = sx < sy ? sx : sy;
  _ox = (int)((_panelW - effW * _scale) / 2.0f);
  _oy = (int)((_panelH - effH * _scale) / 2.0f);
}

// Map a logical rect to a physical rect. Returns false when fully clipped.
bool DisplayManager::mapRect(int lx, int ly, int w, int h, int& px, int& py,
                             int& pw, int& ph) const {
  float s = _scale;
  if (_swapAxes) {
    // Logical surface rotated 90° onto the physical panel.
    px = _ox + (int)(ly * s);
    py = _oy + (int)((_cfg.logicalW - (lx + w)) * s);
    pw = (int)(h * s);
    ph = (int)(w * s);
  } else {
    px = _ox + (int)(lx * s);
    py = _oy + (int)(ly * s);
    pw = (int)(w * s);
    ph = (int)(h * s);
  }
  if (px >= _panelW || py >= _panelH || px + pw <= 0 || py + ph <= 0) return false;
  if (px < 0) { pw += px; px = 0; }
  if (py < 0) { ph += py; py = 0; }
  if (px + pw > _panelW) pw = _panelW - px;
  if (py + ph > _panelH) ph = _panelH - py;
  return pw > 0 && ph > 0;
}

void DisplayManager::drawPixel(int lx, int ly, uint16_t c) {
  if (!_active || !_display) return;
  int px, py, pw, ph;
  if (!mapRect(lx, ly, 1, 1, px, py, pw, ph)) return;
  _display->drawPixel(px, py, c);
}

void DisplayManager::fillRect(int lx, int ly, int w, int h, uint16_t c) {
  if (!_active || !_display || w <= 0 || h <= 0) return;
  int px, py, pw, ph;
  if (!mapRect(lx, ly, w, h, px, py, pw, ph)) return;
  _display->fillRect(px, py, pw, ph, c);
}

void DisplayManager::flush() {
  if (_active && _display) _display->flush();
}

void DisplayManager::setBacklightPct(uint8_t pct) {
  if (pct > 100) pct = 100;
  _backlightPct = pct;
  if (_active && _display) {
    _display->setBacklightPct(_awake ? _backlightPct : 0);
  }
}

void DisplayManager::wake() {
  _lastActivityMs = millis();
  if (_awake) return;
  _awake = true;
  if (_active && _display) {
    _display->setPowered(true);
    _display->setBacklightPct(_backlightPct);
  }
}

void DisplayManager::sleep() {
  if (!_awake) return;
  _awake = false;
  if (_active && _display) {
    _display->setBacklightPct(0);
    _display->setPowered(false);
  }
}

void DisplayManager::update(uint32_t nowMs) {
  if (!_enabled) return;

  int timeoutS = _cfg.screenTimeoutS;
  if (timeoutS > 0 && _awake && nowMs - _lastActivityMs > (uint32_t)timeoutS * 1000u) {
    sleep();
  }
  if (!_awake) return;

  uint32_t periodMs = 1000u / (_cfg.uiFps ? _cfg.uiFps : 30);
  if (nowMs - _lastRenderMs < periodMs) return;
  _lastRenderMs = nowMs;

  if (_drawCb && _active) {
    _drawCb(_drawCtx, *this);
    flush();
  }
}

bool DisplayManager::pollTouch(TouchPoint& out) {
  out = TouchPoint();
  if (!_enabled || !_touch) return false;
  TouchPoint raw;
  if (!_touch->poll(raw)) return false;

  if (raw.touched) {
    _lastActivityMs = millis();
    wake();
    float s = _scale;
    float tfx = ((float)raw.x - _ox) / s;
    float tfy = ((float)raw.y - _oy) / s;
    int lx, ly;
    if (_swapAxes) {
      lx = (int)_cfg.logicalW - 1 - (int)tfy;
      ly = (int)tfx;
    } else {
      lx = (int)tfx;
      ly = (int)tfy;
    }
    if (lx < 0) lx = 0;
    if (ly < 0) ly = 0;
    if (lx >= _cfg.logicalW) lx = _cfg.logicalW - 1;
    if (ly >= _cfg.logicalH) ly = _cfg.logicalH - 1;
    out.x = lx;
    out.y = ly;
    out.touched = true;
    return true;
  }
  out.x = -1;
  out.y = -1;
  out.touched = false;
  return true;
}
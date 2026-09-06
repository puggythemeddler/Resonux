#pragma once
#include "util/Rgb.h"

class LedFrame {
public:
  explicit LedFrame(int n) : _n(n) {
    _px = new Rgb[_n];
    clear();
  }
  ~LedFrame() { delete[] _px; }
  LedFrame(const LedFrame&) = delete;
  LedFrame& operator=(const LedFrame&) = delete;

  int size() const { return _n; }
  Rgb& at(int i) { return _px[i]; }
  const Rgb& at(int i) const { return _px[i]; }
  void set(int i, const Rgb& c) { _px[i] = c; }
  Rgb* data() { return _px; }

  void clear() {
    for (int i = 0; i < _n; ++i) _px[i] = Rgb{0, 0, 0};
  }

  void fill(const Rgb& c) {
    for (int i = 0; i < _n; ++i) _px[i] = c;
  }

  int index(float pos) const {
    int i = (int)(pos * _n);
    if (i >= _n) i = _n - 1;
    if (i < 0) i = 0;
    return i;
  }

private:
  int   _n;
  Rgb*  _px;
};
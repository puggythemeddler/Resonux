#pragma once

struct Smoother {
  float value = 0.0f;

  void reset(float v = 0.0f) { value = v; }

  float update(float target, float attack, float release) {
    if (target > value)
      value += (target - value) * attack;
    else
      value += (target - value) * release;
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;
    return value;
  }
};
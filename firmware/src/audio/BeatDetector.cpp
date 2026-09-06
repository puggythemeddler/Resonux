#include "audio/BeatDetector.h"

void BeatDetector::reset() {
  _energyAvg = 0.0f;
  _energyVar = 0.0f;
  _lastBeatAt = 0;
  _warm = false;
}

void BeatDetector::feed(float energy, float sensitivity, int minGapMs,
                        uint32_t nowMs, bool& beatOut, float& strengthOut) {
  beatOut = false;
  strengthOut = 0.0f;
  if (energy < 0.0f) energy = 0.0f;

  _energyAvg += (energy - _energyAvg) * 0.05f;
  float d = energy - _energyAvg;
  _energyVar += (d * d - _energyVar) * 0.05f;
  if (!_warm && _energyAvg > 1e-5f) _warm = true;

  float c = -0.0025714f * _energyVar + 1.5171429f;
  if (c < 1.05f) c = 1.05f;
  c *= sensitivity;

  float minEnergy = _energyAvg * 1.2f;
  bool gapOk = nowMs - _lastBeatAt >= (uint32_t)minGapMs;
  if (_warm && energy > _energyAvg * c && energy > minEnergy && gapOk) {
    beatOut = true;
    _lastBeatAt = nowMs;
    float st = (energy / (_energyAvg * c)) - 1.0f;
    st *= 5.0f;
    if (st < 0.0f) st = 0.0f;
    if (st > 1.0f) st = 1.0f;
    strengthOut = st;
  }
}
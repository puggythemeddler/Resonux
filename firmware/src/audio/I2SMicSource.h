#pragma once
#include "audio/AudioSource.h"

struct MicPins {
  int sck;
  int ws;
  int data;
};

class I2SMicSource : public AudioSource {
public:
  I2SMicSource(MicPins pins, float sampleRate,
               int dmaBufCount = 8, int dmaBufLen = 512);
  ~I2SMicSource();

  bool begin() override;
  int readSamples(float* out, int maxCount) override;
  float sampleRate() const override { return _sampleRate; }
  const char* name() const override { return "i2s_inmp441"; }

private:
  MicPins _pins;
  float   _sampleRate;
  int     _dmaBufCount;
  int     _dmaBufLen;
  int32_t* _raw = nullptr;
  int     _rawCount = 0;
};
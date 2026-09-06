#pragma once

class AudioSource {
public:
  virtual ~AudioSource() {}

  virtual bool begin() = 0;
  virtual int readSamples(float* out, int maxCount) = 0;
  virtual float sampleRate() const = 0;
  virtual const char* name() const = 0;
};
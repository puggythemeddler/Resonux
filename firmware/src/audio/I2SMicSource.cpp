#include "audio/I2SMicSource.h"
#include <driver/i2s.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const i2s_port_t kI2sPort = I2S_NUM_0;

I2SMicSource::I2SMicSource(MicPins pins, float sampleRate,
                           int dmaBufCount, int dmaBufLen)
    : _pins(pins), _sampleRate(sampleRate),
      _dmaBufCount(dmaBufCount), _dmaBufLen(dmaBufLen) {}

I2SMicSource::~I2SMicSource() {
  if (_raw) {
    free(_raw);
    _raw = nullptr;
  }
}

bool I2SMicSource::begin() {
  _rawCount = _dmaBufLen;
  _raw = (int32_t*)malloc(_dmaBufLen * sizeof(int32_t));
  if (!_raw) return false;

  i2s_config_t config = {};
  config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  config.sample_rate = (int)_sampleRate;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  config.dma_buf_count = _dmaBufCount;
  config.dma_buf_len = _dmaBufLen;
  config.use_apll = false;
  config.tx_desc_auto_clear = false;
  config.fixed_mclk = 0;

  if (i2s_driver_install(kI2sPort, &config, 0, NULL) != ESP_OK) return false;

  i2s_pin_config_t pin = {};
  pin.bck_io_num = _pins.sck;
  pin.ws_io_num = _pins.ws;
  pin.data_out_num = I2S_PIN_NO_CHANGE;
  pin.data_in_num = _pins.data;

  if (i2s_set_pin(kI2sPort, &pin) != ESP_OK) {
    i2s_driver_uninstall(kI2sPort);
    return false;
  }
  i2s_zero_dma_buffer(kI2sPort);
  return true;
}

int I2SMicSource::readSamples(float* out, int maxCount) {
  int produced = 0;
  while (produced < maxCount) {
    size_t bytesRead = 0;
    int wanted = _rawCount;
    if (wanted > maxCount - produced) wanted = maxCount - produced;
    if (i2s_read(kI2sPort, _raw, wanted * sizeof(int32_t), &bytesRead,
                 portMAX_DELAY) != ESP_OK) {
      return produced;
    }
    int n = (int)(bytesRead / sizeof(int32_t));
    for (int i = 0; i < n; ++i) {
      int32_t s = _raw[i] >> 8;
      out[produced++] = (float)s * (1.0f / 8388608.0f);
    }
    if (n == 0) break;
  }
  return produced;
}
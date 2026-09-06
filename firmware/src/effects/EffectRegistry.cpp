#include "effects/EffectRegistry.h"
#include "effects/fx/BassPulseEffect.h"
#include "effects/fx/BeatFlashEffect.h"
#include "effects/fx/BeatRippleEffect.h"
#include "effects/fx/ColorEnergyEffect.h"
#include "effects/fx/CustomMappingEffect.h"
#include "effects/fx/EnergyPulseEffect.h"
#include "effects/fx/FrequencyColorEffect.h"
#include "effects/fx/FrequencyWaveEffect.h"
#include "effects/fx/GradientEffect.h"
#include "effects/fx/MusicWaveEffect.h"
#include "effects/fx/RainbowMusicEffect.h"
#include "effects/fx/RunningWaveEffect.h"
#include "effects/fx/SparkEffect.h"
#include "effects/fx/SpectrumAnalyzerEffect.h"
#include "effects/fx/VuMeterEffect.h"

namespace fx {

Effect* create(int id) {
  switch (id) {
    case EFFECT_SPECTRUM: return new SpectrumAnalyzerEffect();
    case EFFECT_BASS_PULSE: return new BassPulseEffect();
    case EFFECT_BEAT_FLASH: return new BeatFlashEffect();
    case EFFECT_FREQ_WAVE: return new FrequencyWaveEffect();
    case EFFECT_FREQUENCY_COLOR: return new FrequencyColorEffect();
    case EFFECT_RAINBOW: return new RainbowMusicEffect();
    case EFFECT_VU_METER: return new VuMeterEffect();
    case EFFECT_ENERGY_PULSE: return new EnergyPulseEffect();
    case EFFECT_RUNNING_WAVE: return new RunningWaveEffect();
    case EFFECT_SPARK: return new SparkEffect();
    case EFFECT_GRADIENT: return new GradientEffect();
    case EFFECT_BEAT_RIPPLE: return new BeatRippleEffect();
    case EFFECT_MUSIC_WAVE: return new MusicWaveEffect();
    case EFFECT_COLOR_ENERGY: return new ColorEnergyEffect();
    case EFFECT_CUSTOM_MAPPING: return new CustomMappingEffect();
    default: return new SpectrumAnalyzerEffect();
  }
}

const char* nameOf(int id) {
  if (id < 0 || id >= EFFECT_COUNT) id = EFFECT_SPECTRUM;
  switch (id) {
    case EFFECT_SPECTRUM: return "Spec-trum Analyzer";
    case EFFECT_BASS_PULSE: return "Bass Pulse";
    case EFFECT_BEAT_FLASH: return "Beat Flash";
    case EFFECT_FREQ_WAVE: return "Frequency Wave";
    case EFFECT_FREQUENCY_COLOR: return "Frequency To Color";
    case EFFECT_RAINBOW: return "Rainbow Music";
    case EFFECT_VU_METER: return "VU Meter";
    case EFFECT_ENERGY_PULSE: return "Energy Pulse";
    case EFFECT_RUNNING_WAVE: return "Running Wave";
    case EFFECT_SPARK: return "High Frequency Spark";
    case EFFECT_GRADIENT: return "Bass To Treble Gradient";
    case EFFECT_BEAT_RIPPLE: return "Beat Ripple";
    case EFFECT_MUSIC_WAVE: return "Music Wave";
    case EFFECT_COLOR_ENERGY: return "Color Energy";
    case EFFECT_CUSTOM_MAPPING: return "Custom Mapping";
    default: return "Unknown";
  }
}

}  // namespace fx
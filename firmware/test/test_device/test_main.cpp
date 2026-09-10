#include <unity.h>

#include <cstdio>
#include <string.h>

#include "audio/SourceKind.h"
#include "audio/SourceSelector.h"
#include "audio/TestToneSource.h"
#include "device/DeviceProfile.h"
#include "device/DeviceRegistry.h"
#include "device/DeviceTypes.h"
#include "theme/ThemeBlend.h"

using namespace dev;

// ---------------------------------------------------------------- capabilities
static void test_capability_idents_roundtrip() {
  for (int i = 0; i < kCapabilityCount; ++i) {
    const CapabilityInfo& c = capabilityAt(i);
    TEST_ASSERT_NOT_NULL(c.id);
    TEST_ASSERT_EQUAL_STRING(c.id, capabilityIdent(c.bit));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)c.bit,
                             (uint32_t)capabilityByIdent(c.id).bit);
  }
}

static void test_capability_unknown_ident_is_none() {
  TEST_ASSERT_EQUAL_UINT32(CAP_NONE, capabilityByIdent("no-such-cap").bit);
  TEST_ASSERT_EQUAL_STRING("", capabilityIdent(CAP_NONE));
}

static void test_device_clear_resets_row() {
  DeviceInfo d;
  strcpy(d.id, "wifi:resonux-01");
  d.capabilities = CAP_NETWORK;
  d.status = STATUS_CONFIGURED;
  d.persisted = true;
  deviceClear(d);
  TEST_ASSERT_EQUAL_STRING("", d.id);
  TEST_ASSERT_EQUAL_UINT32(CAP_NONE, d.capabilities);
  TEST_ASSERT_FALSE(d.persisted);
}

// ------------------------------------------------------------------- registry
static void test_registry_add_and_find() {
  DeviceRegistry r;
  DeviceInfo d;
  strcpy(d.id, "wifi:resonux-01");
  strcpy(d.name, "Resonux");
  int idx = r.upsert(d);
  TEST_ASSERT_EQUAL(0, idx);
  TEST_ASSERT_EQUAL(1, r.count());
  const DeviceInfo* f = r.find("wifi:resonux-01");
  TEST_ASSERT_NOT_NULL(f);
  TEST_ASSERT_EQUAL_STRING("Resonux", f->name);
}

static void test_registry_upsert_dedupes() {
  DeviceRegistry r;
  DeviceInfo a;
  strcpy(a.id, "wifi:resonux-01");
  a.capabilities = CAP_NETWORK;
  int first = r.upsert(a);
  DeviceInfo b;
  strcpy(b.id, "wifi:resonux-01");
  b.capabilities = CAP_NETWORK | CAP_ARTNET;
  int second = r.upsert(b);
  TEST_ASSERT_EQUAL(first, second);  // same slot
  TEST_ASSERT_EQUAL(1, r.count());
  TEST_ASSERT_EQUAL_UINT32(CAP_NETWORK | CAP_ARTNET,
                           r.find("wifi:resonux-01")->capabilities);
}

static void test_registry_remove() {
  DeviceRegistry r;
  DeviceInfo d;
  strcpy(d.id, "usb:1acf:2001");
  r.upsert(d);
  TEST_ASSERT_TRUE(r.remove("usb:1acf:2001"));
  TEST_ASSERT_FALSE(r.remove("usb:1acf:2001"));   // already gone
  TEST_ASSERT_NULL(r.find("usb:1acf:2001"));
  TEST_ASSERT_EQUAL(0, r.count());
}

static void test_registry_capacity_limits() {
  DeviceRegistry r;
  DeviceInfo d;
  for (int i = 0; i < kMaxDevices + 3; ++i) {
    snprintf(d.id, sizeof(d.id), "dev-%02d", i);
    int idx = r.upsert(d);
    if (i < kMaxDevices) TEST_ASSERT_TRUE(idx >= 0);
    else TEST_ASSERT_TRUE(r.full());
  }
  TEST_ASSERT_EQUAL(kMaxDevices, r.count());
}

static void test_registry_clear_transient_keeps_persisted() {
  DeviceRegistry r;
  DeviceInfo trusted;
  strcpy(trusted.id, "manual:resonux-2");
  trusted.persisted = true;
  r.upsert(trusted);
  DeviceInfo transient_;
  strcpy(transient_.id, "wifi:peer-1");
  r.upsert(transient_);
  TEST_ASSERT_EQUAL(2, r.count());
  r.clearTransient();
  TEST_ASSERT_EQUAL(1, r.count());
  TEST_ASSERT_NOT_NULL(r.find("manual:resonux-2"));
  TEST_ASSERT_NULL(r.find("wifi:peer-1"));
}

// ------------------------------------------------------------------- profiles
static void test_profile_validation_rejects_empty() {
  TEST_ASSERT_FALSE(validateProfileShape("", CAP_AUDIO_INPUT));
  TEST_ASSERT_FALSE(validateProfileShape("id", CAP_NONE));
  char longId[kMaxProfileIdLen + 8];
  memset(longId, 'x', sizeof(longId));
  longId[sizeof(longId) - 1] = '\0';
  TEST_ASSERT_FALSE(validateProfileShape(longId, CAP_NETWORK));
  TEST_ASSERT_TRUE(validateProfileShape("ok", CAP_NETWORK | CAP_ARTNET));
}

static void test_profile_am006_capabilities_and_conditioning() {
  DeviceProfile p = profileById("am006");
  TEST_ASSERT_TRUE(p.id[0]);
  TEST_ASSERT_TRUE((p.capabilities & CAP_AUDIO_OUTPUT) != 0u);
  TEST_ASSERT_TRUE(p.needsConditioning);
  TEST_ASSERT_TRUE(profileHasConnection(p, CONN_SPEAKER_LEVEL));
}

static void test_profile_unknown_id_is_empty() {
  DeviceProfile p = profileById("does-not-exist");
  TEST_ASSERT_FALSE(p.id[0]);
}

static void test_profile_builtin_resonux_covers_controller() {
  DeviceProfile p = profileById("resonux");
  TEST_ASSERT_TRUE(p.id[0]);
  TEST_ASSERT_TRUE((p.capabilities & CAP_ADDRESSABLE_LED) != 0u);
  TEST_ASSERT_TRUE((p.capabilities & CAP_ARTNET) != 0u);
  TEST_ASSERT_FALSE(p.needsConditioning);
}

// ------------------------------------------------------------------ classify
static void test_classify_unknown_device_needs_investigation() {
  DeviceInfo d;
  strcpy(d.id, "radio:????");
  DeviceMatch m = classify(d);
  TEST_ASSERT_FALSE(m.known);
  TEST_ASSERT_FALSE(m.compatible);
  TEST_ASSERT_FALSE(m.safeToConnect);
  TEST_ASSERT_EQUAL((int)STATUS_DETECTED, (int)m.status);
}

static void test_classify_risky_profile_not_safe_until_confirmed() {
  DeviceInfo d;
  strcpy(d.id, "jack:am006");
  deviceClear(d);
  strcpy(d.profileId, "am006");
  DeviceMatch m = classify(d);
  TEST_ASSERT_TRUE(m.known);
  TEST_ASSERT_TRUE(m.compatible);
  TEST_ASSERT_FALSE(m.safeToConnect);
  TEST_ASSERT_EQUAL((int)STATUS_COMPATIBLE, (int)m.status);
}

static void test_classify_safe_profile_is_safe() {
  DeviceInfo d;
  strcpy(d.id, "gpio:mic0");
  strcpy(d.profileId, "inmp441_mic");
  DeviceMatch m = classify(d);
  TEST_ASSERT_TRUE(m.known);
  TEST_ASSERT_TRUE(m.safeToConnect);
  TEST_ASSERT_EQUAL((int)STATUS_SAFE_TO_CONNECT, (int)m.status);
}

static void test_classify_configured_shortcuts_to_safe() {
  DeviceInfo d;
  strcpy(d.id, "net:node-7");
  d.status = STATUS_CONFIGURED;
  DeviceMatch m = classify(d);
  TEST_ASSERT_TRUE(m.safeToConnect);
  TEST_ASSERT_EQUAL((int)STATUS_CONFIGURED, (int)m.status);
}

static void test_classify_bad_profile_id_treated_unknown() {
  DeviceInfo d;
  strcpy(d.id, "net:node-8");
  strcpy(d.profileId, "ghost-profile");
  DeviceMatch m = classify(d);
  TEST_ASSERT_TRUE(m.known);
  TEST_ASSERT_FALSE(m.compatible);
  TEST_ASSERT_EQUAL((int)STATUS_NEEDS_INVESTIGATION, (int)m.status);
}

// ------------------------------------------------------------------- selector
static void test_selector_manual_never_auto_switches() {
  SourceSelector s;
  s.configure(false, SOURCE_MIC, SOURCE_TEST, SOURCE_MIC);
  // mic unavailable but auto is OFF → no switch, just a note
  uint32_t mask = sourceMaskOf(SOURCE_TEST);
  SourceKind cur = s.update(100, mask);
  TEST_ASSERT_EQUAL((int)SOURCE_MIC, (int)cur);
  TEST_ASSERT_EQUAL((int)ChangeReason::Unavailable, (int)s.reason());
  TEST_ASSERT_EQUAL_STRING("source_unavailable", s.reasonText());
}

static void test_selector_auto_fallback_after_debounce() {
  SourceSelector s;
  s.configure(true, SOURCE_MIC, SOURCE_TEST, SOURCE_MIC);
  uint32_t mik = sourceMaskOf(SOURCE_MIC);
  uint32_t test = sourceMaskOf(SOURCE_TEST);

  SourceKind cur = s.update(0, mik);
  TEST_ASSERT_EQUAL((int)SOURCE_MIC, (int)cur);
  TEST_ASSERT_EQUAL((int)ChangeReason::None, (int)s.reason());

  // mic dies just now -> candidate armed, nothing committed (no surprises)
  cur = s.update(100, test);
  TEST_ASSERT_EQUAL((int)SOURCE_MIC, (int)cur);
  TEST_ASSERT_EQUAL((int)ChangeReason::AutoCandidate, (int)s.reason());
  TEST_ASSERT_EQUAL_STRING("auto_waiting", s.reasonText());

  // still within the debounce window -> still holding
  cur = s.update(1400, test);
  TEST_ASSERT_EQUAL((int)SOURCE_MIC, (int)cur);

  // past hold + past dwell -> commits to test tone
  cur = s.update(3000, test);
  TEST_ASSERT_EQUAL((int)SOURCE_TEST, (int)cur);
  TEST_ASSERT_EQUAL((int)ChangeReason::AutoFallback, (int)s.reason());
}

static void test_selector_auto_returns_to_preferred_with_reason() {
  SourceSelector s;
  s.configure(true, SOURCE_MIC, SOURCE_TEST, SOURCE_MIC);
  uint32_t mik = sourceMaskOf(SOURCE_MIC);
  uint32_t test = sourceMaskOf(SOURCE_TEST);

  // force a committed fallback first
  s.update(0, test);        // arm
  s.update(3000, test);     // commit -> TEST
  TEST_ASSERT_EQUAL((int)SOURCE_TEST, (int)s.current());

  // mic returns -> armed at 6000, commit after hold; dwell already elapsed?
  uint32_t cur = s.update(6000, mik | test);
  TEST_ASSERT_EQUAL((int)SOURCE_TEST, (int)cur);  // candidate waiting
  cur = s.update(8000, mik | test);               // 2000ms later -> commit
  TEST_ASSERT_EQUAL((int)SOURCE_MIC, (int)cur);
  TEST_ASSERT_EQUAL((int)ChangeReason::AutoPreferred, (int)s.reason());
}

static void test_selector_manual_switch_is_immediate() {
  SourceSelector s;
  s.configure(true, SOURCE_MIC, SOURCE_TEST, SOURCE_MIC);
  s.setManual(SOURCE_TEST, 0);
  TEST_ASSERT_EQUAL((int)SOURCE_TEST, (int)s.current());
  TEST_ASSERT_EQUAL((int)ChangeReason::Manual, (int)s.reason());
}

static void test_selector_source_names_translate() {
  TEST_ASSERT_EQUAL((int)SOURCE_MIC, (int)sourceKindFromIdent("mic"));
  TEST_ASSERT_EQUAL((int)SOURCE_TEST, (int)sourceKindFromIdent("test"));
  TEST_ASSERT_EQUAL((int)SOURCE_NONE, (int)sourceKindFromIdent("bogus"));
  TEST_ASSERT_TRUE(sourceIsLocal(SOURCE_MIC));
  TEST_ASSERT_TRUE(sourceIsLocal(SOURCE_TEST));
  TEST_ASSERT_FALSE(sourceIsLocal(SOURCE_USB_AUDIO));
}

// ------------------------------------------------------------------ test tone
static void test_tone_sine_bounded_and_nonzero() {
  TestToneSource tone(16000.0f);
  TEST_ASSERT_TRUE(tone.begin());
  TEST_ASSERT_EQUAL_FLOAT(16000.0f, tone.sampleRate());
  TEST_ASSERT_EQUAL_STRING("test_tone", tone.name());
  float buf[2048];
  int n = tone.readSamples(buf, 2048);
  TEST_ASSERT_EQUAL(2048, n);
  float peak = 0.0f;
  for (int i = 0; i < n; ++i) {
    TEST_ASSERT_TRUE(buf[i] >= -1.01f && buf[i] <= 1.01f);
    float a = buf[i] < 0 ? -buf[i] : buf[i];
    if (a > peak) peak = a;
  }
  TEST_ASSERT_TRUE(peak > 0.2f);  // genuinely audible signal
  TEST_ASSERT_TRUE(peak <= 1.0f); // never clipping
}

// -------------------------------------------------------------------- blend
static void test_blend_endpoints() {
  Themes::ThemeFrame from;
  Themes::ThemeFrame to;
  from.brightness = 0.2f;
  to.brightness = 0.8f;
  Themes::ThemeFrame at0 = Themes::blendThemeFrame(from, to, 0.0f);
  Themes::ThemeFrame at1 = Themes::blendThemeFrame(from, to, 1.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.2f, at0.brightness);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.8f, at1.brightness);
}

static void test_blend_midpoint_scalar_and_colour() {
  Themes::ThemeFrame from;
  Themes::ThemeFrame to;
  from.brightness = 0.0f;
  to.brightness = 1.0f;
  from.primary = Rgb{0, 0, 0};
  to.primary = Rgb{100, 40, 60};
  from.saturation = 0.2f;
  to.saturation = 0.8f;
  from.themeId = "from";
  to.themeId = "to";

  Themes::ThemeFrame mid = Themes::blendThemeFrame(from, to, 0.5f);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, mid.brightness);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, mid.saturation);
  TEST_ASSERT_EQUAL_UINT8(50, mid.primary.r);
  TEST_ASSERT_EQUAL_UINT8(20, mid.primary.g);
  TEST_ASSERT_EQUAL_UINT8(30, mid.primary.b);
  TEST_ASSERT_EQUAL_STRING("to", mid.themeId);  // carries target identity
}

static void test_blend_preferred_effect_flips_at_midpoint() {
  Themes::ThemeFrame from;
  Themes::ThemeFrame to;
  from.preferredEffect = 0;
  to.preferredEffect = 5;
  Themes::ThemeFrame a = Themes::blendThemeFrame(from, to, 0.4f);
  Themes::ThemeFrame b = Themes::blendThemeFrame(from, to, 0.6f);
  TEST_ASSERT_EQUAL_INT(0, a.preferredEffect);
  TEST_ASSERT_EQUAL_INT(5, b.preferredEffect);
}

// --------------------------------------------------------------------- main
int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_capability_idents_roundtrip);
  RUN_TEST(test_capability_unknown_ident_is_none);
  RUN_TEST(test_device_clear_resets_row);
  RUN_TEST(test_registry_add_and_find);
  RUN_TEST(test_registry_upsert_dedupes);
  RUN_TEST(test_registry_remove);
  RUN_TEST(test_registry_capacity_limits);
  RUN_TEST(test_registry_clear_transient_keeps_persisted);
  RUN_TEST(test_profile_validation_rejects_empty);
  RUN_TEST(test_profile_am006_capabilities_and_conditioning);
  RUN_TEST(test_profile_unknown_id_is_empty);
  RUN_TEST(test_profile_builtin_resonux_covers_controller);
  RUN_TEST(test_classify_unknown_device_needs_investigation);
  RUN_TEST(test_classify_risky_profile_not_safe_until_confirmed);
  RUN_TEST(test_classify_safe_profile_is_safe);
  RUN_TEST(test_classify_configured_shortcuts_to_safe);
  RUN_TEST(test_classify_bad_profile_id_treated_unknown);
  RUN_TEST(test_selector_manual_never_auto_switches);
  RUN_TEST(test_selector_auto_fallback_after_debounce);
  RUN_TEST(test_selector_auto_returns_to_preferred_with_reason);
  RUN_TEST(test_selector_manual_switch_is_immediate);
  RUN_TEST(test_selector_source_names_translate);
  RUN_TEST(test_tone_sine_bounded_and_nonzero);
  RUN_TEST(test_blend_endpoints);
  RUN_TEST(test_blend_midpoint_scalar_and_colour);
  RUN_TEST(test_blend_preferred_effect_flips_at_midpoint);
  return UNITY_END();
}
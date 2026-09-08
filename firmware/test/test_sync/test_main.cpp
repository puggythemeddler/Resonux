#include <unity.h>

#include "audio/AudioFrame.h"
#include "sync/SyncClock.h"
#include "sync/SyncProtocol.h"
#include <string.h>

// ---------------------------------------------------------------- SyncClock
void test_clock_zero_latency() {
  SyncClock c;
  c.observe(1000u, 1000u);
  TEST_ASSERT_TRUE(c.have());
  TEST_ASSERT_EQUAL_INT32(0, c.offsetMs());
  TEST_ASSERT_EQUAL_UINT32(5000u, c.toLocal(5000u));
}

void test_clock_estimates_offset_from_latency() {
  // master time 1000, local arrival 1500 -> latency 500 -> offset ~250
  SyncClock c;
  for (int i = 0; i < 16; ++i) c.observe(1500u + i, 1000u + i);
  // offset should converge very close to 250
  TEST_ASSERT_INT32_WITHIN(80, 250, c.offsetMs());
  // toLocal: master time "1000" landed midway between the far timestamps
  TEST_ASSERT_UINT32_WITHIN(160, 1250u, c.toLocal(1000u));
}

void test_clock_first_observation_seeds_directly() {
  SyncClock c;
  c.observe(0x10000000u, 0x0F000000u);  // large, divergent uptimes
  TEST_ASSERT_EQUAL_INT32((int32_t)(0x01000000u / 2), c.offsetMs());
}

void test_clock_wrap_safe() {
  SyncClock c;
  // millis() wrapped: local=0xFFFFFFFE, master=0xFFFFFE00 -> latency 510
  c.observe(0xFFFFFFFEu, 0xFFFFFE00u);
  TEST_ASSERT_EQUAL_INT32(255, c.offsetMs());
  TEST_ASSERT_EQUAL_UINT32(0xFFFFFEFFu, c.toLocal(0xFFFFFE00u));
}

void test_clock_ahead_master_negative_offset() {
  SyncClock c;
  // slave clock behind? master packet arrived 200ms after being sent locally.
  // local=800, remote=1000 (master is ahead) -> latency -200 -> offset -100
  c.observe(800u, 1000u);
  TEST_ASSERT_EQUAL_INT32(-100, c.offsetMs());
  TEST_ASSERT_EQUAL_UINT32((uint32_t)(int32_t)900, c.toLocal(1000u));
}

// -------------------------------------------------------------- SyncProtocol
void test_pack_unpack_roundtrip() {
  AudioFrame f;
  f.timeMs = 123456u;
  f.bandCount = 9;
  f.beat = true;
  f.amplitude = 0.7f;
  f.bass = 0.9f;
  f.lowMid = 0.5f;
  f.mid = 0.4f;
  f.highMid = 0.3f;
  f.treble = 0.2f;
  f.beatStrength = 3.5f;
  for (int i = 0; i < 9; ++i) {
    f.bands[i] = i * 0.1f;
    f.peaks[i] = i * 0.2f;
  }
  syncpkt::Packet p;
  syncpkt::packPacket(p, f, 42u);
  TEST_ASSERT_TRUE(syncpkt::validPacket(p, sizeof(p)));
  TEST_ASSERT_EQUAL_UINT32(42u, p.seq);
  TEST_ASSERT_EQUAL_UINT32(123456u, p.masterTime);

  AudioFrame g;
  syncpkt::unpackPacket(g, p);
  TEST_ASSERT_EQUAL_UINT32(123456u, g.timeMs);  // offset applied by caller
  TEST_ASSERT_EQUAL_UINT8(9, g.bandCount);
  TEST_ASSERT_TRUE(g.beat);
  TEST_ASSERT_EQUAL_FLOAT(g.amplitude, f.amplitude);
  TEST_ASSERT_EQUAL_FLOAT(g.bass, f.bass);
  TEST_ASSERT_EQUAL_FLOAT(g.beatStrength, f.beatStrength);
  for (int i = 0; i < 9; ++i) {
    TEST_ASSERT_EQUAL_FLOAT(f.bands[i], g.bands[i]);
    TEST_ASSERT_EQUAL_FLOAT(f.peaks[i], g.peaks[i]);
  }
}

void test_packet_rejects_garbage() {
  syncpkt::Packet p;
  memset(&p, 0, sizeof(p));
  size_t bytes = sizeof(p);
  TEST_ASSERT_FALSE(syncpkt::validPacket(p, bytes));  // bad magic
  p.magic = syncpkt::kSyncMagic;
  TEST_ASSERT_FALSE(syncpkt::validPacket(p, bytes));  // bad version
  p.version = syncpkt::kSyncVersion;
  TEST_ASSERT_TRUE(syncpkt::validPacket(p, bytes));
  p.bandCount = 200;  // absurd
  TEST_ASSERT_FALSE(syncpkt::validPacket(p, bytes));
}

void test_packet_rejects_truncated_datagram() {
  syncpkt::Packet p;
  syncpkt::packPacket(p, AudioFrame(), 1u);
  TEST_ASSERT_FALSE(syncpkt::validPacket(p, sizeof(p) - 1));
  TEST_ASSERT_TRUE(syncpkt::validPacket(p, sizeof(p)));
}

// --------------------------------------------------------------------- main
int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_clock_zero_latency);
  RUN_TEST(test_clock_estimates_offset_from_latency);
  RUN_TEST(test_clock_first_observation_seeds_directly);
  RUN_TEST(test_clock_wrap_safe);
  RUN_TEST(test_clock_ahead_master_negative_offset);
  RUN_TEST(test_pack_unpack_roundtrip);
  RUN_TEST(test_packet_rejects_garbage);
  RUN_TEST(test_packet_rejects_truncated_datagram);
  return UNITY_END();
}
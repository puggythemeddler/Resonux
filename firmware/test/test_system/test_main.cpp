#include "system/SystemMode.h"
#include <unity.h>
#include <string.h>

using namespace sys;

void setUp(void) {}
void tearDown(void) {}

static void test_initial_state_running() {
  SystemMode m;
  TEST_ASSERT_EQUAL((int)State::Running, (int)m.state());
  TEST_ASSERT_EQUAL((int)Action::None, (int)m.pending());
  TEST_ASSERT_FALSE(m.shuttingDown());
}

static void test_restart_transition() {
  SystemMode m;
  TEST_ASSERT_TRUE(m.request(Action::Restart));
  TEST_ASSERT_EQUAL((int)State::RestartRequested, (int)m.state());
  TEST_ASSERT_EQUAL((int)Action::Restart, (int)m.pending());
  TEST_ASSERT_TRUE(m.shuttingDown());

  m.beginShutdown();
  TEST_ASSERT_EQUAL((int)State::ShuttingDown, (int)m.state());

  m.complete();
  TEST_ASSERT_EQUAL((int)State::Restarting, (int)m.state());
}

static void test_power_off_transition() {
  SystemMode m;
  TEST_ASSERT_TRUE(m.request(Action::PowerOff));
  TEST_ASSERT_EQUAL((int)State::PowerOffRequested, (int)m.state());
  TEST_ASSERT_EQUAL((int)Action::PowerOff, (int)m.pending());

  m.beginShutdown();
  m.complete();
  TEST_ASSERT_EQUAL((int)State::Sleeping, (int)m.state());
}

static void test_second_request_rejected_while_shutting_down() {
  SystemMode m;
  TEST_ASSERT_TRUE(m.request(Action::Restart));
  TEST_ASSERT_FALSE(m.request(Action::PowerOff));  // already committed
  TEST_ASSERT_EQUAL((int)Action::Restart, (int)m.pending());
  m.beginShutdown();
  TEST_ASSERT_FALSE(m.request(Action::Restart));  // mid-shutdown
  TEST_ASSERT_EQUAL((int)State::ShuttingDown, (int)m.state());
}

static void test_none_request_rejected() {
  SystemMode m;
  TEST_ASSERT_FALSE(m.request(Action::None));
  TEST_ASSERT_EQUAL((int)State::Running, (int)m.state());
}

static void test_state_names() {
  SystemMode m;
  TEST_ASSERT_EQUAL_STRING("running", m.stateName());
  TEST_ASSERT_TRUE(m.request(Action::Restart));
  TEST_ASSERT_EQUAL_STRING("restart_requested", m.stateName());
  m.beginShutdown();
  TEST_ASSERT_EQUAL_STRING("shutting_down", m.stateName());
  m.complete();
  TEST_ASSERT_EQUAL_STRING("restarting", m.stateName());
  TEST_ASSERT_EQUAL_STRING("restart", m.actionName());
}

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_initial_state_running);
  RUN_TEST(test_restart_transition);
  RUN_TEST(test_power_off_transition);
  RUN_TEST(test_second_request_rejected_while_shutting_down);
  RUN_TEST(test_none_request_rejected);
  RUN_TEST(test_state_names);
  return UNITY_END();
}
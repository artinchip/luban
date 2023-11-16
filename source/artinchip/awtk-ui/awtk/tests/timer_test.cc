﻿#include <string>
#include "tkc/utils.h"
#include "base/timer.h"
#include "gtest/gtest.h"

using std::string;

static string s_log;
static uint64_t s_now = 0;
static uint64_t timer_get_time() {
  return s_now;
}

static void timer_set_time(uint32_t now) {
  s_now = now;
}

static void timer_clear_log(void) {
  s_log = "";
}

static ret_t timer_once(const timer_info_t* timer) {
  s_log += "o:";
  return RET_OK;
}

static ret_t timer_repeat(const timer_info_t* timer) {
  s_log += "r:";
  return RET_REPEAT;
}

static ret_t timer_remove_in_timer(const timer_info_t* timer) {
  s_log += "rm:";
  uint32_t id = (char*)(timer->ctx) - (char*)NULL;

  timer_manager_remove(timer->timer_manager, id);

  return RET_REPEAT;
}

static ret_t timer_add_in_timer(const timer_info_t* timer) {
  s_log += "a:";

  timer_manager_add(timer->timer_manager, timer_repeat, NULL, 0);
  timer_manager_add(timer->timer_manager, timer_repeat, NULL, 100);

  return RET_REPEAT;
}

static string repeat_str(const string& substr, uint32_t nr) {
  string str;
  while (nr > 0) {
    str += substr;
    nr--;
  }

  return str;
}

#define NR 100u

TEST(Timer, once) {
  uint32_t i = 0;
  uint32_t ids[NR];
  timer_set_time(0);
  timer_manager_t* tm = timer_manager_create(timer_get_time);

  for (i = 0; i < NR; i++) {
    ids[i] = timer_manager_add(tm, timer_once, NULL, i + 1);
    ASSERT_EQ(timer_manager_next_time(tm), 1u);
    ASSERT_EQ(ids[i] > 0, true);
    ASSERT_EQ(timer_manager_find(tm, ids[i])->id, ids[i]);
    ASSERT_EQ(timer_manager_count(tm), i + 1u);
  }

  timer_clear_log();
  ASSERT_EQ(timer_manager_dispatch(tm), RET_OK);
  ASSERT_EQ(timer_manager_count(tm), NR);
  ASSERT_EQ(s_log, "");

  timer_set_time(100);
  ASSERT_EQ(timer_manager_dispatch(tm), RET_OK);
  ASSERT_EQ(timer_manager_count(tm), 0u);
  ASSERT_EQ(s_log, repeat_str("o:", NR));

  timer_manager_destroy(tm);
}

TEST(Timer, reset) {
  timer_manager_t* tm = timer_manager_create(timer_get_time);
  uint32_t id = timer_manager_add(tm, timer_once, NULL, 100);

  timer_set_time(100);
  timer_reset(id);

  timer_clear_log();
  ASSERT_EQ(timer_manager_dispatch(tm), RET_OK);
  ASSERT_EQ(timer_manager_count(tm), 1u);
  ASSERT_EQ(s_log, "");

  timer_set_time(200);
  ASSERT_EQ(timer_manager_dispatch(tm), RET_OK);
  ASSERT_EQ(timer_manager_count(tm), 0u);
  ASSERT_EQ(s_log, "o:");

  timer_manager_destroy(tm);
}

TEST(Timer, suspend_and_resume) {
  uint32_t id = 0;
  timer_manager_t* s_timer_manager = NULL;
  timer_manager_t* tm = timer_manager_create(timer_get_time);
  timer_set_time(0);

  s_timer_manager = timer_manager();
  timer_manager_set(tm);
  id = timer_manager_add(tm, timer_repeat, NULL, 100);

  timer_set_time(100);

  timer_clear_log();
  ASSERT_EQ(timer_manager_dispatch(tm), RET_OK);
  ASSERT_EQ(timer_manager_count(tm), 1u);
  ASSERT_EQ(s_log, "r:");

  timer_set_time(200);
  ASSERT_EQ(timer_suspend(id), RET_OK);
  ASSERT_EQ(timer_manager_dispatch(tm), RET_OK);
  ASSERT_EQ(timer_manager_count(tm), 1u);
  ASSERT_EQ(s_log, "r:");

  timer_set_time(300);
  ASSERT_EQ(timer_manager_dispatch(tm), RET_OK);
  ASSERT_EQ(timer_manager_count(tm), 1u);
  ASSERT_EQ(s_log, "r:");
  ASSERT_EQ(timer_resume(id), RET_OK);

  timer_set_time(400);
  ASSERT_EQ(timer_manager_dispatch(tm), RET_OK);
  ASSERT_EQ(timer_manager_count(tm), 1u);
  ASSERT_EQ(s_log, "r:r:");

  timer_manager_set(s_timer_manager);
  timer_manager_destroy(tm);
}

TEST(Timer, modify) {
  uint32_t id = 0;
  timer_manager_t* tm = timer_manager_create(timer_get_time);
  timer_set_time(200);
  id = timer_manager_add(tm, timer_once, NULL, 100);

  timer_set_time(200);
  timer_modify(id, 200);

  timer_clear_log();
  ASSERT_EQ(timer_manager_dispatch(tm), RET_OK);
  ASSERT_EQ(timer_manager_count(tm), 1u);
  ASSERT_EQ(s_log, "");

  timer_set_time(400);
  ASSERT_EQ(timer_manager_dispatch(tm), RET_OK);
  ASSERT_EQ(timer_manager_count(tm), 0u);
  ASSERT_EQ(s_log, "o:");

  timer_manager_destroy(tm);
}

TEST(Timer, repeat) {
  uint32_t i = 0;
  uint32_t ids[NR];
  timer_set_time(0);
  timer_manager_t* tm = timer_manager_create(timer_get_time);

  for (i = 0; i < NR; i++) {
    ids[i] = timer_manager_add(tm, timer_repeat, NULL, i + 1);
    ASSERT_EQ(timer_manager_next_time(tm), 1u);
    ASSERT_EQ(timer_manager_find(tm, ids[i])->id, ids[i]);
    ASSERT_EQ(ids[i] > 0, true);
    ASSERT_EQ(timer_manager_count(tm), i + 1u);
  }

  timer_clear_log();
  ASSERT_EQ(timer_manager_dispatch(tm), RET_OK);
  ASSERT_EQ(timer_manager_count(tm), NR);
  ASSERT_EQ(s_log, "");

  timer_clear_log();
  timer_set_time(100);
  ASSERT_EQ(timer_manager_dispatch(tm), RET_OK);
  ASSERT_EQ(timer_manager_count(tm), NR);
  ASSERT_EQ(s_log, repeat_str("r:", NR));

  for (i = 0; i < NR; i++) {
    uint32_t id = ids[i];
    ASSERT_EQ(timer_manager_remove(tm, id), RET_OK);
    ASSERT_EQ(timer_manager_count(tm), NR - i - 1u);
  }
  ASSERT_EQ(timer_manager_count(tm), 0u);

  timer_manager_destroy(tm);
}

TEST(Timer, removeInTimer) {
  timer_set_time(0);
  timer_manager_t* tm = timer_manager_create(timer_get_time);

  uint32_t id1 = timer_manager_add(tm, timer_repeat, NULL, 200);
  timer_manager_add(tm, timer_remove_in_timer, (char*)NULL + id1, 100);

  timer_clear_log();
  timer_set_time(100);
  ASSERT_EQ(timer_manager_dispatch(tm), RET_OK);
  ASSERT_EQ(timer_manager_count(tm), 1u);
  ASSERT_EQ(s_log, "rm:");

  timer_manager_destroy(tm);
}

TEST(Timer, addInTimer) {
  timer_set_time(0);
  timer_manager_t* tm = timer_manager_create(timer_get_time);

  timer_manager_add(tm, timer_add_in_timer, NULL, 100);
  ASSERT_EQ(timer_manager_next_time(tm), 100u);

  timer_clear_log();
  timer_set_time(100);
  ASSERT_EQ(timer_manager_dispatch(tm), RET_OK);
  ASSERT_EQ(timer_manager_count(tm), 3u);
  ASSERT_EQ(s_log, "a:r:");

  timer_manager_destroy(tm);
}

TEST(Timer, id) {
  uint32_t id1 = 0;
  uint32_t id2 = 0;
  timer_set_time(0);
  timer_manager_t* tm = timer_manager_create(timer_get_time);

  id1 = timer_manager_add(tm, timer_add_in_timer, NULL, 100);
  tm->next_timer_id = id1;
  id2 = timer_manager_add(tm, timer_add_in_timer, NULL, 100);
  ASSERT_NE(id1, id2);

  timer_manager_destroy(tm);
}

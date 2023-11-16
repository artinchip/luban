﻿#include "tkc/named_value.h"
#include "gtest/gtest.h"

#include <string>

using std::string;

TEST(NamedValue, basic) {
  named_value_t named_value;
  named_value_t* nv = &named_value;

  named_value_init(nv, NULL, NULL);

  ASSERT_EQ(nv->name, (char*)NULL);
  ASSERT_EQ(nv->value.type, VALUE_TYPE_INVALID);

  named_value_deinit(nv);
}

TEST(NamedValue, init) {
  value_t v;
  named_value_t named_value;
  named_value_t* nv = &named_value;

  value_set_int(&v, 123);
  named_value_init(nv, "name", &v);

  ASSERT_EQ(string(nv->name), string("name"));
  ASSERT_EQ(value_int(&v), value_int(&(nv->value)));

  named_value_deinit(nv);

  ASSERT_EQ(nv->name, (char*)NULL);
  ASSERT_EQ(nv->value.type, VALUE_TYPE_INVALID);
}

TEST(NamedValue, create_ex) {
  value_t v;
  named_value_t* nv = named_value_create_ex("name", value_set_str(&v, "jim"));

  ASSERT_STREQ(nv->name, "name");
  ASSERT_STREQ(value_str(&(nv->value)), "jim");

  named_value_destroy(nv);
}

TEST(NamedValue, create_ex1) {
  value_t v;
  named_value_t* nv = named_value_create_ex("name", value_set_str(&v, "jim"));

  ASSERT_STREQ(nv->name, "name");
  ASSERT_STREQ(value_str(&(nv->value)), "jim");
  ASSERT_EQ(named_value_set_name(nv, "tom"), RET_OK);
  ASSERT_EQ(named_value_set_name(nv, "jike"), RET_OK);

  named_value_destroy(nv);
}

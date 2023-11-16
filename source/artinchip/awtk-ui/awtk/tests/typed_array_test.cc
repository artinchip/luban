﻿#include "tkc/typed_array.h"
#include "tkc/object_typed_array.h"
#include "gtest/gtest.h"

TEST(TypedArray, basic) {
  value_t v;
  typed_array_t* a = typed_array_create(VALUE_TYPE_UINT8, 0);
  ASSERT_EQ(a != NULL, true);

  ASSERT_EQ(typed_array_insert(a, 0, value_set_uint8(&v, 0)), RET_OK);
  ASSERT_EQ(typed_array_insert(a, 1, value_set_uint8(&v, 1)), RET_OK);
  ASSERT_EQ(typed_array_insert(a, 2, value_set_uint8(&v, 2)), RET_OK);
  ASSERT_EQ(a->size, 3u);

  ASSERT_EQ(typed_array_get(a, 0, &v), RET_OK);
  ASSERT_EQ(value_uint8(&v), 0);
  ASSERT_EQ(typed_array_get(a, 1, &v), RET_OK);
  ASSERT_EQ(value_uint8(&v), 1);
  ASSERT_EQ(typed_array_get(a, 2, &v), RET_OK);
  ASSERT_EQ(value_uint8(&v), 2);

  ASSERT_EQ(typed_array_set(a, 0, value_set_uint8(&v, 100)), RET_OK);
  ASSERT_EQ(typed_array_get(a, 0, &v), RET_OK);
  ASSERT_EQ(value_uint8(&v), 100);

  ASSERT_EQ(typed_array_set(a, 1, value_set_uint8(&v, 200)), RET_OK);
  ASSERT_EQ(typed_array_get(a, 1, &v), RET_OK);
  ASSERT_EQ(value_uint8(&v), 200);

  ASSERT_EQ(typed_array_set(a, 2, value_set_uint8(&v, 230)), RET_OK);
  ASSERT_EQ(typed_array_get(a, 2, &v), RET_OK);
  ASSERT_EQ(value_uint8(&v), 230);

  ASSERT_EQ(typed_array_remove(a, 1), RET_OK);
  ASSERT_EQ(typed_array_get(a, 1, &v), RET_OK);
  ASSERT_EQ(value_uint8(&v), 230);
  ASSERT_EQ(a->size, 2u);

  ASSERT_EQ(typed_array_remove(a, 0), RET_OK);
  ASSERT_EQ(typed_array_get(a, 0, &v), RET_OK);
  ASSERT_EQ(value_uint8(&v), 230);
  ASSERT_EQ(a->size, 1u);

  ASSERT_EQ(typed_array_remove(a, 0), RET_OK);
  ASSERT_EQ(a->size, 0u);
  ASSERT_EQ(typed_array_remove(a, 0), RET_BAD_PARAMS);
  ASSERT_EQ(a->size, 0u);

  ASSERT_EQ(typed_array_tail(a, &v), RET_BAD_PARAMS);

  typed_array_destroy(a);
}

TEST(TypedArray, push) {
  value_t v;
  uint8_t i = 0;
  typed_array_t* a = typed_array_create(VALUE_TYPE_UINT8, 0);
  ASSERT_EQ(a != NULL, true);
  for (i = 0; i < 255; i++) {
    ASSERT_EQ(a->size, (uint32_t)i);
    ASSERT_EQ(typed_array_push(a, value_set_uint8(&v, i)), RET_OK);
    ASSERT_EQ(typed_array_tail(a, &v), RET_OK);
    ASSERT_EQ(value_uint8(&v), i);
  }

  for (; i > 0; i--) {
    ASSERT_EQ(a->size, (uint32_t)i);
    ASSERT_EQ(typed_array_tail(a, &v), RET_OK);
    ASSERT_EQ(value_uint8(&v), i - 1);
    ASSERT_EQ(typed_array_pop(a, &v), RET_OK);
    ASSERT_EQ(value_uint8(&v), i - 1);
  }
  typed_array_destroy(a);
}

TEST(TypedArray, insert) {
  value_t v;
  uint8_t i = 0;
  typed_array_t* a = typed_array_create(VALUE_TYPE_UINT8, 0);
  ASSERT_EQ(a != NULL, true);
  for (i = 0; i < 255; i++) {
    ASSERT_EQ(a->size, (uint32_t)i);
    ASSERT_EQ(typed_array_insert(a, 0, value_set_uint8(&v, i)), RET_OK);
    ASSERT_EQ(typed_array_get(a, 0, &v), RET_OK);
    ASSERT_EQ(value_uint8(&v), i);
  }

  ASSERT_EQ(typed_array_clear(a), RET_OK);
  ASSERT_EQ(a->size, 0u);

  typed_array_destroy(a);
}

TEST(TypedArray, pointer) {
  typed_array_t* a = typed_array_create(VALUE_TYPE_POINTER, 0);
  ASSERT_EQ(a == NULL, true);
}

TEST(TypedArray, insert_uint64) {
  value_t v;
  uint8_t i = 0;
  typed_array_t* a = typed_array_create(VALUE_TYPE_UINT64, 0);
  ASSERT_EQ(a != NULL, true);
  ASSERT_EQ(a->element_size, sizeof(uint64_t));
  for (i = 0; i < 255; i++) {
    ASSERT_EQ(a->size, (uint32_t)i);
    ASSERT_EQ(typed_array_insert(a, 0, value_set_uint64(&v, i)), RET_OK);
    ASSERT_EQ(typed_array_get(a, 0, &v), RET_OK);
    ASSERT_EQ(value_uint64(&v), (uint64_t)i);
  }

  ASSERT_EQ(typed_array_set(a, 10, value_set_uint64(&v, 0x1122334455667788)), RET_OK);
  ASSERT_EQ(typed_array_get(a, 10, &v), RET_OK);
  ASSERT_EQ(value_uint64(&v), 0x1122334455667788u);
  ASSERT_EQ(typed_array_clear(a), RET_OK);
  ASSERT_EQ(a->size, 0u);

  typed_array_destroy(a);
}

TEST(TypedArray, insert_double) {
  value_t v;
  uint8_t i = 0;
  typed_array_t* a = typed_array_create(VALUE_TYPE_DOUBLE, 10);
  ASSERT_EQ(a != NULL, true);
  ASSERT_EQ(a->element_size, sizeof(double_t));
  for (i = 0; i < 255; i++) {
    ASSERT_EQ(a->size, (uint32_t)i);
    ASSERT_EQ(typed_array_insert(a, 0, value_set_double(&v, i)), RET_OK);
    ASSERT_EQ(typed_array_get(a, 0, &v), RET_OK);
    ASSERT_EQ(value_double(&v), (double)i);
  }

  ASSERT_EQ(typed_array_set(a, 10, value_set_double(&v, 0x112233)), RET_OK);
  ASSERT_EQ(typed_array_get(a, 10, &v), RET_OK);
  ASSERT_EQ(value_double(&v), (double)0x112233);
  ASSERT_EQ(typed_array_clear(a), RET_OK);
  ASSERT_EQ(a->size, 0u);

  typed_array_destroy(a);
}

TEST(TypedArray, insert_uint32) {
  value_t v;
  uint8_t i = 0;
  typed_array_t* a = typed_array_create(VALUE_TYPE_UINT32, 0);
  ASSERT_EQ(a != NULL, true);
  ASSERT_EQ(a->element_size, sizeof(uint32_t));
  for (i = 0; i < 255; i++) {
    ASSERT_EQ(a->size, (uint32_t)i);
    ASSERT_EQ(typed_array_insert(a, 0, value_set_uint32(&v, i)), RET_OK);
    ASSERT_EQ(typed_array_get(a, 0, &v), RET_OK);
    ASSERT_EQ(value_uint32(&v), (uint32_t)i);
  }

  ASSERT_EQ(typed_array_set(a, 10, value_set_uint32(&v, 0x11223344)), RET_OK);
  ASSERT_EQ(typed_array_get(a, 10, &v), RET_OK);
  ASSERT_EQ(value_uint32(&v), 0x11223344u);
  ASSERT_EQ(typed_array_clear(a), RET_OK);
  ASSERT_EQ(a->size, 0u);

  typed_array_destroy(a);
}

TEST(TypedArray, object1) {
  value_t v;
  uint8_t i = 0;
  tk_object_t* obj = object_typed_array_create(VALUE_TYPE_UINT32, 0);
  typed_array_t* a = OBJECT_TYPED_ARRAY(obj)->arr;

  ASSERT_EQ(a != NULL, true);
  ASSERT_EQ(a->element_size, sizeof(uint32_t));
  for (i = 0; i < 255; i++) {
    ASSERT_EQ(a->size, (uint32_t)i);
    ASSERT_EQ(typed_array_insert(a, 0, value_set_uint32(&v, i)), RET_OK);
    ASSERT_EQ(typed_array_get(a, 0, &v), RET_OK);
    ASSERT_EQ(value_uint32(&v), (uint32_t)i);
  }

  ASSERT_EQ(tk_object_get_prop_int(obj, "size", 0), (int32_t)i);
  ASSERT_EQ(tk_object_get_prop_int(obj, "bytes", 0), (int32_t)(i * sizeof(uint32_t)));
  ASSERT_EQ(tk_object_get_prop_pointer(obj, "data"), (void*)a->data);

  TK_OBJECT_UNREF(obj);
}

TEST(TypedArray, object2) {
  value_t v;
  uint8_t i = 0;
  tk_object_t* obj = object_typed_array_create(VALUE_TYPE_UINT32, 0);
  typed_array_t* a = OBJECT_TYPED_ARRAY(obj)->arr;

  ASSERT_EQ(a != NULL, true);
  ASSERT_EQ(a->element_size, sizeof(uint32_t));
  for (i = 0; i < 255; i++) {
    ASSERT_EQ(a->size, (uint32_t)i);
    ASSERT_EQ(typed_array_insert(a, 0, value_set_uint32(&v, i)), RET_OK);
    ASSERT_EQ(typed_array_get(a, 0, &v), RET_OK);
    ASSERT_EQ(value_uint32(&v), (uint32_t)i);
  }

  ASSERT_EQ(tk_object_get_prop_int(obj, TK_OBJECT_PROP_SIZE, 0), (int32_t)i);
  ASSERT_EQ(tk_object_set_prop_int(obj, "[0]", 123), RET_OK);
  ASSERT_EQ(tk_object_get_prop_int(obj, "[0]", 0), 123);
  ASSERT_EQ(tk_object_set_prop_int(obj, "[1]", 1234), RET_OK);
  ASSERT_EQ(tk_object_get_prop_int(obj, "[1]", 0), 1234);

  TK_OBJECT_UNREF(obj);
}

TEST(TypedArray, insert_bool) {
  value_t v;
  uint8_t i = 0;
  typed_array_t* a = typed_array_create(VALUE_TYPE_BOOL, 0);
  ASSERT_EQ(a != NULL, true);
  ASSERT_EQ(a->element_size, sizeof(uint8_t));
  for (i = 0; i < 255; i++) {
    ASSERT_EQ(a->size, (uint32_t)i);
    ASSERT_EQ(typed_array_insert(a, 0, value_set_bool(&v, i % 2 == 1)), RET_OK);
    ASSERT_EQ(typed_array_get(a, 0, &v), RET_OK);
    ASSERT_EQ(value_bool(&v), i % 2 == 1);
  }

  ASSERT_EQ(typed_array_clear(a), RET_OK);
  ASSERT_EQ(a->size, 0u);

  typed_array_destroy(a);
}

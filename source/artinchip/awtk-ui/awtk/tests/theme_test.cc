﻿
#include "base/enums.h"
#include "base/theme.h"
#include "base/widget.h"
#include "tkc/buffer.h"
#include "base/style_factory.h"
#include "base/theme_xml.h"
#include "gtest/gtest.h"
#include <stdlib.h>

#include <string>
using std::string;

static const char* state_names[] = {WIDGET_STATE_NORMAL,  WIDGET_STATE_PRESSED, WIDGET_STATE_OVER,
                                    WIDGET_STATE_DISABLE, WIDGET_STATE_FOCUSED, NULL};

static const char* widget_types[] = {WIDGET_TYPE_WINDOW_MANAGER,
                                     WIDGET_TYPE_NORMAL_WINDOW,
                                     WIDGET_TYPE_TOOL_BAR,
                                     WIDGET_TYPE_DIALOG,
                                     WIDGET_TYPE_POPUP,
                                     WIDGET_TYPE_SPRITE,
                                     WIDGET_TYPE_KEYBOARD,
                                     WIDGET_TYPE_DND,
                                     WIDGET_TYPE_LABEL,
                                     WIDGET_TYPE_BUTTON,
                                     WIDGET_TYPE_IMAGE,
                                     WIDGET_TYPE_EDIT,
                                     WIDGET_TYPE_PROGRESS_BAR,
                                     WIDGET_TYPE_GROUP_BOX,
                                     WIDGET_TYPE_CHECK_BUTTON,
                                     WIDGET_TYPE_RADIO_BUTTON,
                                     WIDGET_TYPE_DIALOG_TITLE,
                                     WIDGET_TYPE_DIALOG_CLIENT,
                                     WIDGET_TYPE_SLIDER,
                                     WIDGET_TYPE_VIEW,
                                     WIDGET_TYPE_COMBO_BOX,
                                     WIDGET_TYPE_COMBO_BOX_ITEM,
                                     WIDGET_TYPE_SLIDE_VIEW,
                                     WIDGET_TYPE_PAGES,
                                     WIDGET_TYPE_TAB_BUTTON,
                                     WIDGET_TYPE_TAB_CONTROL,
                                     WIDGET_TYPE_TAB_BUTTON_GROUP,
                                     WIDGET_TYPE_BUTTON_GROUP,
                                     WIDGET_TYPE_CANDIDATES,
                                     WIDGET_TYPE_SPIN_BOX,
                                     WIDGET_TYPE_DRAGGER,
                                     WIDGET_TYPE_SCROLL_BAR,
                                     WIDGET_TYPE_SCROLL_BAR_DESKTOP,
                                     WIDGET_TYPE_SCROLL_BAR_MOBILE,
                                     WIDGET_TYPE_SCROLL_VIEW,
                                     WIDGET_TYPE_LIST_VIEW,
                                     WIDGET_TYPE_LIST_VIEW_H,
                                     WIDGET_TYPE_LIST_ITEM,
                                     NULL};
theme_t* GenThemeData(uint32_t state_nr, uint32_t name_nr) {
  str_t str;
  str_init(&str, 100000);

  for (int32_t i = 0; widget_types[i]; i++) {
    const char* type = widget_types[i];
    str_append_format(&str, 100, "<%s>\n", type);
    str_append_format(&str, 100, "<style name=\"%s\">\n", TK_DEFAULT_STYLE);
    for (uint32_t state = 0; state < state_nr; state++) {
      str_append_format(&str, 100, "<%s ", state_names[state]);
      for (uint32_t k = 0; k < name_nr; k++) {
        char name[32];
        char value[32];

        snprintf(name, sizeof(name), "%d", k);
        snprintf(value, sizeof(value), "%d", k);
        str_append_format(&str, 100, " %s=\"%s\"", name, value);
      }
      str_append_format(&str, 100, ">\n</%s>\n", state_names[state]);
    }
    str_append_format(&str, 100, "</%s>\n", type);
    str_append_format(&str, 100, "</style>");
  }

  theme_t* t = theme_xml_create(str.str);
  str_reset(&str);

  return t;
}

TEST(Theme, saveLoad) {
  uint8_t v8;
  uint16_t v16;
  int32_t i32;
  uint32_t v32;
  uint8_t buff[15];
  uint8_t* p = buff;

  save_uint8(p, 0x1f);
  save_uint16(p, 0x2f2f);
  save_int32(p, 50);
  save_int32(p, -100);
  save_uint32(p, 0x3f3f3f3f);

  p = buff;
  load_uint8(p, v8);
  ASSERT_EQ(v8, 0x1f);

  load_uint16(p, v16);
  ASSERT_EQ(v16, 0x2f2f);

  load_int32(p, i32);
  ASSERT_EQ(i32, 50);

  load_int32(p, i32);
  ASSERT_EQ(i32, -100);

  load_uint32(p, v32);
  ASSERT_EQ(v32, 0x3f3f3f3fu);
}

TEST(Theme, basic) {
  uint32_t state_nr = 5;
  uint32_t name_nr = 5;
  const uint8_t* style_data;

  theme_t* t = GenThemeData(state_nr, name_nr);

  for (int32_t i = 0; widget_types[i]; i++) {
    const char* type = widget_types[i];
    for (uint32_t state = 0; state < state_nr; state++) {
      style_data = theme_find_style(t, type, 0, state_names[state]);
      ASSERT_EQ(style_data != NULL, true);
      ASSERT_EQ(tk_str_eq(theme_get_style_type(t), THEME_DEFAULT_STYLE_TYPE), true);
      style_t* s = style_factory_create_style(NULL, theme_get_style_type(t));
      ASSERT_EQ(s != NULL, true);
      ASSERT_EQ(style_set_style_data(s, style_data, NULL), RET_OK);
      for (uint32_t k = 0; k < name_nr; k++) {
        char name[32];
        snprintf(name, sizeof(name), "%d", k);
        uint32_t v = (uint32_t)style_get_int(s, name, 0);
        ASSERT_EQ(v, k);
      }
      ASSERT_EQ(style_destroy(s), RET_OK);
    }
  }

  theme_destroy(t);
}

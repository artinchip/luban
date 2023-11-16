﻿
#include "base/theme.h"
#include "base/widget.h"
#include "gtest/gtest.h"
#include "base/style_factory.h"
#include "base/theme_xml.h"
#include <stdlib.h>

#include <string>
using std::string;

TEST(ThemeGen, basic0) {
  theme_t* theme = NULL;
  const uint8_t* style_data = NULL;
  const char* str = "<widget><style><normal margin=\"-10\" bg_color=\"#fafbfc\"/></style></widget>";

  style_t* s = style_factory_create_style(NULL, theme_get_style_type(theme));

  theme = theme_xml_create(str);

  style_data = theme_find_style(theme, WIDGET_TYPE_NONE, TK_DEFAULT_STYLE, WIDGET_STATE_NORMAL);
  ASSERT_EQ(style_data != NULL, true);
  ASSERT_EQ(style_set_style_data(s, style_data, WIDGET_STATE_NORMAL), RET_OK);
  ASSERT_EQ(style_get_int(s, STYLE_ID_MARGIN, 0), -10);
  ASSERT_EQ(style_get_uint(s, STYLE_ID_BG_COLOR, 0), 0xfffcfbfau);
  style_destroy(s);
  theme_destroy(theme);
}

TEST(ThemeGen, basic1) {
  theme_t* theme = NULL;
  const uint8_t* style_data = NULL;
  const char* str =
      "<widget><style><normal bg_color=\"#fafbfc\" fg_color=\"#223344\" font_name=\"sans\" "
      "font_size=\"12\"/></style></widget>";

  style_t* s = style_factory_create_style(NULL, theme_get_style_type(theme));

  theme = theme_xml_create(str);

  style_data = theme_find_style(theme, WIDGET_TYPE_NONE, TK_DEFAULT_STYLE, WIDGET_STATE_NORMAL);
  ASSERT_EQ(style_data != NULL, true);
  ASSERT_EQ(style_set_style_data(s, style_data, WIDGET_STATE_NORMAL), RET_OK);
  ASSERT_EQ(style_get_uint(s, STYLE_ID_BG_COLOR, 0), 0xfffcfbfau);
  ASSERT_EQ(style_get_uint(s, STYLE_ID_FG_COLOR, 0), 0xff443322u);
  ASSERT_EQ(style_get_int(s, STYLE_ID_FONT_SIZE, 0), 12);
  ASSERT_EQ(style_get_str(s, STYLE_ID_FONT_NAME, ""), string("sans"));
  style_destroy(s);
  theme_destroy(theme);
}

TEST(ThemeGen, basic2) {
  theme_t* theme = NULL;
  const uint8_t* style_data = NULL;
  const char* str =
      "<widget>\
      <style><normal bg_color=\"#112233\" fg_color=\"#223344\" font_name=\"sans\" font_size=\"12\"/></style>\
      <style><over bg_color=\"#112233\" fg_color=\"#223344\" font_name=\"sans\" font_size=\"12\"/></style>\
      <style><focus bg_color=\"#112233\" fg_color=\"#223344\" font_name=\"sans\" font_size=\"12\"/></style>\
      </widget>";

  style_t* s = style_factory_create_style(NULL, theme_get_style_type(theme));

  theme = theme_xml_create(str);

  style_data = theme_find_style(theme, WIDGET_TYPE_NONE, TK_DEFAULT_STYLE, WIDGET_STATE_OVER);
  ASSERT_EQ(style_data != NULL, true);
  ASSERT_EQ(style_set_style_data(s, style_data, WIDGET_STATE_OVER), RET_OK);
  ASSERT_EQ(style_get_uint(s, STYLE_ID_BG_COLOR, 0), 0xff332211u);
  ASSERT_EQ(style_get_uint(s, STYLE_ID_FG_COLOR, 0), 0xff443322u);
  ASSERT_EQ(style_get_int(s, STYLE_ID_FONT_SIZE, 0), 12);
  ASSERT_EQ(style_get_str(s, STYLE_ID_FONT_NAME, ""), string("sans"));
  style_destroy(s);
  theme_destroy(theme);
}

TEST(ThemeGen, basic3) {
  theme_t* theme = NULL;
  const uint8_t* style_data = NULL;
  const char* str =
      "<widget>\
        <style name=\"default\"><normal bg_color=\"#112233\" fg_color=\"#223344\" font_name=\"sans\" font_size=\"12\"/></style>\
        <style name=\"1\"><normal bg_color=\"#112233\" fg_color=\"#223344\" font_name=\"sans\" font_size=\"12\"/></style>\
        <style name=\"2\"><normal bg_color=\"#112233\" fg_color=\"#223344\" font_name=\"sans\" font_size=\"12\"/></style>\
      </widget>\
      <button>\
        <style name=\"default\"><normal bg_color=\"#112233\" fg_color=\"#223344\" font_name=\"sans\" font_size=\"12\"/></style>\
        <style name=\"1\"><normal bg_color=\"#112233\" fg_color=\"#223344\" font_name=\"sans\" font_size=\"12\"/></style>\
        <style name=\"2\"><normal bg_color=\"#112233\" fg_color=\"#223344\" font_name=\"sans\" font_size=\"12\"/></style>\
      </button>\
      <label>\
        <style name=\"default\"><normal bg_color=\"#112233\" fg_color=\"#223344\" font_name=\"sans\" font_size=\"12\"/></style>\
        <style name=\"1\"><normal bg_color=\"#112233\" fg_color=\"#223344\" font_name=\"sans\" font_size=\"12\"/></style>\
        <style name=\"2\"><normal bg_color=\"#112233\" fg_color=\"#223344\" font_name=\"sans\" font_size=\"12\"/></style>\
      </label>\
      ";

  style_t* s = style_factory_create_style(NULL, theme_get_style_type(theme));

  theme = theme_xml_create(str);

  style_data = theme_find_style(theme, WIDGET_TYPE_BUTTON, TK_DEFAULT_STYLE, WIDGET_STATE_NORMAL);
  ASSERT_EQ(style_data != NULL, true);
  ASSERT_EQ(style_set_style_data(s, style_data, WIDGET_STATE_NORMAL), RET_OK);
  ASSERT_EQ(style_get_uint(s, STYLE_ID_BG_COLOR, 0), 0xff332211u);
  ASSERT_EQ(style_get_uint(s, STYLE_ID_FG_COLOR, 0), 0xff443322u);
  ASSERT_EQ(style_get_int(s, STYLE_ID_FONT_SIZE, 0), 12);
  ASSERT_EQ(style_get_str(s, STYLE_ID_FONT_NAME, ""), string("sans"));
  style_destroy(s);
  theme_destroy(theme);
}

TEST(ThemeGen, basic4) {
  theme_t* theme = NULL;
  const uint8_t* style_data = NULL;
  const char* str =
      "<widget>\
        <style name=\"default\"><normal bg_color=\"#112233\" fg_color=\"#223344\" font_name=\"sans\" font_size=\"12\"/></style>\
        <style name=\"1\"><normal bg_color=\"#112233\" fg_color=\"#223344\" font_name=\"sans\" font_size=\"12\"/></style>\
        <style name=\"2\"><normal bg_color=\"#112233\" fg_color=\"#223344\" font_name=\"sans\" font_size=\"12\"/></style>\
      </widget>\
      <button>\
        <style name=\"default\"> \
          <normal bg_color=\"#112233\" fg_color=\"#223344\" font_name=\"sans\" font_size=\"12\"/> \
          <over bg_color=\"#112244\" fg_color=\"#223355\" font_name=\"sans\" font_size=\"12\"/> \
          <focus bg_color=\"#112233\" fg_color=\"#223344\" font_name=\"sans\" font_size=\"12\"/> \
        </style>\
        <style name=\"1\"><normal bg_color=\"#112233\" fg_color=\"#223344\" font_name=\"sans\" font_size=\"12\"/></style>\
        <style name=\"2\"><normal bg_color=\"#112233\" fg_color=\"#223344\" font_name=\"sans\" font_size=\"12\"/></style>\
      </button>\
      <label>\
        <style name=\"default\"><normal bg_color=\"#112233\" fg_color=\"#223344\" font_name=\"sans\" font_size=\"12\"/></style>\
        <style name=\"1\"><normal bg_color=\"#112233\" fg_color=\"#223344\" font_name=\"sans\" font_size=\"12\"/></style>\
        <style name=\"2\"><normal bg_color=\"#112233\" fg_color=\"#223344\" font_name=\"sans\" font_size=\"12\"/></style>\
      </label>\
      ";

  style_t* s = style_factory_create_style(NULL, theme_get_style_type(theme));

  theme = theme_xml_create(str);

  style_data = theme_find_style(theme, WIDGET_TYPE_BUTTON, TK_DEFAULT_STYLE, WIDGET_STATE_OVER);
  ASSERT_EQ(style_data != NULL, true);
  ASSERT_EQ(style_set_style_data(s, style_data, WIDGET_STATE_OVER), RET_OK);
  ASSERT_EQ(style_get_uint(s, STYLE_ID_BG_COLOR, 0), 0xff442211u);
  ASSERT_EQ(style_get_uint(s, STYLE_ID_FG_COLOR, 0), 0xff553322u);
  ASSERT_EQ(style_get_int(s, STYLE_ID_FONT_SIZE, 0), 12);
  ASSERT_EQ(style_get_str(s, STYLE_ID_FONT_NAME, ""), string("sans"));
  style_destroy(s);
  theme_destroy(theme);
}

TEST(ThemeGen, state) {
  theme_t* theme = NULL;
  const uint8_t* style_data = NULL;
  const char* str =
      "<button><style><over bg_color=\"#f1f2f3\" fg_color=\"#fafbfc\" font_name=\"sans\" font_size=\"12\" /></style></button>\
       <button><style><pressed bg_color=\"rgb(255,255,0)\" fg_color=\"rgba(255,255,0,0.5)\" border_color=\"#ff00ff\"/></style></button>";

  style_t* s = style_factory_create_style(NULL, theme_get_style_type(theme));

  theme = theme_xml_create(str);

  style_data = theme_find_style(theme, WIDGET_TYPE_BUTTON, TK_DEFAULT_STYLE, WIDGET_STATE_OVER);
  ASSERT_EQ(style_data != NULL, true);
  ASSERT_EQ(style_set_style_data(s, style_data, WIDGET_STATE_OVER), RET_OK);
  ASSERT_EQ(style_get_int(s, STYLE_ID_FONT_SIZE, 0), 12);
  ASSERT_EQ(style_get_uint(s, STYLE_ID_BG_COLOR, 0), 0xfff3f2f1u);
  ASSERT_EQ(style_get_uint(s, STYLE_ID_FG_COLOR, 0), 0xfffcfbfau);
  ASSERT_EQ(style_get_str(s, STYLE_ID_FONT_NAME, ""), string("sans"));

  style_data = theme_find_style(theme, WIDGET_TYPE_BUTTON, TK_DEFAULT_STYLE, WIDGET_STATE_PRESSED);
  ASSERT_EQ(style_data != NULL, true);
  ASSERT_EQ(style_set_style_data(s, style_data, WIDGET_STATE_PRESSED), RET_OK);
  ASSERT_EQ((uint32_t)style_get_int(s, STYLE_ID_BG_COLOR, 0), 0xff00ffffu);
  style_destroy(s);
  theme_destroy(theme);
}

TEST(ThemeGen, style_type) {
  theme_t* theme = NULL;
  const uint8_t* style_data = NULL;
  const char* str =
      "<button><style name=\"yellow\"><over bg_color=\"yellow\" fg_color=\"#fafbfc\" font_name=\"sans\" font_size=\"12\" /></style></button>\
       <button><style name=\"yellow\"><pressed bg_color=\"rgb(255,255,0)\" fg_color=\"rgba(255,255,0,0.5)\" border_color=\"#ff00ff\" /></style></button>";

  style_t* s = style_factory_create_style(NULL, theme_get_style_type(theme));

  theme = theme_xml_create(str);

  style_data = theme_find_style(theme, WIDGET_TYPE_BUTTON, "yellow", WIDGET_STATE_OVER);
  ASSERT_EQ(style_data != NULL, true);

  style_data = theme_find_style(theme, WIDGET_TYPE_BUTTON, "yellow", WIDGET_STATE_PRESSED);
  ASSERT_EQ(style_data != NULL, true);
  style_destroy(s);
  theme_destroy(theme);
}

TEST(ThemeGen, inher) {
  theme_t* theme = NULL;
  const uint8_t* style_data = NULL;
  const char* str =
      "<button font_size=\"12\"><style name=\"yellow\" font_name=\"sans\"><over bg_color=\"yellow\" fg_color=\"#fafbfc\" /></style>\
       <style name=\"yellow\"><pressed margin=\"-10\" bg_color=\"rgb(255,255,0)\" font_name=\"serif\" font_size=\"14\" /></style></button>";

  style_t* s = style_factory_create_style(NULL, theme_get_style_type(theme));

  theme = theme_xml_create(str);

  style_data = theme_find_style(theme, WIDGET_TYPE_BUTTON, "yellow", WIDGET_STATE_OVER);
  ASSERT_EQ(style_data != NULL, true);
  ASSERT_EQ(style_set_style_data(s, style_data, WIDGET_STATE_OVER), RET_OK);
  ASSERT_EQ(style_get_int(s, STYLE_ID_FONT_SIZE, 0), 12);
  ASSERT_EQ(style_get_str(s, STYLE_ID_FONT_NAME, ""), string("sans"));

  style_data = theme_find_style(theme, WIDGET_TYPE_BUTTON, "yellow", WIDGET_STATE_PRESSED);
  ASSERT_EQ(style_data != NULL, true);
  ASSERT_EQ(style_set_style_data(s, style_data, WIDGET_STATE_PRESSED), RET_OK);
  ASSERT_EQ(style_get_int(s, STYLE_ID_MARGIN, 0), -10);
  ASSERT_EQ(style_get_int(s, STYLE_ID_FONT_SIZE, 0), 14);
  ASSERT_EQ(style_get_str(s, STYLE_ID_FONT_NAME, ""), string("serif"));
  style_destroy(s);
  theme_destroy(theme);
}

TEST(ThemeGen, cdata) {
  theme_t* theme = NULL;
  const uint8_t* style_data = NULL;
  const char* str =
      "<button><property name=\"font_size\"><![CDATA[12]]></property>\
       <style name=\"yellow\"><property name=\"font_name\"><![CDATA[sans]]></property>\
       <over bg_color=\"yellow\" fg_color=\"#fafbfc\"><property name=\"text_color\"><![CDATA[#fdfeff]]></property>\
       </over></style></button>";

  style_t* s = style_factory_create_style(NULL, theme_get_style_type(theme));

  theme = theme_xml_create(str);

  style_data = theme_find_style(theme, WIDGET_TYPE_BUTTON, "yellow", WIDGET_STATE_OVER);
  ASSERT_EQ(style_data != NULL, true);
  ASSERT_EQ(style_set_style_data(s, style_data, WIDGET_STATE_OVER), RET_OK);
  ASSERT_EQ(style_get_int(s, STYLE_ID_FONT_SIZE, 0), 12);
  ASSERT_EQ(style_get_str(s, STYLE_ID_FONT_NAME, ""), string("sans"));
  ASSERT_EQ(style_get_uint(s, STYLE_ID_TEXT_COLOR, 0), 0xfffffefd);
  style_destroy(s);
  theme_destroy(theme);
}

TEST(ThemeGen, border) {
  theme_t* theme = NULL;
  const uint8_t* style_data = NULL;
  const char* str = "<button><style><normal border=\"left\" /></style></button>";

  theme = theme_xml_create(str);

  style_t* s = style_factory_create_style(NULL, theme_get_style_type(theme));
  style_data = theme_find_style(theme, WIDGET_TYPE_BUTTON, TK_DEFAULT_STYLE, WIDGET_STATE_NORMAL);
  ASSERT_EQ(style_set_style_data(s, style_data, WIDGET_STATE_NORMAL), RET_OK);
  ASSERT_EQ(style_get_int(s, STYLE_ID_BORDER, 0), BORDER_LEFT);
  theme_destroy(theme);

  str = "<button><style><normal border=\"right\" /></style></button>";
  theme = theme_xml_create(str);
  style_data = theme_find_style(theme, WIDGET_TYPE_BUTTON, TK_DEFAULT_STYLE, WIDGET_STATE_NORMAL);
  ASSERT_EQ(style_set_style_data(s, style_data, WIDGET_STATE_NORMAL), RET_OK);
  ASSERT_EQ(style_get_int(s, STYLE_ID_BORDER, 0), BORDER_RIGHT);
  theme_destroy(theme);

  str = "<button><style><normal border=\"top\" /></style></button>";
  theme = theme_xml_create(str);
  style_data = theme_find_style(theme, WIDGET_TYPE_BUTTON, TK_DEFAULT_STYLE, WIDGET_STATE_NORMAL);
  ASSERT_EQ(style_set_style_data(s, style_data, WIDGET_STATE_NORMAL), RET_OK);
  ASSERT_EQ(style_get_int(s, STYLE_ID_BORDER, 0), BORDER_TOP);
  theme_destroy(theme);

  str = "<button><style><normal border=\"bottom\" /></style></button>";
  theme = theme_xml_create(str);
  style_data = theme_find_style(theme, WIDGET_TYPE_BUTTON, TK_DEFAULT_STYLE, WIDGET_STATE_NORMAL);
  ASSERT_EQ(style_set_style_data(s, style_data, WIDGET_STATE_NORMAL), RET_OK);
  ASSERT_EQ(style_get_int(s, STYLE_ID_BORDER, 0), BORDER_BOTTOM);
  theme_destroy(theme);

  str = "<button><style><normal border=\"all\" /></style></button>";
  theme = theme_xml_create(str);
  style_data = theme_find_style(theme, WIDGET_TYPE_BUTTON, TK_DEFAULT_STYLE, WIDGET_STATE_NORMAL);
  ASSERT_EQ(style_set_style_data(s, style_data, WIDGET_STATE_NORMAL), RET_OK);
  ASSERT_EQ(style_get_int(s, STYLE_ID_BORDER, 0), BORDER_ALL);
  style_destroy(s);
  theme_destroy(theme);
}

TEST(ThemeGen, active_state) {
  theme_t* theme = NULL;
  color_t def = color_init(0, 0, 0, 0);
  const uint8_t* style_data = NULL;
  const char* str =
      "<tab_button> \
  <style name=\"default\" text_align_h=\"left\" margin=\"4\" border_color=\"#cccccc\" bg_color=\"#eeeeee\"> \
    <normal     text_color=\"#111111\"/> \
    <pressed    text_color=\"#222222\"/> \
    <over       text_color=\"#333333\"/> \
    <normal_of_active     text_color=\"#444444\"/> \
    <pressed_of_active    text_color=\"#555555\"/> \
    <over_of_active       text_color=\"#666666\"/> \
  </style> \
</tab_button>";

  style_t* s = style_factory_create_style(NULL, theme_get_style_type(theme));

  theme = theme_xml_create(str);

  style_data = theme_find_style(theme, WIDGET_TYPE_TAB_BUTTON, "default", WIDGET_STATE_NORMAL);
  ASSERT_EQ(style_set_style_data(s, style_data, WIDGET_STATE_NORMAL), RET_OK);
  ASSERT_EQ(style_get_color(s, STYLE_ID_TEXT_COLOR, def).rgba.r, 0x11);

  style_data = theme_find_style(theme, WIDGET_TYPE_TAB_BUTTON, "default", WIDGET_STATE_PRESSED);
  ASSERT_EQ(style_set_style_data(s, style_data, WIDGET_STATE_PRESSED), RET_OK);
  ASSERT_EQ(style_get_color(s, STYLE_ID_TEXT_COLOR, def).rgba.r, 0x22);

  style_data = theme_find_style(theme, WIDGET_TYPE_TAB_BUTTON, "default", WIDGET_STATE_OVER);
  ASSERT_EQ(style_set_style_data(s, style_data, WIDGET_STATE_OVER), RET_OK);
  ASSERT_EQ(style_get_color(s, STYLE_ID_TEXT_COLOR, def).rgba.r, 0x33);

  style_data =
      theme_find_style(theme, WIDGET_TYPE_TAB_BUTTON, "default", WIDGET_STATE_NORMAL_OF_ACTIVE);
  ASSERT_EQ(style_set_style_data(s, style_data, WIDGET_STATE_NORMAL_OF_ACTIVE), RET_OK);
  ASSERT_EQ(style_get_color(s, STYLE_ID_TEXT_COLOR, def).rgba.r, 0x44);

  style_data =
      theme_find_style(theme, WIDGET_TYPE_TAB_BUTTON, "default", WIDGET_STATE_PRESSED_OF_ACTIVE);
  ASSERT_EQ(style_set_style_data(s, style_data, WIDGET_STATE_PRESSED_OF_ACTIVE), RET_OK);
  ASSERT_EQ(style_get_color(s, STYLE_ID_TEXT_COLOR, def).rgba.r, 0x55);

  style_data =
      theme_find_style(theme, WIDGET_TYPE_TAB_BUTTON, "default", WIDGET_STATE_OVER_OF_ACTIVE);
  ASSERT_EQ(style_set_style_data(s, style_data, WIDGET_STATE_OVER_OF_ACTIVE), RET_OK);
  ASSERT_EQ(style_get_color(s, STYLE_ID_TEXT_COLOR, def).rgba.r, 0x66);
  style_destroy(s);
  theme_destroy(theme);
}

TEST(ThemeGen, selected_state) {
  theme_t* theme = NULL;
  color_t def = color_init(0, 0, 0, 0);
  const uint8_t* style_data = NULL;
  const char* str =
      "<combo_box_item> \
  <style name=\"default\" text_align_h=\"left\" margin=\"4\" border_color=\"#cccccc\" bg_color=\"#eeeeee\"> \
    <normal     text_color=\"#111111\"/> \
    <pressed    text_color=\"#222222\"/> \
    <over       text_color=\"#333333\"/> \
    <normal_of_checked     text_color=\"#444444\"/> \
    <pressed_of_checked    text_color=\"#555555\"/> \
    <over_of_checked       text_color=\"#666666\"/> \
  </style> \
</combo_box_item>";

  style_t* s = style_factory_create_style(NULL, theme_get_style_type(theme));

  theme = theme_xml_create(str);

  style_data = theme_find_style(theme, WIDGET_TYPE_COMBO_BOX_ITEM, "default", WIDGET_STATE_NORMAL);
  ASSERT_EQ(style_set_style_data(s, style_data, WIDGET_STATE_NORMAL), RET_OK);
  ASSERT_EQ(style_get_color(s, STYLE_ID_TEXT_COLOR, def).rgba.r, 0x11);

  style_data = theme_find_style(theme, WIDGET_TYPE_COMBO_BOX_ITEM, "default", WIDGET_STATE_PRESSED);
  ASSERT_EQ(style_set_style_data(s, style_data, WIDGET_STATE_PRESSED), RET_OK);
  ASSERT_EQ(style_get_color(s, STYLE_ID_TEXT_COLOR, def).rgba.r, 0x22);

  style_data = theme_find_style(theme, WIDGET_TYPE_COMBO_BOX_ITEM, "default", WIDGET_STATE_OVER);
  ASSERT_EQ(style_set_style_data(s, style_data, WIDGET_STATE_OVER), RET_OK);
  ASSERT_EQ(style_get_color(s, STYLE_ID_TEXT_COLOR, def).rgba.r, 0x33);

  style_data = theme_find_style(theme, WIDGET_TYPE_COMBO_BOX_ITEM, "default",
                                WIDGET_STATE_NORMAL_OF_CHECKED);
  ASSERT_EQ(style_set_style_data(s, style_data, WIDGET_STATE_NORMAL_OF_CHECKED), RET_OK);
  ASSERT_EQ(style_get_color(s, STYLE_ID_TEXT_COLOR, def).rgba.r, 0x44);

  style_data = theme_find_style(theme, WIDGET_TYPE_COMBO_BOX_ITEM, "default",
                                WIDGET_STATE_PRESSED_OF_CHECKED);
  ASSERT_EQ(style_set_style_data(s, style_data, WIDGET_STATE_PRESSED_OF_CHECKED), RET_OK);
  ASSERT_EQ(style_get_color(s, STYLE_ID_TEXT_COLOR, def).rgba.r, 0x55);

  style_data =
      theme_find_style(theme, WIDGET_TYPE_COMBO_BOX_ITEM, "default", WIDGET_STATE_OVER_OF_CHECKED);
  ASSERT_EQ(style_set_style_data(s, style_data, WIDGET_STATE_OVER_OF_CHECKED), RET_OK);
  ASSERT_EQ(style_get_color(s, STYLE_ID_TEXT_COLOR, def).rgba.r, 0x66);
  style_destroy(s);
  theme_destroy(theme);
}

TEST(ThemeGen, gradient) {
  gradient_t g;
  theme_t* theme = NULL;
  const uint8_t* style_data = NULL;
  const char* str =
      "<widget><style><normal fg_color=\"red\" bg_color=\"linear-gradient(180deg, #FF0000 0%, "
      "#0000FF 100%)\"/></style></widget>";

  style_t* s = style_factory_create_style(NULL, theme_get_style_type(theme));

  theme = theme_xml_create(str);

  style_data = theme_find_style(theme, WIDGET_TYPE_NONE, TK_DEFAULT_STYLE, WIDGET_STATE_NORMAL);
  ASSERT_EQ(style_data != NULL, true);
  ASSERT_EQ(style_set_style_data(s, style_data, WIDGET_STATE_NORMAL), RET_OK);
  ASSERT_EQ(style_get_gradient(s, STYLE_ID_BG_COLOR, &g) != NULL, true);

  ASSERT_EQ(g.type, GRADIENT_LINEAR);
  ASSERT_EQ(g.nr, 2u);
  ASSERT_EQ(g.degree, 180u);

  ASSERT_EQ(g.stops[0].offset, 0.0f);
  ASSERT_EQ(g.stops[0].color.rgba.r, 0xff);
  ASSERT_EQ(g.stops[1].offset, 1.0f);
  ASSERT_EQ(g.stops[1].color.rgba.b, 0xff);

  ASSERT_EQ(style_get_gradient(s, STYLE_ID_FG_COLOR, &g) != NULL, true);
  ASSERT_EQ(g.nr, 1u);
  ASSERT_EQ(g.degree, 0u);

  ASSERT_EQ(g.stops[0].offset, 0.0f);
  ASSERT_EQ(g.stops[0].color.rgba.r, 0xff);
  ASSERT_EQ(g.stops[0].color.rgba.g, 0);
  ASSERT_EQ(g.stops[0].color.rgba.b, 0);
  ASSERT_EQ(g.stops[0].color.rgba.a, 0xff);

  style_destroy(s);
  theme_destroy(theme);
}

TEST(ThemeGen, bool) {
  theme_t* theme = NULL;
  const uint8_t* style_data = NULL;
  const char* str =
      "<widget><style><normal feedback=\"true\" focusable=\"2\" clear_bg=\"1\"/></style></widget>";

  style_t* s = style_factory_create_style(NULL, theme_get_style_type(theme));

  theme = theme_xml_create(str);

  style_data = theme_find_style(theme, WIDGET_TYPE_NONE, TK_DEFAULT_STYLE, WIDGET_STATE_NORMAL);
  ASSERT_EQ(style_data != NULL, true);
  ASSERT_EQ(style_set_style_data(s, style_data, WIDGET_STATE_NORMAL), RET_OK);
  ASSERT_EQ(style_get_int(s, STYLE_ID_FEEDBACK, 0), 1);
  ASSERT_EQ(style_get_int(s, STYLE_ID_FOCUSABLE, 0), 1);
  ASSERT_EQ(style_get_int(s, STYLE_ID_CLEAR_BG, 0), 1);
  style_destroy(s);
  theme_destroy(theme);
}

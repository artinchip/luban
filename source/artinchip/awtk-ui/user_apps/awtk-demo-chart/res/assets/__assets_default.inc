#include "awtk.h"
#include "base/assets_manager.h"
#ifndef WITH_FS_RES
#include "default/inc/ui/home_page.data"
#include "default/inc/ui/system_bar.data"
#include "default/inc/ui/window_bar.data"
#include "default/inc/ui/window_line.data"
#include "default/inc/ui/window_meter.data"
#include "default/inc/ui/window_pie.data"
#include "default/inc/strings/en_US.data"
#include "default/inc/strings/zh_CN.data"
#include "default/inc/styles/default.data"
#ifdef WITH_STB_IMAGE
#else
#include "default/inc/images/arrow_down_d.data"
#include "default/inc/images/arrow_down_f.data"
#include "default/inc/images/arrow_down_n.data"
#include "default/inc/images/arrow_down_o.data"
#include "default/inc/images/arrow_down_p.data"
#include "default/inc/images/arrow_left_d.data"
#include "default/inc/images/arrow_left_f.data"
#include "default/inc/images/arrow_left_n.data"
#include "default/inc/images/arrow_left_o.data"
#include "default/inc/images/arrow_left_p.data"
#include "default/inc/images/arrow_right_d.data"
#include "default/inc/images/arrow_right_f.data"
#include "default/inc/images/arrow_right_n.data"
#include "default/inc/images/arrow_right_o.data"
#include "default/inc/images/arrow_right_p.data"
#include "default/inc/images/arrow_up_d.data"
#include "default/inc/images/arrow_up_f.data"
#include "default/inc/images/arrow_up_n.data"
#include "default/inc/images/arrow_up_o.data"
#include "default/inc/images/arrow_up_p.data"
#include "default/inc/images/backspace.data"
#include "default/inc/images/back_normal.data"
#include "default/inc/images/back_pressed.data"
#include "default/inc/images/bg_main.data"
#include "default/inc/images/check_checked_d.data"
#include "default/inc/images/check_checked_f.data"
#include "default/inc/images/check_checked_n.data"
#include "default/inc/images/check_checked_o.data"
#include "default/inc/images/check_checked_p.data"
#include "default/inc/images/check_d.data"
#include "default/inc/images/check_f.data"
#include "default/inc/images/check_n.data"
#include "default/inc/images/check_o.data"
#include "default/inc/images/check_p.data"
#include "default/inc/images/clock_bg.data"
#include "default/inc/images/clock_hour.data"
#include "default/inc/images/clock_minute.data"
#include "default/inc/images/clock_second.data"
#include "default/inc/images/close_d.data"
#include "default/inc/images/close_n.data"
#include "default/inc/images/close_o.data"
#include "default/inc/images/close_p.data"
#include "default/inc/images/cross.data"
#include "default/inc/images/earth.data"
#include "default/inc/images/graph_normal_en.data"
#include "default/inc/images/graph_normal_zh.data"
#include "default/inc/images/graph_pressed_en.data"
#include "default/inc/images/graph_pressed_zh.data"
#include "default/inc/images/guage_bg.data"
#include "default/inc/images/guage_bg_1.data"
#include "default/inc/images/guage_bg_2.data"
#include "default/inc/images/guage_pointer.data"
#include "default/inc/images/guage_pointer_1.data"
#include "default/inc/images/guage_pointer_2.data"
#include "default/inc/images/histogram_normal_en.data"
#include "default/inc/images/histogram_normal_zh.data"
#include "default/inc/images/histogram_pressed_en.data"
#include "default/inc/images/histogram_pressed_zh.data"
#include "default/inc/images/icon_1_normal.data"
#include "default/inc/images/icon_1_pressed.data"
#include "default/inc/images/icon_2_normal.data"
#include "default/inc/images/icon_2_pressed.data"
#include "default/inc/images/icon_3_normal.data"
#include "default/inc/images/icon_3_pressed.data"
#include "default/inc/images/icon_4_normal.data"
#include "default/inc/images/icon_4_pressed.data"
#include "default/inc/images/icon_5_normal.data"
#include "default/inc/images/icon_5_pressed.data"
#include "default/inc/images/icon_6_normal.data"
#include "default/inc/images/icon_6_pressed.data"
#include "default/inc/images/language_en.data"
#include "default/inc/images/language_zh.data"
#include "default/inc/images/meter_normal_en.data"
#include "default/inc/images/meter_normal_zh.data"
#include "default/inc/images/meter_pressed_en.data"
#include "default/inc/images/meter_pressed_zh.data"
#include "default/inc/images/new_normal.data"
#include "default/inc/images/new_pressed.data"
#include "default/inc/images/num0.data"
#include "default/inc/images/num1.data"
#include "default/inc/images/num2.data"
#include "default/inc/images/num3.data"
#include "default/inc/images/num4.data"
#include "default/inc/images/num5.data"
#include "default/inc/images/num6.data"
#include "default/inc/images/num7.data"
#include "default/inc/images/num8.data"
#include "default/inc/images/num9.data"
#include "default/inc/images/numdot.data"
#include "default/inc/images/pause_mormal.data"
#include "default/inc/images/pause_pressed.data"
#include "default/inc/images/pie_label_normal.data"
#include "default/inc/images/pie_label_select.data"
#include "default/inc/images/pie_normal_en.data"
#include "default/inc/images/pie_normal_zh.data"
#include "default/inc/images/pie_pressed_en.data"
#include "default/inc/images/pie_pressed_zh.data"
#include "default/inc/images/play_normal.data"
#include "default/inc/images/play_pressed.data"
#include "default/inc/images/radio_checked_d.data"
#include "default/inc/images/radio_checked_f.data"
#include "default/inc/images/radio_checked_n.data"
#include "default/inc/images/radio_checked_o.data"
#include "default/inc/images/radio_checked_p.data"
#include "default/inc/images/radio_d.data"
#include "default/inc/images/radio_f.data"
#include "default/inc/images/radio_n.data"
#include "default/inc/images/radio_o.data"
#include "default/inc/images/radio_p.data"
#include "default/inc/images/rounded_n.data"
#include "default/inc/images/rounded_o.data"
#include "default/inc/images/rounded_p.data"
#include "default/inc/images/shift.data"
#include "default/inc/images/shifton.data"
#endif /*WITH_STB_IMAGE*/
#ifdef WITH_VGCANVAS
#endif /*WITH_VGCANVAS*/
#ifdef WITH_TRUETYPE_FONT
#else /*WITH_TRUETYPE_FONT*/
#include "default/inc/fonts/default_18.data"
#endif /*WITH_TRUETYPE_FONT*/
#endif /*WITH_FS_RES*/

ret_t assets_init_default(void) {
  assets_manager_t* am = assets_manager();

#ifdef WITH_FS_RES
  assets_manager_preload(am, ASSET_TYPE_STYLE, "default");
#else
  assets_manager_add(am, ui_home_page);
  assets_manager_add(am, ui_system_bar);
  assets_manager_add(am, ui_window_bar);
  assets_manager_add(am, ui_window_line);
  assets_manager_add(am, ui_window_meter);
  assets_manager_add(am, ui_window_pie);
  assets_manager_add(am, strings_en_US);
  assets_manager_add(am, strings_zh_CN);
  assets_manager_add(am, style_default);
  assets_manager_add(am, image_arrow_down_d);
  assets_manager_add(am, image_arrow_down_f);
  assets_manager_add(am, image_arrow_down_n);
  assets_manager_add(am, image_arrow_down_o);
  assets_manager_add(am, image_arrow_down_p);
  assets_manager_add(am, image_arrow_left_d);
  assets_manager_add(am, image_arrow_left_f);
  assets_manager_add(am, image_arrow_left_n);
  assets_manager_add(am, image_arrow_left_o);
  assets_manager_add(am, image_arrow_left_p);
  assets_manager_add(am, image_arrow_right_d);
  assets_manager_add(am, image_arrow_right_f);
  assets_manager_add(am, image_arrow_right_n);
  assets_manager_add(am, image_arrow_right_o);
  assets_manager_add(am, image_arrow_right_p);
  assets_manager_add(am, image_arrow_up_d);
  assets_manager_add(am, image_arrow_up_f);
  assets_manager_add(am, image_arrow_up_n);
  assets_manager_add(am, image_arrow_up_o);
  assets_manager_add(am, image_arrow_up_p);
  assets_manager_add(am, image_backspace);
  assets_manager_add(am, image_back_normal);
  assets_manager_add(am, image_back_pressed);
  assets_manager_add(am, image_bg_main);
  assets_manager_add(am, image_check_checked_d);
  assets_manager_add(am, image_check_checked_f);
  assets_manager_add(am, image_check_checked_n);
  assets_manager_add(am, image_check_checked_o);
  assets_manager_add(am, image_check_checked_p);
  assets_manager_add(am, image_check_d);
  assets_manager_add(am, image_check_f);
  assets_manager_add(am, image_check_n);
  assets_manager_add(am, image_check_o);
  assets_manager_add(am, image_check_p);
  assets_manager_add(am, image_clock_bg);
  assets_manager_add(am, image_clock_hour);
  assets_manager_add(am, image_clock_minute);
  assets_manager_add(am, image_clock_second);
  assets_manager_add(am, image_close_d);
  assets_manager_add(am, image_close_n);
  assets_manager_add(am, image_close_o);
  assets_manager_add(am, image_close_p);
  assets_manager_add(am, image_cross);
  assets_manager_add(am, image_earth);
  assets_manager_add(am, image_graph_normal_en);
  assets_manager_add(am, image_graph_normal_zh);
  assets_manager_add(am, image_graph_pressed_en);
  assets_manager_add(am, image_graph_pressed_zh);
  assets_manager_add(am, image_guage_bg);
  assets_manager_add(am, image_guage_bg_1);
  assets_manager_add(am, image_guage_bg_2);
  assets_manager_add(am, image_guage_pointer);
  assets_manager_add(am, image_guage_pointer_1);
  assets_manager_add(am, image_guage_pointer_2);
  assets_manager_add(am, image_histogram_normal_en);
  assets_manager_add(am, image_histogram_normal_zh);
  assets_manager_add(am, image_histogram_pressed_en);
  assets_manager_add(am, image_histogram_pressed_zh);
  assets_manager_add(am, image_icon_1_normal);
  assets_manager_add(am, image_icon_1_pressed);
  assets_manager_add(am, image_icon_2_normal);
  assets_manager_add(am, image_icon_2_pressed);
  assets_manager_add(am, image_icon_3_normal);
  assets_manager_add(am, image_icon_3_pressed);
  assets_manager_add(am, image_icon_4_normal);
  assets_manager_add(am, image_icon_4_pressed);
  assets_manager_add(am, image_icon_5_normal);
  assets_manager_add(am, image_icon_5_pressed);
  assets_manager_add(am, image_icon_6_normal);
  assets_manager_add(am, image_icon_6_pressed);
  assets_manager_add(am, image_language_en);
  assets_manager_add(am, image_language_zh);
  assets_manager_add(am, image_meter_normal_en);
  assets_manager_add(am, image_meter_normal_zh);
  assets_manager_add(am, image_meter_pressed_en);
  assets_manager_add(am, image_meter_pressed_zh);
  assets_manager_add(am, image_new_normal);
  assets_manager_add(am, image_new_pressed);
  assets_manager_add(am, image_num0);
  assets_manager_add(am, image_num1);
  assets_manager_add(am, image_num2);
  assets_manager_add(am, image_num3);
  assets_manager_add(am, image_num4);
  assets_manager_add(am, image_num5);
  assets_manager_add(am, image_num6);
  assets_manager_add(am, image_num7);
  assets_manager_add(am, image_num8);
  assets_manager_add(am, image_num9);
  assets_manager_add(am, image_numdot);
  assets_manager_add(am, image_pause_mormal);
  assets_manager_add(am, image_pause_pressed);
  assets_manager_add(am, image_pie_label_normal);
  assets_manager_add(am, image_pie_label_select);
  assets_manager_add(am, image_pie_normal_en);
  assets_manager_add(am, image_pie_normal_zh);
  assets_manager_add(am, image_pie_pressed_en);
  assets_manager_add(am, image_pie_pressed_zh);
  assets_manager_add(am, image_play_normal);
  assets_manager_add(am, image_play_pressed);
  assets_manager_add(am, image_radio_checked_d);
  assets_manager_add(am, image_radio_checked_f);
  assets_manager_add(am, image_radio_checked_n);
  assets_manager_add(am, image_radio_checked_o);
  assets_manager_add(am, image_radio_checked_p);
  assets_manager_add(am, image_radio_d);
  assets_manager_add(am, image_radio_f);
  assets_manager_add(am, image_radio_n);
  assets_manager_add(am, image_radio_o);
  assets_manager_add(am, image_radio_p);
  assets_manager_add(am, image_rounded_n);
  assets_manager_add(am, image_rounded_o);
  assets_manager_add(am, image_rounded_p);
  assets_manager_add(am, image_shift);
  assets_manager_add(am, image_shifton);
#ifdef WITH_VGCANVAS
#endif /*WITH_VGCANVAS*/
#ifdef WITH_TRUETYPE_FONT
#else /*WITH_TRUETYPE_FONT*/
  assets_manager_add(am, font_default_18);
#endif /*WITH_TRUETYPE_FONT*/
#endif

  tk_init_assets();
  return RET_OK;
}

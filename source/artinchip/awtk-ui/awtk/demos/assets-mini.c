#include "awtk.h"
#include "base/assets_manager.h"
#include "../res/assets/default/inc/fonts/default_18.data"
#include "../res/assets/default/inc/images/checked.data"
#include "../res/assets/default/inc/images/earth.data"
#include "../res/assets/default/inc/images/left_off.data"
#include "../res/assets/default/inc/images/left_on.data"
#include "../res/assets/default/inc/images/middle_off.data"
#include "../res/assets/default/inc/images/middle_on.data"
#include "../res/assets/default/inc/images/muted.data"
#include "../res/assets/default/inc/images/question.data"
#include "../res/assets/default/inc/images/radio_checked.data"
#include "../res/assets/default/inc/images/radio_unchecked.data"
#include "../res/assets/default/inc/images/red_btn_n.data"
#include "../res/assets/default/inc/images/red_btn_o.data"
#include "../res/assets/default/inc/images/red_btn_p.data"
#include "../res/assets/default/inc/images/right_off.data"
#include "../res/assets/default/inc/images/right_on.data"
#include "../res/assets/default/inc/images/unchecked.data"
#include "../res/assets/default/inc/images/unmuted.data"
#include "../res/assets/default/inc/styles/default.data"
#include "../res/assets/default/inc/ui/basic.data"
#include "../res/assets/default/inc/ui/system_bar.data"
#include "../res/assets/default/inc/images/rgba.data"
#include "../res/assets/default/inc/images/rgb.data"
#include "../res/assets/default/inc/images/message.data"
#include "../res/assets/default/inc/styles/system_bar.data"
#include "../res/assets/default/inc/images/close_n.data"
#include "../res/assets/default/inc/images/close_p.data"
#include "../res/assets/default/inc/images/close_o.data"

ret_t assets_init(void) {
  assets_manager_t* am = assets_manager();

  assets_manager_add(am, font_default_18);
  assets_manager_add(am, image_checked);
  assets_manager_add(am, image_earth);
  assets_manager_add(am, image_left_off);
  assets_manager_add(am, image_left_on);
  assets_manager_add(am, image_middle_off);
  assets_manager_add(am, image_middle_on);
  assets_manager_add(am, image_muted);
  assets_manager_add(am, image_question);
  assets_manager_add(am, image_radio_checked);
  assets_manager_add(am, image_radio_unchecked);
  assets_manager_add(am, image_red_btn_n);
  assets_manager_add(am, image_red_btn_o);
  assets_manager_add(am, image_red_btn_p);
  assets_manager_add(am, image_right_off);
  assets_manager_add(am, image_right_on);
  assets_manager_add(am, image_unchecked);
  assets_manager_add(am, image_unmuted);
  assets_manager_add(am, style_default);
  assets_manager_add(am, ui_basic);
  assets_manager_add(am, ui_system_bar);
  assets_manager_add(am, image_rgb);
  assets_manager_add(am, image_rgba);
  assets_manager_add(am, image_message);
  assets_manager_add(am, image_close_p);
  assets_manager_add(am, image_close_n);
  assets_manager_add(am, image_close_o);
  assets_manager_add(am, image_question);

  assets_manager_add(am, style_system_bar);

  tk_init_assets();
  return RET_OK;
}

bool_t assets_has_theme(const char* name) {
  return FALSE;
}

ret_t assets_set_global_theme(const char* name) {
  return RET_NOT_IMPL;
}

﻿#include "awtk.h"
#include "common/navigator.h"
#include "../3rd/awtk-widget-chart-view/src/chart_view_register.h"

#ifndef APP_SYSTEM_BAR
#define APP_SYSTEM_BAR "system_bar"
#endif /*APP_SYSTEM_BAR*/

#ifndef APP_BOTTOM_SYSTEM_BAR
#define APP_BOTTOM_SYSTEM_BAR ""
#endif /*APP_BOTTOM_SYSTEM_BAR*/

#ifndef APP_START_PAGE
#define APP_START_PAGE "home_page"
#endif /*APP_START_PAGE*/

/**
 * 注册自定义控件
 */
static ret_t custom_widgets_register(void) {
  chart_view_register();

  return RET_OK;
}

/**
 * 当程序初始化完成时调用，全局只触发一次。
 */
static ret_t application_on_launch(void) {
  return RET_OK;
}

/**
 * 当程序退出时调用，全局只触发一次。
 */
static ret_t application_on_exit(void) {
  return RET_OK;
}

/**
 * 初始化程序
 */
ret_t application_init(void) {
  custom_widgets_register();
  application_on_launch();

  if (strlen(APP_SYSTEM_BAR) > 0) {
    navigator_to(APP_SYSTEM_BAR);
  }

  if (strlen(APP_BOTTOM_SYSTEM_BAR) > 0) {
    navigator_to(APP_BOTTOM_SYSTEM_BAR);
  }

  /* release memory according to the theme, and release unused images after switching themes for 1 second */
  image_manager_unload_unused(image_manager(), 1);
  /* set images buffer size */
  image_manager_set_max_mem_size_of_cached_images(image_manager(), 3036643);
  return navigator_to(APP_START_PAGE);
}

/**
 * 退出程序
 */
ret_t application_exit(void) {
  application_on_exit();
  log_debug("application_exit\n");

  return RET_OK;
}

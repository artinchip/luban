
#include "chart_view/chart_view.h"
#include "chart_view/x_axis.h"
#include "chart_view/y_axis.h"
#include "chart_view/line_series.h"
#include "chart_view/line_series_colorful.h"
#include "chart_view/bar_series.h"
#include "chart_view/bar_series_minmax.h"
#include "chart_view/tooltip.h"
#include "pie_slice/pie_slice.h"

ret_t chart_view_register(void) {
  widget_factory_t* f = widget_factory();

  widget_factory_register(f, WIDGET_TYPE_CHART_VIEW, chart_view_create);
  widget_factory_register(f, WIDGET_TYPE_X_AXIS, x_axis_create);
  widget_factory_register(f, WIDGET_TYPE_Y_AXIS, y_axis_create);
  widget_factory_register(f, WIDGET_TYPE_LINE_SERIES, line_series_create);
  widget_factory_register(f, WIDGET_TYPE_LINE_SERIES_COLORFUL, line_series_colorful_create);
  widget_factory_register(f, WIDGET_TYPE_BAR_SERIES, bar_series_create);
  widget_factory_register(f, WIDGET_TYPE_BAR_SERIES_MINMAX, bar_series_minmax_create);
  widget_factory_register(f, WIDGET_TYPE_TOOLTIP, tooltip_create_default);
  widget_factory_register(f, WIDGET_TYPE_PIE_SLICE, pie_slice_create);

  return RET_OK;
}

const char* chart_view_supported_render_mode(void) {
  return "OpenGL|AGGE-BGR565|AGGE-BGRA8888|AGGE-MONO";
}
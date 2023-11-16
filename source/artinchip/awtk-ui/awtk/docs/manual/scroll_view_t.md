## scroll\_view\_t
### 概述
![image](images/scroll_view_t_0.png)

滚动视图。

scroll\_view\_t是[widget\_t](widget_t.md)的子类控件，widget\_t的函数均适用于scroll\_view\_t控件。

在xml中使用"scroll\_view"标签创建滚动视图控件。如：

```xml
<list_view x="0"  y="30" w="100%" h="-80" item_height="60">
<scroll_view name="view" x="0"  y="0" w="100%" h="100%">
<list_item style="odd" children_layout="default(rows=1,cols=0)">
<image draw_type="icon" w="30" image="earth"/>
<label w="-30" text="1.Hello AWTK !">
<switch x="r:10" y="m" w="60" h="20"/>
</label>
</list_item>
...
</scroll_view>
</list_view>
```

> 滚动视图一般作为列表视图的子控件使用。

> 更多用法请参考：[list\_view\_m.xml](
https://github.com/zlgopen/awtk/blob/master/design/default/ui/list_view_m.xml)

在c代码中使用函数scroll\_view\_create创建列表视图控件。如：

```c
widget_t* scroll_view = scroll_view_create(win, 0, 0, 0, 0);
```

可用通过style来设置控件的显示风格，如背景颜色和边框颜色等(一般情况不需要)。
----------------------------------
### 函数
<p id="scroll_view_t_methods">

| 函数名称 | 说明 | 
| -------- | ------------ | 
| <a href="#scroll_view_t_scroll_view_cast">scroll\_view\_cast</a> | 转换为scroll_view对象(供脚本语言使用)。 |
| <a href="#scroll_view_t_scroll_view_create">scroll\_view\_create</a> | 创建scroll_view对象 |
| <a href="#scroll_view_t_scroll_view_get_widget_vtable">scroll\_view\_get\_widget\_vtable</a> | 获取 scroll_view 虚表。 |
| <a href="#scroll_view_t_scroll_view_scroll_delta_to">scroll\_view\_scroll\_delta\_to</a> | 滚动到指定的偏移量。 |
| <a href="#scroll_view_t_scroll_view_scroll_to">scroll\_view\_scroll\_to</a> | 滚动到指定的偏移量。 |
| <a href="#scroll_view_t_scroll_view_set_move_to_page">scroll\_view\_set\_move\_to\_page</a> | 设置滚动时是否每次翻一页 |
| <a href="#scroll_view_t_scroll_view_set_offset">scroll\_view\_set\_offset</a> | 设置偏移量。 |
| <a href="#scroll_view_t_scroll_view_set_recursive">scroll\_view\_set\_recursive</a> | 设置是否递归查找全部子控件。 |
| <a href="#scroll_view_t_scroll_view_set_recursive_only">scroll\_view\_set\_recursive\_only</a> | 设置是否递归查找全部子控件。(不触发repaint和relayout)。 |
| <a href="#scroll_view_t_scroll_view_set_slide_limit_ratio">scroll\_view\_set\_slide\_limit\_ratio</a> | 设置滑动到极限时可继续滑动区域的占比。 |
| <a href="#scroll_view_t_scroll_view_set_snap_to_page">scroll\_view\_set\_snap\_to\_page</a> | 设置滚动时offset是否按页面对齐。 |
| <a href="#scroll_view_t_scroll_view_set_speed_scale">scroll\_view\_set\_speed\_scale</a> | 设置偏移速度比例。 |
| <a href="#scroll_view_t_scroll_view_set_virtual_h">scroll\_view\_set\_virtual\_h</a> | 设置虚拟高度。 |
| <a href="#scroll_view_t_scroll_view_set_virtual_w">scroll\_view\_set\_virtual\_w</a> | 设置虚拟宽度。 |
| <a href="#scroll_view_t_scroll_view_set_xslidable">scroll\_view\_set\_xslidable</a> | 设置是否允许x方向滑动。 |
| <a href="#scroll_view_t_scroll_view_set_yslidable">scroll\_view\_set\_yslidable</a> | 设置是否允许y方向滑动。 |
### 属性
<p id="scroll_view_t_properties">

| 属性名称 | 类型 | 说明 | 
| -------- | ----- | ------------ | 
| <a href="#scroll_view_t_move_to_page">move\_to\_page</a> | bool\_t | 是否每次翻一页（当 move_to_page 为ture 的时候才有效果，主要用于区分一次翻一页还是一次翻多页）。 |
| <a href="#scroll_view_t_recursive">recursive</a> | bool\_t | 是否递归查找全部子控件。 |
| <a href="#scroll_view_t_slide_limit_ratio">slide\_limit\_ratio</a> | float\_t | 滑动到极限时可继续滑动区域的占比。 |
| <a href="#scroll_view_t_snap_to_page">snap\_to\_page</a> | bool\_t | 滚动时offset是否按页面对齐。 |
| <a href="#scroll_view_t_virtual_h">virtual\_h</a> | wh\_t | 虚拟高度。 |
| <a href="#scroll_view_t_virtual_w">virtual\_w</a> | wh\_t | 虚拟宽度。 |
| <a href="#scroll_view_t_xoffset">xoffset</a> | int32\_t | x偏移量。 |
| <a href="#scroll_view_t_xslidable">xslidable</a> | bool\_t | 是否允许x方向滑动。 |
| <a href="#scroll_view_t_xspeed_scale">xspeed\_scale</a> | float\_t | x偏移速度比例。 |
| <a href="#scroll_view_t_yoffset">yoffset</a> | int32\_t | y偏移量。 |
| <a href="#scroll_view_t_yslidable">yslidable</a> | bool\_t | 是否允许y方向滑动。 |
| <a href="#scroll_view_t_yspeed_scale">yspeed\_scale</a> | float\_t | y偏移速度比例。 |
### 事件
<p id="scroll_view_t_events">

| 事件名称 | 类型  | 说明 | 
| -------- | ----- | ------- | 
| EVT\_SCROLL\_START | event\_t | 开始滚动事件。 |
| EVT\_SCROLL\_END | event\_t | 结束滚动事件。 |
| EVT\_SCROLL | event\_t | 滚动事件。 |
| EVT\_PAGE\_CHANGED | event\_t | 页面改变事件。 |
| EVT\_PAGE\_CHANGING | event\_t | 页面正在改变。 |
#### scroll\_view\_cast 函数
-----------------------

* 函数功能：

> <p id="scroll_view_t_scroll_view_cast">转换为scroll_view对象(供脚本语言使用)。

* 函数原型：

```
widget_t* scroll_view_cast (widget_t* widget);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | widget\_t* | scroll\_view对象。 |
| widget | widget\_t* | scroll\_view对象。 |
#### scroll\_view\_create 函数
-----------------------

* 函数功能：

> <p id="scroll_view_t_scroll_view_create">创建scroll_view对象

* 函数原型：

```
widget_t* scroll_view_create (widget_t* parent, xy_t x, xy_t y, wh_t w, wh_t h);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | widget\_t* | 对象。 |
| parent | widget\_t* | 父控件 |
| x | xy\_t | x坐标 |
| y | xy\_t | y坐标 |
| w | wh\_t | 宽度 |
| h | wh\_t | 高度 |
#### scroll\_view\_get\_widget\_vtable 函数
-----------------------

* 函数功能：

> <p id="scroll_view_t_scroll_view_get_widget_vtable">获取 scroll_view 虚表。

* 函数原型：

```
const widget_vtable_t* scroll_view_get_widget_vtable ();
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | const widget\_vtable\_t* | 成功返回 scroll\_view 虚表。 |
#### scroll\_view\_scroll\_delta\_to 函数
-----------------------

* 函数功能：

> <p id="scroll_view_t_scroll_view_scroll_delta_to">滚动到指定的偏移量。

* 函数原型：

```
ret_t scroll_view_scroll_delta_to (widget_t* widget, int32_t xoffset_delta, int32_t yoffset_delta, int32_t duration);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| widget | widget\_t* | 控件对象。 |
| xoffset\_delta | int32\_t | x偏移量。 |
| yoffset\_delta | int32\_t | y偏移量。 |
| duration | int32\_t | 时间。 |
#### scroll\_view\_scroll\_to 函数
-----------------------

* 函数功能：

> <p id="scroll_view_t_scroll_view_scroll_to">滚动到指定的偏移量。

* 函数原型：

```
ret_t scroll_view_scroll_to (widget_t* widget, int32_t xoffset_end, int32_t yoffset_end, int32_t duration);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| widget | widget\_t* | 控件对象。 |
| xoffset\_end | int32\_t | x偏移量。 |
| yoffset\_end | int32\_t | y偏移量。 |
| duration | int32\_t | 时间。 |
#### scroll\_view\_set\_move\_to\_page 函数
-----------------------

* 函数功能：

> <p id="scroll_view_t_scroll_view_set_move_to_page">设置滚动时是否每次翻一页
备注：当 snap_to_page 为ture 的时候才有效果，主要用于区分一次翻一页还是一次翻多页。

* 函数原型：

```
ret_t scroll_view_set_move_to_page (widget_t* widget, bool_t move_to_page);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| widget | widget\_t* | 控件对象。 |
| move\_to\_page | bool\_t | 是否每次翻一页。 |
#### scroll\_view\_set\_offset 函数
-----------------------

* 函数功能：

> <p id="scroll_view_t_scroll_view_set_offset">设置偏移量。

* 函数原型：

```
ret_t scroll_view_set_offset (widget_t* widget, int32_t xoffset, int32_t yoffset);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| widget | widget\_t* | 控件对象。 |
| xoffset | int32\_t | x偏移量。 |
| yoffset | int32\_t | y偏移量。 |
#### scroll\_view\_set\_recursive 函数
-----------------------

* 函数功能：

> <p id="scroll_view_t_scroll_view_set_recursive">设置是否递归查找全部子控件。

* 函数原型：

```
ret_t scroll_view_set_recursive (widget_t* widget, bool_t recursive);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| widget | widget\_t* | 控件对象。 |
| recursive | bool\_t | 是否递归查找全部子控件。 |
#### scroll\_view\_set\_recursive\_only 函数
-----------------------

* 函数功能：

> <p id="scroll_view_t_scroll_view_set_recursive_only">设置是否递归查找全部子控件。(不触发repaint和relayout)。

* 函数原型：

```
ret_t scroll_view_set_recursive_only (widget_t* widget, bool_t recursive);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| widget | widget\_t* | 控件对象。 |
| recursive | bool\_t | 是否递归查找全部子控件。 |
#### scroll\_view\_set\_slide\_limit\_ratio 函数
-----------------------

* 函数功能：

> <p id="scroll_view_t_scroll_view_set_slide_limit_ratio">设置滑动到极限时可继续滑动区域的占比。

* 函数原型：

```
ret_t scroll_view_set_slide_limit_ratio (widget_t* widget, float_t slide_limit_ratio);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| widget | widget\_t* | 控件对象。 |
| slide\_limit\_ratio | float\_t | 滑动到极限时可继续滑动区域的占比。 |
#### scroll\_view\_set\_snap\_to\_page 函数
-----------------------

* 函数功能：

> <p id="scroll_view_t_scroll_view_set_snap_to_page">设置滚动时offset是否按页面对齐。

* 函数原型：

```
ret_t scroll_view_set_snap_to_page (widget_t* widget, bool_t snap_to_page);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| widget | widget\_t* | 控件对象。 |
| snap\_to\_page | bool\_t | 是否按页面对齐。 |
#### scroll\_view\_set\_speed\_scale 函数
-----------------------

* 函数功能：

> <p id="scroll_view_t_scroll_view_set_speed_scale">设置偏移速度比例。

* 函数原型：

```
ret_t scroll_view_set_speed_scale (widget_t* widget, float_t xspeed_scale, float_t yspeed_scale);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| widget | widget\_t* | 控件对象。 |
| xspeed\_scale | float\_t | x偏移速度比例。 |
| yspeed\_scale | float\_t | y偏移速度比例。 |
#### scroll\_view\_set\_virtual\_h 函数
-----------------------

* 函数功能：

> <p id="scroll_view_t_scroll_view_set_virtual_h">设置虚拟高度。

* 函数原型：

```
ret_t scroll_view_set_virtual_h (widget_t* widget, wh_t h);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| widget | widget\_t* | 控件对象。 |
| h | wh\_t | 虚拟高度。 |
#### scroll\_view\_set\_virtual\_w 函数
-----------------------

* 函数功能：

> <p id="scroll_view_t_scroll_view_set_virtual_w">设置虚拟宽度。

* 函数原型：

```
ret_t scroll_view_set_virtual_w (widget_t* widget, wh_t w);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| widget | widget\_t* | 控件对象。 |
| w | wh\_t | 虚拟宽度。 |
#### scroll\_view\_set\_xslidable 函数
-----------------------

* 函数功能：

> <p id="scroll_view_t_scroll_view_set_xslidable">设置是否允许x方向滑动。

* 函数原型：

```
ret_t scroll_view_set_xslidable (widget_t* widget, bool_t xslidable);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| widget | widget\_t* | 控件对象。 |
| xslidable | bool\_t | 是否允许滑动。 |
#### scroll\_view\_set\_yslidable 函数
-----------------------

* 函数功能：

> <p id="scroll_view_t_scroll_view_set_yslidable">设置是否允许y方向滑动。

* 函数原型：

```
ret_t scroll_view_set_yslidable (widget_t* widget, bool_t yslidable);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| widget | widget\_t* | 控件对象。 |
| yslidable | bool\_t | 是否允许滑动。 |
#### move\_to\_page 属性
-----------------------
> <p id="scroll_view_t_move_to_page">是否每次翻一页（当 move_to_page 为ture 的时候才有效果，主要用于区分一次翻一页还是一次翻多页）。

* 类型：bool\_t

| 特性 | 是否支持 |
| -------- | ----- |
| 可直接读取 | 是 |
| 可直接修改 | 否 |
| 可持久化   | 是 |
| 可脚本化   | 是 |
| 可在IDE中设置 | 是 |
| 可在XML中设置 | 是 |
| 可通过widget\_get\_prop读取 | 是 |
| 可通过widget\_set\_prop修改 | 是 |
#### recursive 属性
-----------------------
> <p id="scroll_view_t_recursive">是否递归查找全部子控件。

* 类型：bool\_t

| 特性 | 是否支持 |
| -------- | ----- |
| 可直接读取 | 是 |
| 可直接修改 | 否 |
| 可持久化   | 是 |
| 可脚本化   | 是 |
| 可在IDE中设置 | 是 |
| 可在XML中设置 | 是 |
| 可通过widget\_get\_prop读取 | 是 |
| 可通过widget\_set\_prop修改 | 是 |
#### slide\_limit\_ratio 属性
-----------------------
> <p id="scroll_view_t_slide_limit_ratio">滑动到极限时可继续滑动区域的占比。

* 类型：float\_t

| 特性 | 是否支持 |
| -------- | ----- |
| 可直接读取 | 是 |
| 可直接修改 | 否 |
| 可持久化   | 是 |
| 可脚本化   | 是 |
| 可在IDE中设置 | 是 |
| 可在XML中设置 | 是 |
| 可通过widget\_get\_prop读取 | 是 |
| 可通过widget\_set\_prop修改 | 是 |
#### snap\_to\_page 属性
-----------------------
> <p id="scroll_view_t_snap_to_page">滚动时offset是否按页面对齐。

* 类型：bool\_t

| 特性 | 是否支持 |
| -------- | ----- |
| 可直接读取 | 是 |
| 可直接修改 | 否 |
| 可持久化   | 是 |
| 可脚本化   | 是 |
| 可在IDE中设置 | 是 |
| 可在XML中设置 | 是 |
| 可通过widget\_get\_prop读取 | 是 |
| 可通过widget\_set\_prop修改 | 是 |
#### virtual\_h 属性
-----------------------
> <p id="scroll_view_t_virtual_h">虚拟高度。

* 类型：wh\_t

| 特性 | 是否支持 |
| -------- | ----- |
| 可直接读取 | 是 |
| 可直接修改 | 否 |
| 可持久化   | 是 |
| 可脚本化   | 是 |
| 可在IDE中设置 | 是 |
| 可在XML中设置 | 是 |
| 可通过widget\_get\_prop读取 | 是 |
| 可通过widget\_set\_prop修改 | 是 |
#### virtual\_w 属性
-----------------------
> <p id="scroll_view_t_virtual_w">虚拟宽度。

* 类型：wh\_t

| 特性 | 是否支持 |
| -------- | ----- |
| 可直接读取 | 是 |
| 可直接修改 | 否 |
| 可持久化   | 是 |
| 可脚本化   | 是 |
| 可在IDE中设置 | 是 |
| 可在XML中设置 | 是 |
| 可通过widget\_get\_prop读取 | 是 |
| 可通过widget\_set\_prop修改 | 是 |
#### xoffset 属性
-----------------------
> <p id="scroll_view_t_xoffset">x偏移量。

* 类型：int32\_t

| 特性 | 是否支持 |
| -------- | ----- |
| 可直接读取 | 是 |
| 可直接修改 | 否 |
| 可持久化   | 是 |
| 可脚本化   | 是 |
| 可在IDE中设置 | 是 |
| 可在XML中设置 | 是 |
| 可通过widget\_get\_prop读取 | 是 |
| 可通过widget\_set\_prop修改 | 是 |
#### xslidable 属性
-----------------------
> <p id="scroll_view_t_xslidable">是否允许x方向滑动。

* 类型：bool\_t

| 特性 | 是否支持 |
| -------- | ----- |
| 可直接读取 | 是 |
| 可直接修改 | 否 |
| 可持久化   | 是 |
| 可脚本化   | 是 |
| 可在IDE中设置 | 是 |
| 可在XML中设置 | 是 |
| 可通过widget\_get\_prop读取 | 是 |
| 可通过widget\_set\_prop修改 | 是 |
#### xspeed\_scale 属性
-----------------------
> <p id="scroll_view_t_xspeed_scale">x偏移速度比例。

* 类型：float\_t

| 特性 | 是否支持 |
| -------- | ----- |
| 可直接读取 | 是 |
| 可直接修改 | 否 |
| 可持久化   | 是 |
| 可脚本化   | 是 |
| 可在IDE中设置 | 是 |
| 可在XML中设置 | 是 |
| 可通过widget\_get\_prop读取 | 是 |
| 可通过widget\_set\_prop修改 | 是 |
#### yoffset 属性
-----------------------
> <p id="scroll_view_t_yoffset">y偏移量。

* 类型：int32\_t

| 特性 | 是否支持 |
| -------- | ----- |
| 可直接读取 | 是 |
| 可直接修改 | 否 |
| 可持久化   | 是 |
| 可脚本化   | 是 |
| 可在IDE中设置 | 是 |
| 可在XML中设置 | 是 |
| 可通过widget\_get\_prop读取 | 是 |
| 可通过widget\_set\_prop修改 | 是 |
#### yslidable 属性
-----------------------
> <p id="scroll_view_t_yslidable">是否允许y方向滑动。

* 类型：bool\_t

| 特性 | 是否支持 |
| -------- | ----- |
| 可直接读取 | 是 |
| 可直接修改 | 否 |
| 可持久化   | 是 |
| 可脚本化   | 是 |
| 可在IDE中设置 | 是 |
| 可在XML中设置 | 是 |
| 可通过widget\_get\_prop读取 | 是 |
| 可通过widget\_set\_prop修改 | 是 |
#### yspeed\_scale 属性
-----------------------
> <p id="scroll_view_t_yspeed_scale">y偏移速度比例。

* 类型：float\_t

| 特性 | 是否支持 |
| -------- | ----- |
| 可直接读取 | 是 |
| 可直接修改 | 否 |
| 可持久化   | 是 |
| 可脚本化   | 是 |
| 可在IDE中设置 | 是 |
| 可在XML中设置 | 是 |
| 可通过widget\_get\_prop读取 | 是 |
| 可通过widget\_set\_prop修改 | 是 |

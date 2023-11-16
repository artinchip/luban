## window\_manager\_t
### 概述
![image](images/window_manager_t_0.png)

窗口管理器。
----------------------------------
### 函数
<p id="window_manager_t_methods">

| 函数名称 | 说明 | 
| -------- | ------------ | 
| <a href="#window_manager_t_window_manager">window\_manager</a> | 获取全局window_manager对象 |
| <a href="#window_manager_t_window_manager_back">window\_manager\_back</a> | 请求关闭顶层窗口。 |
| <a href="#window_manager_t_window_manager_back_to">window\_manager\_back\_to</a> | 回到指定的窗口，关闭之上的全部窗口。 |
| <a href="#window_manager_t_window_manager_back_to_home">window\_manager\_back\_to\_home</a> | 回到主窗口，关闭之上的全部窗口。 |
| <a href="#window_manager_t_window_manager_begin_wait_pointer_cursor">window\_manager\_begin\_wait\_pointer\_cursor</a> | 开始等待鼠标指针。 |
| <a href="#window_manager_t_window_manager_cast">window\_manager\_cast</a> | 转换为window_manager对象(供脚本语言使用)。 |
| <a href="#window_manager_t_window_manager_close_all">window\_manager\_close\_all</a> | 关闭全部窗口。 |
| <a href="#window_manager_t_window_manager_close_window_force">window\_manager\_close\_window\_force</a> | 强制立即关闭窗口。 |
| <a href="#window_manager_t_window_manager_destroy">window\_manager\_destroy</a> | 销毁window_manager。 |
| <a href="#window_manager_t_window_manager_dispatch_input_event">window\_manager\_dispatch\_input\_event</a> | 分发输入事件。 |
| <a href="#window_manager_t_window_manager_dispatch_native_window_event">window\_manager\_dispatch\_native\_window\_event</a> | 处理native window事件。 |
| <a href="#window_manager_t_window_manager_end_wait_pointer_cursor">window\_manager\_end\_wait\_pointer\_cursor</a> | 结束等待鼠标指针。 |
| <a href="#window_manager_t_window_manager_get_pointer_pressed">window\_manager\_get\_pointer\_pressed</a> | 获取指针当前是否按下。 |
| <a href="#window_manager_t_window_manager_get_pointer_x">window\_manager\_get\_pointer\_x</a> | 获取指针当前的X坐标。 |
| <a href="#window_manager_t_window_manager_get_pointer_y">window\_manager\_get\_pointer\_y</a> | 获取指针当前的Y坐标。 |
| <a href="#window_manager_t_window_manager_get_prev_window">window\_manager\_get\_prev\_window</a> | 获取前一个的窗口。 |
| <a href="#window_manager_t_window_manager_get_top_main_window">window\_manager\_get\_top\_main\_window</a> | 获取最上面的主窗口。 |
| <a href="#window_manager_t_window_manager_get_top_window">window\_manager\_get\_top\_window</a> | 获取最上面的窗口。 |
| <a href="#window_manager_t_window_manager_init">window\_manager\_init</a> | 初始化window_manager。 |
| <a href="#window_manager_t_window_manager_is_animating">window\_manager\_is\_animating</a> | 获取当前窗口动画是否正在播放。 |
| <a href="#window_manager_t_window_manager_open_window">window\_manager\_open\_window</a> | 打开窗口。 |
| <a href="#window_manager_t_window_manager_paint">window\_manager\_paint</a> | 绘制。 |
| <a href="#window_manager_t_window_manager_resize">window\_manager\_resize</a> | 调整原生窗口的大小。 |
| <a href="#window_manager_t_window_manager_set">window\_manager\_set</a> | 设置缺省的窗口管理器。 |
| <a href="#window_manager_t_window_manager_set_cursor">window\_manager\_set\_cursor</a> | 设置鼠标指针。 |
| <a href="#window_manager_t_window_manager_set_ignore_input_events">window\_manager\_set\_ignore\_input\_events</a> | 设置是否忽略用户输入事件。 |
| <a href="#window_manager_t_window_manager_set_max_fps">window\_manager\_set\_max\_fps</a> | 限制最大帧率。 |
| <a href="#window_manager_t_window_manager_set_screen_saver_time">window\_manager\_set\_screen\_saver\_time</a> | 设置屏保时间。 |
| <a href="#window_manager_t_window_manager_set_show_fps">window\_manager\_set\_show\_fps</a> | 设置是否显示FPS。 |
| <a href="#window_manager_t_window_manager_switch_to">window\_manager\_switch\_to</a> | 切换到指定窗口。 |
### 属性
<p id="window_manager_t_properties">

| 属性名称 | 类型 | 说明 | 
| -------- | ----- | ------------ | 
| <a href="#window_manager_t_global_emitter">global\_emitter</a> | emitter\_t* | 全局事情分发器。 |
### 事件
<p id="window_manager_t_events">

| 事件名称 | 类型  | 说明 | 
| -------- | ----- | ------- | 
| EVT\_TOP\_WINDOW\_CHANGED | window\_event\_t | 顶层窗口改变的事件。 |
| EVT\_SCREEN\_SAVER | window\_event\_t | 在指定的时间内，没有用户输入事件，由窗口管理器触发。 |
| EVT\_ORIENTATION\_CHANGED | event\_t | 屏幕旋转事件。 |
| EVT\_SYSTEM | system\_event\_t | SDL系统事件。 |
#### window\_manager 函数
-----------------------

* 函数功能：

> <p id="window_manager_t_window_manager">获取全局window_manager对象

* 函数原型：

```
widget_t* window_manager ();
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | widget\_t* | 对象。 |
#### window\_manager\_back 函数
-----------------------

* 函数功能：

> <p id="window_manager_t_window_manager_back">请求关闭顶层窗口。

> 如果顶层窗口时模态对话框，用DIALOG\_QUIT\_NONE调用dialog\_quit。

* 函数原型：

```
ret_t window_manager_back (widget_t* widget);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| widget | widget\_t* | 窗口管理器对象。 |
#### window\_manager\_back\_to 函数
-----------------------

* 函数功能：

> <p id="window_manager_t_window_manager_back_to">回到指定的窗口，关闭之上的全部窗口。

> 如果顶层窗口时模态对话框，用DIALOG\_QUIT\_NONE调用dialog\_quit。

* 函数原型：

```
ret_t window_manager_back_to (widget_t* widget, const char* target);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| widget | widget\_t* | 窗口管理器对象。 |
| target | const char* | 目标窗口的名称。 |
#### window\_manager\_back\_to\_home 函数
-----------------------

* 函数功能：

> <p id="window_manager_t_window_manager_back_to_home">回到主窗口，关闭之上的全部窗口。

> 如果顶层窗口时模态对话框，用DIALOG\_QUIT\_NONE调用dialog\_quit。

* 函数原型：

```
ret_t window_manager_back_to_home (widget_t* widget);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| widget | widget\_t* | 窗口管理器对象。 |
#### window\_manager\_begin\_wait\_pointer\_cursor 函数
-----------------------

* 函数功能：

> <p id="window_manager_t_window_manager_begin_wait_pointer_cursor">开始等待鼠标指针。

* 函数原型：

```
ret_t window_manager_begin_wait_pointer_cursor (widget_t* widget, bool_t ignore_user_input);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。。 |
| widget | widget\_t* | 窗口管理器对象。 |
| ignore\_user\_input | bool\_t | 是否忽略用户输入。 |
#### window\_manager\_cast 函数
-----------------------

* 函数功能：

> <p id="window_manager_t_window_manager_cast">转换为window_manager对象(供脚本语言使用)。

* 函数原型：

```
widget_t* window_manager_cast (widget_t* widget);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | widget\_t* | window\_manager对象。 |
| widget | widget\_t* | window\_manager对象。 |
#### window\_manager\_close\_all 函数
-----------------------

* 函数功能：

> <p id="window_manager_t_window_manager_close_all">关闭全部窗口。

* 函数原型：

```
ret_t window_manager_close_all (widget_t* widget);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| widget | widget\_t* | 窗口管理器对象。 |
#### window\_manager\_close\_window\_force 函数
-----------------------

* 函数功能：

> <p id="window_manager_t_window_manager_close_window_force">强制立即关闭窗口。

> 本函数不会执行窗口动画。

* 函数原型：

```
ret_t window_manager_close_window_force (widget_t* widget, widget_t* window);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| widget | widget\_t* | 窗口管理器对象。 |
| window | widget\_t* | 窗口对象。 |
#### window\_manager\_destroy 函数
-----------------------

* 函数功能：

> <p id="window_manager_t_window_manager_destroy">销毁window_manager。

* 函数原型：

```
ret_t window_manager_destroy (widget_t* widget);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| widget | widget\_t* | window\_manager对象。 |
#### window\_manager\_dispatch\_input\_event 函数
-----------------------

* 函数功能：

> <p id="window_manager_t_window_manager_dispatch_input_event">分发输入事件。

> 一般仅由主循环调用，特殊情况也可以用来注入事件。

* 函数原型：

```
ret_t window_manager_dispatch_input_event (widget_t* widget, event_t* e);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| widget | widget\_t* | 窗口管理器对象。 |
| e | event\_t* | 事件对象。 |
#### window\_manager\_dispatch\_native\_window\_event 函数
-----------------------

* 函数功能：

> <p id="window_manager_t_window_manager_dispatch_native_window_event">处理native window事件。

* 函数原型：

```
ret_t window_manager_dispatch_native_window_event (widget_t* widget, event_t* e, void* handle);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| widget | widget\_t* | 窗口管理器对象。 |
| e | event\_t* | 事件。 |
| handle | void* | native window句柄。 |
#### window\_manager\_end\_wait\_pointer\_cursor 函数
-----------------------

* 函数功能：

> <p id="window_manager_t_window_manager_end_wait_pointer_cursor">结束等待鼠标指针。

* 函数原型：

```
ret_t window_manager_end_wait_pointer_cursor (widget_t* widget);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。。 |
| widget | widget\_t* | 窗口管理器对象。 |
#### window\_manager\_get\_pointer\_pressed 函数
-----------------------

* 函数功能：

> <p id="window_manager_t_window_manager_get_pointer_pressed">获取指针当前是否按下。

* 函数原型：

```
bool_t window_manager_get_pointer_pressed (widget_t* widget);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | bool\_t | 返回指针当前是否按下。 |
| widget | widget\_t* | 窗口管理器对象。 |
#### window\_manager\_get\_pointer\_x 函数
-----------------------

* 函数功能：

> <p id="window_manager_t_window_manager_get_pointer_x">获取指针当前的X坐标。

* 函数原型：

```
xy_t window_manager_get_pointer_x (widget_t* widget);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | xy\_t | 返回指针当前的X坐标。 |
| widget | widget\_t* | 窗口管理器对象。 |
#### window\_manager\_get\_pointer\_y 函数
-----------------------

* 函数功能：

> <p id="window_manager_t_window_manager_get_pointer_y">获取指针当前的Y坐标。

* 函数原型：

```
xy_t window_manager_get_pointer_y (widget_t* widget);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | xy\_t | 返回指针当前的X坐标。 |
| widget | widget\_t* | 窗口管理器对象。 |
#### window\_manager\_get\_prev\_window 函数
-----------------------

* 函数功能：

> <p id="window_manager_t_window_manager_get_prev_window">获取前一个的窗口。

* 函数原型：

```
widget_t* window_manager_get_prev_window (widget_t* widget);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | widget\_t* | 返回窗口对象。 |
| widget | widget\_t* | 窗口管理器对象。 |
#### window\_manager\_get\_top\_main\_window 函数
-----------------------

* 函数功能：

> <p id="window_manager_t_window_manager_get_top_main_window">获取最上面的主窗口。

* 函数原型：

```
widget_t* window_manager_get_top_main_window (widget_t* widget);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | widget\_t* | 返回窗口对象。 |
| widget | widget\_t* | 窗口管理器对象。 |
#### window\_manager\_get\_top\_window 函数
-----------------------

* 函数功能：

> <p id="window_manager_t_window_manager_get_top_window">获取最上面的窗口。

* 函数原型：

```
widget_t* window_manager_get_top_window (widget_t* widget);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | widget\_t* | 返回窗口对象。 |
| widget | widget\_t* | 窗口管理器对象。 |
#### window\_manager\_init 函数
-----------------------

* 函数功能：

> <p id="window_manager_t_window_manager_init">初始化window_manager。

* 函数原型：

```
widget_t* window_manager_init (window_manager_t* wm, const widget_vtable_t* wvt, const window_manager_vtable_t* vt);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | widget\_t* | 返回window\_manager对象。 |
| wm | window\_manager\_t* | window\_manager对象。 |
| wvt | const widget\_vtable\_t* | 控件基类虚表。 |
| vt | const window\_manager\_vtable\_t* | window\_manager虚表。 |
#### window\_manager\_is\_animating 函数
-----------------------

* 函数功能：

> <p id="window_manager_t_window_manager_is_animating">获取当前窗口动画是否正在播放。

* 函数原型：

```
bool_t window_manager_is_animating (widget_t* widget);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | bool\_t | 返回TRUE表示正在播放，FALSE表示没有播放。 |
| widget | widget\_t* | 窗口管理器对象。 |
#### window\_manager\_open\_window 函数
-----------------------

* 函数功能：

> <p id="window_manager_t_window_manager_open_window">打开窗口。

* 函数原型：

```
ret_t window_manager_open_window (widget_t* widget, widget_t* window);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| widget | widget\_t* | 窗口管理器对象。 |
| window | widget\_t* | 窗口对象。 |
#### window\_manager\_paint 函数
-----------------------

* 函数功能：

> <p id="window_manager_t_window_manager_paint">绘制。

> 仅由主循环调用。

* 函数原型：

```
ret_t window_manager_paint (widget_t* widget);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| widget | widget\_t* | 窗口管理器对象。 |
#### window\_manager\_resize 函数
-----------------------

* 函数功能：

> <p id="window_manager_t_window_manager_resize">调整原生窗口的大小。

* 函数原型：

```
ret_t window_manager_resize (widget_t* widget, wh_t w, wh_t h);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| widget | widget\_t* | 窗口管理器对象。 |
| w | wh\_t | 宽度 |
| h | wh\_t | 高度 |
#### window\_manager\_set 函数
-----------------------

* 函数功能：

> <p id="window_manager_t_window_manager_set">设置缺省的窗口管理器。

* 函数原型：

```
ret_t window_manager_set (widget_t* widget);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| widget | widget\_t* | 窗口管理器对象。 |
#### window\_manager\_set\_cursor 函数
-----------------------

* 函数功能：

> <p id="window_manager_t_window_manager_set_cursor">设置鼠标指针。

* 函数原型：

```
ret_t window_manager_set_cursor (widget_t* widget, const char* cursor);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| widget | widget\_t* | 窗口管理器对象。 |
| cursor | const char* | 图片名称(从图片管理器中加载)。 |
#### window\_manager\_set\_ignore\_input\_events 函数
-----------------------

* 函数功能：

> <p id="window_manager_t_window_manager_set_ignore_input_events">设置是否忽略用户输入事件。

* 函数原型：

```
ret_t window_manager_set_ignore_input_events (widget_t* widget, bool_t ignore_input_events);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| widget | widget\_t* | 窗口管理器对象。 |
| ignore\_input\_events | bool\_t | 是否忽略用户输入事件。 |
#### window\_manager\_set\_max\_fps 函数
-----------------------

* 函数功能：

> <p id="window_manager_t_window_manager_set_max_fps">限制最大帧率。

> TK\_MAX\_LOOP\_FPS/max\_fps最好是整数，比如TK\_MAX\_LOOP\_FPS为120，max\_fps可取60/30/20/10等。

* 函数原型：

```
ret_t window_manager_set_max_fps (widget_t* widget, uint32_t max_fps);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| widget | widget\_t* | 窗口管理器对象。 |
| max\_fps | uint32\_t | 最大帧率。 |
#### window\_manager\_set\_screen\_saver\_time 函数
-----------------------

* 函数功能：

> <p id="window_manager_t_window_manager_set_screen_saver_time">设置屏保时间。

* 函数原型：

```
ret_t window_manager_set_screen_saver_time (widget_t* widget, uint32_t screen_saver_time);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| widget | widget\_t* | 窗口管理器对象。 |
| screen\_saver\_time | uint32\_t | 屏保时间(单位毫秒), 为0关闭屏保。 |
#### window\_manager\_set\_show\_fps 函数
-----------------------

* 函数功能：

> <p id="window_manager_t_window_manager_set_show_fps">设置是否显示FPS。

* 函数原型：

```
ret_t window_manager_set_show_fps (widget_t* widget, bool_t show_fps);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| widget | widget\_t* | 窗口管理器对象。 |
| show\_fps | bool\_t | 是否显示FPS。 |
#### window\_manager\_switch\_to 函数
-----------------------

* 函数功能：

> <p id="window_manager_t_window_manager_switch_to">切换到指定窗口。

```c
window_manager_switch_to(wm, win, widget_child(wm, "home"), FALSE);
```

* 函数原型：

```
ret_t window_manager_switch_to (widget_t* widget, widget_t* curr_win, widget_t* target_win, bool_t close);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| widget | widget\_t* | 窗口管理器对象。 |
| curr\_win | widget\_t* | 当前窗口。 |
| target\_win | widget\_t* | 目标窗口(必须存在，可以用widget\_child函数到窗口管理器中查找)。 |
| close | bool\_t | 是否关闭当前窗口。 |
#### global\_emitter 属性
-----------------------
> <p id="window_manager_t_global_emitter">全局事情分发器。

* 类型：emitter\_t*

| 特性 | 是否支持 |
| -------- | ----- |
| 可直接读取 | 是 |
| 可直接修改 | 否 |

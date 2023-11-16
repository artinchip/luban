## timer\_t
### 概述
定时器系统。

> 本定时器精度较低，最高精度为1000/FPS，如果需要高精度的定时器，请用OS提供的定时器。

示例：

```c
static ret_t my_on_timer(const timer_info_t* info) {
widget_t* widget = WIDGET(info->ctx);
...
return RET_REPEAT;
}

...

timer_add(my_on_timer, widget, 1000);
```
> 在非GUI线程请用timer\_queue。
----------------------------------
### 函数
<p id="timer_t_methods">

| 函数名称 | 说明 | 
| -------- | ------------ | 
| <a href="#timer_t_timer_add">timer\_add</a> | 增加一个timer。 |
| <a href="#timer_t_timer_count">timer\_count</a> | 返回timer的个数。 |
| <a href="#timer_t_timer_find">timer\_find</a> | 查找指定ID的timer。 |
| <a href="#timer_t_timer_modify">timer\_modify</a> | 修改指定的timer的duration，修改之后定时器重新开始计时。 |
| <a href="#timer_t_timer_next_time">timer\_next\_time</a> | 返回最近的timer到期时间。 |
| <a href="#timer_t_timer_queue">timer\_queue</a> | 用于非GUI线程增加一个timer，本函数向主循环的事件队列中发送一个增加timer的请求。 |
| <a href="#timer_t_timer_queue_ex">timer\_queue\_ex</a> | 用于非GUI线程增加一个timer，本函数向主循环的事件队列中发送一个增加timer的请求。 |
| <a href="#timer_t_timer_remove">timer\_remove</a> | 删除指定的timer。 |
| <a href="#timer_t_timer_remove_all_by_ctx">timer\_remove\_all\_by\_ctx</a> | 根据上下文删除所有对应的timer。 |
| <a href="#timer_t_timer_reset">timer\_reset</a> | 重置指定的timer，重置之后定时器重新开始计时。 |
| <a href="#timer_t_timer_resume">timer\_resume</a> | 唤醒挂起指定的timer，并且重置定时器重新开始计时 |
| <a href="#timer_t_timer_set_on_destroy">timer\_set\_on\_destroy</a> | 设置一个回调函数，在timer被销毁时调用(方便脚本语言去释放回调函数)。 |
| <a href="#timer_t_timer_suspend">timer\_suspend</a> | 挂起指定的timer，一般用于不断循环触发的计时器。 |
#### timer\_add 函数
-----------------------

* 函数功能：

> <p id="timer_t_timer_add">增加一个timer。

* 函数原型：

```
uint32_t timer_add (timer_func_t on_timer, void* ctx, uint32_t duration);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | uint32\_t | 返回timer的ID，TK\_INVALID\_ID表示失败。 |
| on\_timer | timer\_func\_t | timer回调函数。 |
| ctx | void* | timer回调函数的上下文。 |
| duration | uint32\_t | 时间。 |
#### timer\_count 函数
-----------------------

* 函数功能：

> <p id="timer_t_timer_count">返回timer的个数。

* 函数原型：

```
uint32_t timer_count ();
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | uint32\_t | 返回timer的个数。 |
#### timer\_find 函数
-----------------------

* 函数功能：

> <p id="timer_t_timer_find">查找指定ID的timer。

* 函数原型：

```
const timer_info_t* timer_find (uint32_t timer_id);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | const timer\_info\_t* | 返回timer的信息。 |
| timer\_id | uint32\_t | timerID。 |
#### timer\_modify 函数
-----------------------

* 函数功能：

> <p id="timer_t_timer_modify">修改指定的timer的duration，修改之后定时器重新开始计时。

* 函数原型：

```
ret_t timer_modify (uint32_t timer_id, uint32_t duration);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| timer\_id | uint32\_t | timerID。 |
| duration | uint32\_t | 新的时间。 |
#### timer\_next\_time 函数
-----------------------

* 函数功能：

> <p id="timer_t_timer_next_time">返回最近的timer到期时间。

* 函数原型：

```
uint32_t timer_next_time ();
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | uint32\_t | 返回最近的timer到期时间。 |
#### timer\_queue 函数
-----------------------

* 函数功能：

> <p id="timer_t_timer_queue">用于非GUI线程增加一个timer，本函数向主循环的事件队列中发送一个增加timer的请求。

* 函数原型：

```
ret_t timer_queue (timer_func_t on_timer, void* ctx, uint32_t duration);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| on\_timer | timer\_func\_t | timer回调函数，回调函数返回RET\_REPEAT，则下次继续执行，否则自动移出。 |
| ctx | void* | timer回调函数的上下文。 |
| duration | uint32\_t | 时间。 |
#### timer\_queue\_ex 函数
-----------------------

* 函数功能：

> <p id="timer_t_timer_queue_ex">用于非GUI线程增加一个timer，本函数向主循环的事件队列中发送一个增加timer的请求。

* 函数原型：

```
ret_t timer_queue_ex (timer_func_t on_timer, void* ctx, uint32_t duration, tk_destroy_t on_destroy, void* on_destroy_ctx);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| on\_timer | timer\_func\_t | timer回调函数，回调函数返回RET\_REPEAT，则下次继续执行，否则自动移出。 |
| ctx | void* | timer回调函数的上下文。 |
| duration | uint32\_t | 时间。 |
| on\_destroy | tk\_destroy\_t | 回调函数。 |
| on\_destroy\_ctx | void* | 回调函数上下文。 |
#### timer\_remove 函数
-----------------------

* 函数功能：

> <p id="timer_t_timer_remove">删除指定的timer。

* 函数原型：

```
ret_t timer_remove (uint32_t timer_id);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| timer\_id | uint32\_t | timerID。 |
#### timer\_remove\_all\_by\_ctx 函数
-----------------------

* 函数功能：

> <p id="timer_t_timer_remove_all_by_ctx">根据上下文删除所有对应的timer。

* 函数原型：

```
ret_t timer_remove_all_by_ctx (void* ctx);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| ctx | void* | timer回调函数的上下文。 |
#### timer\_reset 函数
-----------------------

* 函数功能：

> <p id="timer_t_timer_reset">重置指定的timer，重置之后定时器重新开始计时。

* 函数原型：

```
ret_t timer_reset (uint32_t timer_id);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| timer\_id | uint32\_t | timerID。 |
#### timer\_resume 函数
-----------------------

* 函数功能：

> <p id="timer_t_timer_resume">唤醒挂起指定的timer，并且重置定时器重新开始计时

* 函数原型：

```
ret_t timer_resume (uint32_t timer_id);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| timer\_id | uint32\_t | timerID。 |
#### timer\_set\_on\_destroy 函数
-----------------------

* 函数功能：

> <p id="timer_t_timer_set_on_destroy">设置一个回调函数，在timer被销毁时调用(方便脚本语言去释放回调函数)。

* 函数原型：

```
ret_t timer_set_on_destroy (uint32_t timer_id, tk_destroy_t on_destroy, void* on_destroy_ctx);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| timer\_id | uint32\_t | timerID。 |
| on\_destroy | tk\_destroy\_t | 回调函数。 |
| on\_destroy\_ctx | void* | 回调函数上下文。 |
#### timer\_suspend 函数
-----------------------

* 函数功能：

> <p id="timer_t_timer_suspend">挂起指定的timer，一般用于不断循环触发的计时器。

* 函数原型：

```
ret_t timer_suspend (uint32_t timer_id);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| timer\_id | uint32\_t | timerID。 |

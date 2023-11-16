## event\_t
### 概述
事件基类。
----------------------------------
### 函数
<p id="event_t_methods">

| 函数名称 | 说明 | 
| -------- | ------------ | 
| <a href="#event_t_event_cast">event\_cast</a> | 转换为event对象。 |
| <a href="#event_t_event_clone">event\_clone</a> | clone事件对象。 |
| <a href="#event_t_event_create">event\_create</a> | 创建event对象。 |
| <a href="#event_t_event_destroy">event\_destroy</a> | 销毁事件对象。 |
| <a href="#event_t_event_from_name">event\_from\_name</a> | 将事件名转换成事件的值。 |
| <a href="#event_t_event_get_type">event\_get\_type</a> | 获取event类型。 |
| <a href="#event_t_event_init">event\_init</a> | 初始化事件。 |
### 属性
<p id="event_t_properties">

| 属性名称 | 类型 | 说明 | 
| -------- | ----- | ------------ | 
| <a href="#event_t_native_window_handle">native\_window\_handle</a> | void* | 原生窗口句柄。 |
| <a href="#event_t_size">size</a> | uint32\_t | 结构体的大小。 |
| <a href="#event_t_target">target</a> | void* | 事件发生的目标对象。 |
| <a href="#event_t_time">time</a> | uint64\_t | 事件发生的时间点（该时间点并非真实时间）。 |
| <a href="#event_t_type">type</a> | uint32\_t | 类型。 |
#### event\_cast 函数
-----------------------

* 函数功能：

> <p id="event_t_event_cast">转换为event对象。

> 供脚本语言使用

* 函数原型：

```
event_t* event_cast (event_t* event);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | event\_t* | event对象。 |
| event | event\_t* | event对象。 |
#### event\_clone 函数
-----------------------

* 函数功能：

> <p id="event_t_event_clone">clone事件对象。

* 函数原型：

```
event_t* event_clone (event_t* event);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | event\_t* | 返回事件对象。 |
| event | event\_t* | event对象。 |
#### event\_create 函数
-----------------------

* 函数功能：

> <p id="event_t_event_create">创建event对象。

主要给脚本语言使用。

* 函数原型：

```
event_t* event_create (uint32_t type);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | event\_t* | 返回事件对象。 |
| type | uint32\_t | 事件类型。 |
#### event\_destroy 函数
-----------------------

* 函数功能：

> <p id="event_t_event_destroy">销毁事件对象。

主要给脚本语言使用。

* 函数原型：

```
ret_t event_destroy (event_t* event);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| event | event\_t* | event对象。 |
#### event\_from\_name 函数
-----------------------

* 函数功能：

> <p id="event_t_event_from_name">将事件名转换成事件的值。

* 函数原型：

```
int32_t event_from_name (const char* name);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | int32\_t | 返回事件的值。 |
| name | const char* | 事件名。 |
#### event\_get\_type 函数
-----------------------

* 函数功能：

> <p id="event_t_event_get_type">获取event类型。

* 函数原型：

```
uint32_t event_get_type (event_t* event);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | uint32\_t | 返回event类型。 |
| event | event\_t* | event对象。 |
#### event\_init 函数
-----------------------

* 函数功能：

> <p id="event_t_event_init">初始化事件。

* 函数原型：

```
event_t event_init (uint32_t type, void* target);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | event\_t | 事件对象。 |
| type | uint32\_t | 事件类型。 |
| target | void* | 目标对象。 |
#### native\_window\_handle 属性
-----------------------
> <p id="event_t_native_window_handle">原生窗口句柄。

* 类型：void*

| 特性 | 是否支持 |
| -------- | ----- |
| 可直接读取 | 是 |
| 可直接修改 | 否 |
#### size 属性
-----------------------
> <p id="event_t_size">结构体的大小。

* 类型：uint32\_t

| 特性 | 是否支持 |
| -------- | ----- |
| 可直接读取 | 是 |
| 可直接修改 | 否 |
| 可脚本化   | 是 |
#### target 属性
-----------------------
> <p id="event_t_target">事件发生的目标对象。

* 类型：void*

| 特性 | 是否支持 |
| -------- | ----- |
| 可直接读取 | 是 |
| 可直接修改 | 否 |
| 可脚本化   | 是 |
#### time 属性
-----------------------
> <p id="event_t_time">事件发生的时间点（该时间点并非真实时间）。

* 类型：uint64\_t

| 特性 | 是否支持 |
| -------- | ----- |
| 可直接读取 | 是 |
| 可直接修改 | 否 |
| 可脚本化   | 是 |
#### type 属性
-----------------------
> <p id="event_t_type">类型。

* 类型：uint32\_t

| 特性 | 是否支持 |
| -------- | ----- |
| 可直接读取 | 是 |
| 可直接修改 | 否 |
| 可脚本化   | 是 |

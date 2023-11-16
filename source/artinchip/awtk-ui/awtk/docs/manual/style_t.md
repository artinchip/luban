## style\_t
### 概述
控件风格。

widget从style对象中，获取诸如字体、颜色和图片相关的参数，根据这些参数来绘制界面。

```c
style_t* style = widget->astyle;
int32_t margin = style_get_int(style, STYLE_ID_MARGIN, 2);
int32_t icon_at = style_get_int(style, STYLE_ID_ICON_AT, ICON_AT_AUTO);
uint16_t font_size = style_get_int(style, STYLE_ID_FONT_SIZE, TK_DEFAULT_FONT_SIZE);
```

属性名称的请参考[style\_id](style_id_t.md)
----------------------------------
### 函数
<p id="style_t_methods">

| 函数名称 | 说明 | 
| -------- | ------------ | 
| <a href="#style_t_style_destroy">style\_destroy</a> | 销毁style对象 |
| <a href="#style_t_style_get">style\_get</a> | 获取指定状态的指定属性的值。 |
| <a href="#style_t_style_get_color">style\_get\_color</a> | 获取指定name的颜色值。 |
| <a href="#style_t_style_get_gradient">style\_get\_gradient</a> | 获取指定name的渐变颜色值。 |
| <a href="#style_t_style_get_int">style\_get\_int</a> | 获取指定name的整数格式的值。 |
| <a href="#style_t_style_get_str">style\_get\_str</a> | 获取指定name的字符串格式的值。 |
| <a href="#style_t_style_get_style_state">style\_get\_style\_state</a> | 获取风格对象的风格状态 |
| <a href="#style_t_style_get_style_type">style\_get\_style\_type</a> | 获取 style 的风格类型。 |
| <a href="#style_t_style_get_uint">style\_get\_uint</a> | 获取指定name的无符号整数格式的值。 |
| <a href="#style_t_style_is_mutable">style\_is\_mutable</a> | 检查style是否是mutable的。 |
| <a href="#style_t_style_is_valid">style\_is\_valid</a> | 检查style对象是否有效 |
| <a href="#style_t_style_normalize_value">style\_normalize\_value</a> | 对值进行正规化。 |
| <a href="#style_t_style_notify_widget_state_changed">style\_notify\_widget\_state\_changed</a> | widget状态改变时，通知style更新数据。 |
| <a href="#style_t_style_set">style\_set</a> | 设置指定状态的指定属性的值(仅仅对mutable的style有效)。 |
| <a href="#style_t_style_set_style_data">style\_set\_style\_data</a> | 把风格对象数据设置到风格对象中 |
| <a href="#style_t_style_update_state">style\_update\_state</a> | 更新风格对象的状态以及对应的数据 |
#### style\_destroy 函数
-----------------------

* 函数功能：

> <p id="style_t_style_destroy">销毁style对象

* 函数原型：

```
ret_t style_destroy (style_t* s);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| s | style\_t* | style对象。 |
#### style\_get 函数
-----------------------

* 函数功能：

> <p id="style_t_style_get">获取指定状态的指定属性的值。

* 函数原型：

```
ret_t style_get (style_t* s, const char* state, const char* name, value_t* value);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| s | style\_t* | style对象。 |
| state | const char* | 状态。 |
| name | const char* | 属性名。 |
| value | value\_t* | 值。 |
#### style\_get\_color 函数
-----------------------

* 函数功能：

> <p id="style_t_style_get_color">获取指定name的颜色值。

* 函数原型：

```
color_t style_get_color (style_t* s, const char* name, color_t defval);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | color\_t | 返回颜色值。 |
| s | style\_t* | style对象。 |
| name | const char* | 属性名。 |
| defval | color\_t | 缺省值。 |
#### style\_get\_gradient 函数
-----------------------

* 函数功能：

> <p id="style_t_style_get_gradient">获取指定name的渐变颜色值。

* 函数原型：

```
gradient_t* style_get_gradient (style_t* s, const char* name, gradient_t* gradient);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | gradient\_t* | 返回渐变颜色值。 |
| s | style\_t* | style对象。 |
| name | const char* | 属性名。 |
| gradient | gradient\_t* | 返回值。 |
#### style\_get\_int 函数
-----------------------

* 函数功能：

> <p id="style_t_style_get_int">获取指定name的整数格式的值。

* 函数原型：

```
int32_t style_get_int (style_t* s, const char* name, int32_t defval);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | int32\_t | 返回整数格式的值。 |
| s | style\_t* | style对象。 |
| name | const char* | 属性名。 |
| defval | int32\_t | 缺省值。 |
#### style\_get\_str 函数
-----------------------

* 函数功能：

> <p id="style_t_style_get_str">获取指定name的字符串格式的值。

* 函数原型：

```
const char* style_get_str (style_t* s, const char* name, const char* defval);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | const char* | 返回字符串格式的值。 |
| s | style\_t* | style对象。 |
| name | const char* | 属性名。 |
| defval | const char* | 缺省值。 |
#### style\_get\_style\_state 函数
-----------------------

* 函数功能：

> <p id="style_t_style_get_style_state">获取风格对象的风格状态

* 函数原型：

```
const char* style_get_style_state (style_t* s);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | const char* | 返回风格状态。 |
| s | style\_t* | style对象。 |
#### style\_get\_style\_type 函数
-----------------------

* 函数功能：

> <p id="style_t_style_get_style_type">获取 style 的风格类型。

* 函数原型：

```
const char* style_get_style_type (style_t* s);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | const char* | 返回风格类型。 |
| s | style\_t* | style对象。 |
#### style\_get\_uint 函数
-----------------------

* 函数功能：

> <p id="style_t_style_get_uint">获取指定name的无符号整数格式的值。

* 函数原型：

```
uint32_t style_get_uint (style_t* s, const char* name, uint32_t defval);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | uint32\_t | 返回无符号整数格式的值。 |
| s | style\_t* | style对象。 |
| name | const char* | 属性名。 |
| defval | uint32\_t | 缺省值。 |
#### style\_is\_mutable 函数
-----------------------

* 函数功能：

> <p id="style_t_style_is_mutable">检查style是否是mutable的。

* 函数原型：

```
bool_t style_is_mutable (style_t* s);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | bool\_t | 返回TRUE表示是，否则表示不是。 |
| s | style\_t* | style对象。 |
#### style\_is\_valid 函数
-----------------------

* 函数功能：

> <p id="style_t_style_is_valid">检查style对象是否有效

* 函数原型：

```
bool_t style_is_valid (style_t* s);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | bool\_t | 返回是否有效。 |
| s | style\_t* | style对象。 |
#### style\_normalize\_value 函数
-----------------------

* 函数功能：

> <p id="style_t_style_normalize_value">对值进行正规化。

* 函数原型：

```
ret_t style_normalize_value (const char* name, const char* value, value_t* out);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| name | const char* | 名称。 |
| value | const char* | 值。 |
| out | value\_t* | 返回的值。 |
#### style\_notify\_widget\_state\_changed 函数
-----------------------

* 函数功能：

> <p id="style_t_style_notify_widget_state_changed">widget状态改变时，通知style更新数据。

* 函数原型：

```
ret_t style_notify_widget_state_changed (style_t* s, widget_t* widget);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| s | style\_t* | style对象。 |
| widget | widget\_t* | 控件对象。 |
#### style\_set 函数
-----------------------

* 函数功能：

> <p id="style_t_style_set">设置指定状态的指定属性的值(仅仅对mutable的style有效)。

* 函数原型：

```
ret_t style_set (style_t* s, const char* state, const char* name, const value_t* value);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| s | style\_t* | style对象。 |
| state | const char* | 状态。 |
| name | const char* | 属性名。 |
| value | const value\_t* | 值。 |
#### style\_set\_style\_data 函数
-----------------------

* 函数功能：

> <p id="style_t_style_set_style_data">把风格对象数据设置到风格对象中

* 函数原型：

```
ret_t style_set_style_data (style_t* s, const uint8_t* data, const char* state);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| s | style\_t* | style对象。 |
| data | const uint8\_t* | 风格对象数据 |
| state | const char* | 风格状态 |
#### style\_update\_state 函数
-----------------------

* 函数功能：

> <p id="style_t_style_update_state">更新风格对象的状态以及对应的数据
备注：根据 widget_type 和 style_name 以及 widget_state 在 theme 对象中查找对应的数据并且更新到 style 对象中

* 函数原型：

```
ret_t style_update_state (style_t* s, theme_t* theme, const char* widget_type, const char* style_name, const char* widget_state);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| s | style\_t* | style对象。 |
| theme | theme\_t* | theme对象。 |
| widget\_type | const char* | 控件的类型名。 |
| style\_name | const char* | style的名称。 |
| widget\_state | const char* | 控件的状态。 |

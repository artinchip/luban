## value\_t
### 概述
一个通用数据类型，用来存放整数、浮点数、字符串和其它对象。

在C/C++中，一般不需动态创建对象，直接声明并初始化即可。如：

```c
value_t v;
value_set_int(&v, 100);
```

> 在脚本语言中，需要动态创建对象。
----------------------------------
### 函数
<p id="value_t_methods">

| 函数名称 | 说明 | 
| -------- | ------------ | 
| <a href="#value_t_value_abs">value\_abs</a> | 将v取绝对值，并放入result对象。 |
| <a href="#value_t_value_add">value\_add</a> | 将v和other求和，并放入result对象。 |
| <a href="#value_t_value_binary_data">value\_binary\_data</a> | 获取为binary_data的值。 |
| <a href="#value_t_value_bit_and">value\_bit\_and</a> | 将v和other按位取与，并放入result对象。 |
| <a href="#value_t_value_bit_not">value\_bit\_not</a> | 将v按位取反，并放入result对象。 |
| <a href="#value_t_value_bit_or">value\_bit\_or</a> | 将v和other按位取或，并放入result对象。 |
| <a href="#value_t_value_bit_xor">value\_bit\_xor</a> | 将v和other按位取异或，并放入result对象。 |
| <a href="#value_t_value_bitmap">value\_bitmap</a> | 获取类型为位图对象。 |
| <a href="#value_t_value_bool">value\_bool</a> | 获取类型为bool的值。 |
| <a href="#value_t_value_cast">value\_cast</a> | 转换为value对象。 |
| <a href="#value_t_value_copy">value\_copy</a> | 拷贝value的值。 |
| <a href="#value_t_value_create">value\_create</a> | 创建value对象。 |
| <a href="#value_t_value_deep_copy">value\_deep\_copy</a> | 深拷贝value的值。 |
| <a href="#value_t_value_destroy">value\_destroy</a> | 销毁value对象。 |
| <a href="#value_t_value_div">value\_div</a> | 将v和other求商，并放入result对象。 |
| <a href="#value_t_value_double">value\_double</a> | 获取类型为double的值。 |
| <a href="#value_t_value_dup_binary_data">value\_dup\_binary\_data</a> | 设置类型为binary_data的值(复制数据)。 |
| <a href="#value_t_value_dup_str">value\_dup\_str</a> | 设置类型为字符串的值(并拷贝字符串)。 |
| <a href="#value_t_value_dup_str_with_len">value\_dup\_str\_with\_len</a> | 设置类型为字符串的值(并拷贝字符串)。 |
| <a href="#value_t_value_dup_wstr">value\_dup\_wstr</a> | 设置类型为宽字符串的值(并拷贝宽字符串)。 |
| <a href="#value_t_value_equal">value\_equal</a> | 判断两个value是否相同。 |
| <a href="#value_t_value_expt">value\_expt</a> | 计算v的other次幂，并放入result对象。 |
| <a href="#value_t_value_float">value\_float</a> | 获取类型为float\_t的值。 |
| <a href="#value_t_value_float32">value\_float32</a> | 获取类型为float的值。 |
| <a href="#value_t_value_func">value\_func</a> | 获取类型为func的值。 |
| <a href="#value_t_value_func_def">value\_func\_def</a> | 获取类型为func_def的值。 |
| <a href="#value_t_value_get_bit">value\_get\_bit</a> | 将v指定的位数取出，并放入result对象。 |
| <a href="#value_t_value_gradient">value\_gradient</a> | 获取为gradient的值。 |
| <a href="#value_t_value_id">value\_id</a> | 获取类型为ID的值。 |
| <a href="#value_t_value_int">value\_int</a> | 转换为int的值。 |
| <a href="#value_t_value_int16">value\_int16</a> | 获取类型为int16的值。 |
| <a href="#value_t_value_int32">value\_int32</a> | 获取类型为int32的值。 |
| <a href="#value_t_value_int64">value\_int64</a> | 获取类型为int64的值。 |
| <a href="#value_t_value_int8">value\_int8</a> | 获取类型为int8的值。 |
| <a href="#value_t_value_is_null">value\_is\_null</a> | 判断value是否为空值。 |
| <a href="#value_t_value_lshift">value\_lshift</a> | 将v左移指定的位数，并将结果放入result对象。 |
| <a href="#value_t_value_lshift_r">value\_lshift\_r</a> | 将v循环左移指定的位数，并将结果放入result对象。 |
| <a href="#value_t_value_max">value\_max</a> | 从数组中选择最大值，并放入result对象。 |
| <a href="#value_t_value_min">value\_min</a> | 从数组中选择最小值，并放入result对象。 |
| <a href="#value_t_value_mod">value\_mod</a> | 将v和other求模，并放入result对象。 |
| <a href="#value_t_value_mul">value\_mul</a> | 将v和other求积，并放入result对象。 |
| <a href="#value_t_value_object">value\_object</a> | 转换为object的值。 |
| <a href="#value_t_value_pointer">value\_pointer</a> | 获取类型为pointer的值。 |
| <a href="#value_t_value_rect">value\_rect</a> | 获取类型为矩形区域数据。 |
| <a href="#value_t_value_reset">value\_reset</a> | 重置value对象。 |
| <a href="#value_t_value_rshift">value\_rshift</a> | 将v右移指定的位数，并将结果放入result对象。 |
| <a href="#value_t_value_rshift_r">value\_rshift\_r</a> | 将v循环右移指定的位数，并将结果放入result对象。 |
| <a href="#value_t_value_set_binary_data">value\_set\_binary\_data</a> | 设置类型为binary_data的值。 |
| <a href="#value_t_value_set_bit">value\_set\_bit</a> | 将v指定的位数设置为bit，并放入result对象。 |
| <a href="#value_t_value_set_bitmap">value\_set\_bitmap</a> | 设置类型为位图对象。 |
| <a href="#value_t_value_set_bool">value\_set\_bool</a> | 设置类型为bool的值。 |
| <a href="#value_t_value_set_double">value\_set\_double</a> | 设置类型为double的值。 |
| <a href="#value_t_value_set_float">value\_set\_float</a> | 设置类型为float\_t的值。 |
| <a href="#value_t_value_set_float32">value\_set\_float32</a> | 设置类型为float的值。 |
| <a href="#value_t_value_set_func">value\_set\_func</a> | 设置类型为func的值。 |
| <a href="#value_t_value_set_func_def">value\_set\_func\_def</a> | 设置类型为func_def的值。 |
| <a href="#value_t_value_set_gradient">value\_set\_gradient</a> | 设置类型为gradient的值。 |
| <a href="#value_t_value_set_id">value\_set\_id</a> | 设置类型为ID的值。 |
| <a href="#value_t_value_set_int">value\_set\_int</a> | 设置类型为int的值。 |
| <a href="#value_t_value_set_int16">value\_set\_int16</a> | 设置类型为int16的值。 |
| <a href="#value_t_value_set_int32">value\_set\_int32</a> | 设置类型为int32的值。 |
| <a href="#value_t_value_set_int64">value\_set\_int64</a> | 设置类型为int64的值。 |
| <a href="#value_t_value_set_int8">value\_set\_int8</a> | 设置类型为int8的值。 |
| <a href="#value_t_value_set_object">value\_set\_object</a> | 设置类型为object的值。 |
| <a href="#value_t_value_set_pointer">value\_set\_pointer</a> | 设置类型为pointer的值。 |
| <a href="#value_t_value_set_pointer_ex">value\_set\_pointer\_ex</a> | 设置类型为pointer的值。 |
| <a href="#value_t_value_set_rect">value\_set\_rect</a> | 设置类型为矩形区域数据。 |
| <a href="#value_t_value_set_sized_str">value\_set\_sized\_str</a> | 设置类型为带长度的字符串的值。 |
| <a href="#value_t_value_set_str">value\_set\_str</a> | 设置类型为字符串的值。 |
| <a href="#value_t_value_set_token">value\_set\_token</a> | 设置类型为token的值。 |
| <a href="#value_t_value_set_ubjson">value\_set\_ubjson</a> | 设置类型为ubjson的值。 |
| <a href="#value_t_value_set_uint16">value\_set\_uint16</a> | 设置类型为uint16的值。 |
| <a href="#value_t_value_set_uint32">value\_set\_uint32</a> | 设置类型为uint32的值。 |
| <a href="#value_t_value_set_uint64">value\_set\_uint64</a> | 设置类型为uint64的值。 |
| <a href="#value_t_value_set_uint8">value\_set\_uint8</a> | 设置类型为uint8的值。 |
| <a href="#value_t_value_set_wstr">value\_set\_wstr</a> | 设置类型为宽字符串的值。 |
| <a href="#value_t_value_sized_str">value\_sized\_str</a> | 获取为sized_str的值。 |
| <a href="#value_t_value_str">value\_str</a> | 获取类型为字符串的值。 |
| <a href="#value_t_value_str_ex">value\_str\_ex</a> | 获取类型为字符串的值。 |
| <a href="#value_t_value_sub">value\_sub</a> | 将v和other求差，并放入result对象。 |
| <a href="#value_t_value_toggle_bit">value\_toggle\_bit</a> | 将v指定的位数取反，并将结果放入result对象。 |
| <a href="#value_t_value_token">value\_token</a> | 获取token的值。 |
| <a href="#value_t_value_type_name">value\_type\_name</a> | 获取指定类型数据的名称。 |
| <a href="#value_t_value_type_size">value\_type\_size</a> | 获取指定类型数据大小。 |
| <a href="#value_t_value_ubjson">value\_ubjson</a> | 获取为ubjson的值。 |
| <a href="#value_t_value_uint16">value\_uint16</a> | 获取类型为uint16的值。 |
| <a href="#value_t_value_uint32">value\_uint32</a> | 获取类型为uint32的值。 |
| <a href="#value_t_value_uint64">value\_uint64</a> | 获取类型为uint64的值。 |
| <a href="#value_t_value_uint8">value\_uint8</a> | 获取类型为uint8的值。 |
| <a href="#value_t_value_wstr">value\_wstr</a> | 获取类型为宽字符串的值。 |
#### value\_abs 函数
-----------------------

* 函数功能：

> <p id="value_t_value_abs">将v取绝对值，并放入result对象。

* 函数原型：

```
ret_t value_abs (value_t* v, value_t* result);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| v | value\_t* | value对象(必须是数值类型)。 |
| result | value\_t* | 返回结果的value对象。 |
#### value\_add 函数
-----------------------

* 函数功能：

> <p id="value_t_value_add">将v和other求和，并放入result对象。

* 函数原型：

```
ret_t value_add (value_t* v, value_t* other, value_t* result);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| v | value\_t* | value对象(必须是数值类型)。 |
| other | value\_t* | value对象(必须是数值类型)。 |
| result | value\_t* | 返回结果的value对象。 |
#### value\_binary\_data 函数
-----------------------

* 函数功能：

> <p id="value_t_value_binary_data">获取为binary_data的值。

* 函数原型：

```
binary_data_t* value_binary_data (const value_t* v);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | binary\_data\_t* | 值。 |
| v | const value\_t* | value对象。 |
#### value\_bit\_and 函数
-----------------------

* 函数功能：

> <p id="value_t_value_bit_and">将v和other按位取与，并放入result对象。

* 函数原型：

```
ret_t value_bit_and (value_t* v, value_t* other, value_t* result);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| v | value\_t* | value对象(必须是整数类型)。 |
| other | value\_t* | value对象(必须是整数类型)。 |
| result | value\_t* | 返回结果的value对象。 |
#### value\_bit\_not 函数
-----------------------

* 函数功能：

> <p id="value_t_value_bit_not">将v按位取反，并放入result对象。

* 函数原型：

```
ret_t value_bit_not (value_t* v, value_t* result);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| v | value\_t* | value对象(必须是整数类型)。 |
| result | value\_t* | 返回结果的value对象。 |
#### value\_bit\_or 函数
-----------------------

* 函数功能：

> <p id="value_t_value_bit_or">将v和other按位取或，并放入result对象。

* 函数原型：

```
ret_t value_bit_or (value_t* v, value_t* other, value_t* result);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| v | value\_t* | value对象(必须是整数类型)。 |
| other | value\_t* | value对象(必须是整数类型)。 |
| result | value\_t* | 返回结果的value对象。 |
#### value\_bit\_xor 函数
-----------------------

* 函数功能：

> <p id="value_t_value_bit_xor">将v和other按位取异或，并放入result对象。

* 函数原型：

```
ret_t value_bit_xor (value_t* v, value_t* other, value_t* result);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| v | value\_t* | value对象(必须是整数类型)。 |
| other | value\_t* | value对象(必须是整数类型)。 |
| result | value\_t* | 返回结果的value对象。 |
#### value\_bitmap 函数
-----------------------

* 函数功能：

> <p id="value_t_value_bitmap">获取类型为位图对象。

* 函数原型：

```
void* value_bitmap (const value_t* v);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | void* | 位图对象。 |
| v | const value\_t* | value对象。 |
#### value\_bool 函数
-----------------------

* 函数功能：

> <p id="value_t_value_bool">获取类型为bool的值。

* 函数原型：

```
bool_t value_bool (const value_t* v);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | bool\_t | 值。 |
| v | const value\_t* | value对象。 |
#### value\_cast 函数
-----------------------

* 函数功能：

> <p id="value_t_value_cast">转换为value对象。

> 供脚本语言使用

* 函数原型：

```
value_t* value_cast (value_t* value);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | 对象。 |
| value | value\_t* | value对象。 |
#### value\_copy 函数
-----------------------

* 函数功能：

> <p id="value_t_value_copy">拷贝value的值。

* 函数原型：

```
ret_t value_copy (value_t* dst, const value_t* src);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| dst | value\_t* | 目的value对象。 |
| src | const value\_t* | 源value对象。 |
#### value\_create 函数
-----------------------

* 函数功能：

> <p id="value_t_value_create">创建value对象。

* 函数原型：

```
value_t* value_create ();
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | 对象。 |
#### value\_deep\_copy 函数
-----------------------

* 函数功能：

> <p id="value_t_value_deep_copy">深拷贝value的值。
dst使用完成后，要调用value_reset，确保不会发生内存泄漏。

* 函数原型：

```
ret_t value_deep_copy (value_t* dst, const value_t* src);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| dst | value\_t* | 目的value对象。 |
| src | const value\_t* | 源value对象。 |
#### value\_destroy 函数
-----------------------

* 函数功能：

> <p id="value_t_value_destroy">销毁value对象。

* 函数原型：

```
ret_t value_destroy (value_t* v);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| v | value\_t* | value对象。 |
#### value\_div 函数
-----------------------

* 函数功能：

> <p id="value_t_value_div">将v和other求商，并放入result对象。

* 函数原型：

```
ret_t value_div (value_t* v, value_t* other, value_t* result);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| v | value\_t* | value对象(必须是数值类型)。 |
| other | value\_t* | value对象(必须是数值类型)。 |
| result | value\_t* | 返回结果的value对象。 |
#### value\_double 函数
-----------------------

* 函数功能：

> <p id="value_t_value_double">获取类型为double的值。

* 函数原型：

```
double value_double (const value_t* v);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | double | 值。 |
| v | const value\_t* | value对象。 |
#### value\_dup\_binary\_data 函数
-----------------------

* 函数功能：

> <p id="value_t_value_dup_binary_data">设置类型为binary_data的值(复制数据)。

* 函数原型：

```
value_t* value_dup_binary_data (value_t* v, const void* data, uint32_t size);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | value对象本身。 |
| v | value\_t* | value对象。 |
| data | const void* | 待设置的值。 |
| size | uint32\_t | 长度。 |
#### value\_dup\_str 函数
-----------------------

* 函数功能：

> <p id="value_t_value_dup_str">设置类型为字符串的值(并拷贝字符串)。

* 函数原型：

```
value_t* value_dup_str (value_t* v, const char* value);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | value对象本身。 |
| v | value\_t* | value对象。 |
| value | const char* | 待设置的值。 |
#### value\_dup\_str\_with\_len 函数
-----------------------

* 函数功能：

> <p id="value_t_value_dup_str_with_len">设置类型为字符串的值(并拷贝字符串)。

* 函数原型：

```
value_t* value_dup_str_with_len (value_t* v, const char* value, uint32_t len);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | value对象本身。 |
| v | value\_t* | value对象。 |
| value | const char* | 待设置的值。 |
| len | uint32\_t | 长度。 |
#### value\_dup\_wstr 函数
-----------------------

* 函数功能：

> <p id="value_t_value_dup_wstr">设置类型为宽字符串的值(并拷贝宽字符串)。

* 函数原型：

```
value_t* value_dup_wstr (value_t* v, const wchar_t* value);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | value对象本身。 |
| v | value\_t* | value对象。 |
| value | const wchar\_t* | 待设置的值。 |
#### value\_equal 函数
-----------------------

* 函数功能：

> <p id="value_t_value_equal">判断两个value是否相同。

* 函数原型：

```
bool_t value_equal (const value_t* value, const value_t* other);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | bool\_t | 为空值返回TRUE，否则返回FALSE。 |
| value | const value\_t* | value对象。 |
| other | const value\_t* | value对象。 |
#### value\_expt 函数
-----------------------

* 函数功能：

> <p id="value_t_value_expt">计算v的other次幂，并放入result对象。

* 函数原型：

```
ret_t value_expt (value_t* v, value_t* other, value_t* result);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| v | value\_t* | value对象(必须是数值类型)。 |
| other | value\_t* | value对象(必须是整数类型)。 |
| result | value\_t* | 返回结果的value对象。 |
#### value\_float 函数
-----------------------

* 函数功能：

> <p id="value_t_value_float">获取类型为float\_t的值。

* 函数原型：

```
float_t value_float (const value_t* v);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | float\_t | 值。 |
| v | const value\_t* | value对象。 |
#### value\_float32 函数
-----------------------

* 函数功能：

> <p id="value_t_value_float32">获取类型为float的值。

* 函数原型：

```
float value_float32 (const value_t* v);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | float | 值。 |
| v | const value\_t* | value对象。 |
#### value\_func 函数
-----------------------

* 函数功能：

> <p id="value_t_value_func">获取类型为func的值。

* 函数原型：

```
void* value_func (const value_t* v);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | void* | 值。 |
| v | const value\_t* | value对象。 |
#### value\_func\_def 函数
-----------------------

* 函数功能：

> <p id="value_t_value_func_def">获取类型为func_def的值。

* 函数原型：

```
void* value_func_def (const value_t* v);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | void* | 值。 |
| v | const value\_t* | value对象。 |
#### value\_get\_bit 函数
-----------------------

* 函数功能：

> <p id="value_t_value_get_bit">将v指定的位数取出，并放入result对象。

* 函数原型：

```
ret_t value_get_bit (value_t* v, value_t* result, uint32_t n);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| v | value\_t* | value对象(必须是整数类型)。 |
| result | value\_t* | 返回结果的value对象。 |
| n | uint32\_t | 位数。 |
#### value\_gradient 函数
-----------------------

* 函数功能：

> <p id="value_t_value_gradient">获取为gradient的值。

* 函数原型：

```
binary_data_t* value_gradient (const value_t* v);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | binary\_data\_t* | 值。 |
| v | const value\_t* | value对象。 |
#### value\_id 函数
-----------------------

* 函数功能：

> <p id="value_t_value_id">获取类型为ID的值。

* 函数原型：

```
const char* value_id (const value_t* v);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | const char* | 值。 |
| v | const value\_t* | value对象。 |
#### value\_int 函数
-----------------------

* 函数功能：

> <p id="value_t_value_int">转换为int的值。

* 函数原型：

```
int value_int (const value_t* v);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | int | 值。 |
| v | const value\_t* | value对象。 |
#### value\_int16 函数
-----------------------

* 函数功能：

> <p id="value_t_value_int16">获取类型为int16的值。

* 函数原型：

```
int16_t value_int16 (const value_t* v);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | int16\_t | 值。 |
| v | const value\_t* | value对象。 |
#### value\_int32 函数
-----------------------

* 函数功能：

> <p id="value_t_value_int32">获取类型为int32的值。

* 函数原型：

```
int32_t value_int32 (const value_t* v);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | int32\_t | 值。 |
| v | const value\_t* | value对象。 |
#### value\_int64 函数
-----------------------

* 函数功能：

> <p id="value_t_value_int64">获取类型为int64的值。

* 函数原型：

```
int64_t value_int64 (const value_t* v);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | int64\_t | 值。 |
| v | const value\_t* | value对象。 |
#### value\_int8 函数
-----------------------

* 函数功能：

> <p id="value_t_value_int8">获取类型为int8的值。

* 函数原型：

```
int8_t value_int8 (const value_t* v);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | int8\_t | 值。 |
| v | const value\_t* | value对象。 |
#### value\_is\_null 函数
-----------------------

* 函数功能：

> <p id="value_t_value_is_null">判断value是否为空值。

* 函数原型：

```
bool_t value_is_null (value_t* value);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | bool\_t | 为空值返回TRUE，否则返回FALSE。 |
| value | value\_t* | value对象。 |
#### value\_lshift 函数
-----------------------

* 函数功能：

> <p id="value_t_value_lshift">将v左移指定的位数，并将结果放入result对象。

* 函数原型：

```
ret_t value_lshift (value_t* v, value_t* result, uint32_t n);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| v | value\_t* | value对象(必须是整数类型)。 |
| result | value\_t* | 返回结果的value对象。 |
| n | uint32\_t | 位数。 |
#### value\_lshift\_r 函数
-----------------------

* 函数功能：

> <p id="value_t_value_lshift_r">将v循环左移指定的位数，并将结果放入result对象。

* 函数原型：

```
ret_t value_lshift_r (value_t* v, value_t* result, uint32_t n);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| v | value\_t* | value对象(必须是整数类型)。 |
| result | value\_t* | 返回结果的value对象。 |
| n | uint32\_t | 位数。 |
#### value\_max 函数
-----------------------

* 函数功能：

> <p id="value_t_value_max">从数组中选择最大值，并放入result对象。

* 函数原型：

```
ret_t value_max (value_t* arr, uint32_t size, value_t* result);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| arr | value\_t* | 数组。 |
| size | uint32\_t | 数组元素个数 |
| result | value\_t* | 返回结果的value对象。 |
#### value\_min 函数
-----------------------

* 函数功能：

> <p id="value_t_value_min">从数组中选择最小值，并放入result对象。

* 函数原型：

```
ret_t value_min (value_t* arr, uint32_t size, value_t* result);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| arr | value\_t* | 数组。 |
| size | uint32\_t | 数组元素个数 |
| result | value\_t* | 返回结果的value对象。 |
#### value\_mod 函数
-----------------------

* 函数功能：

> <p id="value_t_value_mod">将v和other求模，并放入result对象。

* 函数原型：

```
ret_t value_mod (value_t* v, value_t* other, value_t* result);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| v | value\_t* | value对象(必须是整数类型)。 |
| other | value\_t* | value对象(必须是整数类型)。 |
| result | value\_t* | 返回结果的value对象。 |
#### value\_mul 函数
-----------------------

* 函数功能：

> <p id="value_t_value_mul">将v和other求积，并放入result对象。

* 函数原型：

```
ret_t value_mul (value_t* v, value_t* other, value_t* result);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| v | value\_t* | value对象(必须是数值类型)。 |
| other | value\_t* | value对象(必须是数值类型)。 |
| result | value\_t* | 返回结果的value对象。 |
#### value\_object 函数
-----------------------

* 函数功能：

> <p id="value_t_value_object">转换为object的值。

* 函数原型：

```
tk_object_t* value_object (const value_t* v);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | tk\_object\_t* | 值。 |
| v | const value\_t* | value对象。 |
#### value\_pointer 函数
-----------------------

* 函数功能：

> <p id="value_t_value_pointer">获取类型为pointer的值。

* 函数原型：

```
void* value_pointer (const value_t* v);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | void* | 值。 |
| v | const value\_t* | value对象。 |
#### value\_rect 函数
-----------------------

* 函数功能：

> <p id="value_t_value_rect">获取类型为矩形区域数据。

* 函数原型：

```
rect_t* value_rect (const value_t* v);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | rect\_t* | 返回矩形区域数据。 |
| v | const value\_t* | value对象。 |
#### value\_reset 函数
-----------------------

* 函数功能：

> <p id="value_t_value_reset">重置value对象。

* 函数原型：

```
ret_t value_reset (value_t* v);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| v | value\_t* | value对象。 |
#### value\_rshift 函数
-----------------------

* 函数功能：

> <p id="value_t_value_rshift">将v右移指定的位数，并将结果放入result对象。

* 函数原型：

```
ret_t value_rshift (value_t* v, value_t* result, uint32_t n);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| v | value\_t* | value对象(必须是整数类型)。 |
| result | value\_t* | 返回结果的value对象。 |
| n | uint32\_t | 位数。 |
#### value\_rshift\_r 函数
-----------------------

* 函数功能：

> <p id="value_t_value_rshift_r">将v循环右移指定的位数，并将结果放入result对象。

* 函数原型：

```
ret_t value_rshift_r (value_t* v, value_t* result, uint32_t n);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| v | value\_t* | value对象(必须是整数类型)。 |
| result | value\_t* | 返回结果的value对象。 |
| n | uint32\_t | 位数。 |
#### value\_set\_binary\_data 函数
-----------------------

* 函数功能：

> <p id="value_t_value_set_binary_data">设置类型为binary_data的值。

* 函数原型：

```
value_t* value_set_binary_data (value_t* v, void* data, uint32_t size);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | value对象本身。 |
| v | value\_t* | value对象。 |
| data | void* | 待设置的值。 |
| size | uint32\_t | 长度。 |
#### value\_set\_bit 函数
-----------------------

* 函数功能：

> <p id="value_t_value_set_bit">将v指定的位数设置为bit，并放入result对象。

* 函数原型：

```
ret_t value_set_bit (value_t* v, value_t* result, uint32_t n, bool_t bit);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| v | value\_t* | value对象(必须是整数类型)。 |
| result | value\_t* | 返回结果的value对象。 |
| n | uint32\_t | 位数。 |
| bit | bool\_t | 值。 |
#### value\_set\_bitmap 函数
-----------------------

* 函数功能：

> <p id="value_t_value_set_bitmap">设置类型为位图对象。

* 函数原型：

```
value_t* value_set_bitmap (value_t* v, void* bitmap);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | value对象本身。 |
| v | value\_t* | value对象。 |
| bitmap | void* | 待设置的值。 |
#### value\_set\_bool 函数
-----------------------

* 函数功能：

> <p id="value_t_value_set_bool">设置类型为bool的值。

* 函数原型：

```
value_t* value_set_bool (value_t* v, bool_t value);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | value对象本身。 |
| v | value\_t* | value对象。 |
| value | bool\_t | 待设置的值。 |
#### value\_set\_double 函数
-----------------------

* 函数功能：

> <p id="value_t_value_set_double">设置类型为double的值。

* 函数原型：

```
value_t* value_set_double (value_t* v, double value);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | value对象本身。 |
| v | value\_t* | value对象。 |
| value | double | 待设置的值。 |
#### value\_set\_float 函数
-----------------------

* 函数功能：

> <p id="value_t_value_set_float">设置类型为float\_t的值。

* 函数原型：

```
value_t* value_set_float (value_t* v, float_t value);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | value对象本身。 |
| v | value\_t* | value对象。 |
| value | float\_t | 待设置的值。 |
#### value\_set\_float32 函数
-----------------------

* 函数功能：

> <p id="value_t_value_set_float32">设置类型为float的值。

* 函数原型：

```
value_t* value_set_float32 (value_t* v, float value);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | value对象本身。 |
| v | value\_t* | value对象。 |
| value | float | 待设置的值。 |
#### value\_set\_func 函数
-----------------------

* 函数功能：

> <p id="value_t_value_set_func">设置类型为func的值。

* 函数原型：

```
value_t* value_set_func (value_t* v, void* value);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | value对象本身。 |
| v | value\_t* | value对象。 |
| value | void* | 待设置的值。 |
#### value\_set\_func\_def 函数
-----------------------

* 函数功能：

> <p id="value_t_value_set_func_def">设置类型为func_def的值。

* 函数原型：

```
value_t* value_set_func_def (value_t* v, void* value);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | value对象本身。 |
| v | value\_t* | value对象。 |
| value | void* | 待设置的值。 |
#### value\_set\_gradient 函数
-----------------------

* 函数功能：

> <p id="value_t_value_set_gradient">设置类型为gradient的值。

* 函数原型：

```
value_t* value_set_gradient (value_t* v, void* data, uint32_t size);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | value对象本身。 |
| v | value\_t* | value对象。 |
| data | void* | 待设置的值。 |
| size | uint32\_t | 长度。 |
#### value\_set\_id 函数
-----------------------

* 函数功能：

> <p id="value_t_value_set_id">设置类型为ID的值。

* 函数原型：

```
value_t* value_set_id (value_t* v, const char* value, uint32_t len);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | value对象本身。 |
| v | value\_t* | value对象。 |
| value | const char* | 待设置的值。 |
| len | uint32\_t | 长度。 |
#### value\_set\_int 函数
-----------------------

* 函数功能：

> <p id="value_t_value_set_int">设置类型为int的值。

* 函数原型：

```
value_t* value_set_int (value_t* v, int32_t value);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | value对象本身。 |
| v | value\_t* | value对象。 |
| value | int32\_t | 待设置的值。 |
#### value\_set\_int16 函数
-----------------------

* 函数功能：

> <p id="value_t_value_set_int16">设置类型为int16的值。

* 函数原型：

```
value_t* value_set_int16 (value_t* v, int16_t value);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | value对象本身。 |
| v | value\_t* | value对象。 |
| value | int16\_t | 待设置的值。 |
#### value\_set\_int32 函数
-----------------------

* 函数功能：

> <p id="value_t_value_set_int32">设置类型为int32的值。

* 函数原型：

```
value_t* value_set_int32 (value_t* v, int32_t value);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | value对象本身。 |
| v | value\_t* | value对象。 |
| value | int32\_t | 待设置的值。 |
#### value\_set\_int64 函数
-----------------------

* 函数功能：

> <p id="value_t_value_set_int64">设置类型为int64的值。

* 函数原型：

```
value_t* value_set_int64 (value_t* v, int64_t value);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | value对象本身。 |
| v | value\_t* | value对象。 |
| value | int64\_t | 待设置的值。 |
#### value\_set\_int8 函数
-----------------------

* 函数功能：

> <p id="value_t_value_set_int8">设置类型为int8的值。

* 函数原型：

```
value_t* value_set_int8 (value_t* v, int8_t value);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | value对象本身。 |
| v | value\_t* | value对象。 |
| value | int8\_t | 待设置的值。 |
#### value\_set\_object 函数
-----------------------

* 函数功能：

> <p id="value_t_value_set_object">设置类型为object的值。

* 函数原型：

```
value_t* value_set_object (value_t* v, tk_object_t* value);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | value对象本身。 |
| v | value\_t* | value对象。 |
| value | tk\_object\_t* | 待设置的值。 |
#### value\_set\_pointer 函数
-----------------------

* 函数功能：

> <p id="value_t_value_set_pointer">设置类型为pointer的值。

* 函数原型：

```
value_t* value_set_pointer (value_t* v, void* value);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | value对象本身。 |
| v | value\_t* | value对象。 |
| value | void* | 待设置的值。 |
#### value\_set\_pointer\_ex 函数
-----------------------

* 函数功能：

> <p id="value_t_value_set_pointer_ex">设置类型为pointer的值。

* 函数原型：

```
value_t* value_set_pointer_ex (value_t* v, void* value, tk_destroy_t destroy);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | value对象本身。 |
| v | value\_t* | value对象。 |
| value | void* | 待设置的值。 |
| destroy | tk\_destroy\_t | 销毁函数。 |
#### value\_set\_rect 函数
-----------------------

* 函数功能：

> <p id="value_t_value_set_rect">设置类型为矩形区域数据。

* 函数原型：

```
value_t* value_set_rect (value_t* v, rect_t r);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | value对象本身。 |
| v | value\_t* | value对象。 |
| r | rect\_t | 待设置的值。 |
#### value\_set\_sized\_str 函数
-----------------------

* 函数功能：

> <p id="value_t_value_set_sized_str">设置类型为带长度的字符串的值。

* 函数原型：

```
value_t* value_set_sized_str (value_t* v, char* str, uint32_t size);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | value对象本身。 |
| v | value\_t* | value对象。 |
| str | char* | 待设置的值。 |
| size | uint32\_t | 长度。 |
#### value\_set\_str 函数
-----------------------

* 函数功能：

> <p id="value_t_value_set_str">设置类型为字符串的值。

* 函数原型：

```
value_t* value_set_str (value_t* v, const char* value);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | value对象本身。 |
| v | value\_t* | value对象。 |
| value | const char* | 待设置的值。 |
#### value\_set\_token 函数
-----------------------

* 函数功能：

> <p id="value_t_value_set_token">设置类型为token的值。

* 函数原型：

```
value_t* value_set_token (value_t* v, uint32_t value);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | value对象本身。 |
| v | value\_t* | value对象。 |
| value | uint32\_t | 待设置的值。 |
#### value\_set\_ubjson 函数
-----------------------

* 函数功能：

> <p id="value_t_value_set_ubjson">设置类型为ubjson的值。

* 函数原型：

```
value_t* value_set_ubjson (value_t* v, void* data, uint32_t size);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | value对象本身。 |
| v | value\_t* | value对象。 |
| data | void* | 待设置的值。 |
| size | uint32\_t | 长度。 |
#### value\_set\_uint16 函数
-----------------------

* 函数功能：

> <p id="value_t_value_set_uint16">设置类型为uint16的值。

* 函数原型：

```
value_t* value_set_uint16 (value_t* v, uint16_t value);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | value对象本身。 |
| v | value\_t* | value对象。 |
| value | uint16\_t | 待设置的值。 |
#### value\_set\_uint32 函数
-----------------------

* 函数功能：

> <p id="value_t_value_set_uint32">设置类型为uint32的值。

* 函数原型：

```
value_t* value_set_uint32 (value_t* v, uint32_t value);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | value对象本身。 |
| v | value\_t* | value对象。 |
| value | uint32\_t | 待设置的值。 |
#### value\_set\_uint64 函数
-----------------------

* 函数功能：

> <p id="value_t_value_set_uint64">设置类型为uint64的值。

* 函数原型：

```
value_t* value_set_uint64 (value_t* v, uint64_t value);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | value对象本身。 |
| v | value\_t* | value对象。 |
| value | uint64\_t | 待设置的值。 |
#### value\_set\_uint8 函数
-----------------------

* 函数功能：

> <p id="value_t_value_set_uint8">设置类型为uint8的值。

* 函数原型：

```
value_t* value_set_uint8 (value_t* v, uint8_t value);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | value对象本身。 |
| v | value\_t* | value对象。 |
| value | uint8\_t | 待设置的值。 |
#### value\_set\_wstr 函数
-----------------------

* 函数功能：

> <p id="value_t_value_set_wstr">设置类型为宽字符串的值。

* 函数原型：

```
value_t* value_set_wstr (value_t* v, const wchar_t* value);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | value\_t* | value对象本身。 |
| v | value\_t* | value对象。 |
| value | const wchar\_t* | 待设置的值。 |
#### value\_sized\_str 函数
-----------------------

* 函数功能：

> <p id="value_t_value_sized_str">获取为sized_str的值。

* 函数原型：

```
sized_str_t* value_sized_str (const value_t* v);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | sized\_str\_t* | 值。 |
| v | const value\_t* | value对象。 |
#### value\_str 函数
-----------------------

* 函数功能：

> <p id="value_t_value_str">获取类型为字符串的值。

* 函数原型：

```
const char* value_str (const value_t* v);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | const char* | 值。 |
| v | const value\_t* | value对象。 |
#### value\_str\_ex 函数
-----------------------

* 函数功能：

> <p id="value_t_value_str_ex">获取类型为字符串的值。

* 函数原型：

```
const char* value_str_ex (const value_t* v, char* buff, uint32_t size);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | const char* | 值。 |
| v | const value\_t* | value对象。 |
| buff | char* | 用于格式转换的缓冲区（如果 v 对象为 string 类型的话，不会把字符串数据拷贝到 buff 中）。 |
| size | uint32\_t | 缓冲区大小。 |
#### value\_sub 函数
-----------------------

* 函数功能：

> <p id="value_t_value_sub">将v和other求差，并放入result对象。

* 函数原型：

```
ret_t value_sub (value_t* v, value_t* other, value_t* result);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| v | value\_t* | value对象(必须是数值类型)。 |
| other | value\_t* | value对象(必须是数值类型)。 |
| result | value\_t* | 返回结果的value对象。 |
#### value\_toggle\_bit 函数
-----------------------

* 函数功能：

> <p id="value_t_value_toggle_bit">将v指定的位数取反，并将结果放入result对象。

* 函数原型：

```
ret_t value_toggle_bit (value_t* v, value_t* result, uint32_t n);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| v | value\_t* | value对象(必须是整数类型)。 |
| result | value\_t* | 返回结果的value对象。 |
| n | uint32\_t | 位数。 |
#### value\_token 函数
-----------------------

* 函数功能：

> <p id="value_t_value_token">获取token的值。

* 函数原型：

```
uint32_t value_token (const value_t* v);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | uint32\_t | 值。 |
| v | const value\_t* | value对象。 |
#### value\_type\_name 函数
-----------------------

* 函数功能：

> <p id="value_t_value_type_name">获取指定类型数据的名称。

* 函数原型：

```
const char* value_type_name (value_type_t type);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | const char* | 返回对应数据类型的名称。 |
| type | value\_type\_t | 类型。 |
#### value\_type\_size 函数
-----------------------

* 函数功能：

> <p id="value_t_value_type_size">获取指定类型数据大小。

* 函数原型：

```
uint32_t value_type_size (value_type_t type);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | uint32\_t | 返回对应数据类型的长度。 |
| type | value\_type\_t | 类型。 |
#### value\_ubjson 函数
-----------------------

* 函数功能：

> <p id="value_t_value_ubjson">获取为ubjson的值。

* 函数原型：

```
binary_data_t* value_ubjson (const value_t* v);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | binary\_data\_t* | 值。 |
| v | const value\_t* | value对象。 |
#### value\_uint16 函数
-----------------------

* 函数功能：

> <p id="value_t_value_uint16">获取类型为uint16的值。

* 函数原型：

```
uint16_t value_uint16 (const value_t* v);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | uint16\_t | 值。 |
| v | const value\_t* | value对象。 |
#### value\_uint32 函数
-----------------------

* 函数功能：

> <p id="value_t_value_uint32">获取类型为uint32的值。

* 函数原型：

```
uint32_t value_uint32 (const value_t* v);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | uint32\_t | 值。 |
| v | const value\_t* | value对象。 |
#### value\_uint64 函数
-----------------------

* 函数功能：

> <p id="value_t_value_uint64">获取类型为uint64的值。

* 函数原型：

```
uint64_t value_uint64 (const value_t* v);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | uint64\_t | 值。 |
| v | const value\_t* | value对象。 |
#### value\_uint8 函数
-----------------------

* 函数功能：

> <p id="value_t_value_uint8">获取类型为uint8的值。

* 函数原型：

```
uint8_t value_uint8 (const value_t* v);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | uint8\_t | 值。 |
| v | const value\_t* | value对象。 |
#### value\_wstr 函数
-----------------------

* 函数功能：

> <p id="value_t_value_wstr">获取类型为宽字符串的值。

* 函数原型：

```
const wchar_t* value_wstr (const value_t* v);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | const wchar\_t* | 值。 |
| v | const value\_t* | value对象。 |

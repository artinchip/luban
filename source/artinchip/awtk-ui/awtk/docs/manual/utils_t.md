## utils\_t
### 概述
工具类。
----------------------------------
### 函数
<p id="utils_t_methods">

| 函数名称 | 说明 | 
| -------- | ------------ | 
| <a href="#utils_t_compare_always_equal">compare\_always\_equal</a> | 始终返回相等。 |
| <a href="#utils_t_data_url_copy">data\_url\_copy</a> | 将数据从源URL拷贝到目标URL。 |
| <a href="#utils_t_default_destroy">default\_destroy</a> | 缺省的destroy函数。释放data指向的内存。 |
| <a href="#utils_t_dummy_destroy">dummy\_destroy</a> | 空的destroy函数。 |
| <a href="#utils_t_file_read_as_unix_text">file\_read\_as\_unix\_text</a> | 读取文本文件。并将windows换行(\r\n)或macos换行(\r)转换为uinux换行(\n)。 |
| <a href="#utils_t_filename_to_name">filename\_to\_name</a> | 从完整文件名中获取文件名。 |
| <a href="#utils_t_filename_to_name_ex">filename\_to\_name\_ex</a> | 从完整文件名中获取文件名。 |
| <a href="#utils_t_image_region_parse">image\_region\_parse</a> | 解析子图的区域信息。 |
| <a href="#utils_t_object_to_json">object\_to\_json</a> | 将对象转换成json字符串。 |
| <a href="#utils_t_pointer_compare">pointer\_compare</a> | 指针比较。 |
| <a href="#utils_t_ret_code_from_name">ret\_code\_from\_name</a> | 将ret_t的文本名称转换成对应的值。 |
| <a href="#utils_t_ret_code_to_name">ret\_code\_to\_name</a> | 将ret_t转换成对应的文本名称。 |
| <a href="#utils_t_tk_atob">tk\_atob</a> | 将字符串转换为布尔类型。 |
| <a href="#utils_t_tk_atof">tk\_atof</a> | 将字符串转换为浮点类型。 |
| <a href="#utils_t_tk_atoi">tk\_atoi</a> | 将字符串转换为整型数。 |
| <a href="#utils_t_tk_atol">tk\_atol</a> | 将字符串转换为整型。 |
| <a href="#utils_t_tk_atoul">tk\_atoul</a> | 将字符串转换为整型。 |
| <a href="#utils_t_tk_eval_ratio_or_px">tk\_eval\_ratio\_or\_px</a> | 如果expr以px/PX结束，直接返回expr前面的数值。 |
| <a href="#utils_t_tk_ftoa">tk\_ftoa</a> | 将浮点型转换为字符串。 |
| <a href="#utils_t_tk_is_ui_thread">tk\_is\_ui\_thread</a> | 判断当前线程是否是UI线程。 |
| <a href="#utils_t_tk_is_valid_name">tk\_is\_valid\_name</a> | 判断是否是有效的属性名。 |
| <a href="#utils_t_tk_itoa">tk\_itoa</a> | 将整型转换为字符串。 |
| <a href="#utils_t_tk_lltoa">tk\_lltoa</a> | 将整型转换为字符串。 |
| <a href="#utils_t_tk_memcpy">tk\_memcpy</a> | 内存拷贝。 |
| <a href="#utils_t_tk_memcpy16">tk\_memcpy16</a> | 拷贝数据2字节。 |
| <a href="#utils_t_tk_memcpy32">tk\_memcpy32</a> | 拷贝数据4字节。 |
| <a href="#utils_t_tk_memdup">tk\_memdup</a> | 内存拷贝。 |
| <a href="#utils_t_tk_memset16">tk\_memset16</a> | 设置数据2字节。 |
| <a href="#utils_t_tk_memset24">tk\_memset24</a> | 设置数据3字节。 |
| <a href="#utils_t_tk_memset32">tk\_memset32</a> | 设置数据4字节。 |
| <a href="#utils_t_tk_normalize_key_name">tk\_normalize\_key\_name</a> | 标准化key_name |
| <a href="#utils_t_tk_pixel_copy">tk\_pixel\_copy</a> | 已bpp字节为标准拷贝数据。 |
| <a href="#utils_t_tk_pointer_from_int">tk\_pointer\_from\_int</a> | 将int转换成指针。 |
| <a href="#utils_t_tk_pointer_to_int">tk\_pointer\_to\_int</a> | 将指针转换成int。 |
| <a href="#utils_t_tk_qsort">tk\_qsort</a> | 快速排序。 |
| <a href="#utils_t_tk_replace_char">tk\_replace\_char</a> | 替换字符。 |
| <a href="#utils_t_tk_replace_locale">tk\_replace\_locale</a> | 将文本中的$locale$替换为对应的语言。 |
| <a href="#utils_t_tk_set_ui_thread">tk\_set\_ui\_thread</a> | 设置UI线程的ID。 |
| <a href="#utils_t_tk_skip_to_num">tk\_skip\_to\_num</a> | 跳过字符串函数，如：字符串"hello123world"，返回的结果是"123world"。 |
| <a href="#utils_t_tk_snprintf">tk\_snprintf</a> | 将可变参数(...)按照format格式化字符串，并将字符串复制到str中。 |
| <a href="#utils_t_tk_sscanf">tk\_sscanf</a> | 从字符串读取格式化输入。 |
| <a href="#utils_t_tk_str_append">tk\_str\_append</a> | 字符串追加函数。 |
| <a href="#utils_t_tk_str_copy">tk\_str\_copy</a> | 字符串拷贝函数。 |
| <a href="#utils_t_tk_str_end_with">tk\_str\_end\_with</a> | 检查字符串是否以指定的字符串appendix结尾。 |
| <a href="#utils_t_tk_str_is_in_array">tk\_str\_is\_in\_array</a> | 检查字符串是否在字符串数组中。 |
| <a href="#utils_t_tk_str_start_with">tk\_str\_start\_with</a> | 检查字符串是否以指定的字符串prefix开头。 |
| <a href="#utils_t_tk_str_tolower">tk\_str\_tolower</a> | 将大写字母转换为小写字母。 |
| <a href="#utils_t_tk_str_totitle">tk\_str\_totitle</a> | 将单词首字母转换为大写字母。 |
| <a href="#utils_t_tk_str_toupper">tk\_str\_toupper</a> | 将小写字母转换为大写字母。 |
| <a href="#utils_t_tk_strcmp">tk\_strcmp</a> | 字符串比较函数。 |
| <a href="#utils_t_tk_strcpy">tk\_strcpy</a> | 将src所指向的字符串复制到dst。 |
| <a href="#utils_t_tk_strdup">tk\_strdup</a> | 字符串拷贝函数。 |
| <a href="#utils_t_tk_stricmp">tk\_stricmp</a> | 字符串比较函数（不区分大小写）。 |
| <a href="#utils_t_tk_strlen">tk\_strlen</a> | 获取字符串的长度。str为空时返回0。 |
| <a href="#utils_t_tk_strncpy">tk\_strncpy</a> | 将src所指向的字符串复制到dst，最多复制len个字符串，并在[len]位置添加'\0'。 |
| <a href="#utils_t_tk_strncpy_s">tk\_strncpy\_s</a> | 将src所指向的字符串复制到dst，最多复制min(dst_len-1, src_len)个字符串，并在[len]位置添加'\0'。 |
| <a href="#utils_t_tk_strndup">tk\_strndup</a> | 字符串拷贝函数，最多复制len个字符串。 |
| <a href="#utils_t_tk_strnlen">tk\_strnlen</a> | 获取字符串的长度。str为空时返回0。 |
| <a href="#utils_t_tk_strrstr">tk\_strrstr</a> | 从后往前查找指定的字符串。 |
| <a href="#utils_t_tk_strtoi">tk\_strtoi</a> | 将字符串转换为整型。 |
| <a href="#utils_t_tk_strtol">tk\_strtol</a> | 将字符串转换为长整型。 |
| <a href="#utils_t_tk_strtoll">tk\_strtoll</a> | 将字符串转换为长整型。 |
| <a href="#utils_t_tk_under_score_to_camel">tk\_under\_score\_to\_camel</a> | 将下划线名字转成驼峰名字。 |
| <a href="#utils_t_tk_vsnprintf">tk\_vsnprintf</a> | 将可变参数ap按照format格式化字符串，并将字符串复制到str中。 |
| <a href="#utils_t_tk_watob">tk\_watob</a> | 将宽字符串转换为布尔类型。 |
| <a href="#utils_t_tk_watof">tk\_watof</a> | 将宽字符串转换为浮点类型。 |
| <a href="#utils_t_tk_watoi">tk\_watoi</a> | 将宽字符串转换为整型。 |
| <a href="#utils_t_tk_watoi_n">tk\_watoi\_n</a> | 将宽字符串转换为整型。 |
| <a href="#utils_t_tk_wild_card_match">tk\_wild\_card\_match</a> | 简单的通配符匹配。*匹配0到多个字符，?匹配1个字符。 |
| <a href="#utils_t_tk_wstr_count_c">tk\_wstr\_count\_c</a> | 统计UCS字符串中某个字符出现的次数。 |
| <a href="#utils_t_tk_wstr_dup_utf8">tk\_wstr\_dup\_utf8</a> | 将utf8字符串拷贝为UCS字符串。 |
| <a href="#utils_t_tk_wstrcmp">tk\_wstrcmp</a> | 字符串比较函数。 |
| <a href="#utils_t_tk_wstrdup">tk\_wstrdup</a> | 宽字符串拷贝函数。 |
| <a href="#utils_t_tk_wstricmp">tk\_wstricmp</a> | 字符串比较函数（不区分大小写）。 |
| <a href="#utils_t_xml_file_expand_read">xml\_file\_expand\_read</a> | expand include process instruction to file content: <?include filename="view_me.inc" ?> |
#### compare\_always\_equal 函数
-----------------------

* 函数功能：

> <p id="utils_t_compare_always_equal">始终返回相等。

* 函数原型：

```
int compare_always_equal (const void* a, const void* b);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | int | 始终返回0。 |
| a | const void* | 数据a。 |
| b | const void* | 数据b。 |
#### data\_url\_copy 函数
-----------------------

* 函数功能：

> <p id="utils_t_data_url_copy">将数据从源URL拷贝到目标URL。

* 函数原型：

```
ret_t data_url_copy (const char* dst_url, const char* src_url);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| dst\_url | const char* | 目标URL。 |
| src\_url | const char* | 源URL。 |
#### default\_destroy 函数
-----------------------

* 函数功能：

> <p id="utils_t_default_destroy">缺省的destroy函数。释放data指向的内存。

* 函数原型：

```
ret_t default_destroy (void* data);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| data | void* | 数据。 |
#### dummy\_destroy 函数
-----------------------

* 函数功能：

> <p id="utils_t_dummy_destroy">空的destroy函数。

* 函数原型：

```
ret_t dummy_destroy (void* data);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| data | void* | 数据。 |
#### file\_read\_as\_unix\_text 函数
-----------------------

* 函数功能：

> <p id="utils_t_file_read_as_unix_text">读取文本文件。并将windows换行(\r\n)或macos换行(\r)转换为uinux换行(\n)。

* 函数原型：

```
char* file_read_as_unix_text (const char* filename, uint32_t* size);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | char* | 返回读取的数据，需要调用TKMEM\_FREE释放。 |
| filename | const char* | 文件名。 |
| size | uint32\_t* | 返回实际读取的长度。 |
#### filename\_to\_name 函数
-----------------------

* 函数功能：

> <p id="utils_t_filename_to_name">从完整文件名中获取文件名。

* 函数原型：

```
ret_t filename_to_name (const char* filename, char* str, uint32_t size);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| filename | const char* | 完整的文件名。 |
| str | char* | 用于返回文件名。 |
| size | uint32\_t | 文件名(str参数)的最大长度。 |
#### filename\_to\_name\_ex 函数
-----------------------

* 函数功能：

> <p id="utils_t_filename_to_name_ex">从完整文件名中获取文件名。

* 函数原型：

```
ret_t filename_to_name_ex (const char* filename, char* str, uint32_t size, bool_t remove_extname);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| filename | const char* | 完整的文件名。 |
| str | char* | 用于返回文件名。 |
| size | uint32\_t | 文件名(str参数)的最大长度。 |
| remove\_extname | bool\_t | 是否移除扩展名。 |
#### image\_region\_parse 函数
-----------------------

* 函数功能：

> <p id="utils_t_image_region_parse">解析子图的区域信息。

* 函数原型：

```
ret_t image_region_parse (uint32_t img_w, uint32_t img_h, const char* region, rect_t* r);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| img\_w | uint32\_t | 图片宽度。 |
| img\_h | uint32\_t | 图片宽度。 |
| region | const char* | region。 |
| r | rect\_t* | 返回具体位置。 |
#### object\_to\_json 函数
-----------------------

* 函数功能：

> <p id="utils_t_object_to_json">将对象转换成json字符串。

* 函数原型：

```
ret_t object_to_json (tk_object_t* obj, str_t* str);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| obj | tk\_object\_t* | object对象。 |
| str | str\_t* | 用于返回结果。 |
#### pointer\_compare 函数
-----------------------

* 函数功能：

> <p id="utils_t_pointer_compare">指针比较。

* 函数原型：

```
int pointer_compare (const void* a, const void* b);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | int | 返回0表示相等，返回负数表示小于，返回整数表示大于。 |
| a | const void* | 数据a。 |
| b | const void* | 数据b。 |
#### ret\_code\_from\_name 函数
-----------------------

* 函数功能：

> <p id="utils_t_ret_code_from_name">将ret_t的文本名称转换成对应的值。

* 函数原型：

```
ret_t ret_code_from_name (const char* name);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回对应的值。 |
| name | const char* | 字符串。 |
#### ret\_code\_to\_name 函数
-----------------------

* 函数功能：

> <p id="utils_t_ret_code_to_name">将ret_t转换成对应的文本名称。

* 函数原型：

```
const char* ret_code_to_name (ret_t ret);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | const char* | 返回对应的名称。 |
| ret | ret\_t | 代码。 |
#### tk\_atob 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_atob">将字符串转换为布尔类型。

* 函数原型：

```
bool_t tk_atob (const char* str);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | bool\_t | 返回转换后的布尔类型。 |
| str | const char* | 要转换为布尔类型的字符串。 |
#### tk\_atof 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_atof">将字符串转换为浮点类型。

* 函数原型：

```
double tk_atof (const char* str);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | double | 返回转换后的浮点类型。 |
| str | const char* | 要转换为浮点类型的字符串。 |
#### tk\_atoi 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_atoi">将字符串转换为整型数。

* 函数原型：

```
int32_t tk_atoi (const char* str);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | int32\_t | 返回转换后的整型。 |
| str | const char* | 要转换为整型数的字符串。 |
#### tk\_atol 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_atol">将字符串转换为整型。

* 函数原型：

```
int64_t tk_atol (const char* str);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | int64\_t | 返回转换后的整型。 |
| str | const char* | 要转换为整型的字符串。 |
#### tk\_atoul 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_atoul">将字符串转换为整型。

* 函数原型：

```
uint64_t tk_atoul (const char* str);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | uint64\_t | 返回转换后的整型。 |
| str | const char* | 要转换为整型的字符串。 |
#### tk\_eval\_ratio\_or\_px 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_eval_ratio_or_px">如果expr以px/PX结束，直接返回expr前面的数值。
如果expr以%结束，而且数值大于1，返回(数值 * value)/100。
否则将数值当作比例，返回(数值 * value)。

> 为了兼容以前的处理，如果expr以%结束，但是数值在0到1之间，此时忽略%。

```c
tk_eval_ratio_or_px("0.5", 100) => 50
tk_eval_ratio_or_px("20px", 100) => 20
tk_eval_ratio_or_px("20%", 100) => 20
tk_eval_ratio_or_px("0.5%", 100) => 50
```

* 函数原型：

```
float_t tk_eval_ratio_or_px (const char* expr, int32_t value);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | float\_t | 返回计算结果。 |
| expr | const char* | 表达式(如100px, 0.5等) |
| value | int32\_t | 值。 |
#### tk\_ftoa 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_ftoa">将浮点型转换为字符串。

* 函数原型：

```
const char* tk_ftoa (char* str, int len, double f);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | const char* | 返回字符串。 |
| str | char* | 保存字符串缓冲区。 |
| len | int | 缓冲区大小。 |
| f | double | 要转换的浮点型。 |
#### tk\_is\_ui\_thread 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_is_ui_thread">判断当前线程是否是UI线程。

* 函数原型：

```
bool_t tk_is_ui_thread ();
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | bool\_t | 返回TRUE表示是，否则表示否。 |
#### tk\_is\_valid\_name 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_is_valid_name">判断是否是有效的属性名。

* 函数原型：

```
bool_t tk_is_valid_name (const char* name);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | bool\_t | 如果名称有效返回TRUE，否则返回FALSE。 |
| name | const char* | 名字字符串。 |
#### tk\_itoa 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_itoa">将整型转换为字符串。

* 函数原型：

```
const char* tk_itoa (char* str, int len, int n);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | const char* | 返回字符串。 |
| str | char* | 保存字符串缓冲区。 |
| len | int | 缓冲区大小。 |
| n | int | 要转换的整型。 |
#### tk\_lltoa 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_lltoa">将整型转换为字符串。

* 函数原型：

```
const char* tk_lltoa (char* str, int len, int64_t n);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | const char* | 返回字符串。 |
| str | char* | 保存字符串缓冲区。 |
| len | int | 缓冲区大小。 |
| n | int64\_t | 要转换的整型。 |
#### tk\_memcpy 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_memcpy">内存拷贝。
在地址对齐的情况下并且少于 64 个字节，效率会比 memcpy 要快，否则会退化为 memcpy。

* 函数原型：

```
void* tk_memcpy (void* dst, const void* src, uint32_t len);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | void* | 返回成功返回 dst 地址，失败返回 NULL。 |
| dst | void* | 目标字符串。 |
| src | const void* | 源字符串。 |
| len | uint32\_t | 拷贝长度。 |
#### tk\_memcpy16 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_memcpy16">拷贝数据2字节。

* 函数原型：

```
uint16_t* tk_memcpy16 (uint16_t* dst, uint16_t* src, uint32_t size);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | uint16\_t* | 返回设置好的buff。 |
| dst | uint16\_t* | 目标 |
| src | uint16\_t* | 源。 |
| size | uint32\_t | 个数。 |
#### tk\_memcpy32 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_memcpy32">拷贝数据4字节。

* 函数原型：

```
uint32_t* tk_memcpy32 (uint32_t* dst, uint32_t* src, uint32_t size);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | uint32\_t* | 返回设置好的buff。 |
| dst | uint32\_t* | 目标 |
| src | uint32\_t* | 源。 |
| size | uint32\_t | 个数。 |
#### tk\_memdup 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_memdup">内存拷贝。

* 函数原型：

```
void* tk_memdup (const void* data, uint32_t len);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | void* | 返回指向的向新的内存指针，如果失败则返回NULL。 |
| data | const void* | 原内存。 |
| len | uint32\_t | 长度。 |
#### tk\_memset16 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_memset16">设置数据2字节。

* 函数原型：

```
uint16_t* tk_memset16 (uint16_t* buff, uint16_t val, uint32_t size);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | uint16\_t* | 返回设置好的buff。 |
| buff | uint16\_t* | buff |
| val | uint16\_t | 值。 |
| size | uint32\_t | 个数。 |
#### tk\_memset24 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_memset24">设置数据3字节。

* 函数原型：

```
uint32_t* tk_memset24 (uint32_t* buff, void* val, uint32_t size);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | uint32\_t* | 返回设置好的buff。 |
| buff | uint32\_t* | buff。 |
| val | void* | 值。 |
| size | uint32\_t | 个数。 |
#### tk\_memset32 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_memset32">设置数据4字节。

* 函数原型：

```
uint32_t* tk_memset32 (uint32_t* buff, uint32_t val, uint32_t size);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | uint32\_t* | 返回设置好的buff。 |
| buff | uint32\_t* | buff。 |
| val | uint32\_t | 值。 |
| size | uint32\_t | 个数。 |
#### tk\_normalize\_key\_name 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_normalize_key_name">标准化key_name

* 函数原型：

```
const char* tk_normalize_key_name (const char* name, char* fixed_name);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | const char* | 返回标准化后的字符串。 |
| name | const char* | key\_name。 |
| fixed\_name | char* | 保存标准化后的字符串。 |
#### tk\_pixel\_copy 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_pixel_copy">已bpp字节为标准拷贝数据。

* 函数原型：

```
void* tk_pixel_copy (void* dst, const void* src, uint32_t size, uint8_t bpp);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | void* | 返回设置好的buff。 |
| dst | void* | 目标 |
| src | const void* | 源。 |
| size | uint32\_t | 个数。 |
| bpp | uint8\_t | 单个数据的字节数。 |
#### tk\_pointer\_from\_int 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_pointer_from_int">将int转换成指针。

> 常用于将int类型的数据作为回调函数的ctx。

* 函数原型：

```
void* tk_pointer_from_int (int32_t v);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | void* | 返回对应的指针。 |
| v | int32\_t | 整数。 |
#### tk\_pointer\_to\_int 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_pointer_to_int">将指针转换成int。

>与tk_pointer_from_int配套使用，也就是pointer本身必须就是整数，而不是指针，否则pointer会被截断。

* 函数原型：

```
int32_t tk_pointer_to_int (const void* p);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | int32\_t | 返回对应的int数据。 |
| p | const void* | 指针。 |
#### tk\_qsort 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_qsort">快速排序。

* 函数原型：

```
ret_t tk_qsort (void** array, size_t nr, tk_compare_t cmp);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| array | void** | 数据。 |
| nr | size\_t | 元素个数。 |
| cmp | tk\_compare\_t | 比较函数。 |
#### tk\_replace\_char 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_replace_char">替换字符。

* 函数原型：

```
char* tk_replace_char (char* str, char from, char to);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | char* | 返回str。 |
| str | char* | 原字符串。 |
| from | char | 被替换的字符。 |
| to | char | 替换成的字符。 |
#### tk\_replace\_locale 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_replace_locale">将文本中的$locale$替换为对应的语言。

* 函数原型：

```
ret_t tk_replace_locale (const char* name, char* out, const char* locale);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| name | const char* | 文本。 |
| out | char* | 替换后保存的字符串。 |
| locale | const char* | 语言。 |
#### tk\_set\_ui\_thread 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_set_ui_thread">设置UI线程的ID。

* 函数原型：

```
ret_t tk_set_ui_thread (uint64_t ui_thread_id);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| ui\_thread\_id | uint64\_t | UI线程的ID。 |
#### tk\_skip\_to\_num 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_skip_to_num">跳过字符串函数，如：字符串"hello123world"，返回的结果是"123world"。

* 函数原型：

```
const char* tk_skip_to_num (const char* str);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | const char* | 返回转换后的字符串。 |
| str | const char* | 要输入的原始字符串。 |
#### tk\_snprintf 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_snprintf">将可变参数(...)按照format格式化字符串，并将字符串复制到str中。

* 函数原型：

```
int tk_snprintf (char* str, size_t size, const char* format);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | int | 返回格式化后的字符串长度。 |
| str | char* | 目标字符串。 |
| size | size\_t | 拷贝字节数。 |
| format | const char* | 格式化字符串。 |
#### tk\_sscanf 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_sscanf">从字符串读取格式化输入。

* 函数原型：

```
int tk_sscanf (const char* str, const char* format);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | int | 返回成功匹配和赋值的个数。 |
| str | const char* | 要输入的字符串。 |
| format | const char* | 格式化字符串。 |
#### tk\_str\_append 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_str_append">字符串追加函数。

* 函数原型：

```
ret_t tk_str_append (char* str, uint32_t max_len, const char* s);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| str | char* | 被追加字符串。 |
| max\_len | uint32\_t | 字符串的最大长度。 |
| s | const char* | 要追加的字符串。 |
#### tk\_str\_copy 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_str_copy">字符串拷贝函数。
> XXX: 要求dst为NULL或内存块的首地址，本函数调用之后，dst可能无效，请保留返回的地址
该函数会自动申请内存，调用后需要使用TKMEM_FREE释放。

* 函数原型：

```
char* tk_str_copy (char* dst, const char* src);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | char* | 返回指向的复制字符串指针，如果失败则返回NULL。 |
| dst | char* | 目标字符串。 |
| src | const char* | 源字符串。 |
#### tk\_str\_end\_with 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_str_end_with">检查字符串是否以指定的字符串appendix结尾。

* 函数原型：

```
bool_t tk_str_end_with (const char* str, const char* appendix);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | bool\_t | 返回TRUE表示是；否则表示不是。 |
| str | const char* | 要检查字符串。 |
| appendix | const char* | 被检查的字符串。 |
#### tk\_str\_is\_in\_array 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_str_is_in_array">检查字符串是否在字符串数组中。

* 函数原型：

```
bool_t tk_str_is_in_array (const char* str, const char** str_array, uint32_t array_size);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | bool\_t | 返回TRUE表示在，否则表示不在。 |
| str | const char* | 字符串。 |
| str\_array | const char** | 字符串数组。 |
| array\_size | uint32\_t | 字符串数组中字符串的个数。 |
#### tk\_str\_start\_with 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_str_start_with">检查字符串是否以指定的字符串prefix开头。

* 函数原型：

```
bool_t tk_str_start_with (const char* str, const char* prefix);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | bool\_t | 返回TRUE表示是；否则表示不是。 |
| str | const char* | 要检查字符串。 |
| prefix | const char* | 被检查的字符串。 |
#### tk\_str\_tolower 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_str_tolower">将大写字母转换为小写字母。
>修改传入的字符串。

* 函数原型：

```
char* tk_str_tolower (char* str);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | char* | 返回转换后的小写字母字符串。 |
| str | char* | 要被转换成小写字母的字符串。 |
#### tk\_str\_totitle 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_str_totitle">将单词首字母转换为大写字母。
>修改传入的字符串。

* 函数原型：

```
char* tk_str_totitle (char* str);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | char* | 返回转换后的字符串。 |
| str | char* | 被转换的字符串。 |
#### tk\_str\_toupper 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_str_toupper">将小写字母转换为大写字母。
>修改传入的字符串。

* 函数原型：

```
char* tk_str_toupper (char* str);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | char* | 返回转换后的大写字母字符串。 |
| str | char* | 要被转换成大写字母的字符串。 |
#### tk\_strcmp 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_strcmp">字符串比较函数。

* 函数原型：

```
int32_t tk_strcmp (const char* a, const char* b);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | int32\_t | 如果返回值=-1，则表示a为NULL；如果返回值=1，则表示b为NULL；如果返回值<0，则表示a小于b；如果返回值>0，则表示a大于b；如果返回值=0，则表示a等于b。 |
| a | const char* | 要进行比较的第一个字符串。 |
| b | const char* | 要进行比较的第二个字符串。 |
#### tk\_strcpy 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_strcpy">将src所指向的字符串复制到dst。

* 函数原型：

```
char* tk_strcpy (char* dst, const char* src);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | char* | 返回目标字符串。 |
| dst | char* | 目标字符串。 |
| src | const char* | 原字符串。 |
#### tk\_strdup 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_strdup">字符串拷贝函数。

* 函数原型：

```
char* tk_strdup (const char* str);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | char* | 返回指向的复制字符串指针，如果失败则返回NULL。 |
| str | const char* | 原字符串。 |
#### tk\_stricmp 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_stricmp">字符串比较函数（不区分大小写）。

* 函数原型：

```
int32_t tk_stricmp (const char* a, const char* b);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | int32\_t | 如果返回值=-1，则表示a为NULL；如果返回值=1，则表示b为NULL；如果返回值<0，则表示a小于b；如果返回值>0，则表示a大于b；如果返回值=0，则表示a等于b。 |
| a | const char* | 要进行比较的第一个字符串。 |
| b | const char* | 要进行比较的第二个字符串。 |
#### tk\_strlen 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_strlen">获取字符串的长度。str为空时返回0。

* 函数原型：

```
uint32_t tk_strlen (const char* str);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | uint32\_t | 返回字符串的长度。 |
| str | const char* | 字符串。 |
#### tk\_strncpy 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_strncpy">将src所指向的字符串复制到dst，最多复制len个字符串，并在[len]位置添加'\0'。

> 请确保dst的长度>=(len+1)

* 函数原型：

```
char* tk_strncpy (char* dst, const char* src, size_t len);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | char* | 返回目标字符串。 |
| dst | char* | 目标字符串。 |
| src | const char* | 源字符串。 |
| len | size\_t | 要复制的字符串个数。 |
#### tk\_strncpy\_s 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_strncpy_s">将src所指向的字符串复制到dst，最多复制min(dst_len-1, src_len)个字符串，并在[len]位置添加'\0'。
如果 dst 和 src 的地址对齐的话，效率会比 strncpy 高，如果小于 64 个字节的话，效率是最高的。

* 函数原型：

```
char* tk_strncpy_s (char* dst, size_t dst_len, const char* src, size_t src_len);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | char* | 返回目标字符串。 |
| dst | char* | 目标字符串。 |
| dst\_len | size\_t | 目标字符串内存长度。 |
| src | const char* | 源字符串。 |
| src\_len | size\_t | 要复制的字符串个数。 |
#### tk\_strndup 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_strndup">字符串拷贝函数，最多复制len个字符串。

* 函数原型：

```
char* tk_strndup (const char* str, uint32_t len);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | char* | 返回指向的复制字符串指针，如果失败则返回NULL。 |
| str | const char* | 原字符串。 |
| len | uint32\_t | 要复制的字符串个数。 |
#### tk\_strnlen 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_strnlen">获取字符串的长度。str为空时返回0。

* 函数原型：

```
uint32_t tk_strnlen (const char* str, uint32_t maxlen);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | uint32\_t | 返回字符串的长度。 |
| str | const char* | 字符串。 |
| maxlen | uint32\_t | 最大长度。 |
#### tk\_strrstr 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_strrstr">从后往前查找指定的字符串。

* 函数原型：

```
const char* tk_strrstr (const char* str, const char* substr);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | const char* | 返回字符串的位置或者NULL。 |
| str | const char* | 字符串。 |
| substr | const char* | 子字符串。 |
#### tk\_strtoi 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_strtoi">将字符串转换为整型。

* 函数原型：

```
int32_t tk_strtoi (const char* str, const char** end, int base);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | int32\_t | 返回转换后的整型。 |
| str | const char* | 要转换为整型的字符串。 |
| end | const char** | 对类型char*的对象的引用。 |
| base | int | 基数。 |
#### tk\_strtol 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_strtol">将字符串转换为长整型。

* 函数原型：

```
long tk_strtol (const char* str, const char** end, int base);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | long | 返回转换后的长整型。 |
| str | const char* | 要转换为长整型的字符串。 |
| end | const char** | 对类型char*的对象的引用。 |
| base | int | 基数。 |
#### tk\_strtoll 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_strtoll">将字符串转换为长整型。

* 函数原型：

```
int64_t tk_strtoll (const char* str, const char** end, int base);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | int64\_t | 返回转换后的长整型。 |
| str | const char* | 要转换为长整型的字符串。 |
| end | const char** | 对类型char*的对象的引用。 |
| base | int | 基数。 |
#### tk\_under\_score\_to\_camel 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_under_score_to_camel">将下划线名字转成驼峰名字。

* 函数原型：

```
const char* tk_under_score_to_camel (const char* name, char* out, uint32_t max_out_size);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | const char* | 返回T驼峰名字。 |
| name | const char* | 下划线名字。 |
| out | char* | 驼峰名字(保存结果)。 |
| max\_out\_size | uint32\_t | 结果最大长度。 |
#### tk\_vsnprintf 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_vsnprintf">将可变参数ap按照format格式化字符串，并将字符串复制到str中。

* 函数原型：

```
int tk_vsnprintf (char* str, size_t size, const char* format, va_list ap);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | int | 返回格式化后的字符串长度。 |
| str | char* | 目标字符串。 |
| size | size\_t | 拷贝字节数。 |
| format | const char* | 格式化字符串。 |
| ap | va\_list | 可变参数。 |
#### tk\_watob 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_watob">将宽字符串转换为布尔类型。

* 函数原型：

```
bool_t tk_watob (const wchar_t* str);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | bool\_t | 返回转换后的布尔类型。 |
| str | const wchar\_t* | 要转换为布尔类型的宽字符串。 |
#### tk\_watof 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_watof">将宽字符串转换为浮点类型。

* 函数原型：

```
double tk_watof (const wchar_t* str);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | double | 返回转换后的浮点类型。 |
| str | const wchar\_t* | 要转换为浮点类型的宽字符串。 |
#### tk\_watoi 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_watoi">将宽字符串转换为整型。

* 函数原型：

```
int tk_watoi (const wchar_t* str);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | int | 返回转换后的整型。 |
| str | const wchar\_t* | 要转换为整型的宽字符串。 |
#### tk\_watoi\_n 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_watoi_n">将宽字符串转换为整型。

* 函数原型：

```
int tk_watoi_n (const wchar_t* str, uint32_t len);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | int | 返回转换后的整型。 |
| str | const wchar\_t* | 要转换为整型的宽字符串。 |
| len | uint32\_t | 字符串长度。 |
#### tk\_wild\_card\_match 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_wild_card_match">简单的通配符匹配。*匹配0到多个字符，?匹配1个字符。
示例：

```c
tk_wild_card_match("*c", "abc");
tk_wild_card_match("a?c", "abc");
```

* 函数原型：

```
bool_t tk_wild_card_match (const char* pattern, const char* str);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | bool\_t | 返回TRUE表示匹配，否则表示不匹配。 |
| pattern | const char* | 字符串。 |
| str | const char* | 字符串。 |
#### tk\_wstr\_count\_c 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_wstr_count_c">统计UCS字符串中某个字符出现的次数。

* 函数原型：

```
uint32_t tk_wstr_count_c (const wchar_t* str, wchar_t c);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | uint32\_t | 返回字符出现的次数。 |
| str | const wchar\_t* | 字符串。 |
| c | wchar\_t | 字符。 |
#### tk\_wstr\_dup\_utf8 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_wstr_dup_utf8">将utf8字符串拷贝为UCS字符串。

* 函数原型：

```
wchar_t* tk_wstr_dup_utf8 (const char* str);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | wchar\_t* | 返回UCS字符串(需要调用TKMEM\_FREE释放)。 |
| str | const char* | utf8编码的字符串。 |
#### tk\_wstrcmp 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_wstrcmp">字符串比较函数。

* 函数原型：

```
int32_t tk_wstrcmp (const wchar_t* a, const wchar_t* b);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | int32\_t | 如果返回值=-1，则表示a为NULL；如果返回值=1，则表示b为NULL；如果返回值<0，则表示a小于b；如果返回值>0，则表示a大于b；如果返回值=0，则表示a等于b。 |
| a | const wchar\_t* | 要进行比较的第一个字符串。 |
| b | const wchar\_t* | 要进行比较的第二个字符串。 |
#### tk\_wstrdup 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_wstrdup">宽字符串拷贝函数。

* 函数原型：

```
wchar_t* tk_wstrdup (const wchar_t* str);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | wchar\_t* | 返回指向的复制宽字符串指针，如果失败则返回NULL。 |
| str | const wchar\_t* | 原宽字符串。 |
#### tk\_wstricmp 函数
-----------------------

* 函数功能：

> <p id="utils_t_tk_wstricmp">字符串比较函数（不区分大小写）。

* 函数原型：

```
int32_t tk_wstricmp (const wchar_t* a, const wchar_t* b);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | int32\_t | 如果返回值=-1，则表示a为NULL；如果返回值=1，则表示b为NULL；如果返回值<0，则表示a小于b；如果返回值>0，则表示a大于b；如果返回值=0，则表示a等于b。 |
| a | const wchar\_t* | 要进行比较的第一个字符串。 |
| b | const wchar\_t* | 要进行比较的第二个字符串。 |
#### xml\_file\_expand\_read 函数
-----------------------

* 函数功能：

> <p id="utils_t_xml_file_expand_read">expand include process instruction to file content: <?include filename="view_me.inc" ?>

* 函数原型：

```
ret_t xml_file_expand_read (const char* filename, str_t* s);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | ret\_t | 返回RET\_OK表示成功，否则表示失败。 |
| filename | const char* | 文件名。 |
| s | str\_t* | 用于返回内容。 |

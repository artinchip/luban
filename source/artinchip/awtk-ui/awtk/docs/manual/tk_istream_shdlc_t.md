## tk\_istream\_shdlc\_t
### 概述
![image](images/tk_istream_shdlc_t_0.png)

reliable istream base on simple HDLC
----------------------------------
### 函数
<p id="tk_istream_shdlc_t_methods">

| 函数名称 | 说明 | 
| -------- | ------------ | 
| <a href="#tk_istream_shdlc_t_tk_istream_shdlc_create">tk\_istream\_shdlc\_create</a> | 创建istream对象。 |
### 属性
<p id="tk_istream_shdlc_t_properties">

| 属性名称 | 类型 | 说明 | 
| -------- | ----- | ------------ | 
| <a href="#tk_istream_shdlc_t_retry_times">retry\_times</a> | uint8\_t | 失败重传次数。 |
| <a href="#tk_istream_shdlc_t_timeout">timeout</a> | uint32\_t | 读写超时时间(ms) |
#### tk\_istream\_shdlc\_create 函数
-----------------------

* 函数功能：

> <p id="tk_istream_shdlc_t_tk_istream_shdlc_create">创建istream对象。

> 只能由iostream_shdlc调用。

* 函数原型：

```
tk_istream_t* tk_istream_shdlc_create (tk_iostream_shdlc_t* iostream);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | tk\_istream\_t* | 返回istream对象。 |
| iostream | tk\_iostream\_shdlc\_t* | iostream对象。 |
#### retry\_times 属性
-----------------------
> <p id="tk_istream_shdlc_t_retry_times">失败重传次数。

* 类型：uint8\_t

#### timeout 属性
-----------------------
> <p id="tk_istream_shdlc_t_timeout">读写超时时间(ms)

* 类型：uint32\_t


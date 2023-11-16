## tcp\_t
### 概述

----------------------------------
### 函数
<p id="tcp_t_methods">

| 函数名称 | 说明 | 
| -------- | ------------ | 
| <a href="#tcp_t_tk_tcp_accept">tk\_tcp\_accept</a> | 监听指定端口，成功返回sock句柄。 |
| <a href="#tcp_t_tk_tcp_connect">tk\_tcp\_connect</a> | 连接到指定服务器。 |
| <a href="#tcp_t_tk_tcp_listen">tk\_tcp\_listen</a> | 监听指定端口，成功返回sock句柄。 |
#### tk\_tcp\_accept 函数
-----------------------

* 函数功能：

> <p id="tcp_t_tk_tcp_accept">监听指定端口，成功返回sock句柄。

* 函数原型：

```
int tk_tcp_accept (int sock);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | int | 返回sock句柄。 |
| sock | int | socket句柄。 |
#### tk\_tcp\_connect 函数
-----------------------

* 函数功能：

> <p id="tcp_t_tk_tcp_connect">连接到指定服务器。

* 函数原型：

```
int tk_tcp_connect (const char* host, int port);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | int | 返回sock句柄。 |
| host | const char* | 主机名或IP地址。 |
| port | int | 端口号。 |
#### tk\_tcp\_listen 函数
-----------------------

* 函数功能：

> <p id="tcp_t_tk_tcp_listen">监听指定端口，成功返回sock句柄。

* 函数原型：

```
int tk_tcp_listen (int port);
```

* 参数说明：

| 参数 | 类型 | 说明 |
| -------- | ----- | --------- |
| 返回值 | int | 返回sock句柄。 |
| port | int | 端口号。 |

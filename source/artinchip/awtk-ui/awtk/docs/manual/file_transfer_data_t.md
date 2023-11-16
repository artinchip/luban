## file\_transfer\_data\_t
### 概述
文件数据包(sender->receiver)。
----------------------------------
### 属性
<p id="file_transfer_data_t_properties">

| 属性名称 | 类型 | 说明 | 
| -------- | ----- | ------------ | 
| <a href="#file_transfer_data_t_crc">crc</a> | uint32\_t | 本次传输文件数据的CRC。 |
| <a href="#file_transfer_data_t_offset">offset</a> | uint32\_t | 本次传输文件数据的偏移量。 |
| <a href="#file_transfer_data_t_size">size</a> | uint32\_t | 本次传输文件数据的长度。 |
| <a href="#file_transfer_data_t_type">type</a> | uint32\_t | 包的类型。 |
#### crc 属性
-----------------------
> <p id="file_transfer_data_t_crc">本次传输文件数据的CRC。

* 类型：uint32\_t

| 特性 | 是否支持 |
| -------- | ----- |
| 可直接读取 | 是 |
| 可直接修改 | 是 |
#### offset 属性
-----------------------
> <p id="file_transfer_data_t_offset">本次传输文件数据的偏移量。

* 类型：uint32\_t

| 特性 | 是否支持 |
| -------- | ----- |
| 可直接读取 | 是 |
| 可直接修改 | 是 |
#### size 属性
-----------------------
> <p id="file_transfer_data_t_size">本次传输文件数据的长度。

* 类型：uint32\_t

| 特性 | 是否支持 |
| -------- | ----- |
| 可直接读取 | 是 |
| 可直接修改 | 是 |
#### type 属性
-----------------------
> <p id="file_transfer_data_t_type">包的类型。

* 类型：uint32\_t

| 特性 | 是否支持 |
| -------- | ----- |
| 可直接读取 | 是 |
| 可直接修改 | 是 |

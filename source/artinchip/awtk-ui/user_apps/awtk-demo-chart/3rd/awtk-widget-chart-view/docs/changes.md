# 最新动态

* v1.1.0
  * series_fifo_data 改为 series_data，内部数据结构前缀改为series_data_；
  * series_fifo 添加 series_fifo_set_block_event 接口，添加 will 事件，统一事件流程为 will 事件返回 stop 终止；
  * 完善 series_fifo_event 事件结构，不同事件独立结构；
  * 将时间坐标轴的 recent_index 属性删除，改为通过回调函数传入；
  * series 控件新增 animator_create 属性；
  * 完善 series_fifo_default 接口。

* v1.0.4
  * 修改 object_fifo、object_fifo_default 为 series_fifo、series_fifo_default，并完善相关接口；
  * 新增 series_set_fifo 接口和 series_set_prepare_fifo 接口；
  * 完善时间坐标轴。

* v1.0.3
  * 完善 object_fifo 以及 object_fifo_default；
  * 新增 object_fifo_data；
  * 完善示例程序。

* v1.0.2
  * 将 fifo 改为 object\_fifo，新增派生类 object\_fifo\_default 以方便 mvvm 使用；
  * 根据 object\_fifo 完善 series 控件的绘制逻辑，将绘制代码放到 object\_fifo 的回调函数中；
  * 新增时间坐标轴；
  * 完善示例程序。

* v1.0.1
  * 新增 series\_clear() 接口；
  * 修复 series\_count() 接口返回值类型有误的问题；
  * 完善 axis.h 的注释。

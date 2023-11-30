# AWTK 简要说明

本项目的编译工具是CMAKE，依托AIC SDK进行编译。配置AWTK 参数需要在AIC SDK 根目录使用命令

使用**make menuconfig** 进入可视化界面进行配置对SDK进行配置。具体使用可以参考AIC docs

## 1. 配置说明

配置所在路径：在AIC SDK 根目录使用命令make menuconfig 进入可视化界面，选择AWTK GUI

```sh
ArtInChip Luban SDK Configuration
    ArtInChip packages  -->
    [*] awtk-ui  ----

```

配置项只供选择是否启用AWTK，选择启用后，编译AWTK 并编译 chart demo，然后默认开机后运行 chart demo例程

### 1.1 默认配置说明

**不建议**修改默认配置，一般情况，简单修改**启动脚本**即可满足资源配置和打印调试信息输出要求。

AWTK 资源路径默认设置为**当前路径**，也就是宏 **APP_RES_ROOT = ./** 。如果需要修改APP 路径，在./CMakeLists.txt 添加代码 add_definitions(-DAPP_RES_ROOT = /sdcard/)。

AWTK 日志等级默认为，**debug**等级。debug 等级这里由于和AIC 的宏定义冲突了，将原先的DEBUG 改为了TK_DEBUG。如果需要修改日志等级为 info等级，添加宏 "TK_DEBUG"，在./CMakeLists.txt 添加代码 add_definitions(-DTK_DEBUG)

修改AWTK源码部分代码：awtk\src\awtk_main.inc 中
```c
#ifdef TK_NDEBUG   // 原先是NDEBUG
  log_set_log_level(LOG_LEVEL_INFO);
#else
  log_set_log_level(LOG_LEVEL_DEBUG);
#endif /*TK_NDEBUG*/
  log_info("Build at: %s %s\n", __DATE__, __TIME__);
```

如果需要自己更改AWTK 配置，可以参考 ./CMakeLists.txt 文件，参考代码
```c
# 这里是添加AWTK 编译的信息
add_definitions(-DWITH_STB_IMAGE)
add_definitions(-DWITH_STB_FONT)
add_definitions(-DWITH_TEXT_BIDI=1)
add_definitions(-DWITH_DEC_IMAGE)
add_definitions(-DWITH_AIC_G2D)
#add_definitions(-DTK_DEBUG)
#add_definitions(-DAPP_RES_ROOT = /sdcard/)
```

### 1.2 启动脚本说明

启动脚本在：../../../package/artinchip/awtk-ui/S00test_awtk
打开AWTK 后，系统启动后默认运行启动脚本内容。对于里面内容不需要修改，仅仅需要修改**切换路径的命令**和**输出信息的路径**即可。

关键信息：
```sh
cd /usr/local/share/awtk_data/res # 切换到资源路径
#PID=`$DAEMON $DAEMONOPTS > /dev/console 2>&1 & echo $!` # 将AWTK 打印信息到控制台
PID=`$DAEMON $DAEMONOPTS > /dev/null 2>&1 & echo $!`
```

    cd /usr/local/share/awtk_data/res
cd 命令用于切换所在目录。因为AWTK 资源路径默认设置为**当前路径**，只有切换到资源所在路径才能正常运行。

    PID=`$DAEMON $DAEMONOPTS > /dev/stderr 2>&1 & echo $!`
这个命令用于输出调试信息到控制台，输出DEBUG 等级信息，用于调试

    PID=`$DAEMON $DAEMONOPTS > /dev/null 2>&1 & echo $!`
这个命令用于丢弃调试信息，使用这个命令后，AWTK 将不会有任何输出到控制台

### 1.3 内存配置

AWTK 使用到GE和VE。GE用于2D加速，VE用于图片解码。依赖dma-buf、CMA。一般而言，使用`默认配置`即可。默认配置下，dma-buf、CMA默认打开。一般情况只根据使其情况配置**CMA Size**。CMA Size 主要用于存储解码后的图片和额外分配一块屏幕内存。

在SDK根目录下执行 make kernel-menuconfig，进入kernel的功能配置，按如下选择：

```sh
Linux
    Memory Management options
         [*] Contiguous Memory Allocator
```

CMA区域的大小配置，如下默认配置为 16MB，建议修改为20M，可以根据具体需求来配置size大小
```sh
Linux
    Library routines
        [*] DMA Contiguous Memory Allocator
        (20)  Size in Mega Bytes
```

打开 dma-buf
在SDK根目录下执行 make kernel-menuconfig，进入kernel的功能配置，按如下选择：

```sh
Linux
    Device Drivers
        DMABUF options
            [*] Explicit Synchronization Framework
            [*]   Sync File Validation Framework
            [*] userspace dmabuf misc driver
            [*] DMA-BUF Userland Memory Heaps
                [*]   DMA-BUF CMA Heap
```

## 2.编译说明

AWTK编译用到的所有相关宏均定义在 ./CMakeList.txt 中

相关宏的说明可以参考：awtk/docs/porting_common.md

如果需要编译自己的 app，需要修改./CMakeList.txt。AWTK Designed 生成的资源文件也需要做出一些调整，不需要的资源可以不进行安装。例如仅仅留下默认主题文件（default\raw）。安装资源到板子可以参考下面代码：资源默认安装到 /usr/local/share 目录下

```sh
# set project
set(PRJ_APP demo_char)

# set user app path
set(USER_APPS_ROOT  ${CMAKE_CURRENT_SOURCE_DIR}/user_apps)
set(USER_DEMO_CHART ${USER_APPS_ROOT}/awtk-demo-chart)

## demo source file
set(DEMO_SOURCE_FILES "")

# add user 3rd
set(USER_DEMO_3RD ${USER_DEMO_CHART}/3rd/awtk-widget-chart-view/src)
file(GLOB DEMO_3RD_SRC
    ${USER_DEMO_3RD}/*.c
    ${USER_DEMO_3RD}/base/*.c
    ${USER_DEMO_3RD}/chart_view/*.c
    ${USER_DEMO_3RD}/pie_slice/*.c
)
set(DEMO_SOURCE_FILES ${DEMO_SOURCE_FILES} ${DEMO_3RD_SRC})

# add user src
set(USER_DEMO_SRC ${USER_DEMO_CHART}/src)
file(GLOB USER_DEMO_SRC
    ${USER_DEMO_SRC}/*.c
    ${USER_DEMO_SRC}/pages/*.c
    ${USER_DEMO_SRC}/common/*.c
)
set(DEMO_SOURCE_FILES ${DEMO_SOURCE_FILES} ${USER_DEMO_SRC})


# install resource data to /usr/local/share
if(DEFINED CMAKE_INSTALL_FULL_DATAROOTDIR)
    install(DIRECTORY user_apps/awtk-demo-chart/res/assets/default/raw
    DESTINATION ${CMAKE_INSTALL_FULL_DATAROOTDIR}/awtk_data/res/assets/default) # 仅安装需要的资源到板子中，其他不安装，默认安装到 /usr/local/share 目录下
endif() # CMAKE_INSTALL_FULL_DATAROOTDIR

```

## 3. 功能支持说明
luban SDK支持AWTK的绝大部分功能，支持部分三方功能，详细可以看./CMakeList.txt

关键支持功能如下：
- 拼音输入法，包含软键盘
- 动画功能，动画缓冲
- unicode 换行算法
- 双向排列算法
- 文件系统
- STB 库解码，包括truetype字体解码
- CSV 文件读写

支持的三方库如下：
- miniz
- ubjson
- cjson
- hal
- mbedtls

额外注意：

高效旋转目前不支持，主要原因是g2d_rotate_image_ex() 和 g2d_blend_image_rotate() 接口还没进行对接。启用文件系统情况下，不要定义宏 `WITH_FAST_LCD_PORTRAIT` 即可。关于高效旋转更多请看文档：./awtk/docs/how_to_use_fast_lcd_portrait.md。

## 4 优化建议

### 4.1 运行速度优化建议

- 提前加载图片资源到内存中（assets_manager_preload）
- 将图片数据存放到 /usr/local/share 区，从SD 卡读取数据较慢
- 增大图片缓冲区。使用函数image_manager_set_max_mem_size_of_cached_images(image_manager(), 1366430)，以增大缓冲区。

### 4.2 优化内存使用

- 将frame buffer格式由RGB888/ARGB8888 改变为RGB565
- 使用image_manager_set_max_mem_size_of_cached_images(image_manager(), 1366430) 设置合理的缓冲区大小。
- image_manager_unload_unused(image_manager(), 1); 配置从图片管理器中卸载指定时间内没有使用的图片。
- 关闭窗口动画缓存。

关于图片资源内存的优化建议
- 使用更低分辨率的图片
- 如果不需要使用到透明度，可以将32位图转换为24/16位图
- JPEG 格式相比PNG格式更省内存，但没有透明度。
- JPEG和PNG 格式选择，可以根据 [5. 硬件图片解码格式进行选择](#5硬件图片解码格式支持)



## 5.硬件图片解码格式支持

关于格式描述方式：AWTK和AIC的格式的字节序都是小端，但是格式名称描述不一样，比如：AIC 格式描述为ABGR_8888 对应 AWTK 格式描述为RGBA8888。本文档和配置文件均按照AIC 的格式描述进行描述。

PNG格式不支持硬件解码为RGB565格式，JPEG格式仅支持解码为YUV格式。

支持格式：
|       图片格式       | RGB565  | RGB888/ARGB8888 |
| --------------------| --------| ----------------|
|        PNG          |  不支持  |       支持      |
|       JEPG/JPG      |  不支持  |      不支持     |


在硬件解码需要注意两点：
- PNG 格式，原图是什么格式，将解码成什么格式。如果想要减少内存的使用，可以将使用原来PNG 24位的图片转换为16位图片
- JGP/JGEG 格式，解码为YUV 格式，然后将YUV 格式转换为RGB 格式以供AWTK 使用。

推荐图片使用：在默认配置，`动画功能打开`的情况下。如果关闭了动画功能或者内存比较紧张的情况下，请参考上面解码支持和图片使用注意点自行选择。

推荐使用32位 PNG图片，这样解码效率最高

## 6. 对接流程

除了图片解码的流程和AWTK 标准流程不一样，其他流程都是按照AWTK 的软件流程进行对接的。可以参考AWTK 官方的移植文档和这里的对接源码。由于Linux AWTK基本已经对接好很多接口了，文件系统，系统时钟，多线程都不需要进行额外的对接。仅仅对接好显示设备、输入设备、硬件加速和解码部分即可。

对接源码结构：
```sh
    -> aic_g2d   # 加速(GE)和解码(VE)部分对接代码
        -> aic_dec_asset_frame.c # AIC 图片解码帧内存申请和图片头信息解析
        -> aic_dec_asset.c # AIC 硬件图片解码实现，使用VE 进行实现
        -> aic_g2d.c # AIC G2D和图片格式转换实现，使用GE 进行实现
        -> aic_graphic_buffer.c # AIC graphic buffer实现，AWTK 基于此结构体进行具体图片信息管理
        -> aic_linux_mem.c # AIC GE 和VE 需要用到的CMA 内存管理

    -> input_thread # 触摸对接代码
        -> touch_thread.c # 触摸模块代码，目前仅用到这个
        -> input_dispatcher.c # 触摸调试代码

    -> lcd_linux # lcd 对接代码
        -> fb_info.h # 获取fb 信息实现
        -> lcd_linux_fb.c # lcd 移植具体实现

    -> main_loop_linux.c # 自己实现的mai_loop 函数
    -> devices.c # 设备加载接口
```

图片解码流程做了一些调整，因为硬件解码需要CMA 内存，如果CMA 内存不够软件解码。关于CMA 内存也可以理解为资源的一部分，资源分配失败就返回。

**AIC 图片硬件解码流程**：

获取资源 + 硬件解码 -> 图片格式转化 -> 加入管理器进行管理

**AWTK 图片软件解码流程**：

获取资源 -> STB 软件解码 + 图片格式转化 -> 加入管理器进行管理

AIC 硬件解码和AWTK 软件解码的区别就是，在获取资源函数的时候就进行硬件解码。如果解码失败，则交由软件再次进行资源获取和进行下一步的调用STB库进行解码。

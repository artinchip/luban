# V1.2.9 #

## 新增 ##
- USB：支持DTS中配置阻抗匹配参数
- QtLauncher：集成WiFi manager、QtKeyboard（支持中文）
- 新增p、m两个shell命令，和Luban-Lite保持同样的用法
- 打包镜像：支持SM2、SM4算法的签名
- AiPQ工具：支持通过UART调试MIPI DSI屏幕
- Qt：增加Framebuffer旋转功能
- Target GDB：升级版本号为V14.2
- RTP：增加X、Y坐标的镜像接口
- 打包：支持解包、重新打包的场景需求
- test-wdt：增加持续喂狗的功能
- 新增WiFi支持：aic8800、asr5505s、rtl8733bu、rtl8733bs支持BT功能
- 新增第三方包：libmodbus、tcpdump、avahi、libdaemon、v4l-utils
- 新增示例：test-coredump、p2p_auto

## 优化 ##
- 启动：精简启动过程中的log
- SPI：优化系统高负载时的DMA处理流程
- I2C：优化不同速率的超时机制
- 烧写：优化NOR的烧写速度
- PBP：增强SID的写保护机制；退出PBP前关闭UART
- Audio：优化动态音量调节的效果
- UART：配置参数期间进入loopback模式
- 打包：自动计算分区大小以限制Jffs2镜像的size
- SPI：优化互斥锁的使用流程
- RTC：解耦SYS_BAK功能，将其移到WRI模块，RTC可随时关闭

## 修改 ##
- LVGL：调整启动脚本中的RTP校准处理
- Disp：修正关闭CONFIG_PM时的编译依赖
- GE：完善YUV格式的兼容性检查
- MPP：修正PNG解码中的刷Cache处理
- USB：修正OHCI休眠的处理流程
- SPI：修正全双工的CPU模式处理流程
- ADB：启动脚本中支持停止adbd服务
- 烧写：完善sparse格式区分的异常情况处理
- toolchain：增加'-mno-dup-loop-header'优化选项
- SD卡：完善SD卡自动挂载的处理逻辑
- WiFi：更新ASR5505固件，适配了ASR5532u
- MPP：
  - 修正wav文件播放时的seek处理
  - 修正某些场景下的播放启动失败问题
- env：修正eMMC方案中分区名称冲突的问题
- PM：默认打开休眠功能，可降低系统功耗
- Audio：修正全局数组的边界处理
- 启动：默认挂载mnt目录为tmpfs，解决NOR方案中rootfs只读时的写需求



# V1.2.7 #

## 新增##
- LVGL：新增支持V9.0版本，并适配了AicUIBuilder
- 调屏：支持和 AiPQ V1.1.1 工具配合使用
- SPI NAND：支持双Device ID的外设
- SPI ENC：支持NAND介质的烧写和启动
- Secuity：支持固件加密
- PBP：支持关闭PBP启动时的log
- aicupg：支持通过USB/UART获取设备侧的运行log；支持用命令进入U-Boot 升级模式
- PM：支持休眠时进入DDR自刷新
- DVP：支持外接AHD的视频采集方案；test-dvp支持旋转再显示
- 新增驱动：EPWM、CAP、TP2825
- 新增SPI NAND：F35SQB004G、GD5F1GM7xUExxG、ZB35Q04A
- 新增第三方包：fscrypt、host-go、libcurl
- 新增示例：test-spi
- 新增工具支持：内核的size分析、CPU perf分析、烤机测试
- MPP：支持AVI、MKV文件格式,支持RTSP流媒体协议
- Qt：支持H264视频播放
- 烧写：SD卡烧写界面增加进度条显示
- GMAC：增加自测功能、增加Tx数据统计信息
- OneStep：新增list_module命令，可查看当前已经打开的模块清单
- 新增器件支持：
- WiFi：AIC8800
- NAND：GD5F1GM7UEYIG
- 新增第三方包：libtirpc（Busybox的NFS依赖）

##优化##
- LVGL：支持旋转、透明色的硬件加速；当没有FB设备时正常退出；整体性能优化
- Audio：优化关闭功放时的噪声处理；减少处理延迟
- DE：LVDS的双link可以单独配置参数
- GMAC：优化数据传输通路，性能有大幅提升
- OTA：优化兼容性和稳定性；支持eMMC介质
- DDR：优化DDR参数提升稳定性和兼容性；优化内存拷贝性能
- SPI：优化DMA传输的结束状态判断
- I2C：优化传输信号的延迟配置
- MPP：优化H264解码器的容错处理
- USB：优化mismatch的处理流程
- Tsensor：优化温度的校准精度
- DE：优化Scaler的算法系数
- i优化.ko动态加载的速度
- MPP：优化VE解码时的容错处理
- SD卡：优化自动挂载的处理逻辑
- NOR方案：优化demo88 NOR的内存配置
- U-Boot：优化串口putc的异常处理逻辑
- OneStep：在Luban和Luban-Lite SDK环境来回切换时，先清理之前的OneStep命令

##修改##
- RTC：修改LDO11为0.9V（稳定性更好）
- Qt：默认打开keyboard/mouse/qtjpeg/qtpng/qtfreetype，同步更新prebuilt包
- 工具链：升级版本到V2.10.1，解决个别.ko加载时的链接报错问题
- SPIENC：修正坏块的处理逻辑
- test-dvp：修改默认格式为NV16
- source/artinchip中的代码统一使用Apache-2.0协议
- OTA：多个方案的recovery分区大小需调大，解决烧写失败问题
- OneStep：修正list_module功能的运行环境，使用SDK自带的Python3环境
- LVGL：修正部分情况下的stride参数配置
- demo88 NOR：修改rootfs分区格式为squashfs，解决烧写后不能启动的问题
- MPP：完善MKV解析的配置项
- Qt launcher：删除prebuilt包，每次编译采用源码编译
- UART：Rx的IO增加上拉属性
- SPL：修正userid开启后出现的一次画面闪烁问题
- GMAC：修正组播时的地址过滤；修正IEEE1588的参数配置
- TSensor：修正高低温告警的阈值计算
- toolchain：删除过时的工具链包：
- riscv64-linux-x86_64-20210512.tar.gz
- riscv64-linux-glibc-x86_64-V2.10.0.tar.gz
- test_adc：改从sysfs节点获取slope、offset参数

# V1.2.3 #

##新增##

- 新增驱动模块：PSADC、EPWM、rx8025t
- USB Gadget：增加f_iap功能
- 新增器件支持：
   - NAND：xtx
   - WiFi：asr5505、sv6x5x
- 新增第三方包：msnlink/zjinnova/Carbit的投屏方案适配、GDB、libpjsip、libsrtp、libatomic_ops
- 新增方案：demo88 nor musl
- 默认打开SD卡、U盘的自动挂载功能

##优化##

- 功耗优化：包括驱动、PBP等
- Audio：支持Fade in/out
- LVGL：优化透明度的处理性能；支持缩放和任意角度旋转的组合；
- 启动流程全面支持自适应DDR Size处理
- NOR方案：支持自动计算分区信息
- USB：增加外部阻抗的参数配置
- SPI：支持配置Sample Delay、Rx Delay
- SID：增加时序参数可配置
- DMA：增强通道参数的有效性检查
- package：支持子目录、外部源码目录的编译管理
- i2c-tools：支持10bit模式
- 简化U-Boot的DTS配置，合并board-u-boot.dtsi到board.dts

##修改##

- DDR Init：默认打开展频功能，便于ESD测试
- MPP：修正部分MP4文件的播放错误
- test_fb增加"-b"参数显示渐变色块，方便验证硬件环境
- test_gpio增加output的配置功能


# V1.2.0 #

## 新增 ##
- MPP：新增seek功能；支持aac音频格式
- OTA：支持eMMC方案
- 支持通过DTS设置U-Boot pin
- 新增示例：keyadc-test、gpio-test
- 新增第三方包：lame
- OneStep：增加buildall、rebuildall命令
## 优化 ##
- MIPI屏xm91080支持reset pin配置、backlight控制
- 支持从Efuse解析GMAC MAC地址
- FB：支持从sysfs配置LCD时序；支持设置旋转角度
- LVGL Demo：支持旋转/缩放后再显示；支持build-in图片的模式；支持3 Framebuffer（默认关闭）
## 修改 ##
- UserID：默认关闭此功能，以节省启动时间；支持从空的分区解析ID
- demo128：默认打开UART4/5/6
- MPP：修正编码器的一处内存泄漏；修正PTS解析错误；优化文件IO访问方式；ve编译为动态库
- USB：修正USB Host模块在Reboot过程中的非法指针问题
- 根目录中的scripts重命名为tools，移动其中的gcc相关源码包到dl目录aicupg命令改用reboot命令进行重启

# V1.1.9c #
## 新增 ##
- 支持U盘升级（USB0）
- USB线刷、SD卡升级过程增加了logo图片显示
- 第三方package：
	- QT5（仅编译配置文件，不含源码）、cairo、pixman
	- gstreamer1及aic插件
	- GDB和GDBServer（仅有预编译好的二进制，不含源码）
	- 其他：OpenSSH、perl、libgpiod
- 新增NAND型号：GD5F1GQ5UExxG
- OneStep：增加快速打开menuconfig的me、km、um命令
- 新增WRI模块的驱动，方便从sysfs节点获取上一次的启动原因

## 优化 ##
- 功耗管理：完善屏、USB等模块的休眠过程，系统休眠功耗降到400mW左右
- LVGL：升级版本到V8.3.2，性能更优
- LVGL demo：优化透明背景的填充方式；增加中文字库支持
- QT4：完成与openssl-1.1.1的适配
- DDR3：优化DDR3的启动参数，提升稳定性
- CMU：优化父时钟的频率计算精度

## 修改 ##
- DMA：修正多通道在并发时的中断状态误判问题
- DE：在初始化时增加40ms延迟，解决第一帧图的屏闪问题
- 完善Tsen、uart、usb多个模块的lock处理流程
- mpp：使用COM marker方式进行数据填充
- GPIO：增加功能0作为Disable状态；PN口重命名为PU，和Spec同步
- pinmux冲突检查：支持pins1、pins2这样的pin节点配置

# V1.1.9a #
## 新增 ##
- 支持安全启动。
- 新增OTA，支持从本地、SD卡、U盘、网络进行系统升级。
- 支持系统级休眠（CPU进入Idle模式），支持GPIO中断唤醒。
- 支持烧号模式。
- 显示：支持内置0/90/180/270°的旋转显示。
- 新增NAND型号支持：F50L1G41XA、F50L2G41XA。
- 新增WiFi型号支持：rtl8821cs。
- 新增MIPI DSI屏支持：hx8394。
- 新增方案：demo100_nand。
- SID：增加Tsen的校准参数nvmem接口。
- 打包过程中会针对不同封装进行模块级别的有效性检查。
- Tsen：新增notify接口，可支持高温保护。

## 优化 ##

- SPI：优化NAND的访问速度。
- DMA：优化DMA虚拟通道的管理、增加DDMA通道管理，以提升处理速度。
- RTC：优化校准精度到0.3Hz。
- SPL：快速启动模式也支持logo显示。

## 修改 ##

- SPI：最高工作频率提升到133MHz（部分外设可支持）。
- 显示：修正Pixclk频率，满足60帧的显示效果。
- 方案 demo_spinand 重命名为 demo88_nand。

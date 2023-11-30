
# V1.2.2 #

## 新增 ##
- Display：支持在线调屏
- MPP：支持mp4封装格式
- U-Boot：支持MIPI驱动；支持xz压缩格式；支持DDR Size自适应处理
- FS：用户态支持jffs2、squashfs
- 提供独立编译 linux/uboot/dtb 的示例脚本
- 新增器件支持：

  - NAND：BYTe BY5F1GQ5UAYIG
  - NOR：gd25q256
  - panel：sw070wv20
- 新增第三方包：sqlite、cJSON、freetype、libkcapi-1.4、pulseaudio、libsamplerate、预编译4个常用的Benchmark、
- 新增方案：demo88 NOR
- OneStep新增命令：del_board

## 优化 ##
- USB：支持OTG的动态切换
- PBP：支持Boot pin功能

## 修改 ##
- SPL：提升size限制为128KB
- AWTK：修正若干Bug，并进行多处优化
- LVGL：重构目录结构，支持freetype字体
- Falcon：修正SD卡启动时的处理流程
- 编译：NAND方案默认不再生成4K block版本的镜像；打包原始DTS文件
- SPINAND分区 Image 大小默认从image_cfg.json 中获取，
- 测试示例规范命名格式为：test_*


# V1.2.1 #

## 新增 ##
- 新增AWTK的支持
- 新增几款mipi屏的支持
- 新增test-blkdev
- 新增 .gitignore
## 优化 ##
- SPL容量策略 
- GStreamer 播放格式添加
## 修改 ##
- OneStep 工具移植到tools目录

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
